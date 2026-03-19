#include "DashboardView.h"

#include "StringUtil.h"
#include "Theme.h"
#include "Win32Util.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace {
constexpr int ID_BTN_DASHBOARD_ACTION = 2001;
constexpr int ID_BTN_CARD1 = 2002;
constexpr int ID_BTN_CARD2 = 2003;
constexpr int ID_BTN_CARD3 = 2004;
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
    patientSection_ = PatientSection::Appointments;
    doctorSection_ = DoctorSection::Admissions;
    pharmacistSection_ = PharmacistSection::Dispensing;
    secretarySection_ = SecretarySection::Billing;
    inventorySection_ = InventorySection::Inventory;
    hrSection_ = HrSection::Shifts;
    SetWindowTextW(historyLabel_, L"");
    UpdateContent();
    UpdateActionConfig();
    ClearActionInputs();
    SetActionStatus(L"");
    InvalidateRect(parent_, nullptr, TRUE);
    UpdateWindow(parent_);
}

void DashboardView::Show(bool visible) {
    ShowControls(visible);
}

bool DashboardView::IsOwnedControl(HWND hwnd) const {
    return std::find(controls_.begin(), controls_.end(), hwnd) != controls_.end();
}

bool DashboardView::HandleCommand(WPARAM wParam, LPARAM) {
    switch (LOWORD(wParam)) {
        case ID_BTN_DASHBOARD_ACTION:
            return SaveAction();
        case ID_BTN_CARD1:
            if (currentUser_.role == "Patient") {
                SetPatientSection(PatientSection::Appointments);
            } else if (currentUser_.role == "Doctor") {
                SetDoctorSection(DoctorSection::Admissions);
            } else if (currentUser_.role == "Pharmacist") {
                SetPharmacistSection(PharmacistSection::Dispensing);
            } else if (currentUser_.role == "Secretary") {
                SetSecretarySection(SecretarySection::Billing);
            } else if (currentUser_.role == "Inventory Manager") {
                SetInventorySection(InventorySection::Inventory);
            } else if (currentUser_.role == "HR Manager") {
                SetHrSection(HrSection::Shifts);
            } else {
                return false;
            }
            return true;
        case ID_BTN_CARD2:
            if (currentUser_.role == "Patient") {
                SetPatientSection(PatientSection::Bills);
            } else if (currentUser_.role == "Doctor") {
                SetDoctorSection(DoctorSection::Prescriptions);
            } else if (currentUser_.role == "Pharmacist") {
                SetPharmacistSection(PharmacistSection::Stock);
            } else if (currentUser_.role == "Secretary") {
                SetSecretarySection(SecretarySection::Surgery);
            } else if (currentUser_.role == "Inventory Manager") {
                SetInventorySection(InventorySection::Orders);
            } else if (currentUser_.role == "HR Manager") {
                SetHrSection(HrSection::Staff);
            } else {
                return false;
            }
            return true;
        case ID_BTN_CARD3:
            if (currentUser_.role == "Patient") {
                SetPatientSection(PatientSection::History);
            } else if (currentUser_.role == "Doctor") {
                SetDoctorSection(DoctorSection::Records);
            } else if (currentUser_.role == "Pharmacist") {
                SetPharmacistSection(PharmacistSection::Patients);
            } else if (currentUser_.role == "Secretary") {
                SetSecretarySection(SecretarySection::Patients);
            } else if (currentUser_.role == "Inventory Manager") {
                SetInventorySection(InventorySection::Alerts);
            } else if (currentUser_.role == "HR Manager") {
                SetHrSection(HrSection::Conflicts);
            } else {
                return false;
            }
            return true;
    }

    return false;
}

bool DashboardView::IsDashboardButtonId(UINT controlId) const {
    return controlId == ID_BTN_DASHBOARD_ACTION ||
           controlId == ID_BTN_CARD1 ||
           controlId == ID_BTN_CARD2 ||
           controlId == ID_BTN_CARD3;
}

bool DashboardView::IsActiveDashboardButton(UINT controlId) const {
    if (currentUser_.role == "Doctor") {
        return (controlId == ID_BTN_CARD1 && doctorSection_ == DoctorSection::Admissions) ||
               (controlId == ID_BTN_CARD2 && doctorSection_ == DoctorSection::Prescriptions) ||
               (controlId == ID_BTN_CARD3 && doctorSection_ == DoctorSection::Records);
    }

    if (currentUser_.role == "Pharmacist") {
        return (controlId == ID_BTN_CARD1 && pharmacistSection_ == PharmacistSection::Dispensing) ||
               (controlId == ID_BTN_CARD2 && pharmacistSection_ == PharmacistSection::Stock) ||
               (controlId == ID_BTN_CARD3 && pharmacistSection_ == PharmacistSection::Patients);
    }

    if (currentUser_.role == "Secretary") {
        return (controlId == ID_BTN_CARD1 && secretarySection_ == SecretarySection::Billing) ||
               (controlId == ID_BTN_CARD2 && secretarySection_ == SecretarySection::Surgery) ||
               (controlId == ID_BTN_CARD3 && secretarySection_ == SecretarySection::Patients);
    }

    if (currentUser_.role == "Inventory Manager") {
        return (controlId == ID_BTN_CARD1 && inventorySection_ == InventorySection::Inventory) ||
               (controlId == ID_BTN_CARD2 && inventorySection_ == InventorySection::Orders) ||
               (controlId == ID_BTN_CARD3 && inventorySection_ == InventorySection::Alerts);
    }

    if (currentUser_.role == "HR Manager") {
        return (controlId == ID_BTN_CARD1 && hrSection_ == HrSection::Shifts) ||
               (controlId == ID_BTN_CARD2 && hrSection_ == HrSection::Staff) ||
               (controlId == ID_BTN_CARD3 && hrSection_ == HrSection::Conflicts);
    }

    if (currentUser_.role != "Patient") {
        return false;
    }

    return (controlId == ID_BTN_CARD1 && patientSection_ == PatientSection::Appointments) ||
           (controlId == ID_BTN_CARD2 && patientSection_ == PatientSection::Bills) ||
           (controlId == ID_BTN_CARD3 && patientSection_ == PatientSection::History);
}

void DashboardView::CreateControls(HWND parent, HINSTANCE instance) {
    welcomeLabel_ = CreateLabel(parent, L"", 318, 56, 520, 40, fonts_.titleFont, instance);
    roleLabel_ = CreateLabel(parent, L"", 318, 98, 520, 26, fonts_.subtitleFont, instance);
    summaryLabel_ = CreateLabel(parent, L"", 318, 136, 520, 60, fonts_.bodyFont, instance);

    card1Button_ = CreateButton(parent, L"", ID_BTN_CARD1, 318, 252, 160, 122, fonts_.smallFont, instance);
    card2Button_ = CreateButton(parent, L"", ID_BTN_CARD2, 493, 252, 160, 122, fonts_.smallFont, instance);
    card3Button_ = CreateButton(parent, L"", ID_BTN_CARD3, 668, 252, 160, 122, fonts_.smallFont, instance);
    actionSectionLabel_ = CreateLabel(parent, L"", 318, 406, 300, 28, fonts_.subtitleFont, instance);
    actionDescriptionLabel_ = CreateLabel(parent, L"", 318, 438, 500, 36, fonts_.bodyFont, instance);
    field1Label_ = CreateLabel(parent, L"", 318, 484, 140, 22, fonts_.smallFont, instance);
    field2Label_ = CreateLabel(parent, L"", 498, 484, 140, 22, fonts_.smallFont, instance);
    field3Label_ = CreateLabel(parent, L"", 678, 484, 140, 22, fonts_.smallFont, instance);
    field1Edit_ = CreateEdit(parent, ID_EDIT_DASH_FIELD1, 318, 508, 150, 32, fonts_.smallFont, instance);
    field2Edit_ = CreateEdit(parent, ID_EDIT_DASH_FIELD2, 498, 508, 150, 32, fonts_.smallFont, instance);
    field3Edit_ = CreateEdit(parent, ID_EDIT_DASH_FIELD3, 678, 508, 150, 32, fonts_.smallFont, instance);
    actionButton_ = CreateButton(parent, L"", ID_BTN_DASHBOARD_ACTION, 318, 552, 210, 40, fonts_.bodyFont, instance);
    actionStatusLabel_ = CreateLabel(parent, L"", 548, 560, 280, 24, fonts_.smallFont, instance);
    historyLabel_ = CreateLabel(parent, L"", 318, 486, 510, 110, fonts_.smallFont, instance);

    controls_ = {
        welcomeLabel_, roleLabel_, summaryLabel_,
        card1Button_, card2Button_, card3Button_, actionSectionLabel_, actionDescriptionLabel_,
        field1Label_, field2Label_, field3Label_, field1Edit_, field2Edit_, field3Edit_,
        actionButton_, actionStatusLabel_, historyLabel_
    };
}

void DashboardView::ShowControls(bool visible) const {
    for (HWND control : controls_) {
        ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
    }
}

void DashboardView::UpdateCardLabel(HWND label, const Card& card) const {
    const std::wstring text = card.title + L"\n\n" + card.description;
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
    UpdateCardLabel(card1Button_, card1);
    UpdateCardLabel(card2Button_, card2);
    UpdateCardLabel(card3Button_, card3);
}

void DashboardView::UpdateActionConfig() {
    actionConfig_ = GetActionConfigForRole();
    SetWindowTextW(actionSectionLabel_, actionConfig_.sectionTitle.c_str());
    SetWindowTextW(actionDescriptionLabel_, actionConfig_.sectionDescription.c_str());
    SetWindowTextW(field1Label_, actionConfig_.field1Label.c_str());
    SetWindowTextW(field2Label_, actionConfig_.field2Label.c_str());
    SetWindowTextW(field3Label_, actionConfig_.field3Label.c_str());
    SetWindowTextW(actionButton_, actionConfig_.buttonLabel.c_str());

    const int formCommand = actionConfig_.showForm ? SW_SHOW : SW_HIDE;
    ShowWindow(field1Label_, formCommand);
    ShowWindow(field2Label_, formCommand);
    ShowWindow(field3Label_, formCommand);
    ShowWindow(field1Edit_, formCommand);
    ShowWindow(field2Edit_, formCommand);
    ShowWindow(field3Edit_, formCommand);
    ShowWindow(actionButton_, formCommand);
    ShowWindow(historyLabel_, actionConfig_.showForm ? SW_HIDE : SW_SHOW);

    if (!actionConfig_.showForm) {
        UpdatePatientHistory();
    }

    InvalidateRect(parent_, nullptr, TRUE);
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
        if (patientSection_ == PatientSection::Appointments) {
            return {
                L"Book an appointment",
                L"Create a patient appointment request and save it locally.",
                L"Date",
                L"Specialty",
                L"Doctor",
                L"Save Appointment",
                L"Appointment request saved.",
                "patient_appointments",
                true
            };
        }
        if (patientSection_ == PatientSection::Bills) {
            return {
                L"Pay a bill",
                L"Record a bill payment for the patient account.",
                L"Bill ID",
                L"Method",
                L"Amount",
                L"Save Payment",
                L"Bill payment saved.",
                "patient_payments",
                true
            };
        }
        return {
            L"Visit history",
            L"Recent appointment and payment activity for this patient.",
            L"",
            L"",
            L"",
            L"",
            L"",
            "",
            false
        };
    }
    if (currentUser_.role == "Doctor") {
        if (doctorSection_ == DoctorSection::Admissions) {
            return {
                L"Create an admission",
                L"Store a patient admission request for the current shift.",
                L"Patient",
                L"Clinic",
                L"Reason",
                L"Save Admission",
                L"Admission saved.",
                "doctor_admissions",
                true
            };
        }
        if (doctorSection_ == DoctorSection::Prescriptions) {
            return {
                L"Issue a prescription",
                L"Capture a medication order for a patient.",
                L"Patient",
                L"Medicine",
                L"Dosage",
                L"Save Prescription",
                L"Prescription saved.",
                "doctor_prescriptions",
                true
            };
        }
        return {
            L"Update a medical record",
            L"Write a concise patient note and save it to the doctor's records.",
            L"Patient",
            L"Record type",
            L"Note",
            L"Save Record Note",
            L"Record note saved.",
            "doctor_records",
            true
        };
    }
    if (currentUser_.role == "Pharmacist") {
        if (pharmacistSection_ == PharmacistSection::Dispensing) {
            return {
                L"Record medicine dispensing",
                L"Track a medication handoff for the pharmacy queue.",
                L"Patient",
                L"Medicine",
                L"Quantity",
                L"Save Dispensing",
                L"Dispensing record saved.",
                "pharmacist_dispensing",
                true
            };
        }
        if (pharmacistSection_ == PharmacistSection::Stock) {
            return {
                L"Update medicine stock",
                L"Store a pharmacy stock check or restock update.",
                L"Medicine",
                L"Available qty",
                L"Batch note",
                L"Save Stock Update",
                L"Stock update saved.",
                "pharmacist_stock",
                true
            };
        }
        return {
            L"Review patient treatment",
            L"Record a patient medication review or pharmacy follow-up.",
            L"Patient",
            L"Prescription ID",
            L"Status",
            L"Save Review",
            L"Patient review saved.",
            "pharmacist_reviews",
            true
        };
    }
    if (currentUser_.role == "Secretary") {
        if (secretarySection_ == SecretarySection::Billing) {
            return {
                L"Issue a patient bill",
                L"Create and store a billing record for a patient visit.",
                L"Patient",
                L"Service",
                L"Amount",
                L"Save Bill",
                L"Billing record saved.",
                "secretary_billing",
                true
            };
        }
        if (secretarySection_ == SecretarySection::Surgery) {
            return {
                L"Schedule a surgery",
                L"Store a surgery scheduling request for the secretary team.",
                L"Patient",
                L"Date",
                L"Operating room",
                L"Save Surgery",
                L"Surgery schedule saved.",
                "secretary_surgeries",
                true
            };
        }
        return {
            L"Register a patient support task",
            L"Track a patient-facing front-desk task or coordination note.",
            L"Patient",
            L"Task type",
            L"Note",
            L"Save Patient Task",
            L"Patient support task saved.",
            "secretary_patient_tasks",
            true
        };
    }
    if (currentUser_.role == "Inventory Manager") {
        if (inventorySection_ == InventorySection::Inventory) {
            return {
                L"Update inventory status",
                L"Store a medicine or equipment stock snapshot.",
                L"Item",
                L"Available qty",
                L"Location",
                L"Save Inventory",
                L"Inventory update saved.",
                "inventory_status",
                true
            };
        }
        if (inventorySection_ == InventorySection::Orders) {
            return {
                L"Place a restock order",
                L"Create a supply request for medicine or equipment.",
                L"Item",
                L"Quantity",
                L"Supplier",
                L"Save Order",
                L"Restock order saved.",
                "inventory_orders",
                true
            };
        }
        return {
            L"Log a stock alert",
            L"Record a low-stock or unavailable item alert.",
            L"Item",
            L"Alert level",
            L"Note",
            L"Save Alert",
            L"Inventory alert saved.",
            "inventory_alerts",
            true
        };
    }
    if (currentUser_.role == "HR Manager") {
        if (hrSection_ == HrSection::Shifts) {
            return {
                L"Schedule a shift",
                L"Create a staffing assignment and store it locally.",
                L"Employee",
                L"Shift date",
                L"Department",
                L"Save Shift",
                L"Shift saved.",
                "hr_shifts",
                true
            };
        }
        if (hrSection_ == HrSection::Staff) {
            return {
                L"Update staff assignment",
                L"Store a staff role or department update.",
                L"Employee",
                L"Role",
                L"Department",
                L"Save Staff Update",
                L"Staff update saved.",
                "hr_staff_updates",
                true
            };
        }
        return {
            L"Log a schedule conflict",
            L"Record a staffing or leave conflict for review.",
            L"Employee",
            L"Conflict date",
            L"Issue",
            L"Save Conflict",
            L"Conflict saved.",
            "hr_conflicts",
            true
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
        "dashboard_actions",
        true
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
    if (!actionConfig_.showForm) {
        UpdatePatientHistory();
        return true;
    }

    const std::wstring field1 = GetWindowTextString(field1Edit_);
    const std::wstring field2 = GetWindowTextString(field2Edit_);
    const std::wstring field3 = GetWindowTextString(field3Edit_);

    if (field1.empty() || field2.empty() || field3.empty()) {
        SetActionStatus(L"Please fill in every field.");
        return true;
    }

    std::string errorMessage;
    if (!repository_.SaveAction(
            actionConfig_.storageKey,
            currentUser_,
            ToUtf8(field1),
            ToUtf8(field2),
            ToUtf8(field3),
            errorMessage)) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    ClearActionInputs();
    SetActionStatus(actionConfig_.successMessage);
    if (currentUser_.role == "Patient") {
        UpdatePatientHistory();
    }
    return true;
}

void DashboardView::SetPatientSection(PatientSection section) {
    if (currentUser_.role != "Patient") {
        return;
    }

    patientSection_ = section;
    UpdateActionConfig();
    ClearActionInputs();
    SetActionStatus(L"");
    InvalidateRect(parent_, nullptr, TRUE);
    UpdateWindow(parent_);
}

void DashboardView::SetDoctorSection(DoctorSection section) {
    if (currentUser_.role != "Doctor") {
        return;
    }

    doctorSection_ = section;
    UpdateActionConfig();
    ClearActionInputs();
    SetActionStatus(L"");
    InvalidateRect(parent_, nullptr, TRUE);
    UpdateWindow(parent_);
}

void DashboardView::SetPharmacistSection(PharmacistSection section) {
    if (currentUser_.role != "Pharmacist") {
        return;
    }

    pharmacistSection_ = section;
    UpdateActionConfig();
    ClearActionInputs();
    SetActionStatus(L"");
    InvalidateRect(parent_, nullptr, TRUE);
    UpdateWindow(parent_);
}

void DashboardView::SetSecretarySection(SecretarySection section) {
    if (currentUser_.role != "Secretary") {
        return;
    }

    secretarySection_ = section;
    UpdateActionConfig();
    ClearActionInputs();
    SetActionStatus(L"");
    InvalidateRect(parent_, nullptr, TRUE);
    UpdateWindow(parent_);
}

void DashboardView::SetInventorySection(InventorySection section) {
    if (currentUser_.role != "Inventory Manager") {
        return;
    }

    inventorySection_ = section;
    UpdateActionConfig();
    ClearActionInputs();
    SetActionStatus(L"");
    InvalidateRect(parent_, nullptr, TRUE);
    UpdateWindow(parent_);
}

void DashboardView::SetHrSection(HrSection section) {
    if (currentUser_.role != "HR Manager") {
        return;
    }

    hrSection_ = section;
    UpdateActionConfig();
    ClearActionInputs();
    SetActionStatus(L"");
    InvalidateRect(parent_, nullptr, TRUE);
    UpdateWindow(parent_);
}

std::vector<std::wstring> DashboardView::LoadRecentPatientHistory() const {
    std::vector<std::wstring> history = repository_.LoadRecentPatientHistory(currentUser_);
    if (!history.empty()) {
        return history;
    }

    history.clear();

    const auto readFile = [&](const std::string& fileName, const std::wstring& prefix) {
        std::ifstream input(fileName);
        if (!input.is_open()) {
            return;
        }

        std::string line;
        while (std::getline(input, line)) {
            if (line.empty()) {
                continue;
            }

            std::stringstream ss(line);
            std::string role;
            std::string username;
            std::string field1;
            std::string field2;
            std::string field3;

            std::getline(ss, role, '|');
            std::getline(ss, username, '|');
            std::getline(ss, field1, '|');
            std::getline(ss, field2, '|');
            std::getline(ss, field3, '|');

            if (username == currentUser_.username) {
                history.push_back(prefix + L": " + ToWide(field1) + L" | " + ToWide(field2) + L" | " + ToWide(field3));
            }
        }
    };

    readFile("patient_appointments.txt", L"Appointment");
    readFile("patient_bill_payments.txt", L"Payment");

    if (history.size() > 6) {
        history.erase(history.begin(), history.end() - 6);
    }

    return history;
}

void DashboardView::UpdatePatientHistory() {
    const std::vector<std::wstring> history = LoadRecentPatientHistory();
    if (history.empty()) {
        SetWindowTextW(historyLabel_, L"No patient history saved yet.");
        return;
    }

    std::wstring combined;
    for (std::size_t i = 0; i < history.size(); ++i) {
        if (!combined.empty()) {
            combined += L"\n";
        }
        combined += history[i];
    }

    SetWindowTextW(historyLabel_, combined.c_str());
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

    RECT actionCard = {290, 390, 846, 622};
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
