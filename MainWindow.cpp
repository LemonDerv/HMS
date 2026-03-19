#include "MainWindow.h"

#include "StringUtil.h"
#include "Theme.h"
#include "Win32Util.h"

#include <algorithm>

namespace {
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
constexpr int ID_BTN_LOGOUT = 1401;
constexpr wchar_t kWindowClassName[] = L"HMSMainWindow";

void ShowControls(const std::vector<HWND>& controls, bool visible) {
    for (HWND control : controls) {
        ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
    }
}
}

MainWindow::MainWindow(HINSTANCE instance) : instance_(instance), auth_("users.txt") {}

bool MainWindow::Create() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = MainWindow::WindowProc;
    wc.hInstance = instance_;
    wc.lpszClassName = kWindowClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);

    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    hwnd_ = CreateWindowExW(
        0,
        kWindowClassName,
        L"Hospital Management System",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        920,
        860,
        nullptr,
        nullptr,
        instance_,
        this);

    return hwnd_ != nullptr;
}

int MainWindow::Run(int cmdShow) {
    ShowWindow(hwnd_, cmdShow);
    UpdateWindow(hwnd_);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    MainWindow* self = nullptr;

    if (message == WM_NCCREATE) {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (!self) {
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    return self->HandleMessage(message, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            InitializeFonts();
            CreateAuthControls();
            CreateDashboard();
            SetPage(Page::Auth);
            SetAuthMode(AuthMode::Login);
            return 0;

        case WM_COMMAND:
            if (currentPage_ == Page::Dashboard && dashboard_.HandleCommand(wParam, lParam)) {
                return 0;
            }

            switch (LOWORD(wParam)) {
                case ID_BTN_TAB_LOGIN:
                    SetAuthMode(AuthMode::Login);
                    return 0;
                case ID_BTN_TAB_REGISTER:
                    SetAuthMode(AuthMode::Register);
                    return 0;
                case ID_BTN_LOGIN:
                    HandleLogin();
                    return 0;
                case ID_BTN_REGISTER:
                    HandleRegister();
                    return 0;
                case ID_BTN_LOGOUT:
                    Logout();
                    return 0;
            }
            return 0;

        case WM_DRAWITEM:
            DrawButton(reinterpret_cast<LPDRAWITEMSTRUCT>(lParam));
            return TRUE;

        case WM_CTLCOLORSTATIC: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            HWND control = reinterpret_cast<HWND>(lParam);

            if (IsHeroControl(control)) {
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(244, 248, 252));
                return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
            }

            if (currentPage_ == Page::Dashboard && dashboard_.IsOwnedControl(control)) {
                SetBkMode(hdc, OPAQUE);
                SetBkColor(hdc, Theme::kCardBackground);
                SetTextColor(hdc, Theme::kBodyText);
                static HBRUSH dashboardBrush = CreateSolidBrush(Theme::kCardBackground);
                return reinterpret_cast<INT_PTR>(dashboardBrush);
            }

            SetBkMode(hdc, OPAQUE);
            if (statusLabel_ == control) {
                if (statusKind_ == StatusKind::Success) {
                    SetBkColor(hdc, Theme::kSuccess);
                    SetTextColor(hdc, Theme::kSuccessText);
                    static HBRUSH successBrush = CreateSolidBrush(Theme::kSuccess);
                    return reinterpret_cast<INT_PTR>(successBrush);
                }
                if (statusKind_ == StatusKind::Error) {
                    SetBkColor(hdc, Theme::kError);
                    SetTextColor(hdc, Theme::kErrorText);
                    static HBRUSH errorBrush = CreateSolidBrush(Theme::kError);
                    return reinterpret_cast<INT_PTR>(errorBrush);
                }
                SetBkColor(hdc, Theme::kCardBackground);
                SetTextColor(hdc, Theme::kMutedText);
                static HBRUSH infoBrush = CreateSolidBrush(Theme::kCardBackground);
                return reinterpret_cast<INT_PTR>(infoBrush);
            }

            SetBkColor(hdc, Theme::kCardBackground);
            SetTextColor(hdc, Theme::kBodyText);
            static HBRUSH whiteBrush = CreateSolidBrush(Theme::kCardBackground);
            return reinterpret_cast<INT_PTR>(whiteBrush);
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = reinterpret_cast<HDC>(wParam);
            SetBkColor(hdc, RGB(255, 255, 255));
            SetTextColor(hdc, RGB(28, 31, 36));
            return reinterpret_cast<INT_PTR>(GetStockObject(WHITE_BRUSH));
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd_, &ps);
            PaintWindow(hdc);
            EndPaint(hwnd_, &ps);
            return 0;
        }

        case WM_DESTROY:
            ReleaseFonts();
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

void MainWindow::InitializeFonts() {
    fonts_.heroTitle = CreateFontW(38, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                   OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                   VARIABLE_PITCH, L"Bahnschrift");
    fonts_.heroBody = CreateFontW(21, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  VARIABLE_PITCH, L"Bahnschrift");
    fonts_.title = CreateFontW(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               VARIABLE_PITCH, L"Bahnschrift");
    fonts_.subtitle = CreateFontW(19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                  OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  VARIABLE_PITCH, L"Segoe UI Semibold");
    fonts_.body = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              VARIABLE_PITCH, L"Segoe UI");
    fonts_.small = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               VARIABLE_PITCH, L"Segoe UI");
    fonts_.button = CreateFontW(17, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                VARIABLE_PITCH, L"Segoe UI");
}

void MainWindow::ReleaseFonts() {
    DeleteObject(fonts_.heroTitle);
    DeleteObject(fonts_.heroBody);
    DeleteObject(fonts_.title);
    DeleteObject(fonts_.subtitle);
    DeleteObject(fonts_.body);
    DeleteObject(fonts_.small);
    DeleteObject(fonts_.button);
}

HWND MainWindow::CreateLabel(const wchar_t* text, int x, int y, int w, int h, HFONT font, std::vector<HWND>* bucket) {
    HWND label = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, hwnd_, nullptr, instance_, nullptr);
    SetControlFont(label, font);
    if (bucket) {
        bucket->push_back(label);
    }
    authControls_.push_back(label);
    return label;
}

HWND MainWindow::CreateEdit(int id, int x, int y, int w, int h, bool password) {
    HWND edit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | (password ? ES_PASSWORD : 0),
        x, y, w, h, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
    SetControlFont(edit, fonts_.body);
    authControls_.push_back(edit);
    return edit;
}

HWND MainWindow::CreateButton(const wchar_t* text, int id, int x, int y, int w, int h) {
    HWND button = CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        x, y, w, h, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance_, nullptr);
    SetControlFont(button, fonts_.button);
    authControls_.push_back(button);
    return button;
}

void MainWindow::CreateAuthControls() {
    CreateLabel(L"Hospital", 44, 62, 220, 40, fonts_.heroTitle, &heroControls_);
    CreateLabel(L"Management", 44, 102, 250, 40, fonts_.heroTitle, &heroControls_);
    CreateLabel(L"System", 44, 142, 220, 40, fonts_.heroTitle, &heroControls_);
    CreateLabel(L"Secure access for patients and hospital staff.\nMove from login straight into the live dashboard.", 44, 220, 260, 66, fonts_.body, &heroControls_);
    CreateLabel(L"Appointments, billing, records", 44, 350, 250, 28, fonts_.small, &heroControls_);
    CreateLabel(L"Prescriptions, stock, staffing", 44, 382, 250, 28, fonts_.small, &heroControls_);
    CreateLabel(L"One sign-in point for every role", 44, 414, 250, 28, fonts_.small, &heroControls_);

    authTitleLabel_ = CreateLabel(L"Welcome back", 340, 68, 330, 38, fonts_.title);
    authSubtitleLabel_ = CreateLabel(L"Sign in to continue to the hospital workspace.", 340, 108, 420, 28, fonts_.body);

    btnTabLogin_ = CreateButton(L"Log In", ID_BTN_TAB_LOGIN, 340, 160, 118, 42);
    btnTabRegister_ = CreateButton(L"Register", ID_BTN_TAB_REGISTER, 472, 160, 118, 42);

    statusLabel_ = CreateLabel(L"Use your hospital account to continue.", 340, 222, 480, 28, fonts_.small);

    loginControls_.push_back(CreateLabel(L"Username", 340, 282, 130, 24, fonts_.small));
    loginControls_.push_back(CreateLabel(L"Password", 340, 362, 130, 24, fonts_.small));
    loginControls_.push_back(CreateEdit(ID_EDIT_LOGIN_USERNAME, 340, 310, 320, 38));
    loginControls_.push_back(CreateEdit(ID_EDIT_LOGIN_PASSWORD, 340, 390, 320, 38, true));
    loginControls_.push_back(CreateButton(L"Open Dashboard", ID_BTN_LOGIN, 340, 464, 220, 46));

    registerControls_.push_back(CreateLabel(L"Full name", 340, 264, 120, 24, fonts_.small));
    registerControls_.push_back(CreateLabel(L"Username", 340, 322, 120, 24, fonts_.small));
    registerControls_.push_back(CreateLabel(L"Password", 340, 380, 120, 24, fonts_.small));
    registerControls_.push_back(CreateLabel(L"Confirm password", 340, 438, 150, 24, fonts_.small));
    registerControls_.push_back(CreateLabel(L"Role", 340, 496, 120, 24, fonts_.small));
    registerControls_.push_back(CreateEdit(ID_EDIT_REGISTER_FULLNAME, 340, 290, 340, 34));
    registerControls_.push_back(CreateEdit(ID_EDIT_REGISTER_USERNAME, 340, 348, 340, 34));
    registerControls_.push_back(CreateEdit(ID_EDIT_REGISTER_PASSWORD, 340, 406, 340, 34, true));
    registerControls_.push_back(CreateEdit(ID_EDIT_REGISTER_CONFIRM, 340, 464, 340, 34, true));

    HWND roleCombo = CreateWindowExW(
        0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
        340, 522, 250, 240, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_COMBO_ROLE)), instance_, nullptr);
    SetControlFont(roleCombo, fonts_.body);
    registerControls_.push_back(roleCombo);
    authControls_.push_back(roleCombo);

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

    registerControls_.push_back(CreateButton(L"Create Account", ID_BTN_REGISTER, 340, 584, 220, 46));
}

void MainWindow::CreateDashboard() {
    btnLogout_ = CreateWindowExW(
        0, L"BUTTON", L"Logout", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        44, 760, 178, 46, hwnd_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_LOGOUT)), instance_, nullptr);
    SetControlFont(btnLogout_, fonts_.button);

    DashboardFonts dashboardFonts{fonts_.title, fonts_.subtitle, fonts_.body, fonts_.small};
    dashboard_.Initialize(hwnd_, dashboardFonts, instance_);
}

void MainWindow::SetPage(Page page) {
    currentPage_ = page;
    const bool authVisible = page == Page::Auth;
    ShowControls(authControls_, authVisible);
    dashboard_.Show(!authVisible);
    ShowWindow(btnLogout_, authVisible ? SW_HIDE : SW_SHOW);
    InvalidateRect(hwnd_, nullptr, TRUE);
}

void MainWindow::SetAuthMode(AuthMode mode) {
    authMode_ = mode;

    const bool loginVisible = mode == AuthMode::Login;
    ShowControls(loginControls_, loginVisible);
    ShowControls(registerControls_, !loginVisible);

    if (loginVisible) {
        SetWindowTextW(authTitleLabel_, L"Welcome back");
        SetWindowTextW(authSubtitleLabel_, L"Sign in to continue to the hospital workspace.");
        SetStatusMessage(L"Use your hospital account to continue.", StatusKind::Info);
    } else {
        SetWindowTextW(authTitleLabel_, L"Create an account");
        SetWindowTextW(authSubtitleLabel_, L"Register now and continue directly into the dashboard.");
        SetStatusMessage(L"Choose a role and create your secure account.", StatusKind::Info);
    }
}

void MainWindow::SetStatusMessage(const std::wstring& text, StatusKind kind) {
    statusKind_ = kind;
    SetWindowTextW(statusLabel_, text.c_str());
    InvalidateRect(statusLabel_, nullptr, TRUE);
    UpdateWindow(statusLabel_);
}

void MainWindow::ClearAuthInputs() {
    for (HWND control : authControls_) {
        wchar_t className[32] = {};
        GetClassNameW(control, className, 32);
        if (std::wstring(className) == L"EDIT") {
            SetWindowTextW(control, L"");
        }
    }
}

void MainWindow::ResetRegisterForm() {
    SetWindowTextW(GetDlgItem(hwnd_, ID_EDIT_REGISTER_FULLNAME), L"");
    SetWindowTextW(GetDlgItem(hwnd_, ID_EDIT_REGISTER_USERNAME), L"");
    SetWindowTextW(GetDlgItem(hwnd_, ID_EDIT_REGISTER_PASSWORD), L"");
    SetWindowTextW(GetDlgItem(hwnd_, ID_EDIT_REGISTER_CONFIRM), L"");
    SendMessageW(GetDlgItem(hwnd_, ID_COMBO_ROLE), CB_SETCURSEL, 0, 0);
}

void MainWindow::HandleLogin() {
    const std::wstring username = GetWindowTextString(GetDlgItem(hwnd_, ID_EDIT_LOGIN_USERNAME));
    const std::wstring password = GetWindowTextString(GetDlgItem(hwnd_, ID_EDIT_LOGIN_PASSWORD));

    if (username.empty() || password.empty()) {
        SetStatusMessage(L"Please fill in both login fields.", StatusKind::Error);
        return;
    }

    const auto user = auth_.Login(ToUtf8(username), ToUtf8(password));
    if (!user.has_value()) {
        SetStatusMessage(L"Login failed. Invalid username or password.", StatusKind::Error);
        return;
    }

    SetWindowTextW(GetDlgItem(hwnd_, ID_EDIT_LOGIN_PASSWORD), L"");
    EnterDashboard(*user, L"Login successful. Dashboard ready.");
}

void MainWindow::HandleRegister() {
    const std::wstring fullName = GetWindowTextString(GetDlgItem(hwnd_, ID_EDIT_REGISTER_FULLNAME));
    const std::wstring username = GetWindowTextString(GetDlgItem(hwnd_, ID_EDIT_REGISTER_USERNAME));
    const std::wstring password = GetWindowTextString(GetDlgItem(hwnd_, ID_EDIT_REGISTER_PASSWORD));
    const std::wstring confirm = GetWindowTextString(GetDlgItem(hwnd_, ID_EDIT_REGISTER_CONFIRM));

    wchar_t roleBuffer[128] = {};
    GetWindowTextW(GetDlgItem(hwnd_, ID_COMBO_ROLE), roleBuffer, 128);

    if (fullName.empty() || username.empty() || password.empty() || confirm.empty()) {
        SetStatusMessage(L"Please complete every registration field.", StatusKind::Error);
        return;
    }
    if (password != confirm) {
        SetStatusMessage(L"Passwords do not match.", StatusKind::Error);
        return;
    }

    User user{ToUtf8(fullName), ToUtf8(username), ToUtf8(password), ToUtf8(roleBuffer)};

    std::string errorMessage;
    if (!auth_.RegisterUser(user, errorMessage)) {
        SetStatusMessage(ToWide(errorMessage), StatusKind::Error);
        return;
    }

    ResetRegisterForm();
    EnterDashboard(user, L"Account created successfully. Dashboard ready.");
}

void MainWindow::EnterDashboard(const User& user, const std::wstring& statusText) {
    currentUser_ = user;
    dashboard_.SetUser(user);
    SetPage(Page::Dashboard);
    SetStatusMessage(statusText, StatusKind::Success);
}

void MainWindow::Logout() {
    currentUser_.reset();
    ClearAuthInputs();
    ResetRegisterForm();
    SetPage(Page::Auth);
    SetAuthMode(AuthMode::Login);
}

void MainWindow::PaintWindow(HDC hdc) const {
    RECT client;
    GetClientRect(hwnd_, &client);

    HBRUSH background = CreateSolidBrush(Theme::kWindowBackground);
    FillRect(hdc, &client, background);
    DeleteObject(background);

    PaintSidebar(hdc);

    if (currentPage_ == Page::Auth) {
        PaintAuthPage(hdc);
    } else {
        PaintDashboardPage(hdc);
    }
}

void MainWindow::PaintSidebar(HDC hdc) const {
    RECT sidebar = {20, 20, 266, 820};
    DrawVerticalGradient(hdc, sidebar, Theme::kSidebarStart, Theme::kSidebarEnd);

    HBRUSH accentBrush = CreateSolidBrush(RGB(139, 207, 255));
    HGDIOBJ oldBrush = SelectObject(hdc, accentBrush);
    HGDIOBJ oldPen = SelectObject(hdc, GetStockObject(NULL_PEN));
    Ellipse(hdc, 42, 304, 74, 336);
    Ellipse(hdc, 196, 76, 238, 118);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(accentBrush);
}

void MainWindow::PaintAuthPage(HDC hdc) const {
    RECT card = {310, 32, 860, 650};
    HBRUSH cardBrush = CreateSolidBrush(Theme::kCardBackground);
    HPEN borderPen = CreatePen(PS_SOLID, 1, Theme::kBorder);
    HGDIOBJ oldBrush = SelectObject(hdc, cardBrush);
    HGDIOBJ oldPen = SelectObject(hdc, borderPen);
    RoundRect(hdc, card.left, card.top, card.right, card.bottom, 28, 28);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(cardBrush);
    DeleteObject(borderPen);

    HPEN dividerPen = CreatePen(PS_SOLID, 1, RGB(228, 234, 241));
    oldPen = SelectObject(hdc, dividerPen);
    MoveToEx(hdc, 340, 262, nullptr);
    LineTo(hdc, 780, 262);
    SelectObject(hdc, oldPen);
    DeleteObject(dividerPen);
}

void MainWindow::PaintDashboardPage(HDC hdc) const {
    dashboard_.Paint(hdc);
}

void MainWindow::DrawButton(LPDRAWITEMSTRUCT drawItem) const {
    RECT rect = drawItem->rcItem;
    HDC hdc = drawItem->hDC;
    const UINT controlId = drawItem->CtlID;

    COLORREF fill = Theme::kMutedButton;
    COLORREF border = Theme::kMutedButtonBorder;
    COLORREF text = RGB(39, 62, 92);

    if (controlId == ID_BTN_LOGIN || controlId == ID_BTN_REGISTER || controlId == ID_BTN_LOGOUT || controlId == 2001) {
        fill = Theme::kPrimary;
        border = Theme::kPrimary;
        text = RGB(255, 255, 255);
    } else {
        const bool activeTab =
            (controlId == ID_BTN_TAB_LOGIN && authMode_ == AuthMode::Login && currentPage_ == Page::Auth) ||
            (controlId == ID_BTN_TAB_REGISTER && authMode_ == AuthMode::Register && currentPage_ == Page::Auth);
        if (activeTab) {
            fill = Theme::kPrimaryDark;
            border = Theme::kPrimaryDark;
            text = RGB(255, 255, 255);
        }
    }

    if ((drawItem->itemState & ODS_SELECTED) == ODS_SELECTED) {
        fill = BlendColor(fill, RGB(0, 0, 0), 0.10);
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
        InflateRect(&focusRect, -5, -5);
        DrawFocusRect(hdc, &focusRect);
    }
}

bool MainWindow::IsHeroControl(HWND hwnd) const {
    return std::find(heroControls_.begin(), heroControls_.end(), hwnd) != heroControls_.end();
}
