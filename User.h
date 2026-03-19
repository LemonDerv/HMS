#pragma once

#include <string>

struct User {
    int id = 0;
    std::string fullName;
    std::string username;
    std::string password;
    std::string role;
};
