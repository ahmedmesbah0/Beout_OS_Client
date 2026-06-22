#include "api_server.hpp"
#include <json.hpp>
#include <iostream>
#include <random>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include "../activation/activation_manager.hpp"
#include "../activation/machine_id.hpp"

using json = nlohmann::json;

namespace {

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

} // namespace

namespace beout_os {
namespace api {

ApiServer::ApiServer(const std::string& cert_path, const std::string& private_key_path, std::shared_ptr<database::DatabaseManager> db)
    : db_(std::move(db)) {
    server_ = std::make_unique<httplib::SSLServer>(cert_path.c_str(), private_key_path.c_str());
    
    // Seed default admin password if not already present in the database
    std::string existing_hash = db_->get_config("admin_password_hash").value_or("");
    if (existing_hash.empty()) {
        db_->set_config("admin_password_hash", hash_password("admin"));
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
    // Serve static files from the React app
    server_->set_mount_point("/", "../dashboard/dist");
    
    // CORS Preflight
    server_->Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 204;
    });

    // Helper to check authentication
    auto check_auth = [&](const httplib::Request& req, httplib::Response& res) -> bool {
        if (!req.has_header("Authorization")) {
            res.status = 401;
            res.set_content(json{{"error", "Unauthorized"}}.dump(), "application/json");
            return false;
        }
        std::string auth_header = req.get_header_value("Authorization");
        std::lock_guard<std::mutex> lock(session_mutex_);
        if (auth_header != "Bearer " + current_session_token_ || current_session_token_.empty()) {
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
        std::string server_url = db_->get_config("license_server_url").value_or("https://update.beout.ai");
        std::string host = server_url;
        int port = 443;
        if (host.rfind("https://", 0) == 0) {
            host = host.substr(8);
            port = 443;
        } else if (host.rfind("http://", 0) == 0) {
            host = host.substr(7);
            port = 80;
        }
        size_t colon_pos = host.find(':');
        if (colon_pos != std::string::npos) {
            try {
                port = std::stoi(host.substr(colon_pos + 1));
            } catch (...) {}
            host = host.substr(0, colon_pos);
        }
        try {
            httplib::Client cli(host, port);
            if (port == 443) {
                cli.enable_server_certificate_verification(false);
            }
            cli.set_connection_timeout(1, 0);
            cli.set_read_timeout(1, 0);
            if (auto r = cli.Get("/api/health")) {
                server_online = true;
            } else if (auto r2 = cli.Get("/api/license")) {
                server_online = true;
            } else if (auto r3 = cli.Get("/")) {
                server_online = true;
            }
        } catch (...) {}

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
                res.set_content(json{{"token", current_session_token_}}.dump(), "application/json");
            } else {
                res.status = 401;
                res.set_content(json{{"error", "Invalid credentials"}}.dump(), "application/json");
            }
        } catch (const json::parse_error&) {
            res.status = 400;
            res.set_content(json{{"error", "Invalid JSON"}}.dump(), "application/json");
        }
    });

    // Configuration API
    server_->Get("/api/config", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;

        std::string json_str = db_->get_config("network_interfaces_json").value_or("");
        json interfaces;
        if (json_str.empty()) {
            // Seed defaults from legacy keys or hardcoded values
            std::string wan_dev = db_->get_config("network_WAN_interface").value_or("eth0");
            std::string wan_ip = db_->get_config("network_WAN_ip").value_or("192.168.1.100");
            std::string wan_netmask = db_->get_config("network_WAN_netmask").value_or("255.255.255.0");
            std::string wan_gateway = db_->get_config("network_WAN_gateway").value_or("192.168.1.1");

            std::string lan_dev = db_->get_config("network_LAN_interface").value_or("eth1");
            std::string lan_ip = db_->get_config("network_LAN_ip").value_or("10.0.0.1");
            std::string lan_netmask = db_->get_config("network_LAN_netmask").value_or("255.255.255.0");

            std::string mgmt_dev = db_->get_config("network_MGMT_interface").value_or("eth2");
            std::string mgmt_ip = db_->get_config("network_MGMT_ip").value_or("192.168.100.99");
            std::string mgmt_netmask = db_->get_config("network_MGMT_netmask").value_or("255.255.255.0");
            std::string mgmt_gateway = db_->get_config("network_MGMT_gateway").value_or("192.168.100.1");

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
            {"interfaces", interfaces}
        };
        res.set_content(response.dump(), "application/json");
    });

    // Configuration POST API
    server_->Post("/api/config", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;

        try {
            auto body = json::parse(req.body);
            if (body.contains("interfaces") && body["interfaces"].is_array()) {
                db_->set_config("network_interfaces_json", body["interfaces"].dump());

                // Sync back to legacy configurations for compatibility with CLI client
                for (auto& item : body["interfaces"]) {
                    std::string id = item.value("id", "");
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
                std::system("sudo /opt/beout_os/bin/sync_network.sh &");
            }
            res.set_content(json{{"status", "success"}}.dump(), "application/json");
        } catch (const json::parse_error&) {
            res.status = 400;
            res.set_content(json{{"error", "Invalid JSON"}}.dump(), "application/json");
        }
    });

    // Trigger manual update check API
    server_->Post("/api/update/check", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;

        // Run check_updates.sh in the background
        std::system("sudo /opt/beout_os/bin/check_updates.sh &");
        
        res.set_content(json{{"status", "triggered"}}.dump(), "application/json");
    });

    // License Status API
    server_->Get("/api/license", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;

        std::string status = db_->get_config("activation_status").value_or("INACTIVE");
        std::string key = db_->get_config("activation_license_key").value_or("");
        std::string machine_id = beout_os::activation::MachineId::get();
        std::string server_url = db_->get_config("license_server_url").value_or("https://update.beout.ai");
        std::string verify_ssl = db_->get_config("license_server_verify_ssl").value_or("1");

        json response = {
            {"status", status},
            {"license_key", key},
            {"machine_id", machine_id},
            {"license_server_url", server_url},
            {"license_server_verify_ssl", verify_ssl}
        };
        res.set_content(response.dump(), "application/json");
    });

    // License Activation API
    server_->Post("/api/license/activate", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;

        try {
            auto body = json::parse(req.body);
            std::string license_key = body.value("license_key", "");
            if (license_key.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "License key is required"}}.dump(), "application/json");
                return;
            }

            // Save user-provided Licensing Server Settings if they are passed
            if (body.contains("license_server_url")) {
                db_->set_config("license_server_url", body["license_server_url"]);
            }
            if (body.contains("license_server_verify_ssl")) {
                db_->set_config("license_server_verify_ssl", body["license_server_verify_ssl"]);
            }

            // Get server URL
            std::string server_url = db_->get_config("license_server_url").value_or("https://update.beout.ai");
            std::string machine_id = beout_os::activation::MachineId::get();

            // Prepare client HTTP request to Main Server
            // Parse host and port from URL
            std::string host = server_url;
            int port = 80;
            if (host.rfind("https://", 0) == 0) {
                host = host.substr(8);
                port = 443;
            } else if (host.rfind("http://", 0) == 0) {
                host = host.substr(7);
                port = 80;
            }
            
            size_t colon_pos = host.find(':');
            if (colon_pos != std::string::npos) {
                port = std::stoi(host.substr(colon_pos + 1));
                host = host.substr(0, colon_pos);
            }

            httplib::Client cli(host, port);
            
            // Connection Security: Enforce SSL/TLS certificate verification by default
            std::string verify_ssl = db_->get_config("license_server_verify_ssl").value_or("1");
            if (verify_ssl == "1") {
                cli.enable_server_certificate_verification(true);
                // If the administrator placed a private CA bundle, verify against it
                std::ifstream ca_file("/opt/beout_os/etc/server_ca.pem");
                if (ca_file.good()) {
                    cli.set_ca_cert_path("/opt/beout_os/etc/server_ca.pem");
                }
            } else {
                cli.enable_server_certificate_verification(false);
            }

            json req_payload = {{"machine_id", machine_id}, {"license_key", license_key}};
            auto s_res = cli.Post("/api/license/activate", req_payload.dump(), "application/json");

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
                } else {
                    // Fallback to developer key
                    pub_key = "-----BEGIN PUBLIC KEY-----\nMCowBQYDK2VwAyEAvaUOLMIWZZgDTNnYbTi3r4gpLhMXMgo4PqgXUj1Njmk=\n-----END PUBLIC KEY-----\n";
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
                res.set_content(err_msg, "application/json");
            }
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });
}

} // namespace api
} // namespace beout_os
