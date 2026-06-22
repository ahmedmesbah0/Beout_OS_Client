#include "database_manager.hpp"
#include <sqlite3.h>
#include <iostream>

namespace beout_os {
namespace database {

DatabaseManager::DatabaseManager(const std::string& db_path) : db_path_(db_path) {}

DatabaseManager::~DatabaseManager() {
    if (db_) {
        sqlite3_close(db_);
    }
}

bool DatabaseManager::initialize() {
    std::lock_guard<std::mutex> lock(db_mutex_);
    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }

    const char* create_table_query = R"(
        CREATE TABLE IF NOT EXISTS config (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )";

    return execute_query(create_table_query);
}

bool DatabaseManager::execute_query(const std::string& query) const {
    // Assumes mutex is already locked by the caller
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, query.c_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool DatabaseManager::set_config(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(db_mutex_);
    const char* query = "INSERT INTO config (key, value) VALUES (?, ?) "
                        "ON CONFLICT(key) DO UPDATE SET value=excluded.value;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare set_config statement" << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    
    // Trigger OS-level config sync script asynchronously
    if (success && key.find("network_") == 0) {
        std::system("/opt/beout_os/bin/sync_network.sh &");
    }
    
    return success;
}

std::optional<std::string> DatabaseManager::get_config(const std::string& key) const {
    std::lock_guard<std::mutex> lock(db_mutex_);
    const char* query = "SELECT value FROM config WHERE key = ?;";
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<std::string> result = std::nullopt;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        if (text) {
            result = std::string(reinterpret_cast<const char*>(text));
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

} // namespace database
} // namespace beout_os
