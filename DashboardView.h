#pragma once

#include "User.h"

#include <windows.h>

#include <string>
#include <vector>

struct DashboardFonts {
    HFONT titleFont = nullptr;
    HFONT subtitleFont = nullptr;
    HFONT bodyFont = nullptr;
    HFONT smallFont = nullptr;
};

class DashboardView {
public:
    DashboardView();

    void Initialize(HWND parent, const DashboardFonts& fonts, HINSTANCE instance);
    void SetUser(const User& user);
    void Show(bool visible);
    void Paint(HDC hdc) const;
    bool IsOwnedControl(HWND hwnd) const;
    bool HandleCommand(WPARAM wParam, LPARAM lParam);

private:
    struct Card {
        std::wstring title;
        std::wstring description;
    };

    struct ActionConfig {
        std::wstring sectionTitle;
        std::wstring sectionDescription;
        std::wstring field1Label;
        std::wstring field2Label;
        std::wstring field3Label;
        std::wstring buttonLabel;
        std::wstring successMessage;
        std::string fileName;
    };

    void CreateControls(HWND parent, HINSTANCE instance);
    void UpdateContent();
    void ShowControls(bool visible) const;
    void UpdateCardLabel(HWND label, const Card& card) const;
    void UpdateActionConfig();
    void ClearActionInputs();
    void SetActionStatus(const std::wstring& text);
    bool SaveAction();
    ActionConfig GetActionConfigForRole() const;
    std::string BuildRecordLine(const std::wstring& field1, const std::wstring& field2, const std::wstring& field3) const;

    HWND parent_ = nullptr;
    DashboardFonts fonts_{};
    User currentUser_{};
    bool initialized_ = false;

    HWND welcomeLabel_ = nullptr;
    HWND roleLabel_ = nullptr;
    HWND summaryLabel_ = nullptr;
    HWND card1Label_ = nullptr;
    HWND card2Label_ = nullptr;
    HWND card3Label_ = nullptr;
    HWND actionSectionLabel_ = nullptr;
    HWND actionDescriptionLabel_ = nullptr;
    HWND field1Label_ = nullptr;
    HWND field2Label_ = nullptr;
    HWND field3Label_ = nullptr;
    HWND field1Edit_ = nullptr;
    HWND field2Edit_ = nullptr;
    HWND field3Edit_ = nullptr;
    HWND actionButton_ = nullptr;
    HWND actionStatusLabel_ = nullptr;
    std::vector<HWND> controls_;
    ActionConfig actionConfig_{};
};
