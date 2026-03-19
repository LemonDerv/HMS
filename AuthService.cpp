#include "AuthService.h"

#include "StringUtil.h"

#include <algorithm>
#include <fstream>
#include <sstream>

AuthService::AuthService(std::string databasePath) : databasePath_(std::move(databasePath)) {
    LoadUsers();
}

std::optional<User> AuthService::Login(const std::string& username, const std::string& password) const {
    const std::string normalized = ToLower(username);

    for (const auto& user : users_) {
        if (ToLower(user.username) == normalized && user.password == password) {
            return user;
        }
    }

    return std::nullopt;
}

bool AuthService::RegisterUser(const User& user, std::string& errorMessage) {
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
    if (UsernameExists(user.username)) {
        errorMessage = "This username already exists.";
        return false;
    }

    users_.push_back(user);
    if (!SaveUsers()) {
        users_.pop_back();
        errorMessage = "Unable to save user data.";
        return false;
    }

    return true;
}

bool AuthService::UsernameExists(const std::string& username) const {
    const std::string normalized = ToLower(username);
    return std::any_of(users_.begin(), users_.end(), [&](const User& user) {
        return ToLower(user.username) == normalized;
    });
}

void AuthService::LoadUsers() {
    std::ifstream input(databasePath_);
    if (!input.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        User user;
        std::getline(ss, user.fullName, '|');
        std::getline(ss, user.username, '|');
        std::getline(ss, user.password, '|');
        std::getline(ss, user.role, '|');

        if (!user.username.empty()) {
            users_.push_back(user);
        }
    }
}

bool AuthService::SaveUsers() const {
    std::ofstream output(databasePath_, std::ios::trunc);
    if (!output.is_open()) {
        return false;
    }

    for (const auto& user : users_) {
        output << user.fullName << '|'
               << user.username << '|'
               << user.password << '|'
               << user.role << '\n';
    }

    return true;
}
