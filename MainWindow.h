#pragma once

#include "AuthService.h"
#include "DashboardView.h"
#include "User.h"

#include <windows.h>

#include <optional>
#include <vector>

enum class Page {
    Auth,
    Dashboard
};

enum class StatusKind {
    Info,
    Success,
    Error
};

class MainWindow {
public:
    explicit MainWindow(HINSTANCE instance);

    bool Create();
    int Run(int cmdShow);

private:
    enum class AuthMode {
        Login,
        Register
    };

    struct Fonts {
        HFONT heroTitle = nullptr;
        HFONT heroBody = nullptr;
        HFONT title = nullptr;
        HFONT subtitle = nullptr;
        HFONT body = nullptr;
        HFONT small = nullptr;
        HFONT button = nullptr;
    };

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void InitializeFonts();
    void ReleaseFonts();
    void CreateAuthControls();
    void CreateDashboard();
    void SetPage(Page page);
    void SetAuthMode(AuthMode mode);
    void SetStatusMessage(const std::wstring& text, StatusKind kind);
    void ClearAuthInputs();
    void ResetRegisterForm();
    void HandleLogin();
    void HandleRegister();
    void EnterDashboard(const User& user, const std::wstring& statusText);
    void Logout();

    void PaintWindow(HDC hdc) const;
    void PaintSidebar(HDC hdc) const;
    void PaintAuthPage(HDC hdc) const;
    void PaintDashboardPage(HDC hdc) const;
    void DrawButton(LPDRAWITEMSTRUCT drawItem) const;
    bool IsHeroControl(HWND hwnd) const;

    HWND CreateLabel(const wchar_t* text, int x, int y, int w, int h, HFONT font, std::vector<HWND>* bucket = nullptr);
    HWND CreateEdit(int id, int x, int y, int w, int h, bool password = false);
    HWND CreateButton(const wchar_t* text, int id, int x, int y, int w, int h);

    HINSTANCE instance_;
    HWND hwnd_ = nullptr;
    AuthService auth_;
    DashboardView dashboard_;
    Fonts fonts_{};
    Page currentPage_ = Page::Auth;
    AuthMode authMode_ = AuthMode::Login;
    StatusKind statusKind_ = StatusKind::Info;
    std::optional<User> currentUser_;

    HWND authTitleLabel_ = nullptr;
    HWND authSubtitleLabel_ = nullptr;
    HWND statusLabel_ = nullptr;
    HWND btnTabLogin_ = nullptr;
    HWND btnTabRegister_ = nullptr;
    HWND btnLogout_ = nullptr;

    std::vector<HWND> heroControls_;
    std::vector<HWND> loginControls_;
    std::vector<HWND> registerControls_;
    std::vector<HWND> authControls_;
};
