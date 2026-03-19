#include "DashboardView.h"

#include "StringUtil.h"
#include "Theme.h"
#include "Win32Util.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace {
constexpr int ID_BTN_DASHBOARD_ACTION = 2001;
constexpr int ID_EDIT_DASH_FIELD1 = 2101;
constexpr int ID_EDIT_DASH_FIELD2 = 2102;
constexpr int ID_EDIT_DASH_FIELD3 = 2103;

HWND CreateLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h, HFONT font, HINSTANCE instance) {
    HWND label = CreateWindowExW(
        0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
        x, y, w, h, parent, nullptr, instance, nullptr);
    SetControlFont(label, font);
    return label;
}

HWND CreateEdit(HWND parent, int id, int x, int y, int w, int h, HFONT font, HINSTANCE instance) {
    HWND edit = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    SetControlFont(edit, font);
    return edit;
}

HWND CreateButton(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h, HFONT font, HINSTANCE instance) {
    HWND button = CreateWindowExW(
        0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
        x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    SetControlFont(button, font);
    return button;
}
}

DashboardView::DashboardView() = default;

void DashboardView::Initialize(HWND parent, const DashboardFonts& fonts, HINSTANCE instance) {
    if (initialized_) {
        return;
    }

    parent_ = parent;
    fonts_ = fonts;
    CreateControls(parent, instance);
    initialized_ = true;
}

void DashboardView::SetUser(const User& user) {
    currentUser_ = user;
    UpdateContent();
    UpdateActionConfig();
    ClearActionInputs();
    SetActionStatus(L"");
}

void DashboardView::Show(bool visible) {
    ShowControls(visible);
}

bool DashboardView::IsOwnedControl(HWND hwnd) const {
    return std::find(controls_.begin(), controls_.end(), hwnd) != controls_.end();
}

bool DashboardView::HandleCommand(WPARAM wParam, LPARAM) {
    if (LOWORD(wParam) == ID_BTN_DASHBOARD_ACTION) {
        return SaveAction();
    }

    return false;
}

void DashboardView::CreateControls(HWND parent, HINSTANCE instance) {
    welcomeLabel_ = CreateLabel(parent, L"", 318, 56, 520, 40, fonts_.titleFont, instance);
    roleLabel_ = CreateLabel(parent, L"", 318, 98, 520, 26, fonts_.subtitleFont, instance);
    summaryLabel_ = CreateLabel(parent, L"", 318, 136, 520, 60, fonts_.bodyFont, instance);

    card1Label_ = CreateLabel(parent, L"", 318, 270, 152, 98, fonts_.smallFont, instance);
    card2Label_ = CreateLabel(parent, L"", 493, 270, 152, 98, fonts_.smallFont, instance);
    card3Label_ = CreateLabel(parent, L"", 668, 270, 152, 98, fonts_.smallFont, instance);
    actionSectionLabel_ = CreateLabel(parent, L"", 318, 420, 300, 28, fonts_.subtitleFont, instance);
    actionDescriptionLabel_ = CreateLabel(parent, L"", 318, 454, 500, 40, fonts_.bodyFont, instance);
    field1Label_ = CreateLabel(parent, L"", 318, 506, 140, 22, fonts_.smallFont, instance);
    field2Label_ = CreateLabel(parent, L"", 498, 506, 140, 22, fonts_.smallFont, instance);
    field3Label_ = CreateLabel(parent, L"", 678, 506, 140, 22, fonts_.smallFont, instance);
    field1Edit_ = CreateEdit(parent, ID_EDIT_DASH_FIELD1, 318, 532, 150, 32, fonts_.smallFont, instance);
    field2Edit_ = CreateEdit(parent, ID_EDIT_DASH_FIELD2, 498, 532, 150, 32, fonts_.smallFont, instance);
    field3Edit_ = CreateEdit(parent, ID_EDIT_DASH_FIELD3, 678, 532, 150, 32, fonts_.smallFont, instance);
    actionButton_ = CreateButton(parent, L"", ID_BTN_DASHBOARD_ACTION, 318, 588, 210, 40, fonts_.bodyFont, instance);
    actionStatusLabel_ = CreateLabel(parent, L"", 548, 596, 280, 24, fonts_.smallFont, instance);

    controls_ = {
        welcomeLabel_, roleLabel_, summaryLabel_,
        card1Label_, card2Label_, card3Label_, actionSectionLabel_, actionDescriptionLabel_,
        field1Label_, field2Label_, field3Label_, field1Edit_, field2Edit_, field3Edit_,
        actionButton_, actionStatusLabel_
    };
}

void DashboardView::ShowControls(bool visible) const {
    for (HWND control : controls_) {
        ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
    }
}

void DashboardView::UpdateCardLabel(HWND label, const Card& card) const {
    const std::wstring text = card.title + L"\n" + card.description;
    SetWindowTextW(label, text.c_str());
}

void DashboardView::UpdateContent() {
    SetWindowTextW(welcomeLabel_, (L"Welcome, " + ToWide(currentUser_.fullName)).c_str());
    SetWindowTextW(roleLabel_, (L"Signed in as " + ToWide(currentUser_.role)).c_str());

    std::wstring summary = L"You are now inside the HMS dashboard. This is the landing area for daily actions, shortcuts, and alerts.";
    Card card1{L"Queue", L"Review today's pending tasks and incoming work."};
    Card card2{L"Records", L"Open the latest patients, staff items, or orders."};
    Card card3{L"Actions", L"Jump into the most common role-specific actions."};

    if (currentUser_.role == "Patient") {
        summary = L"Your dashboard keeps appointments, bills, and recent hospital activity in one place.";
        card1 = {L"Appointments", L"Book or manage upcoming visits."};
        card2 = {L"Bills", L"Review outstanding balances and payments."};
        card3 = {L"History", L"See your latest visits and updates."};
    } else if (currentUser_.role == "Doctor") {
        summary = L"Your dashboard surfaces admissions, prescriptions, and patient records for today's shift.";
        card1 = {L"Admissions", L"Create or review patient admissions."};
        card2 = {L"Prescriptions", L"Manage medication and renewals."};
        card3 = {L"Records", L"Open and update medical files."};
    } else if (currentUser_.role == "Pharmacist") {
        summary = L"Your dashboard centralizes medication dispensing and stock visibility.";
        card1 = {L"Dispensing", L"Process medication handoffs."};
        card2 = {L"Stock", L"Track medicine availability."};
        card3 = {L"Patients", L"Review current treatment requests."};
    } else if (currentUser_.role == "Secretary") {
        summary = L"Your dashboard combines patient billing, surgery planning, and front-desk actions.";
        card1 = {L"Billing", L"Create and manage patient accounts."};
        card2 = {L"Surgery", L"Schedule rooms and teams."};
        card3 = {L"Patients", L"Find and assist patient records."};
    } else if (currentUser_.role == "Inventory Manager") {
        summary = L"Your dashboard focuses on stock levels, orders, and restock risk across hospital inventory.";
        card1 = {L"Inventory", L"Monitor current stock levels."};
        card2 = {L"Orders", L"Create and review supply orders."};
        card3 = {L"Alerts", L"See items that need replenishment."};
    } else if (currentUser_.role == "HR Manager") {
        summary = L"Your dashboard is the control center for staffing, shifts, and schedule conflicts.";
        card1 = {L"Shifts", L"Create and revise schedules."};
        card2 = {L"Staff", L"Search personnel and assignments."};
        card3 = {L"Conflicts", L"Review rest-time and leave issues."};
    }

    SetWindowTextW(summaryLabel_, summary.c_str());
    UpdateCardLabel(card1Label_, card1);
    UpdateCardLabel(card2Label_, card2);
    UpdateCardLabel(card3Label_, card3);
}

void DashboardView::UpdateActionConfig() {
    actionConfig_ = GetActionConfigForRole();
    SetWindowTextW(actionSectionLabel_, actionConfig_.sectionTitle.c_str());
    SetWindowTextW(actionDescriptionLabel_, actionConfig_.sectionDescription.c_str());
    SetWindowTextW(field1Label_, actionConfig_.field1Label.c_str());
    SetWindowTextW(field2Label_, actionConfig_.field2Label.c_str());
    SetWindowTextW(field3Label_, actionConfig_.field3Label.c_str());
    SetWindowTextW(actionButton_, actionConfig_.buttonLabel.c_str());
}

void DashboardView::ClearActionInputs() {
    SetWindowTextW(field1Edit_, L"");
    SetWindowTextW(field2Edit_, L"");
    SetWindowTextW(field3Edit_, L"");
}

void DashboardView::SetActionStatus(const std::wstring& text) {
    SetWindowTextW(actionStatusLabel_, text.c_str());
    InvalidateRect(actionStatusLabel_, nullptr, TRUE);
    UpdateWindow(actionStatusLabel_);
}

DashboardView::ActionConfig DashboardView::GetActionConfigForRole() const {
    if (currentUser_.role == "Patient") {
        return {
            L"Book an appointment",
            L"Create a patient appointment request and save it locally.",
            L"Date",
            L"Specialty",
            L"Doctor",
            L"Save Appointment",
            L"Appointment request saved.",
            "patient_appointments.txt"
        };
    }
    if (currentUser_.role == "Doctor") {
        return {
            L"Issue a prescription",
            L"Capture a medication order for a patient.",
            L"Patient",
            L"Medicine",
            L"Dosage",
            L"Save Prescription",
            L"Prescription saved.",
            "doctor_prescriptions.txt"
        };
    }
    if (currentUser_.role == "Pharmacist") {
        return {
            L"Record medicine dispensing",
            L"Track a medication handoff for the pharmacy queue.",
            L"Patient",
            L"Medicine",
            L"Quantity",
            L"Save Dispensing",
            L"Dispensing record saved.",
            "pharmacist_dispensing.txt"
        };
    }
    if (currentUser_.role == "Secretary") {
        return {
            L"Issue a patient bill",
            L"Create and store a billing record for a patient visit.",
            L"Patient",
            L"Service",
            L"Amount",
            L"Save Bill",
            L"Billing record saved.",
            "secretary_billing.txt"
        };
    }
    if (currentUser_.role == "Inventory Manager") {
        return {
            L"Place a restock order",
            L"Create a supply request for medicine or equipment.",
            L"Item",
            L"Quantity",
            L"Supplier",
            L"Save Order",
            L"Restock order saved.",
            "inventory_orders.txt"
        };
    }
    if (currentUser_.role == "HR Manager") {
        return {
            L"Schedule a shift",
            L"Create a staffing assignment and store it locally.",
            L"Employee",
            L"Shift date",
            L"Department",
            L"Save Shift",
            L"Shift saved.",
            "hr_shifts.txt"
        };
    }

    return {
        L"Create a dashboard action",
        L"Save a generic record for this role.",
        L"Field 1",
        L"Field 2",
        L"Field 3",
        L"Save",
        L"Record saved.",
        "dashboard_actions.txt"
    };
}

std::string DashboardView::BuildRecordLine(const std::wstring& field1, const std::wstring& field2, const std::wstring& field3) const {
    std::ostringstream output;
    output << currentUser_.role << '|'
           << currentUser_.username << '|'
           << ToUtf8(field1) << '|'
           << ToUtf8(field2) << '|'
           << ToUtf8(field3);
    return output.str();
}

bool DashboardView::SaveAction() {
    const std::wstring field1 = GetWindowTextString(field1Edit_);
    const std::wstring field2 = GetWindowTextString(field2Edit_);
    const std::wstring field3 = GetWindowTextString(field3Edit_);

    if (field1.empty() || field2.empty() || field3.empty()) {
        SetActionStatus(L"Please fill in every field.");
        return true;
    }

    std::ofstream output(actionConfig_.fileName, std::ios::app);
    if (!output.is_open()) {
        SetActionStatus(L"Unable to save this record.");
        return true;
    }

    output << BuildRecordLine(field1, field2, field3) << '\n';
    ClearActionInputs();
    SetActionStatus(actionConfig_.successMessage);
    return true;
}

void DashboardView::Paint(HDC hdc) const {
    RECT headerCard = {290, 32, 846, 226};
    HBRUSH cardBrush = CreateSolidBrush(Theme::kCardBackground);
    HPEN cardPen = CreatePen(PS_SOLID, 1, Theme::kBorder);
    HGDIOBJ oldBrush = SelectObject(hdc, cardBrush);
    HGDIOBJ oldPen = SelectObject(hdc, cardPen);
    RoundRect(hdc, headerCard.left, headerCard.top, headerCard.right, headerCard.bottom, 26, 26);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(cardBrush);
    DeleteObject(cardPen);

    const RECT cards[] = {
        {318, 252, 478, 374},
        {493, 252, 653, 374},
        {668, 252, 828, 374}
    };

    for (const RECT& card : cards) {
        HBRUSH tileBrush = CreateSolidBrush(RGB(250, 252, 255));
        HPEN tilePen = CreatePen(PS_SOLID, 1, Theme::kBorder);
        HGDIOBJ savedBrush = SelectObject(hdc, tileBrush);
        HGDIOBJ savedPen = SelectObject(hdc, tilePen);
        RoundRect(hdc, card.left, card.top, card.right, card.bottom, 22, 22);
        SelectObject(hdc, savedBrush);
        SelectObject(hdc, savedPen);
        DeleteObject(tileBrush);
        DeleteObject(tilePen);
    }

    RECT actionCard = {290, 402, 846, 646};
    HBRUSH actionBrush = CreateSolidBrush(Theme::kCardBackground);
    HPEN actionPen = CreatePen(PS_SOLID, 1, Theme::kBorder);
    HGDIOBJ oldBrush2 = SelectObject(hdc, actionBrush);
    HGDIOBJ oldPen2 = SelectObject(hdc, actionPen);
    RoundRect(hdc, actionCard.left, actionCard.top, actionCard.right, actionCard.bottom, 24, 24);
    SelectObject(hdc, oldBrush2);
    SelectObject(hdc, oldPen2);
    DeleteObject(actionBrush);
    DeleteObject(actionPen);
}
