#include <windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
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

    std::optional<User> login(const std::string& username, const std::string& password) const {
        const std::string normalized = toLower(username);
        for (const auto& user : users_) {
            if (toLower(user.username) == normalized && user.password == password) {
                return user;
            }
        }
        return std::nullopt;
    }

    bool registerUser(const User& user, std::string& errorMessage) {
        if (trim(user.fullName).empty()) {
            errorMessage = "Full name is required.";
            return false;
        }
        if (trim(user.username).empty()) {
            errorMessage = "Username is required.";
            return false;
        }
        if (trim(user.role).empty()) {
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
        if (usernameExists(user.username)) {
            errorMessage = "This username already exists.";
            return false;
        }

        users_.push_back(user);
        if (!saveUsers()) {
            users_.pop_back();
            errorMessage = "Unable to save user data.";
            return false;
        }
        return true;
    }

private:
    std::string databasePath_;
    std::vector<User> users_;

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

    bool usernameExists(const std::string& username) const {
        const std::string normalized = toLower(username);
        return std::any_of(users_.begin(), users_.end(), [&](const User& user) {
            return toLower(user.username) == normalized;
        });
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
};

enum class PageMode {
    Login,
    Register
};

constexpr int ID_BTN_TAB_LOGIN = 1001;
constexpr int ID_BTN_TAB_REGISTER = 1002;
constexpr int ID_EDIT_LOGIN_USERNAME = 1101;
constexpr int ID_EDIT_LOGIN_PASSWORD = 1102;
constexpr int ID_BTN_LOGIN = 1103;
constexpr int ID_EDIT_REGISTER_FULLNAME = 1201;
constexpr int ID_EDIT_REGISTER_USERNAME = 1202;
constexpr int ID_EDIT_REGISTER_PASSWORD = 1203;
constexpr int ID_EDIT_REGISTER_CONFIRM = 1204;
constexpr int ID_COMBO_ROLE = 1205;
constexpr int ID_BTN_REGISTER = 1206;
constexpr int ID_STATUS_LABEL = 1301;

struct AppState {
    AuthSystem auth{"users.txt"};
    PageMode mode = PageMode::Login;
    HFONT heroTitleFont = nullptr;
    HFONT heroBodyFont = nullptr;
    HFONT cardTitleFont = nullptr;
    HFONT bodyFont = nullptr;
    HFONT labelFont = nullptr;
    HWND statusLabel = nullptr;
    HWND cardTitleLabel = nullptr;
    HWND cardSubtitleLabel = nullptr;
    HWND btnTabLogin = nullptr;
    HWND btnTabRegister = nullptr;
    std::vector<HWND> heroLabels;
    std::vector<HWND> loginControls;
    std::vector<HWND> registerControls;
};

std::wstring toWide(const std::string& text) {
    if (text.empty()) {
        return L"";
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring result(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), size);
    return result;
}

std::string toUtf8(const std::wstring& text) {
    if (text.empty()) {
        return "";
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring getWindowTextString(HWND hwnd) {
    const int length = GetWindowTextLengthW(hwnd);
    std::wstring text(length + 1, L'\0');
    GetWindowTextW(hwnd, text.data(), length + 1);
    text.resize(length);
    return text;
}

void setWindowFont(HWND hwnd, HFONT font) {
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

HWND createLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h, HFONT font, std::vector<HWND>* bucket = nullptr) {
    HWND label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    setWindowFont(label, font);
    if (bucket) {
        bucket->push_back(label);
    }
    return label;
}

HWND createEdit(HWND parent, int id, int x, int y, int w, int h, HFONT font, bool password = false) {
    HWND edit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | (password ? ES_PASSWORD : 0),
        x,
        y,
        w,
        h,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
    setWindowFont(edit, font);
    return edit;
}

HWND createButton(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h, HFONT font) {
    HWND button = CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        x,
        y,
        w,
        h,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
    setWindowFont(button, font);
    return button;
}

bool isHeroLabel(const AppState* state, HWND hwnd) {
    return std::find(state->heroLabels.begin(), state->heroLabels.end(), hwnd) != state->heroLabels.end();
}

void setStatusMessage(AppState* state, const std::wstring& text) {
    SetWindowTextW(state->statusLabel, text.c_str());
    InvalidateRect(state->statusLabel, nullptr, TRUE);
    UpdateWindow(state->statusLabel);
}

void clearEditControls(const std::vector<HWND>& controls) {
    for (HWND control : controls) {
        wchar_t className[32] = {};
        GetClassNameW(control, className, 32);
        if (std::wstring(className) == L"EDIT") {
            SetWindowTextW(control, L"");
        }
    }
}

void resetRegisterForm(HWND hwnd, const AppState* state) {
    clearEditControls(state->registerControls);
    SendMessageW(GetDlgItem(hwnd, ID_COMBO_ROLE), CB_SETCURSEL, 0, 0);
}

std::wstring buildWelcomeMessage(const User& user) {
    std::wstring message = L"Welcome, " + toWide(user.fullName) + L"!\n\nRole: " + toWide(user.role);

    if (user.role == "Patient") {
        message += L"\nAccess: appointments and bill payments.";
    } else if (user.role == "Doctor") {
        message += L"\nAccess: admissions, prescriptions, and patient records.";
    } else if (user.role == "Pharmacist") {
        message += L"\nAccess: medication dispensing.";
    } else if (user.role == "Secretary") {
        message += L"\nAccess: patient billing and surgery scheduling.";
    } else if (user.role == "Inventory Manager") {
        message += L"\nAccess: stock and ordering workflows.";
    } else if (user.role == "HR Manager") {
        message += L"\nAccess: staff and shift management.";
    }

    return message;
}

void updateModeText(AppState* state) {
    if (state->mode == PageMode::Login) {
        SetWindowTextW(state->cardTitleLabel, L"Welcome back");
        SetWindowTextW(state->cardSubtitleLabel, L"Sign in to continue to the hospital workspace.");
        setStatusMessage(state, L"Use your hospital account to continue.");
    } else {
        SetWindowTextW(state->cardTitleLabel, L"Create an account");
        SetWindowTextW(state->cardSubtitleLabel, L"Register a new patient or staff profile.");
        setStatusMessage(state, L"Choose a role and create your secure account.");
    }
}

void switchMode(HWND hwnd, AppState* state, PageMode mode) {
    state->mode = mode;

    const bool loginVisible = mode == PageMode::Login;
    for (HWND control : state->loginControls) {
        ShowWindow(control, loginVisible ? SW_SHOW : SW_HIDE);
    }
    for (HWND control : state->registerControls) {
        ShowWindow(control, loginVisible ? SW_HIDE : SW_SHOW);
    }

    updateModeText(state);
    InvalidateRect(hwnd, nullptr, TRUE);
}

void handleLogin(HWND hwnd, AppState* state) {
    const std::wstring username = getWindowTextString(GetDlgItem(hwnd, ID_EDIT_LOGIN_USERNAME));
    const std::wstring password = getWindowTextString(GetDlgItem(hwnd, ID_EDIT_LOGIN_PASSWORD));

    if (username.empty() || password.empty()) {
        setStatusMessage(state, L"Please fill in both login fields.");
        return;
    }

    const auto user = state->auth.login(toUtf8(username), toUtf8(password));
    if (!user.has_value()) {
        setStatusMessage(state, L"Login failed. Invalid username or password.");
        return;
    }

    setStatusMessage(state, L"Login successful.");
    SetWindowTextW(GetDlgItem(hwnd, ID_EDIT_LOGIN_PASSWORD), L"");
    MessageBoxW(hwnd, buildWelcomeMessage(*user).c_str(), L"HMS Dashboard", MB_OK | MB_ICONINFORMATION);
}

void handleRegister(HWND hwnd, AppState* state) {
    const std::wstring fullName = getWindowTextString(GetDlgItem(hwnd, ID_EDIT_REGISTER_FULLNAME));
    const std::wstring username = getWindowTextString(GetDlgItem(hwnd, ID_EDIT_REGISTER_USERNAME));
    const std::wstring password = getWindowTextString(GetDlgItem(hwnd, ID_EDIT_REGISTER_PASSWORD));
    const std::wstring confirm = getWindowTextString(GetDlgItem(hwnd, ID_EDIT_REGISTER_CONFIRM));

    wchar_t roleBuffer[128] = {};
    GetWindowTextW(GetDlgItem(hwnd, ID_COMBO_ROLE), roleBuffer, 128);

    if (fullName.empty() || username.empty() || password.empty() || confirm.empty()) {
        setStatusMessage(state, L"Please complete every registration field.");
        return;
    }
    if (password != confirm) {
        setStatusMessage(state, L"Passwords do not match.");
        return;
    }

    User user{
        toUtf8(fullName),
        toUtf8(username),
        toUtf8(password),
        toUtf8(roleBuffer)
    };

    std::string errorMessage;
    if (!state->auth.registerUser(user, errorMessage)) {
        setStatusMessage(state, toWide(errorMessage));
        return;
    }

    MessageBoxW(hwnd, L"Registration successful. You can now sign in.", L"HMS", MB_OK | MB_ICONINFORMATION);
    resetRegisterForm(hwnd, state);
    switchMode(hwnd, state, PageMode::Login);
}

COLORREF blend(COLORREF a, COLORREF b, double t) {
    const int r = static_cast<int>(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * t);
    const int g = static_cast<int>(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * t);
    const int bValue = static_cast<int>(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * t);
    return RGB(r, g, bValue);
}

void drawVerticalGradient(HDC hdc, const RECT& rect, COLORREF top, COLORREF bottom) {
    const int height = rect.bottom - rect.top;
    for (int i = 0; i < height; ++i) {
        const double t = height <= 1 ? 0.0 : static_cast<double>(i) / (height - 1);
        HBRUSH brush = CreateSolidBrush(blend(top, bottom, t));
        RECT line = {rect.left, rect.top + i, rect.right, rect.top + i + 1};
        FillRect(hdc, &line, brush);
        DeleteObject(brush);
    }
}

void drawButton(LPDRAWITEMSTRUCT drawItem, const AppState* state) {
    const UINT controlId = drawItem->CtlID;
    RECT rect = drawItem->rcItem;
    HDC hdc = drawItem->hDC;

    COLORREF fill = RGB(231, 238, 247);
    COLORREF text = RGB(39, 62, 92);
    COLORREF border = RGB(202, 214, 228);

    if (controlId == ID_BTN_LOGIN || controlId == ID_BTN_REGISTER) {
        fill = RGB(33, 107, 170);
        text = RGB(255, 255, 255);
        border = RGB(33, 107, 170);
    } else {
        const bool activeTab =
            (controlId == ID_BTN_TAB_LOGIN && state->mode == PageMode::Login) ||
            (controlId == ID_BTN_TAB_REGISTER && state->mode == PageMode::Register);
        if (activeTab) {
            fill = RGB(19, 80, 134);
            text = RGB(255, 255, 255);
            border = RGB(19, 80, 134);
        }
    }

    if ((drawItem->itemState & ODS_SELECTED) == ODS_SELECTED) {
        fill = blend(fill, RGB(0, 0, 0), 0.12);
    }

    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(hdc, brush);
    HGDIOBJ oldPen = SelectObject(hdc, pen);

    RoundRect(hdc, rect.left, rect.top, rect.right, rect.bottom, 18, 18);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, text);

    wchar_t buttonText[128] = {};
    GetWindowTextW(drawItem->hwndItem, buttonText, 128);
    DrawTextW(hdc, buttonText, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if ((drawItem->itemState & ODS_FOCUS) == ODS_FOCUS) {
        RECT focusRect = rect;
        InflateRect(&focusRect, -6, -6);
        DrawFocusRect(hdc, &focusRect);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    AppState* state = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
        case WM_CREATE: {
            auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = reinterpret_cast<AppState*>(createStruct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

            state->heroTitleFont = CreateFontW(38, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                               OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                               VARIABLE_PITCH, L"Bahnschrift");
            state->heroBodyFont = CreateFontW(21, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                              OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                              VARIABLE_PITCH, L"Bahnschrift");
            state->cardTitleFont = CreateFontW(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                               OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                               VARIABLE_PITCH, L"Bahnschrift");
            state->bodyFont = CreateFontW(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                          OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                          VARIABLE_PITCH, L"Segoe UI");
            state->labelFont = CreateFontW(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                           OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                           VARIABLE_PITCH, L"Segoe UI");

            createLabel(hwnd, L"Hospital", 54, 64, 220, 40, state->heroTitleFont, &state->heroLabels);
            createLabel(hwnd, L"Management", 54, 104, 240, 40, state->heroTitleFont, &state->heroLabels);
            createLabel(hwnd, L"System", 54, 144, 220, 40, state->heroTitleFont, &state->heroLabels);
            createLabel(hwnd, L"Secure access for patients, doctors,\npharmacists, secretaries, inventory,\nand HR staff.", 54, 228, 290, 90, state->heroBodyFont, &state->heroLabels);
            createLabel(hwnd, L"Appointments and billing", 54, 370, 250, 28, state->labelFont, &state->heroLabels);
            createLabel(hwnd, L"Prescriptions and records", 54, 406, 250, 28, state->labelFont, &state->heroLabels);
            createLabel(hwnd, L"Stock and shift workflows", 54, 442, 250, 28, state->labelFont, &state->heroLabels);

            state->cardTitleLabel = createLabel(hwnd, L"Welcome back", 418, 88, 300, 38, state->cardTitleFont);
            state->cardSubtitleLabel = createLabel(hwnd, L"Sign in to continue to the hospital workspace.", 418, 126, 360, 26, state->labelFont);

            state->btnTabLogin = createButton(hwnd, L"Log In", ID_BTN_TAB_LOGIN, 418, 178, 120, 42, state->bodyFont);
            state->btnTabRegister = createButton(hwnd, L"Register", ID_BTN_TAB_REGISTER, 550, 178, 120, 42, state->bodyFont);

            state->statusLabel = createLabel(hwnd, L"Use your hospital account to continue.", 418, 236, 340, 24, state->labelFont);

            HWND loginLabelUser = createLabel(hwnd, L"Username", 418, 284, 120, 24, state->labelFont);
            HWND loginLabelPass = createLabel(hwnd, L"Password", 418, 364, 120, 24, state->labelFont);
            state->loginControls.push_back(loginLabelUser);
            state->loginControls.push_back(loginLabelPass);
            state->loginControls.push_back(createEdit(hwnd, ID_EDIT_LOGIN_USERNAME, 418, 314, 312, 38, state->bodyFont));
            state->loginControls.push_back(createEdit(hwnd, ID_EDIT_LOGIN_PASSWORD, 418, 394, 312, 38, state->bodyFont, true));
            state->loginControls.push_back(createButton(hwnd, L"Access Dashboard", ID_BTN_LOGIN, 418, 468, 220, 46, state->bodyFont));

            HWND regLabelName = createLabel(hwnd, L"Full name", 418, 284, 120, 24, state->labelFont);
            HWND regLabelUser = createLabel(hwnd, L"Username", 418, 344, 120, 24, state->labelFont);
            HWND regLabelPass = createLabel(hwnd, L"Password", 418, 404, 120, 24, state->labelFont);
            HWND regLabelConfirm = createLabel(hwnd, L"Confirm password", 418, 464, 150, 24, state->labelFont);
            HWND regLabelRole = createLabel(hwnd, L"Role", 418, 524, 120, 24, state->labelFont);

            state->registerControls.push_back(regLabelName);
            state->registerControls.push_back(regLabelUser);
            state->registerControls.push_back(regLabelPass);
            state->registerControls.push_back(regLabelConfirm);
            state->registerControls.push_back(regLabelRole);
            state->registerControls.push_back(createEdit(hwnd, ID_EDIT_REGISTER_FULLNAME, 418, 310, 312, 34, state->bodyFont));
            state->registerControls.push_back(createEdit(hwnd, ID_EDIT_REGISTER_USERNAME, 418, 370, 312, 34, state->bodyFont));
            state->registerControls.push_back(createEdit(hwnd, ID_EDIT_REGISTER_PASSWORD, 418, 430, 312, 34, state->bodyFont, true));
            state->registerControls.push_back(createEdit(hwnd, ID_EDIT_REGISTER_CONFIRM, 418, 490, 312, 34, state->bodyFont, true));

            HWND roleCombo = CreateWindowExW(
                0,
                L"COMBOBOX",
                L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                418,
                550,
                240,
                240,
                hwnd,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_COMBO_ROLE)),
                GetModuleHandleW(nullptr),
                nullptr);
            setWindowFont(roleCombo, state->bodyFont);
            state->registerControls.push_back(roleCombo);

            const wchar_t* roles[] = {
                L"Patient",
                L"Doctor",
                L"Pharmacist",
                L"Secretary",
                L"Inventory Manager",
                L"HR Manager"
            };
            for (const auto* role : roles) {
                SendMessageW(roleCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(role));
            }
            SendMessageW(roleCombo, CB_SETCURSEL, 0, 0);

            state->registerControls.push_back(createButton(hwnd, L"Create Account", ID_BTN_REGISTER, 418, 598, 220, 46, state->bodyFont));

            switchMode(hwnd, state, PageMode::Login);
            return 0;
        }

        case WM_COMMAND:
            if (!state) {
                return 0;
            }

            switch (LOWORD(wParam)) {
                case ID_BTN_TAB_LOGIN:
                    switchMode(hwnd, state, PageMode::Login);
                    return 0;
                case ID_BTN_TAB_REGISTER:
                    switchMode(hwnd, state, PageMode::Register);
                    return 0;
                case ID_BTN_LOGIN:
                    handleLogin(hwnd, state);
                    return 0;
                case ID_BTN_REGISTER:
                    handleRegister(hwnd, state);
                    return 0;
            }
            return 0;

        case WM_DRAWITEM:
            if (state) {
                drawButton(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam), state);
                return TRUE;
            }
            break;

        case WM_CTLCOLORSTATIC:
            if (state) {
                HDC hdcStatic = reinterpret_cast<HDC>(wParam);
                HWND control = reinterpret_cast<HWND>(lParam);

                if (isHeroLabel(state, control)) {
                    SetBkMode(hdcStatic, TRANSPARENT);
                    SetTextColor(hdcStatic, RGB(244, 248, 252));
                    static HBRUSH hollowBrush = reinterpret_cast<HBRUSH>(GetStockObject(NULL_BRUSH));
                    return reinterpret_cast<INT_PTR>(hollowBrush);
                }

                SetBkMode(hdcStatic, OPAQUE);
                SetBkColor(hdcStatic, RGB(255, 255, 255));

                if (control == state->statusLabel) {
                    SetTextColor(hdcStatic, RGB(61, 88, 115));
                } else {
                    SetTextColor(hdcStatic, RGB(44, 55, 69));
                }

                static HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
                return reinterpret_cast<INT_PTR>(whiteBrush);
            }
            break;

        case WM_CTLCOLOREDIT: {
            HDC hdcEdit = reinterpret_cast<HDC>(wParam);
            SetBkColor(hdcEdit, RGB(255, 255, 255));
            SetTextColor(hdcEdit, RGB(28, 31, 36));
            return reinterpret_cast<INT_PTR>(GetStockObject(WHITE_BRUSH));
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT client;
            GetClientRect(hwnd, &client);

            HBRUSH bg = CreateSolidBrush(RGB(244, 247, 251));
            FillRect(hdc, &client, bg);
            DeleteObject(bg);

            RECT hero = {28, 28, 356, 692};
            drawVerticalGradient(hdc, hero, RGB(13, 58, 97), RGB(37, 110, 156));

            HBRUSH accentBrush = CreateSolidBrush(RGB(139, 207, 255));
            RECT accent = {52, 330, 84, 362};
            Ellipse(hdc, accent.left, accent.top, accent.right, accent.bottom);
            RECT accent2 = {268, 82, 312, 126};
            Ellipse(hdc, accent2.left, accent2.top, accent2.right, accent2.bottom);
            DeleteObject(accentBrush);

            RECT card = {386, 48, 774, 664};
            HBRUSH cardBrush = CreateSolidBrush(RGB(255, 255, 255));
            HPEN cardPen = CreatePen(PS_SOLID, 1, RGB(220, 228, 236));
            HGDIOBJ oldBrush = SelectObject(hdc, cardBrush);
            HGDIOBJ oldPen = SelectObject(hdc, cardPen);
            RoundRect(hdc, card.left, card.top, card.right, card.bottom, 28, 28);
            SelectObject(hdc, oldBrush);
            SelectObject(hdc, oldPen);
            DeleteObject(cardBrush);
            DeleteObject(cardPen);

            HPEN dividerPen = CreatePen(PS_SOLID, 1, RGB(228, 234, 241));
            oldPen = SelectObject(hdc, dividerPen);
            MoveToEx(hdc, 418, 268, nullptr);
            LineTo(hdc, 734, 268);
            SelectObject(hdc, oldPen);
            DeleteObject(dividerPen);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_DESTROY:
            if (state) {
                DeleteObject(state->heroTitleFont);
                DeleteObject(state->heroBodyFont);
                DeleteObject(state->cardTitleFont);
                DeleteObject(state->bodyFont);
                DeleteObject(state->labelFont);
            }
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int cmdShow) {
    const wchar_t kClassName[] = L"HMSModernAuth";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = kClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    AppState state;

    HWND hwnd = CreateWindowExW(
        0,
        kClassName,
        L"Hospital Management System",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        830,
        760,
        nullptr,
        nullptr,
        instance,
        &state);

    if (!hwnd) {
        return 0;
    }

    ShowWindow(hwnd, cmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return 0;
}
