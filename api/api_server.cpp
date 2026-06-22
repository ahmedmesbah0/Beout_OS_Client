#include "api_server.hpp"
#include <json.hpp>
#include <iostream>
#include <random>
#include <fstream>
#include <sstream>
#include "../activation/activation_manager.hpp"
#include "../activation/machine_id.hpp"

using json = nlohmann::json;

namespace beout_os {
namespace api {

ApiServer::ApiServer(const std::string& cert_path, const std::string& private_key_path, std::shared_ptr<database::DatabaseManager> db)
    : db_(std::move(db)) {
    server_ = std::make_unique<httplib::SSLServer>(cert_path.c_str(), private_key_path.c_str());
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
    server_->Get("/api/health", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        
        std::string version = "1.0.0";
        std::ifstream version_file("/etc/beout_os_version");
        if (version_file.is_open()) {
            std::getline(version_file, version);
            // Trim whitespace/newline
            version.erase(version.find_last_not_of(" \t\r\n") + 1);
        }
        
        json response = {{"status", "ok"}, {"version", version}};
        res.set_content(response.dump(), "application/json");
    });

    // Login API
    server_->Post("/api/auth/login", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        try {
            auto body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");

            // For demo purposes, hardcode admin:admin
            if (username == "admin" && password == "admin") {
                // Generate a simple token
                std::lock_guard<std::mutex> lock(session_mutex_);
                current_session_token_ = "DEMO-SESSION-TOKEN-XYZ123";
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

        json response = {
            {"wan_interface", db_->get_config("network_WAN_interface").value_or("")},
            {"wan_ip", db_->get_config("network_WAN_ip").value_or("")},
            {"wan_netmask", db_->get_config("network_WAN_netmask").value_or("")},
            {"wan_gateway", db_->get_config("network_WAN_gateway").value_or("")},
            {"lan_interface", db_->get_config("network_LAN_interface").value_or("")},
            {"lan_ip", db_->get_config("network_LAN_ip").value_or("")},
            {"lan_netmask", db_->get_config("network_LAN_netmask").value_or("")},
            {"mgmt_interface", db_->get_config("network_MGMT_interface").value_or("")},
            {"mgmt_ip", db_->get_config("network_MGMT_ip").value_or("")},
            {"mgmt_netmask", db_->get_config("network_MGMT_netmask").value_or("")},
            {"mgmt_gateway", db_->get_config("network_MGMT_gateway").value_or("")}
        };
        res.set_content(response.dump(), "application/json");
    });

    // Configuration POST API
    server_->Post("/api/config", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;

        try {
            auto body = json::parse(req.body);
            if (body.contains("wan_interface")) db_->set_config("network_WAN_interface", body["wan_interface"]);
            if (body.contains("wan_ip")) db_->set_config("network_WAN_ip", body["wan_ip"]);
            if (body.contains("wan_netmask")) db_->set_config("network_WAN_netmask", body["wan_netmask"]);
            if (body.contains("wan_gateway")) db_->set_config("network_WAN_gateway", body["wan_gateway"]);
            
            if (body.contains("lan_interface")) db_->set_config("network_LAN_interface", body["lan_interface"]);
            if (body.contains("lan_ip")) db_->set_config("network_LAN_ip", body["lan_ip"]);
            if (body.contains("lan_netmask")) db_->set_config("network_LAN_netmask", body["lan_netmask"]);
            
            if (body.contains("mgmt_interface")) db_->set_config("network_MGMT_interface", body["mgmt_interface"]);
            if (body.contains("mgmt_ip")) db_->set_config("network_MGMT_ip", body["mgmt_ip"]);
            if (body.contains("mgmt_netmask")) db_->set_config("network_MGMT_netmask", body["mgmt_netmask"]);
            if (body.contains("mgmt_gateway")) db_->set_config("network_MGMT_gateway", body["mgmt_gateway"]);
            
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
        std::system("/opt/beout_os/bin/check_updates.sh &");
        
        res.set_content(json{{"status", "triggered"}}.dump(), "application/json");
    });

    // License Status API
    server_->Get("/api/license", [&](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (!check_auth(req, res)) return;

        std::string status = db_->get_config("activation_status").value_or("INACTIVE");
        std::string key = db_->get_config("activation_license_key").value_or("");
        std::string machine_id = beout_os::activation::MachineId::get();
        json response = {{"status", status}, {"license_key", key}, {"machine_id", machine_id}};
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

            // Get server URL
            std::string server_url = db_->get_config("license_server_url").value_or("https://updates.behorus.ai");
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
