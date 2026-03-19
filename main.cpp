#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

struct User {
    std::string fullName;
    std::string username;
    std::string password;
    std::string role;
};

class AuthSystem {
public:
    explicit AuthSystem(std::string databasePath) : databasePath_(std::move(databasePath)) {
        loadUsers();
    }

    void run() {
        bool running = true;

        while (running) {
            printHeader();
            std::cout << "1. Log in\n";
            std::cout << "2. Register\n";
            std::cout << "3. Exit\n";

            int choice = readMenuChoice(1, 3);
            switch (choice) {
                case 1:
                    login();
                    break;
                case 2:
                    registerUser();
                    break;
                case 3:
                    running = false;
                    std::cout << "\nThank you for using the Hospital Management System.\n";
                    break;
            }
        }
    }

private:
    const std::vector<std::string> roles_ = {
        "Patient",
        "Doctor",
        "Pharmacist",
        "Secretary",
        "Inventory Manager",
        "HR Manager"
    };

    std::string databasePath_;
    std::vector<User> users_;

    void printHeader() const {
        std::cout << "\n===========================================\n";
        std::cout << "     Hospital Management System (HMS)\n";
        std::cout << "===========================================\n";
    }

    void loadUsers() {
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

    bool saveUsers() const {
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

    static std::string trim(std::string value) {
        auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };

        value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
        value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
        return value;
    }

    static std::string toLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    int readMenuChoice(int min, int max) const {
        while (true) {
            std::cout << "\nChoose an option: ";

            int choice;
            if (std::cin >> choice && choice >= min && choice <= max) {
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                return choice;
            }

            std::cout << "Invalid choice. Please enter a number from " << min << " to " << max << ".\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }

    std::string readNonEmptyLine(const std::string& prompt) const {
        while (true) {
            std::cout << prompt;
            std::string value;
            std::getline(std::cin, value);
            value = trim(value);

            if (!value.empty()) {
                return value;
            }

            std::cout << "This field cannot be empty.\n";
        }
    }

    bool usernameExists(const std::string& username) const {
        const std::string normalized = toLower(username);
        return std::any_of(users_.begin(), users_.end(), [&](const User& user) {
            return toLower(user.username) == normalized;
        });
    }

    std::optional<User> findUser(const std::string& username, const std::string& password) const {
        const std::string normalized = toLower(username);

        for (const auto& user : users_) {
            if (toLower(user.username) == normalized && user.password == password) {
                return user;
            }
        }

        return std::nullopt;
    }

    bool isValidPassword(const std::string& password) const {
        if (password.length() < 6) {
            std::cout << "Password must be at least 6 characters long.\n";
            return false;
        }

        return true;
    }

    std::string chooseRole() const {
        std::cout << "\nAvailable roles:\n";
        for (std::size_t i = 0; i < roles_.size(); ++i) {
            std::cout << std::setw(2) << i + 1 << ". " << roles_[i] << '\n';
        }

        const int choice = readMenuChoice(1, static_cast<int>(roles_.size()));
        return roles_[choice - 1];
    }

    void registerUser() {
        std::cout << "\n--- Register ---\n";

        User newUser;
        newUser.fullName = readNonEmptyLine("Full name: ");

        while (true) {
            newUser.username = readNonEmptyLine("Username: ");

            if (newUser.username.find('|') != std::string::npos) {
                std::cout << "The '|' character is not allowed in usernames.\n";
                continue;
            }

            if (usernameExists(newUser.username)) {
                std::cout << "This username already exists. Please choose another one.\n";
                continue;
            }

            break;
        }

        while (true) {
            std::string password = readNonEmptyLine("Password: ");
            if (!isValidPassword(password)) {
                continue;
            }

            std::string confirmPassword = readNonEmptyLine("Confirm password: ");
            if (password != confirmPassword) {
                std::cout << "Passwords do not match. Please try again.\n";
                continue;
            }

            newUser.password = password;
            break;
        }

        newUser.role = chooseRole();
        users_.push_back(newUser);

        if (!saveUsers()) {
            std::cout << "Registration failed: unable to save user data.\n";
            users_.pop_back();
            return;
        }

        std::cout << "\nRegistration successful. You can now log in as " << newUser.username << ".\n";
    }

    void login() const {
        std::cout << "\n--- Log in ---\n";

        const std::string username = readNonEmptyLine("Username: ");
        const std::string password = readNonEmptyLine("Password: ");

        const auto user = findUser(username, password);
        if (!user.has_value()) {
            std::cout << "\nLogin failed. Invalid username or password.\n";
            return;
        }

        showDashboard(*user);
    }

    void showDashboard(const User& user) const {
        std::cout << "\n===========================================\n";
        std::cout << "Welcome, " << user.fullName << "!\n";
        std::cout << "Role: " << user.role << '\n';
        std::cout << "===========================================\n";

        if (user.role == "Patient") {
            std::cout << "You can continue to appointments and bill payments.\n";
        } else if (user.role == "Doctor") {
            std::cout << "You can continue to admissions, prescriptions, and patient records.\n";
        } else if (user.role == "Pharmacist") {
            std::cout << "You can continue to medication dispensing.\n";
        } else if (user.role == "Secretary") {
            std::cout << "You can continue to billing and surgery scheduling.\n";
        } else if (user.role == "Inventory Manager") {
            std::cout << "You can continue to stock and equipment ordering.\n";
        } else if (user.role == "HR Manager") {
            std::cout << "You can continue to staff and shift management.\n";
        }

        std::cout << "\nPress Enter to return to the main menu...";
        std::cin.get();
    }
};

int main() {
    AuthSystem authSystem("users.txt");
    authSystem.run();
    return 0;
}
