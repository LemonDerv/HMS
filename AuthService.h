#pragma once

#include "User.h"

#include <optional>
#include <string>
#include <vector>

class AuthService {
public:
    explicit AuthService(std::string databasePath);

    std::optional<User> Login(const std::string& username, const std::string& password) const;
    bool RegisterUser(const User& user, std::string& errorMessage);

private:
    bool UsernameExists(const std::string& username) const;
    void LoadUsers();
    bool SaveUsers() const;

    std::string databasePath_;
    std::vector<User> users_;
};
