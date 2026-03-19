#include "DashboardView.h"

#include "StringUtil.h"
#include "Theme.h"
#include "Win32Util.h"

#include <algorithm>

namespace {
HWND CreateLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h, HFONT font, HINSTANCE instance) {
    HWND label = CreateWindowExW(
        0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
        x, y, w, h, parent, nullptr, instance, nullptr);
    SetControlFont(label, font);
    return label;
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
}

void DashboardView::Show(bool visible) {
    ShowControls(visible);
}

bool DashboardView::IsOwnedControl(HWND hwnd) const {
    return std::find(controls_.begin(), controls_.end(), hwnd) != controls_.end();
}

void DashboardView::CreateControls(HWND parent, HINSTANCE instance) {
    welcomeLabel_ = CreateLabel(parent, L"", 318, 58, 520, 42, fonts_.titleFont, instance);
    roleLabel_ = CreateLabel(parent, L"", 318, 102, 520, 28, fonts_.subtitleFont, instance);
    summaryLabel_ = CreateLabel(parent, L"", 318, 142, 520, 54, fonts_.bodyFont, instance);
    quickTitleLabel_ = CreateLabel(parent, L"Today's focus", 318, 226, 220, 28, fonts_.subtitleFont, instance);
    quickBodyLabel_ = CreateLabel(parent, L"", 318, 258, 470, 60, fonts_.bodyFont, instance);

    card1Label_ = CreateLabel(parent, L"", 318, 388, 152, 98, fonts_.smallFont, instance);
    card2Label_ = CreateLabel(parent, L"", 493, 388, 152, 98, fonts_.smallFont, instance);
    card3Label_ = CreateLabel(parent, L"", 668, 388, 152, 98, fonts_.smallFont, instance);

    controls_ = {welcomeLabel_, roleLabel_, summaryLabel_, quickTitleLabel_, quickBodyLabel_, card1Label_, card2Label_, card3Label_};
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
    std::wstring focus = L"Use the left navigation to move between your core workflows, recent activity, and department tools.";
    Card card1{L"Queue", L"Review today's pending tasks and incoming work."};
    Card card2{L"Records", L"Open the latest patients, staff items, or orders."};
    Card card3{L"Actions", L"Jump into the most common role-specific actions."};

    if (currentUser_.role == "Patient") {
        summary = L"Your dashboard keeps appointments, bills, and recent hospital activity in one place.";
        focus = L"Start with appointments, check unpaid bills, or review your recent activity.";
        card1 = {L"Appointments", L"Book or manage upcoming visits."};
        card2 = {L"Bills", L"Review outstanding balances and payments."};
        card3 = {L"History", L"See your latest visits and updates."};
    } else if (currentUser_.role == "Doctor") {
        summary = L"Your dashboard surfaces admissions, prescriptions, and patient records for today's shift.";
        focus = L"Open your patient queue, issue prescriptions, or continue from unfinished clinical work.";
        card1 = {L"Admissions", L"Create or review patient admissions."};
        card2 = {L"Prescriptions", L"Manage medication and renewals."};
        card3 = {L"Records", L"Open and update medical files."};
    } else if (currentUser_.role == "Pharmacist") {
        summary = L"Your dashboard centralizes medication dispensing and stock visibility.";
        focus = L"Check today's dispensing queue and confirm stock before completing requests.";
        card1 = {L"Dispensing", L"Process medication handoffs."};
        card2 = {L"Stock", L"Track medicine availability."};
        card3 = {L"Patients", L"Review current treatment requests."};
    } else if (currentUser_.role == "Secretary") {
        summary = L"Your dashboard combines patient billing, surgery planning, and front-desk actions.";
        focus = L"Start with billing tasks or jump to surgery scheduling and patient coordination.";
        card1 = {L"Billing", L"Create and manage patient accounts."};
        card2 = {L"Surgery", L"Schedule rooms and teams."};
        card3 = {L"Patients", L"Find and assist patient records."};
    } else if (currentUser_.role == "Inventory Manager") {
        summary = L"Your dashboard focuses on stock levels, orders, and restock risk across hospital inventory.";
        focus = L"Review low-stock items first, then place equipment and medicine orders.";
        card1 = {L"Inventory", L"Monitor current stock levels."};
        card2 = {L"Orders", L"Create and review supply orders."};
        card3 = {L"Alerts", L"See items that need replenishment."};
    } else if (currentUser_.role == "HR Manager") {
        summary = L"Your dashboard is the control center for staffing, shifts, and schedule conflicts.";
        focus = L"Open staff scheduling, review conflicts, and manage today's workforce needs.";
        card1 = {L"Shifts", L"Create and revise schedules."};
        card2 = {L"Staff", L"Search personnel and assignments."};
        card3 = {L"Conflicts", L"Review rest-time and leave issues."};
    }

    SetWindowTextW(summaryLabel_, summary.c_str());
    SetWindowTextW(quickBodyLabel_, focus.c_str());
    UpdateCardLabel(card1Label_, card1);
    UpdateCardLabel(card2Label_, card2);
    UpdateCardLabel(card3Label_, card3);
}

void DashboardView::Paint(HDC hdc) const {
    RECT headerCard = {290, 32, 846, 344};
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
        {318, 370, 478, 492},
        {493, 370, 653, 492},
        {668, 370, 828, 492}
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
}
