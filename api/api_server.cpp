#include "api_server.hpp"
#include <json.hpp>
#include <iostream>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdio>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <regex>
#include <ctime>
#include <set>
#include <map>
#include <cstdlib>
#include <memory>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include "../activation/activation_manager.hpp"
#include "../activation/machine_id.hpp"

using json = nlohmann::json;

namespace {

const char* DEBUG_LOG_FILE = "/var/log/beout_os_api_debug.log";

std::string sha256_hash(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), hash);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

std::string generate_salt() {
    unsigned char salt_buf[16];
    if (RAND_bytes(salt_buf, sizeof(salt_buf)) != 1) {
        // Fallback to std::random_device if OpenSSL RAND fails
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<unsigned int> dis(0, 255);
        for (int i = 0; i < 16; ++i) {
            salt_buf[i] = dis(gen);
        }
    }
    std::stringstream ss;
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)salt_buf[i];
    }
    return ss.str();
}

std::string hash_password(const std::string& password) {
    std::string salt = generate_salt();
    return salt + "$" + sha256_hash(salt + password);
}

bool verify_password(const std::string& password, const std::string& db_hash_field) {
    size_t dollar_pos = db_hash_field.find('$');
    if (dollar_pos == std::string::npos) return false;
    std::string salt = db_hash_field.substr(0, dollar_pos);
    std::string hash = db_hash_field.substr(dollar_pos + 1);
    return sha256_hash(salt + password) == hash;
}

std::string generate_session_token() {
    unsigned char token_buf[32];
    if (RAND_bytes(token_buf, sizeof(token_buf)) != 1) {
        // Fallback
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<unsigned int> dis(0, 255);
        for (int i = 0; i < 32; ++i) {
            token_buf[i] = dis(gen);
        }
    }
    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)token_buf[i];
    }
    return ss.str();
}

void write_debug_log(const std::string& level, const std::string& message) {
    std::ofstream log(DEBUG_LOG_FILE, std::ios::app);
    if (!log.is_open()) return;
    std::time_t now = std::time(nullptr);
    char ts[32] = {0};
    std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    log << "[" << ts << "] " << level << ": " << message << std::endl;
}

std::string shell_quote(const std::string& value) {
    std::string quoted = "'";
    for (char c : value) {
        if (c == '\'') quoted += "'\\''";
        else quoted += c;
    }
    quoted += "'";
    return quoted;
}

struct ParsedUrl {
    std::string scheme;
    std::string host;
    int port;
    std::string base_path;
};

bool parse_server_url(const std::string& url, ParsedUrl& parsed, std::string& error) {
    std::string value = url;
    value.erase(0, value.find_first_not_of(" \t\r\n"));
    value.erase(value.find_last_not_of(" \t\r\n") + 1);
    while (!value.empty() && value.back() == '/') value.pop_back();

    parsed.scheme = "http";
    parsed.port = 80;
    if (value.rfind("https://", 0) == 0) {
        parsed.scheme = "https";
        parsed.port = 443;
        value = value.substr(8);
    } else if (value.rfind("http://", 0) == 0) {
        value = value.substr(7);
    }

    size_t path_pos = value.find('/');
    std::string authority = path_pos == std::string::npos ? value : value.substr(0, path_pos);
    parsed.base_path = path_pos == std::string::npos ? "" : value.substr(path_pos);
    while (!parsed.base_path.empty() && parsed.base_path.back() == '/') parsed.base_path.pop_back();

    size_t colon_pos = authority.rfind(':');
    parsed.host = authority;
    if (colon_pos != std::string::npos) {
        parsed.host = authority.substr(0, colon_pos);
        try {
            parsed.port = std::stoi(authority.substr(colon_pos + 1));
        } catch (...) {
            error = "Invalid port in server URL";
            return false;
        }
    }
    if (parsed.host.empty()) {
        error = "Missing host in server URL";
        return false;
    }
    return true;
}

// Helper: execute a shell command and capture stdout.
std::string exec_command(const std::string& cmd, int timeout_sec = 15) {
    (void)timeout_sec;
    write_debug_log("CMD", cmd);
    std::string result;
    FILE* pipe = popen((cmd + " 2>&1").c_str(), "r");
    if (!pipe) {
        write_debug_log("ERROR", "popen failed for command: " + cmd);
        return "ERROR: popen() failed";
    }

    char buf[1024];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        result += buf;
        if (result.size() > 65536) {
            result += "\n... (output truncated at 64KB)\n";
            break;
        }
    }
    int status = pclose(pipe);
    if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        write_debug_log("WARN", "command exited with code " + std::to_string(WEXITSTATUS(status)) + ": " + cmd);
    }
    return result;
}

} // namespace

namespace beout_os {
namespace api {

ApiServer::ApiServer(const std::string& cert_path, const std::string& private_key_path, std::shared_ptr<database::DatabaseManager> db)
    : db_(std::move(db)) {
    server_ = std::make_unique<httplib::SSLServer>(cert_path.c_str(), private_key_path.c_str());

    // Seed initial admin password if not already present
    std::string existing_hash = db_->get_config("admin_password_hash").value_or("");
    const char* force_default_password = std::getenv("BEOUT_OS_FORCE_DEFAULT_ADMIN_PASSWORD");
    if (existing_hash.empty() || (force_default_password && std::string(force_default_password) == "1")) {
        // Match the installer completion screen so first login works without shell access.
        std::string initial_password = "admin";
        std::string password_file_path = "/var/lib/beout_os/initial_admin_password";

        // Write file FIRST so it's available even if DB write fails
        int pw_fd = open(password_file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (pw_fd >= 0) {
            if (write(pw_fd, initial_password.c_str(), initial_password.size()) < 0 || write(pw_fd, "\n", 1) < 0) {
                write_debug_log("ERROR", "failed to write initial admin password file: " + password_file_path);
            }
            close(pw_fd);
            std::cerr << "========================================" << std::endl;
            std::cerr << "  INITIAL ADMIN PASSWORD SEEDED" << std::endl;
            std::cerr << "  Password saved to: " << password_file_path << std::endl;
            std::cerr << "  Change this password on first login." << std::endl;
            std::cerr << "========================================" << std::endl;
        } else {
            std::cerr << "WARNING: Failed to write initial admin password file: "
                      << password_file_path << " (errno=" << errno << ")" << std::endl;
        }

        // Store hash in DB after file is safely written
        db_->set_config("admin_password_hash", hash_password(initial_password));
        write_debug_log("AUTH", "admin password seeded to documented default");
    }

    setup_routes();
}

ApiServer::~ApiServer() {
    stop();
}

void ApiServer::start(const std::string& host, int port) {
    if (!server_->is_valid()) {
        std::cerr << "SSL Server has an error." << std::endl;
        return;
    }
    std::cout << "Starting API Server on https://" << host << ":" << port << std::endl;
    server_->listen(host.c_str(), port);
}

void ApiServer::stop() {
    if (server_ && server_->is_running()) {
        server_->stop();
    }
}

void ApiServer::setup_routes() {
    // Set default headers for CORS robustness
    server_->set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS, DELETE, PUT"},
        {"Access-Control-Allow-Headers", "Content-Type, Authorization"}
    });

    server_->set_logger([](const httplib::Request& req, const httplib::Response& res) {
        write_debug_log("REQ", req.remote_addr + " " + req.method + " " + req.path + " -> " + std::to_string(res.status));
    });

    // Serve static files from the React app
    server_->set_mount_point("/", "../dashboard/dist");
    
    // CORS Preflight
    server_->Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 204;
    });

    // Helper to check authentication
    auto check_auth = [&](const httplib::Request& req, httplib::Response& res) -> bool {
        if (!req.has_header("Authorization")) {
            write_debug_log("AUTH", "missing authorization for " + req.method + " " + req.path);
            res.status = 401;
            res.set_content(json{{"error", "Unauthorized"}}.dump(), "application/json");
            return false;
        }
        std::string auth_header = req.get_header_value("Authorization");
        std::lock_guard<std::mutex> lock(session_mutex_);
        if (auth_header != "Bearer " + current_session_token_ || current_session_token_.empty()) {
            write_debug_log("AUTH", "invalid session for " + req.method + " " + req.path);
            res.status = 401;
            res.set_content(json{{"error", "Invalid Session"}}.dump(), "application/json");
            return false;
        }
        return true;
    };

    // Health API
    server_->Get("/api/health", [&](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        
        std::string version = "1.0.0";
        std::ifstream version_file("/etc/beout_os_version");
        if (version_file.is_open()) {
            std::getline(version_file, version);
            // Trim whitespace/newline
            version.erase(version.find_last_not_of(" \t\r\n") + 1);
        }

        // Get WAN IP from interfaces configuration
        std::string wan_ip = "Unconfigured";
        std::string json_str = db_->get_config("network_interfaces_json").value_or("");
        if (!json_str.empty()) {
            try {
                auto interfaces = json::parse(json_str);
                for (auto& item : interfaces) {
                    if (item.value("id", "") == "wan") {
                        wan_ip = item.value("ip", "Unconfigured");
                        break;
                    }
                }
            } catch (...) {}
        }
        if (wan_ip == "Unconfigured" || wan_ip.empty()) {
            wan_ip = db_->get_config("network_WAN_ip").value_or("Unconfigured");
        }

        // Check Internet status
        bool internet_online = false;
        try {
            httplib::Client cli("http://1.1.1.1");
            cli.set_connection_timeout(1, 0);
            cli.set_read_timeout(1, 0);
            if (auto r = cli.Get("/")) {
                internet_online = true;
            }
        } catch (...) {}

        // Check licensing/update server status
        bool server_online = false;
        std::string server_url = db_->get_config("license_server_url").value_or("");

        // Only probe the server if a URL is configured
        if (!server_url.empty()) {
            ParsedUrl parsed;
            std::string parse_error;
            try {
                if (!parse_server_url(server_url, parsed, parse_error)) {
                    write_debug_log("HEALTH", "invalid server_url=" + server_url + " error=" + parse_error);
                } else {
                    httplib::Client cli(parsed.scheme + "://" + parsed.host + ":" + std::to_string(parsed.port));

                    // Respect user's SSL verification setting
                    std::string verify_ssl = db_->get_config("license_server_verify_ssl").value_or("1");
                    if (parsed.scheme == "https" && verify_ssl == "1") {
                        cli.enable_server_certificate_verification(true);
                        // Use custom CA bundle if provided
                        std::ifstream ca_file("/opt/beout_os/etc/server_ca.pem");
                        if (ca_file.good()) {
                            cli.set_ca_cert_path("/opt/beout_os/etc/server_ca.pem");
                        }
                    } else if (parsed.scheme == "https") {
                        cli.enable_server_certificate_verification(false);
                    }

                    cli.set_connection_timeout(1, 0);
                    cli.set_read_timeout(1, 0);
                    // Only probe /api/health — the standard health endpoint
                    if (auto r = cli.Get((parsed.base_path + "/api/health").c_str())) {
                        server_online = (r->status >= 200 && r->status < 500);
                    }
                }
            } catch (...) {}
        }

        // Get hostname
        std::string hostname = "beoutos";
        std::ifstream host_file("/etc/hostname");
        if (host_file.is_open()) {
            std::getline(host_file, hostname);
            hostname.erase(hostname.find_last_not_of(" \t\r\n") + 1);
        }

        json response = {
            {"status", "ok"},
            {"version", version},
            {"hostname", hostname},
            {"wan_ip", wan_ip},
            {"internet_status", internet_online ? "online" : "offline"},
            {"server_status", server_online ? "online" : "offline"}
        };
        write_debug_log("HEALTH", "version=" + version + " hostname=" + hostname + " wan_ip=" + wan_ip + " internet=" + (internet_online ? std::string("online") : std::string("offline")) + " server=" + (server_online ? std::string("online") : std::string("offline")));
        res.set_content(response.dump(), "application/json");
    });

    // Login API
    server_->Post("/api/auth/login", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        try {
            auto body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");

            std::string stored_hash = db_->get_config("admin_password_hash").value_or("");
            if (stored_hash.empty()) {
                stored_hash = hash_password("admin");
                db_->set_config("admin_password_hash", stored_hash);
            }

            if (username == "admin" && verify_password(password, stored_hash)) {
                std::lock_guard<std::mutex> lock(session_mutex_);
                current_session_token_ = generate_session_token();
                write_debug_log("AUTH", "admin login successful from " + req.remote_addr);
                res.set_content(json{{"token", current_session_token_}}.dump(), "application/json");
            } else {
                write_debug_log("AUTH", "admin login failed from " + req.remote_addr + " username=" + username);
                res.status = 401;
                res.set_content(json{{"error", "Invalid credentials"}}.dump(), "application/json");
            }
        } catch (const json::parse_error&) {
            res.status = 400;
            res.set_content(json{{"error", "Invalid JSON"}}.dump(), "application/json");
        }
    });

    // Helper: detect real system network interfaces
    auto detect_system_interfaces = []() -> std::vector<std::string> {
        std::vector<std::string> result;
        std::string output = exec_command("ls /sys/class/net/ 2>/dev/null");
        std::istringstream iss(output);
        std::string iface;
        while (std::getline(iss, iface)) {
            // Trim whitespace
            iface.erase(0, iface.find_first_not_of(" \t\r\n"));
            iface.erase(iface.find_last_not_of(" \t\r\n") + 1);
            if (!iface.empty() && iface != "lo") {
                result.push_back(iface);
            }
        }
        return result;
    };

    // Helper: validate device name exists on system
    auto validate_device_name = [&](const std::string& dev) -> bool {
        if (dev.empty()) return true; // empty is OK (unassigned)
        // Device names: alphanumerics, dots, dashes, underscores only
        for (char c : dev) {
            if (!std::isalnum(static_cast<unsigned char>(c)) &&
                c != '.' && c != '-' && c != '_') {
                return false;
            }
        }
        // Check if interface exists on system
        auto ifaces = detect_system_interfaces();
        return std::find(ifaces.begin(), ifaces.end(), dev) != ifaces.end();
    };

    // Configuration API
    server_->Get("/api/config", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;

        std::string json_str = db_->get_config("network_interfaces_json").value_or("");
        json interfaces;
        if (json_str.empty()) {
            // Seed defaults from legacy keys, falling back to DHCP (empty IP) defaults
            // Do NOT hardcode IP addresses — use empty (DHCP) to avoid conflicts
            std::string wan_dev = db_->get_config("network_WAN_interface").value_or("");
            std::string wan_ip = db_->get_config("network_WAN_ip").value_or("");
            std::string wan_netmask = db_->get_config("network_WAN_netmask").value_or("255.255.255.0");
            std::string wan_gateway = db_->get_config("network_WAN_gateway").value_or("");

            std::string lan_dev = db_->get_config("network_LAN_interface").value_or("");
            std::string lan_ip = db_->get_config("network_LAN_ip").value_or("");
            std::string lan_netmask = db_->get_config("network_LAN_netmask").value_or("255.255.255.0");

            std::string mgmt_dev = db_->get_config("network_MGMT_interface").value_or("");
            std::string mgmt_ip = db_->get_config("network_MGMT_ip").value_or("");
            std::string mgmt_netmask = db_->get_config("network_MGMT_netmask").value_or("255.255.255.0");
            std::string mgmt_gateway = db_->get_config("network_MGMT_gateway").value_or("");

            // Auto-detect interface names if not configured
            auto sys_ifaces = detect_system_interfaces();
            if (wan_dev.empty() && sys_ifaces.size() > 0) wan_dev = sys_ifaces[0];
            if (lan_dev.empty() && sys_ifaces.size() > 1) lan_dev = sys_ifaces[1];
            if (mgmt_dev.empty() && sys_ifaces.size() > 2) mgmt_dev = sys_ifaces[2];

            interfaces = json::array({
                {{"id", "wan"}, {"name", "wan1"}, {"device", wan_dev}, {"ip", wan_ip}, {"netmask", wan_netmask}, {"gateway", wan_gateway}, {"mgmt_access", true}},
                {{"id", "lan"}, {"name", "lan"}, {"device", lan_dev}, {"ip", lan_ip}, {"netmask", lan_netmask}, {"gateway", ""}, {"mgmt_access", false}},
                {{"id", "mgmt"}, {"name", "mgmt"}, {"device", mgmt_dev}, {"ip", mgmt_ip}, {"netmask", mgmt_netmask}, {"gateway", mgmt_gateway}, {"mgmt_access", true}}
            });
            db_->set_config("network_interfaces_json", interfaces.dump());
        } else {
            try {
                interfaces = json::parse(json_str);
            } catch (...) {
                interfaces = json::array();
            }
        }

        json response = {
            {"interfaces", interfaces},
            {"system_interfaces", detect_system_interfaces()},
            {"kernel_addresses", exec_command("ip -br addr show 2>/dev/null")},
            {"routes", exec_command("ip route show 2>/dev/null")}
        };
        write_debug_log("CONFIG", "returned network config interfaces=" + std::to_string(interfaces.size()));
        res.set_content(response.dump(), "application/json");
    });

    // Helper: validate IPv4 address format
    auto validate_ip = [](const std::string& ip) -> bool {
        if (ip.empty()) return true; // empty is OK (DHCP)
        std::regex ip_regex(R"(^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})$)");
        std::smatch m;
        if (!std::regex_match(ip, m, ip_regex)) return false;
        for (int i = 1; i <= 4; i++) {
            int octet = std::stoi(m[i]);
            if (octet > 255) return false;
        }
        return true;
    };

    // Helper: validate a string is safe for shell interpolation (allows alphanumerics, dots, dashes, underscores, forward slashes, colons)
    auto is_shell_safe = [](const std::string& s) -> bool {
        for (char c : s) {
            if (!std::isalnum(static_cast<unsigned char>(c)) &&
                c != '.' && c != '-' && c != '_' && c != '/' && c != ':') {
                return false;
            }
        }
        return !s.empty();
    };

    // Configuration POST API
    server_->Post("/api/config", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;

        try {
            auto body = json::parse(req.body);
            if (body.contains("interfaces") && body["interfaces"].is_array()) {
                // --- Validate all interfaces before saving ---
                std::set<std::string> seen_ips;
                std::set<std::string> seen_devices;
                for (auto& item : body["interfaces"]) {
                    std::string id = item.value("id", "");
                    std::string ip = item.value("ip", "");

                    // Normalize to lowercase
                    std::transform(id.begin(), id.end(), id.begin(), ::tolower);

                    // Validate IP format
                    if (!ip.empty() && !validate_ip(ip)) {
                        res.status = 400;
                        res.set_content(json{{"error", "Invalid IP address for " + id + ": " + ip}}.dump(), "application/json");
                        return;
                    }
                    std::string netmask = item.value("netmask", "");
                    if (!netmask.empty() && !validate_ip(netmask)) {
                        res.status = 400;
                        res.set_content(json{{"error", "Invalid netmask for " + id + ": " + netmask}}.dump(), "application/json");
                        return;
                    }
                    std::string gateway = item.value("gateway", "");
                    if (!gateway.empty() && !validate_ip(gateway)) {
                        res.status = 400;
                        res.set_content(json{{"error", "Invalid gateway for " + id + ": " + gateway}}.dump(), "application/json");
                        return;
                    }

                    // Check for duplicate IPs
                    if (!ip.empty()) {
                        if (seen_ips.count(ip)) {
                            res.status = 400;
                            res.set_content(json{{"error", "Duplicate IP address across interfaces: " + ip}}.dump(), "application/json");
                            return;
                        }
                        seen_ips.insert(ip);
                    }

                    // Validate device name exists on system
                    std::string device = item.value("device", "");
                    if (!device.empty() && !validate_device_name(device)) {
                        write_debug_log("CONFIG", "rejected unknown device=" + device + " id=" + id);
                        res.status = 400;
                        res.set_content(json{{"error", "Network interface '" + device + "' does not exist on this system"}}.dump(), "application/json");
                        return;
                    }
                    if (!device.empty()) {
                        if (seen_devices.count(device)) {
                            write_debug_log("CONFIG", "rejected duplicate device=" + device + " id=" + id);
                            res.status = 400;
                            res.set_content(json{{"error", "Duplicate adapter device across interfaces: " + device}}.dump(), "application/json");
                            return;
                        }
                        seen_devices.insert(device);
                    }
                }

                db_->set_config("network_interfaces_json", body["interfaces"].dump());
                write_debug_log("CONFIG", "saved network interface JSON: " + body["interfaces"].dump());

                // Sync back to legacy configurations for compatibility with CLI client
                for (auto& item : body["interfaces"]) {
                    std::string id = item.value("id", "");
                    std::transform(id.begin(), id.end(), id.begin(), ::tolower);
                    std::string device = item.value("device", "");
                    std::string ip = item.value("ip", "");
                    std::string netmask = item.value("netmask", "");
                    std::string gateway = item.value("gateway", "");

                    if (id == "wan") {
                        db_->set_config("network_WAN_interface", device);
                        db_->set_config("network_WAN_ip", ip);
                        db_->set_config("network_WAN_netmask", netmask);
                        db_->set_config("network_WAN_gateway", gateway);
                    } else if (id == "lan") {
                        db_->set_config("network_LAN_interface", device);
                        db_->set_config("network_LAN_ip", ip);
                        db_->set_config("network_LAN_netmask", netmask);
                    } else if (id == "mgmt") {
                        db_->set_config("network_MGMT_interface", device);
                        db_->set_config("network_MGMT_ip", ip);
                        db_->set_config("network_MGMT_netmask", netmask);
                        db_->set_config("network_MGMT_gateway", gateway);
                    }
                }

                // Trigger sync network configuration in OS background
                if (std::system("sudo /opt/beout_os/bin/sync_network.sh &") != 0) {
                    write_debug_log("ERROR", "failed to launch sync_network.sh");
                    std::cerr << "Warning: Failed to launch sync_network.sh background process." << std::endl;
                } else {
                    write_debug_log("CONFIG", "launched sync_network.sh");
                }
            }
            res.set_content(json{{"status", "success"}}.dump(), "application/json");
        } catch (const json::parse_error&) {
            write_debug_log("ERROR", "invalid JSON received by /api/config");
            res.status = 400;
            res.set_content(json{{"error", "Invalid JSON"}}.dump(), "application/json");
        }
     });
 
     // Trigger manual update check API
     server_->Post("/api/update/check", [&](const httplib::Request& req, httplib::Response& res) {
         res.set_header("Access-Control-Allow-Origin", "*");
         if (!check_auth(req, res)) return;
 
         // Run check_updates.sh in the background
         if (std::system("sudo /opt/beout_os/bin/check_updates.sh &") != 0) {
             std::cerr << "Warning: Failed to launch check_updates.sh background process." << std::endl;
         }
         
         res.set_content(json{{"status", "triggered"}}.dump(), "application/json");
     });

    // License Status API (public - unregistered devices need to read their status)
    server_->Get("/api/license", [&](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");

        std::string status = db_->get_config("activation_status").value_or("INACTIVE");
        std::string key = db_->get_config("activation_license_key").value_or("");
        std::string machine_id = beout_os::activation::MachineId::get();
        std::string server_url = db_->get_config("license_server_url").value_or("");
        std::string verify_ssl = db_->get_config("license_server_verify_ssl").value_or("1");

        // Read the appliance's own OS version from /etc/beout_os_version
        std::string os_version = "1.0.0";
        std::ifstream ver_file("/etc/beout_os_version");
        if (ver_file.is_open()) {
            std::getline(ver_file, os_version);
            // Trim whitespace
            os_version.erase(0, os_version.find_first_not_of(" \t\r\n"));
            os_version.erase(os_version.find_last_not_of(" \t\r\n") + 1);
        }

        json response = {
            {"status", status},
            {"license_key", key},
            {"machine_id", machine_id},
            {"license_server_url", server_url},
            {"license_server_verify_ssl", verify_ssl},
            {"os_version", os_version}
        };
        res.set_content(response.dump(), "application/json");
    });

    // License Activation API (public - no auth needed for initial registration)
    server_->Post("/api/license/activate", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");

        try {
            auto body = json::parse(req.body);
            std::string license_key = body.value("license_key", "");
            if (license_key.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "License key is required"}}.dump(), "application/json");
                return;
            }

            // Save user-provided Licensing Server Settings if they are passed
            if (body.contains("license_server_url") && !body["license_server_url"].empty()) {
                db_->set_config("license_server_url", body["license_server_url"]);
            }
            if (body.contains("license_server_verify_ssl")) {
                db_->set_config("license_server_verify_ssl", body["license_server_verify_ssl"]);
            }

            // Get server URL — must be explicitly configured
            std::string server_url = db_->get_config("license_server_url").value_or("");
            if (server_url.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "License server URL is not configured. Provide a license_server_url in the request."}}.dump(), "application/json");
                return;
            }
            std::string machine_id = beout_os::activation::MachineId::get();

            ParsedUrl parsed;
            std::string parse_error;
            if (!parse_server_url(server_url, parsed, parse_error)) {
                write_debug_log("LICENSE", "invalid license server URL=" + server_url + " error=" + parse_error);
                res.status = 400;
                res.set_content(json{{"error", parse_error}}.dump(), "application/json");
                return;
            }

            httplib::Client cli(parsed.scheme + "://" + parsed.host + ":" + std::to_string(parsed.port));
            cli.set_connection_timeout(8, 0);
            cli.set_read_timeout(15, 0);
            
            // Connection Security: Enforce SSL/TLS certificate verification by default
            std::string verify_ssl = db_->get_config("license_server_verify_ssl").value_or("1");
            if (parsed.scheme == "https" && verify_ssl == "1") {
                cli.enable_server_certificate_verification(true);
                // If the administrator placed a private CA bundle, verify against it
                std::ifstream ca_file("/opt/beout_os/etc/server_ca.pem");
                if (ca_file.good()) {
                    cli.set_ca_cert_path("/opt/beout_os/etc/server_ca.pem");
                }
            } else if (parsed.scheme == "https") {
                cli.enable_server_certificate_verification(false);
            }

            json req_payload = {{"machine_id", machine_id}, {"license_key", license_key}};
            std::string activate_path = parsed.base_path + "/api/license/activate";
            write_debug_log("LICENSE", "activation request host=" + parsed.host + " port=" + std::to_string(parsed.port) + " scheme=" + parsed.scheme + " path=" + activate_path + " verify_ssl=" + verify_ssl);
            auto s_res = cli.Post(activate_path.c_str(), req_payload.dump(), "application/json");

            if (s_res && s_res->status == 200) {
                auto s_body = json::parse(s_res->body);
                std::string token = s_body.value("activation_token", "");

                // Load verification public key
                std::string pub_key = "";
                std::ifstream pub_file("/opt/beout_os/etc/license_public_key.pem");
                if (pub_file.is_open()) {
                    std::stringstream ss;
                    ss << pub_file.rdbuf();
                    pub_key = ss.str();
                }

                if (pub_key.empty()) {
                    std::cerr << "ERROR: Public key file not found at /opt/beout_os/etc/license_public_key.pem. Cannot verify activation signature." << std::endl;
                    res.status = 500;
                    res.set_content(json{{"error", "Licensing public key is missing from the appliance. Contact your system administrator."}}.dump(), "application/json");
                    return;
                }

                beout_os::activation::ActivationManager act_mgr(db_, pub_key);
                if (act_mgr.apply_token(token)) {
                    db_->set_config("activation_license_key", license_key);
                    res.set_content(json{{"status", "success"}}.dump(), "application/json");
                } else {
                    res.status = 400;
                    res.set_content(json{{"error", "Cryptographic signature verification failed"}}.dump(), "application/json");
                }
            } else {
                res.status = s_res ? s_res->status : 502;
                std::string err_msg = s_res ? s_res->body : "{\"error\":\"Failed to connect to license server\"}";
                write_debug_log("LICENSE", "activation failed status=" + std::to_string(res.status) + " body=" + err_msg);
                res.set_content(err_msg, "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // GET Client Time & Timezone settings
    server_->Get("/api/time", [&](const httplib::Request& req, httplib::Response& res) {
        if (!check_auth(req, res)) return;

        std::string timezone = db_->get_config("system_timezone").value_or("UTC");
        std::string ntp_server = db_->get_config("system_ntp_server").value_or("pool.ntp.org");

        // Try reading actual OS timezone
        std::ifstream tz_file("/etc/timezone");
        if (tz_file.is_open()) {
            std::getline(tz_file, timezone);
            // Trim whitespace/newline
            timezone.erase(timezone.find_last_not_of(" \t\r\n") + 1);
        }

        json response = {
            {"timezone", timezone},
            {"ntp_server", ntp_server}
        };
        res.set_content(response.dump(), "application/json");
    });

    // POST Client Time & Timezone settings
    server_->Post("/api/time", [&](const httplib::Request& req, httplib::Response& res) {
        if (!check_auth(req, res)) return;

        try {
            auto body = json::parse(req.body);
            std::string timezone = body.value("timezone", "");
            std::string ntp_server = body.value("ntp_server", "");

            if (!timezone.empty()) {
                // Security: validate timezone is shell-safe and exists on disk
                if (!is_shell_safe(timezone)) {
                    res.status = 400;
                    res.set_content(json{{"error", "Invalid timezone format: only alphanumerics, dots, dashes, underscores, and forward slashes allowed"}}.dump(), "application/json");
                    return;
                }
                // Verify the timezone file exists before applying
                std::string tz_path = "/usr/share/zoneinfo/" + timezone;
                std::ifstream tz_check(tz_path.c_str());
                if (!tz_check.good()) {
                    res.status = 400;
                    res.set_content(json{{"error", "Unknown timezone: " + timezone}}.dump(), "application/json");
                    return;
                }
                db_->set_config("system_timezone", timezone);
                // System command to set timezone (timezone is validated safe above)
                std::string cmd = "timedatectl set-timezone " + timezone + " 2>/dev/null || ln -sf /usr/share/zoneinfo/" + timezone + " /etc/localtime";
                if (std::system(cmd.c_str()) != 0) {
                    std::cerr << "Warning: Failed to set timezone system settings." << std::endl;
                }
            }

            if (!ntp_server.empty()) {
                // Security: validate NTP server is shell-safe (hostname or IP)
                if (!is_shell_safe(ntp_server)) {
                    res.status = 400;
                    res.set_content(json{{"error", "Invalid NTP server format: only alphanumerics, dots, dashes, underscores, colons, and forward slashes allowed"}}.dump(), "application/json");
                    return;
                }
                db_->set_config("system_ntp_server", ntp_server);
                // System command to set NTP server in systemd-timesyncd config (ntp_server is validated safe above)
                std::ifstream t_file("/etc/systemd/timesyncd.conf");
                if (t_file.good()) {
                    std::string cmd = "sed -i 's/^#\\?NTP=.*/NTP=" + ntp_server + "/' /etc/systemd/timesyncd.conf && systemctl restart systemd-timesyncd 2>/dev/null";
                    if (std::system(cmd.c_str()) != 0) {
                        std::cerr << "Warning: Failed to update NTP server configuration." << std::endl;
                    }
                }
            }

            res.set_content(json{{"status", "success"}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ==========================================
    // DEBUG / DIAGNOSTICS API (auth required)
    // ==========================================

    // Helper: validate hostname/IP for diagnostic commands (allows alphanumerics, dots, dashes, colons)
    auto validate_host = [](const std::string& host) -> bool {
        if (host.empty()) return false;
        for (char c : host) {
            if (!std::isalnum(static_cast<unsigned char>(c)) &&
                c != '.' && c != '-' && c != ':' && c != '/') {
                return false;
            }
        }
        return true;
    };

    // Ping test
    server_->Post("/api/debug/ping", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;
        try {
            auto body = json::parse(req.body);
            std::string host = body.value("host", "");
            int count = body.value("count", 4);
            if (!validate_host(host)) {
                res.status = 400;
                res.set_content(json{{"error", "Invalid host"}}.dump(), "application/json");
                return;
            }
            if (count < 1 || count > 20) count = 4;
            std::string cmd = "ping -c " + std::to_string(count) + " -W 2 -- " + shell_quote(host);
            std::string output = exec_command(cmd, 30);
            res.set_content(json{{"command", cmd}, {"output", output}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Traceroute
    server_->Post("/api/debug/traceroute", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;
        try {
            auto body = json::parse(req.body);
            std::string host = body.value("host", "");
            if (!validate_host(host)) {
                res.status = 400;
                res.set_content(json{{"error", "Invalid host"}}.dump(), "application/json");
                return;
            }
            std::string cmd = "traceroute -m 15 -w 2 -- " + shell_quote(host);
            std::string output = exec_command(cmd, 30);
            res.set_content(json{{"command", cmd}, {"output", output}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // DNS lookup
    server_->Post("/api/debug/dns", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;
        try {
            auto body = json::parse(req.body);
            std::string host = body.value("host", "");
            if (!validate_host(host)) {
                res.status = 400;
                res.set_content(json{{"error", "Invalid host"}}.dump(), "application/json");
                return;
            }
            std::string cmd = "dig +short -- " + shell_quote(host);
            std::string output = exec_command(cmd, 10);
            res.set_content(json{{"command", cmd}, {"output", output}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Routing table
    server_->Get("/api/debug/routes", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;
        std::string output = exec_command("ip route show");
        res.set_content(json{{"command", "ip route show"}, {"output", output}}.dump(), "application/json");
    });

    // ARP table
    server_->Get("/api/debug/arp", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;
        std::string output = exec_command("ip neigh show");
        res.set_content(json{{"command", "ip neigh show"}, {"output", output}}.dump(), "application/json");
    });

    // Active connections
    server_->Get("/api/debug/connections", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;
        std::string output = exec_command("ss -tunap 2>/dev/null || netstat -tunap 2>/dev/null");
        res.set_content(json{{"command", "ss -tunap"}, {"output", output}}.dump(), "application/json");
    });

    // Service status
    server_->Get("/api/debug/services", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;
        std::string output = exec_command("systemctl list-units --type=service --state=running,failed --no-pager 2>/dev/null");
        res.set_content(json{{"command", "systemctl list-units"}, {"output", output}}.dump(), "application/json");
    });

    // System logs (last N lines)
    server_->Get("/api/debug/logs", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;
        std::string lines = req.get_param_value("lines");
        int n = 50;
        if (!lines.empty()) {
            try { n = std::stoi(lines); } catch (...) {}
            if (n < 1) n = 1;
            if (n > 500) n = 500;
        }
        std::string output = exec_command("journalctl -n " + std::to_string(n) + " --no-pager 2>/dev/null || tail -n " + std::to_string(n) + " /var/log/syslog 2>/dev/null || tail -n " + std::to_string(n) + " /var/log/messages 2>/dev/null");
        res.set_content(json{{"command", "journalctl -n " + std::to_string(n)}, {"output", output}}.dump(), "application/json");
    });

    // Firewall rules
    server_->Get("/api/debug/firewall", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;
        std::string output = exec_command("nft list ruleset 2>/dev/null || iptables -L -n -v 2>/dev/null");
        res.set_content(json{{"command", "nft list ruleset"}, {"output", output}}.dump(), "application/json");
    });

    // Process list
    server_->Get("/api/debug/processes", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;
        std::string output = exec_command("ps aux --sort=-%mem | head -50");
        res.set_content(json{{"command", "ps aux"}, {"output", output}}.dump(), "application/json");
    });

    // Full appliance debug logs and state bundle.
    server_->Get("/api/debug/full", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;
        json result = {
            {"api_log", exec_command("tail -n 1000 /var/log/beout_os_api_debug.log 2>/dev/null || true")},
            {"install_log", exec_command("tail -n 1000 /var/log/beout_install.log 2>/dev/null || tail -n 1000 /tmp/beout_install.log 2>/dev/null || true")},
            {"network_sync_log", exec_command("tail -n 1000 /var/log/beout_os_network_sync.log 2>/dev/null || true")},
            {"journal_api", exec_command("journalctl -u beout_os-api -n 300 --no-pager 2>/dev/null || true")},
            {"journal_networking", exec_command("journalctl -u networking -n 300 --no-pager 2>/dev/null || true")},
            {"interfaces_file", exec_command("cat /etc/network/interfaces /etc/network/interfaces.d/beout_os_interfaces 2>/dev/null || true")},
            {"kernel_addresses", exec_command("ip -d -br addr show 2>/dev/null")},
            {"kernel_links", exec_command("ip -d link show 2>/dev/null")},
            {"routes", exec_command("ip route show table all 2>/dev/null")},
            {"dns", exec_command("cat /etc/resolv.conf 2>/dev/null || true")},
            {"services", exec_command("systemctl --failed --no-pager 2>/dev/null || true")}
        };
        write_debug_log("DEBUG", "full debug bundle requested from " + req.remote_addr);
        res.set_content(result.dump(), "application/json");
    });

    // Constrained test-command runner for dashboard diagnostics only.
    server_->Post("/api/debug/test-command", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;
        try {
            auto body = json::parse(req.body);
            std::string name = body.value("command", "");
            std::string arg = body.value("arg", "");
            std::map<std::string, std::string> allowed = {
                {"ip-brief", "ip -br addr show"},
                {"ip-routes", "ip route show table all"},
                {"interfaces-file", "cat /etc/network/interfaces /etc/network/interfaces.d/beout_os_interfaces 2>/dev/null"},
                {"networking-status", "systemctl status networking --no-pager"},
                {"api-status", "systemctl status beout_os-api --no-pager"},
                {"api-logs", "journalctl -u beout_os-api -n 200 --no-pager"},
                {"network-logs", "tail -n 300 /var/log/beout_os_network_sync.log 2>/dev/null || true"},
                {"install-logs", "tail -n 300 /var/log/beout_install.log 2>/dev/null || tail -n 300 /tmp/beout_install.log 2>/dev/null || true"},
                {"ping", "ping -c 4 -W 2 -- " + shell_quote(arg.empty() ? "1.1.1.1" : arg)},
                {"dns", "dig +short -- " + shell_quote(arg.empty() ? "example.com" : arg)}
            };
            if (!allowed.count(name)) {
                write_debug_log("WARN", "rejected test command name=" + name + " from " + req.remote_addr);
                res.status = 400;
                res.set_content(json{{"error", "Command is not in the dashboard test allowlist"}}.dump(), "application/json");
                return;
            }
            if ((name == "ping" || name == "dns") && !validate_host(arg.empty() ? (name == "ping" ? "1.1.1.1" : "example.com") : arg)) {
                res.status = 400;
                res.set_content(json{{"error", "Invalid test argument"}}.dump(), "application/json");
                return;
            }
            std::string cmd = allowed[name];
            std::string output = exec_command(cmd, 30);
            write_debug_log("TEST", "dashboard test command=" + name + " arg=" + arg + " from " + req.remote_addr);
            res.set_content(json{{"command", cmd}, {"output", output}}.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Real system resources (replaces simulated dashboard data)
    server_->Get("/api/debug/resources", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;

        json result;

        // Uptime
        std::ifstream uptime_file("/proc/uptime");
        if (uptime_file.is_open()) {
            double up_seconds;
            uptime_file >> up_seconds;
            int days = (int)up_seconds / 86400;
            int hours = ((int)up_seconds % 86400) / 3600;
            int mins = ((int)up_seconds % 3600) / 60;
            result["uptime"] = std::to_string(days) + "d " + std::to_string(hours) + "h " + std::to_string(mins) + "m";
            result["uptime_seconds"] = (int)up_seconds;
        }

        // Memory
        std::ifstream meminfo("/proc/meminfo");
        if (meminfo.is_open()) {
            std::string line;
            long total = 0, available = 0;
            while (std::getline(meminfo, line)) {
                if (line.find("MemTotal:") == 0) total = std::stol(line.substr(line.find(':') + 1));
                if (line.find("MemAvailable:") == 0) available = std::stol(line.substr(line.find(':') + 1));
            }
            result["memory_total_mb"] = total / 1024;
            result["memory_available_mb"] = available / 1024;
            result["memory_used_mb"] = (total - available) / 1024;
            result["memory_percent"] = total > 0 ? (int)(((total - available) * 100) / total) : 0;
        }

        // CPU load
        std::ifstream loadavg("/proc/loadavg");
        if (loadavg.is_open()) {
            std::string line;
            std::getline(loadavg, line);
            std::istringstream iss(line);
            std::string l1, l5, l15;
            iss >> l1 >> l5 >> l15;
            result["load_1min"] = l1;
            result["load_5min"] = l5;
            result["load_15min"] = l15;
        }

        // Disk usage
        result["disk"] = exec_command("df -h --output=source,size,used,avail,pcent,target 2>/dev/null | tail -n +2");

        // Network interfaces
        result["interfaces"] = exec_command("ip -br addr show 2>/dev/null");

        res.set_content(result.dump(), "application/json");
    });

    // Network interface list (for validation)
    server_->Get("/api/debug/interfaces", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;
        std::string output = exec_command("ls /sys/class/net/ 2>/dev/null");
        res.set_content(json{{"command", "ls /sys/class/net/"}, {"output", output}}.dump(), "application/json");
    });
}

} // namespace api
} // namespace beout_os
