#pragma once

#include <sqlite3.h>

#include <string>

class Database {
public:
    explicit Database(std::string path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool Open(std::string& errorMessage);
    bool IsOpen() const;
    bool Execute(const std::string& sql, std::string& errorMessage) const;
    bool Prepare(const std::string& sql, sqlite3_stmt** statement, std::string& errorMessage) const;
    bool InitializeSchema(std::string& errorMessage) const;

    sqlite3* Handle() const;
    sqlite3_int64 LastInsertRowId() const;
    const std::string& Path() const;

private:
    std::string path_;
    sqlite3* db_ = nullptr;
};
