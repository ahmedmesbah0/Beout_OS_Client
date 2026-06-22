#pragma once

#include <string>
#include <memory>
#include <mutex>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include <httplib.h>

#include "../database/database_manager.hpp"

namespace beout_os {
namespace api {

class ApiServer {
public:
    ApiServer(const std::string& cert_path, const std::string& private_key_path, std::shared_ptr<database::DatabaseManager> db);
    ~ApiServer();

    void start(const std::string& host, int port);
    void stop();

private:
    std::unique_ptr<httplib::SSLServer> server_;
    std::shared_ptr<database::DatabaseManager> db_;

    void setup_routes();

    // Authentication token storage for session management
    std::string current_session_token_;
    std::mutex session_mutex_;
};

} // namespace api
} // namespace beout_os
