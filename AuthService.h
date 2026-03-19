#pragma once

#include "Database.h"
#include "User.h"

#include <optional>
#include <string>

class AuthService {
public:
    AuthService(std::string databasePath, std::string legacyUsersPath = "users.txt");

    bool IsAvailable() const;
    const std::string& InitializationError() const;
    std::optional<User> Login(const std::string& username, const std::string& password) const;
    bool RegisterUser(User& user, std::string& errorMessage);

private:
    bool UsernameExists(const std::string& username) const;
    bool InsertUser(User& user, std::string& errorMessage, bool allowExisting);
    bool CreateProfileForUser(sqlite3_int64 userId, const User& user, std::string& errorMessage) const;
    void ImportLegacyUsers();

    Database database_;
    std::string legacyUsersPath_;
    std::string initializationError_;
};
