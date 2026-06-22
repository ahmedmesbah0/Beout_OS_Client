#pragma once

#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <mutex>

struct sqlite3;

namespace beout_os {
namespace database {

class DatabaseManager {
public:
    DatabaseManager(const std::string& db_path);
    ~DatabaseManager();

    // Prevent copy and assignment
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    bool initialize();
    bool set_config(const std::string& key, const std::string& value);
    std::optional<std::string> get_config(const std::string& key) const;

private:
    std::string db_path_;
    sqlite3* db_{nullptr};
    mutable std::mutex db_mutex_;

    bool execute_query(const std::string& query) const;
};

} // namespace database
} // namespace beout_os
