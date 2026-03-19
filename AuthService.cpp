#include "AuthService.h"

#include "StringUtil.h"

#include <fstream>
#include <sstream>

namespace {
sqlite3_destructor_type SqliteTransient() {
    return SQLITE_TRANSIENT;
}
}

AuthService::AuthService(std::string databasePath, std::string legacyUsersPath)
    : database_(std::move(databasePath)), legacyUsersPath_(std::move(legacyUsersPath)) {
    if (!database_.Open(initializationError_)) {
        return;
    }

    if (!database_.InitializeSchema(initializationError_)) {
        return;
    }

    ImportLegacyUsers();
}

bool AuthService::IsAvailable() const {
    return database_.IsOpen() && initializationError_.empty();
}

const std::string& AuthService::InitializationError() const {
    return initializationError_;
}

std::optional<User> AuthService::Login(const std::string& username, const std::string& password) const {
    if (!IsAvailable()) {
        return std::nullopt;
    }

    sqlite3_stmt* statement = nullptr;
    std::string errorMessage;
    if (!database_.Prepare(
            "SELECT id, full_name, username, password, role "
            "FROM users "
            "WHERE username = ? COLLATE NOCASE AND password = ? "
            "LIMIT 1;",
            &statement,
            errorMessage)) {
        return std::nullopt;
    }

    sqlite3_bind_text(statement, 1, username.c_str(), -1, SqliteTransient());
    sqlite3_bind_text(statement, 2, password.c_str(), -1, SqliteTransient());

    User user;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        user.id = sqlite3_column_int(statement, 0);
        user.fullName = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        user.username = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        user.password = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
        user.role = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
        sqlite3_finalize(statement);
        return user;
    }

    sqlite3_finalize(statement);
    return std::nullopt;
}

bool AuthService::RegisterUser(User& user, std::string& errorMessage) {
    if (!IsAvailable()) {
        errorMessage = initializationError_.empty() ? "Database is not available." : initializationError_;
        return false;
    }

    if (Trim(user.fullName).empty()) {
        errorMessage = "Full name is required.";
        return false;
    }
    if (Trim(user.username).empty()) {
        errorMessage = "Username is required.";
        return false;
    }
    if (Trim(user.role).empty()) {
        errorMessage = "Please select a role.";
        return false;
    }
    if (user.username.find('|') != std::string::npos) {
        errorMessage = "The character '|' is not allowed in usernames.";
        return false;
    }
    if (user.password.length() < 6) {
        errorMessage = "Password must be at least 6 characters long.";
        return false;
    }

    return InsertUser(user, errorMessage, false);
}

bool AuthService::UsernameExists(const std::string& username) const {
    sqlite3_stmt* statement = nullptr;
    std::string errorMessage;
    if (!database_.Prepare(
            "SELECT 1 FROM users WHERE username = ? COLLATE NOCASE LIMIT 1;",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_text(statement, 1, username.c_str(), -1, SqliteTransient());
    const bool exists = sqlite3_step(statement) == SQLITE_ROW;
    sqlite3_finalize(statement);
    return exists;
}

bool AuthService::InsertUser(User& user, std::string& errorMessage, bool allowExisting) {
    if (UsernameExists(user.username)) {
        if (allowExisting) {
            return true;
        }

        errorMessage = "This username already exists.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO users(full_name, username, password, role) VALUES (?, ?, ?, ?);",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_text(statement, 1, user.fullName.c_str(), -1, SqliteTransient());
    sqlite3_bind_text(statement, 2, user.username.c_str(), -1, SqliteTransient());
    sqlite3_bind_text(statement, 3, user.password.c_str(), -1, SqliteTransient());
    sqlite3_bind_text(statement, 4, user.role.c_str(), -1, SqliteTransient());

    const int insertResult = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (insertResult != SQLITE_DONE) {
        errorMessage = sqlite3_errmsg(database_.Handle());
        return false;
    }

    user.id = static_cast<int>(database_.LastInsertRowId());
    return CreateProfileForUser(database_.LastInsertRowId(), user, errorMessage);
}

bool AuthService::CreateProfileForUser(sqlite3_int64 userId, const User& user, std::string& errorMessage) const {
    sqlite3_stmt* statement = nullptr;

    const std::string sql = user.role == "Patient"
        ? "INSERT OR IGNORE INTO patients(user_id, patient_name) VALUES (?, ?);"
        : "INSERT OR IGNORE INTO staff_profiles(user_id, role_name) VALUES (?, ?);";

    if (!database_.Prepare(sql, &statement, errorMessage)) {
        return false;
    }

    sqlite3_bind_int64(statement, 1, userId);
    if (user.role == "Patient") {
        sqlite3_bind_text(statement, 2, user.fullName.c_str(), -1, SqliteTransient());
    } else {
        sqlite3_bind_text(statement, 2, user.role.c_str(), -1, SqliteTransient());
    }

    const int stepResult = sqlite3_step(statement);
    sqlite3_finalize(statement);
    if (stepResult != SQLITE_DONE) {
        errorMessage = sqlite3_errmsg(database_.Handle());
        return false;
    }

    return true;
}

void AuthService::ImportLegacyUsers() {
    std::ifstream input(legacyUsersPath_);
    if (!input.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream stream(line);
        User user;
        std::getline(stream, user.fullName, '|');
        std::getline(stream, user.username, '|');
        std::getline(stream, user.password, '|');
        std::getline(stream, user.role, '|');

        if (user.username.empty() || user.role.empty()) {
            continue;
        }

        std::string ignoredError;
        InsertUser(user, ignoredError, true);
    }
}
