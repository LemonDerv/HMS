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
constexpr int ID_BTN_DASH_SECONDARY = 2005;
constexpr int ID_BTN_DASH_TERTIARY = 2006;
constexpr int ID_EDIT_DASH_FIELD1 = 2101;
constexpr int ID_EDIT_DASH_FIELD2 = 2102;
constexpr int ID_EDIT_DASH_FIELD3 = 2103;
constexpr int ID_EDIT_DASH_FIELD4 = 2104;
constexpr int ID_EDIT_DASH_FIELD5 = 2105;
constexpr int ID_LIST_DASHBOARD = 2201;

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

bool TryParseIntegerValue(const std::string& text, int& value) {
    try {
        std::size_t parsedLength = 0;
        const int parsedValue = std::stoi(text, &parsedLength);
        if (parsedLength != text.size()) {
            return false;
        }
        value = parsedValue;
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<std::string> SplitPayload(const std::string& text) {
    std::vector<std::string> values;
    std::stringstream ss(text);
    std::string part;
    while (std::getline(ss, part, '|')) {
        values.push_back(part);
    }
    return values;
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
    ReleaseMedicalRecordLockIfHeld();
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
            return IsSpecialWorkflowSection() ? HandlePrimaryWorkflowAction() : SaveAction();
        case ID_BTN_DASH_SECONDARY:
            return HandleSecondaryWorkflowAction();
        case ID_BTN_DASH_TERTIARY:
            return HandleTertiaryWorkflowAction();
        case ID_LIST_DASHBOARD:
            if (HIWORD(wParam) == LBN_SELCHANGE) {
                UpdateWorkflowButtons();
                return true;
            }
            return false;
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
           controlId == ID_BTN_DASH_SECONDARY ||
           controlId == ID_BTN_DASH_TERTIARY ||
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
    welcomeLabel_ = CreateLabel(parent, L"", 48, 48, 720, 40, fonts_.titleFont, instance);
    roleLabel_ = CreateLabel(parent, L"", 48, 96, 720, 28, fonts_.subtitleFont, instance);
    summaryLabel_ = CreateLabel(parent, L"", 48, 142, 980, 66, fonts_.bodyFont, instance);

    card1Button_ = CreateButton(parent, L"", ID_BTN_CARD1, 48, 258, 320, 136, fonts_.smallFont, instance);
    card2Button_ = CreateButton(parent, L"", ID_BTN_CARD2, 392, 258, 320, 136, fonts_.smallFont, instance);
    card3Button_ = CreateButton(parent, L"", ID_BTN_CARD3, 736, 258, 320, 136, fonts_.smallFont, instance);
    actionSectionLabel_ = CreateLabel(parent, L"", 48, 434, 360, 30, fonts_.subtitleFont, instance);
    actionDescriptionLabel_ = CreateLabel(parent, L"", 48, 472, 900, 42, fonts_.bodyFont, instance);
    field1Label_ = CreateLabel(parent, L"", 48, 532, 180, 22, fonts_.smallFont, instance);
    field2Label_ = CreateLabel(parent, L"", 262, 532, 180, 22, fonts_.smallFont, instance);
    field3Label_ = CreateLabel(parent, L"", 48, 608, 180, 22, fonts_.smallFont, instance);
    field4Label_ = CreateLabel(parent, L"", 262, 608, 180, 22, fonts_.smallFont, instance);
    field5Label_ = CreateLabel(parent, L"", 48, 684, 394, 22, fonts_.smallFont, instance);
    field1Edit_ = CreateEdit(parent, ID_EDIT_DASH_FIELD1, 48, 558, 180, 36, fonts_.smallFont, instance);
    field2Edit_ = CreateEdit(parent, ID_EDIT_DASH_FIELD2, 262, 558, 180, 36, fonts_.smallFont, instance);
    field3Edit_ = CreateEdit(parent, ID_EDIT_DASH_FIELD3, 48, 634, 180, 36, fonts_.smallFont, instance);
    field4Edit_ = CreateEdit(parent, ID_EDIT_DASH_FIELD4, 262, 634, 180, 36, fonts_.smallFont, instance);
    field5Edit_ = CreateEdit(parent, ID_EDIT_DASH_FIELD5, 48, 710, 394, 36, fonts_.smallFont, instance);
    listTitleLabel_ = CreateLabel(parent, L"", 492, 532, 520, 22, fonts_.smallFont, instance);
    listBox_ = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | LBS_NOTIFY | WS_VSCROLL,
        492, 560, 564, 188, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LIST_DASHBOARD)), instance, nullptr);
    SetControlFont(listBox_, fonts_.smallFont);
    actionButton_ = CreateButton(parent, L"", ID_BTN_DASHBOARD_ACTION, 48, 776, 200, 40, fonts_.bodyFont, instance);
    secondaryActionButton_ = CreateButton(parent, L"", ID_BTN_DASH_SECONDARY, 266, 776, 190, 40, fonts_.smallFont, instance);
    tertiaryActionButton_ = CreateButton(parent, L"", ID_BTN_DASH_TERTIARY, 474, 776, 190, 40, fonts_.smallFont, instance);
    actionStatusLabel_ = CreateLabel(parent, L"", 48, 832, 980, 28, fonts_.smallFont, instance);
    historyLabel_ = CreateLabel(parent, L"", 48, 532, 980, 240, fonts_.smallFont, instance);

    controls_ = {
        welcomeLabel_, roleLabel_, summaryLabel_,
        card1Button_, card2Button_, card3Button_, actionSectionLabel_, actionDescriptionLabel_,
        field1Label_, field2Label_, field3Label_, field4Label_, field5Label_,
        field1Edit_, field2Edit_, field3Edit_, field4Edit_, field5Edit_,
        listTitleLabel_, listBox_,
        actionButton_, secondaryActionButton_, tertiaryActionButton_,
        actionStatusLabel_, historyLabel_
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
    ReleaseMedicalRecordLockIfHeld();
    actionConfig_ = GetActionConfigForRole();
    SetWindowTextW(actionSectionLabel_, actionConfig_.sectionTitle.c_str());
    SetWindowTextW(actionDescriptionLabel_, actionConfig_.sectionDescription.c_str());
    SetWindowTextW(field1Label_, actionConfig_.field1Label.c_str());
    SetWindowTextW(field2Label_, actionConfig_.field2Label.c_str());
    SetWindowTextW(field3Label_, actionConfig_.field3Label.c_str());
    SetWindowTextW(field4Label_, actionConfig_.field4Label.c_str());
    SetWindowTextW(field5Label_, actionConfig_.field5Label.c_str());
    SetWindowTextW(actionButton_, actionConfig_.buttonLabel.c_str());
    SetWindowTextW(secondaryActionButton_, actionConfig_.secondaryButtonLabel.c_str());
    SetWindowTextW(tertiaryActionButton_, actionConfig_.tertiaryButtonLabel.c_str());
    SetWindowTextW(listTitleLabel_, actionConfig_.listTitle.c_str());

    const int formCommand = actionConfig_.showForm ? SW_SHOW : SW_HIDE;
    ShowWindow(field1Label_, actionConfig_.field1Label.empty() ? SW_HIDE : formCommand);
    ShowWindow(field2Label_, actionConfig_.field2Label.empty() ? SW_HIDE : formCommand);
    ShowWindow(field3Label_, actionConfig_.field3Label.empty() ? SW_HIDE : formCommand);
    ShowWindow(field4Label_, actionConfig_.field4Label.empty() ? SW_HIDE : formCommand);
    ShowWindow(field5Label_, actionConfig_.field5Label.empty() ? SW_HIDE : formCommand);
    ShowWindow(field1Edit_, actionConfig_.field1Label.empty() ? SW_HIDE : formCommand);
    ShowWindow(field2Edit_, actionConfig_.field2Label.empty() ? SW_HIDE : formCommand);
    ShowWindow(field3Edit_, actionConfig_.field3Label.empty() ? SW_HIDE : formCommand);
    ShowWindow(field4Edit_, actionConfig_.field4Label.empty() ? SW_HIDE : formCommand);
    ShowWindow(field5Edit_, actionConfig_.field5Label.empty() ? SW_HIDE : formCommand);
    ShowWindow(actionButton_, formCommand);
    ShowWindow(secondaryActionButton_, actionConfig_.showSecondaryButton ? SW_SHOW : SW_HIDE);
    ShowWindow(tertiaryActionButton_, actionConfig_.showTertiaryButton ? SW_SHOW : SW_HIDE);
    ShowWindow(listTitleLabel_, actionConfig_.showList ? SW_SHOW : SW_HIDE);
    ShowWindow(listBox_, actionConfig_.showList ? SW_SHOW : SW_HIDE);
    ShowWindow(historyLabel_, actionConfig_.showForm ? SW_HIDE : SW_SHOW);

    listMode_ = ListMode::None;
    selectedPatientUserId_ = 0;
    pendingRescheduleAppointmentId_ = 0;
    activeReceipt_ = PaymentReceipt{};
    activeReceiptNumber_.clear();
    medicalRecordReadOnly_ = false;
    medicalRecordLockHeld_ = false;
    activePrescriptionWarning_ = PrescriptionWarning{};
    pendingInventoryAlternativePayload_.clear();
    pendingSurgeryPlan_ = SurgeryPlan{};
    pendingHrOverride_ = false;
    pendingHrEmployee_.clear();
    pendingHrDate_.clear();
    pendingHrDepartment_.clear();
    pendingHrStart_.clear();
    pendingHrEnd_.clear();
    ClearList();
    if (!actionConfig_.showForm) {
        UpdatePatientHistory();
    }

    EnableWindow(actionButton_, TRUE);
    EnableWindow(field2Edit_, TRUE);
    EnableWindow(field3Edit_, TRUE);
    UpdateRecordEditState();
    UpdateWorkflowButtons();
    InvalidateRect(parent_, nullptr, TRUE);
}

void DashboardView::ClearActionInputs() {
    SetWindowTextW(field1Edit_, L"");
    SetWindowTextW(field2Edit_, L"");
    SetWindowTextW(field3Edit_, L"");
    SetWindowTextW(field4Edit_, L"");
    SetWindowTextW(field5Edit_, L"");
}

void DashboardView::ReleaseMedicalRecordLockIfHeld() {
    if (!medicalRecordLockHeld_ || selectedPatientUserId_ <= 0 || currentUser_.role != "Doctor") {
        return;
    }

    std::string ignoredError;
    repository_.ReleaseMedicalRecordLock(currentUser_, selectedPatientUserId_, ignoredError);
    medicalRecordLockHeld_ = false;
    medicalRecordReadOnly_ = false;
}

void DashboardView::UpdateRecordEditState() {
    const bool recordSection = currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Records;
    const BOOL editable = (!recordSection || !medicalRecordReadOnly_) ? TRUE : FALSE;
    EnableWindow(field2Edit_, editable);
    EnableWindow(field3Edit_, editable);
    EnableWindow(actionButton_, editable);
}

void DashboardView::SetActionStatus(const std::wstring& text) {
    SetWindowTextW(actionStatusLabel_, text.c_str());
    InvalidateRect(actionStatusLabel_, nullptr, TRUE);
    UpdateWindow(actionStatusLabel_);
}

bool DashboardView::IsSpecialWorkflowSection() const {
    return (currentUser_.role == "Patient" && patientSection_ == PatientSection::Appointments) ||
           (currentUser_.role == "Patient" && patientSection_ == PatientSection::Bills) ||
           (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Admissions) ||
           (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Prescriptions) ||
           (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Records) ||
           (currentUser_.role == "Pharmacist" && pharmacistSection_ == PharmacistSection::Patients) ||
           (currentUser_.role == "Secretary" && secretarySection_ == SecretarySection::Billing) ||
           (currentUser_.role == "Secretary" && secretarySection_ == SecretarySection::Surgery) ||
           (currentUser_.role == "Inventory Manager" && inventorySection_ == InventorySection::Orders) ||
           (currentUser_.role == "HR Manager" && hrSection_ == HrSection::Shifts);
}

void DashboardView::PopulateList(const std::wstring& title, const std::vector<WorkflowItem>& items) {
    listItems_ = items;
    SetWindowTextW(listTitleLabel_, title.c_str());
    SendMessageW(listBox_, LB_RESETCONTENT, 0, 0);
    for (const auto& item : items) {
        SendMessageW(listBox_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(ToWide(item.displayText).c_str()));
    }
    if (!items.empty()) {
        SendMessageW(listBox_, LB_SETCURSEL, 0, 0);
    }
    UpdateWorkflowButtons();
}

void DashboardView::ClearList() {
    listItems_.clear();
    SendMessageW(listBox_, LB_RESETCONTENT, 0, 0);
}

int DashboardView::GetSelectedListIndex() const {
    return static_cast<int>(SendMessageW(listBox_, LB_GETCURSEL, 0, 0));
}

std::string DashboardView::GetSelectedPayload() const {
    const int index = GetSelectedListIndex();
    if (index == LB_ERR || index < 0 || index >= static_cast<int>(listItems_.size())) {
        return "";
    }
    return listItems_[index].payload;
}

void DashboardView::UpdateWorkflowButtons() {
    if (!IsSpecialWorkflowSection()) {
        return;
    }

    if (currentUser_.role == "Patient" && patientSection_ == PatientSection::Appointments) {
        const std::string payload = GetSelectedPayload();
        const std::vector<std::string> parts = SplitPayload(payload);
        const bool reservedSelection = parts.size() >= 8 && parts[0] == "appointment" && parts[7] == "reserved";
        if (listMode_ == ListMode::PatientAppointments && reservedSelection) {
            SetWindowTextW(actionButton_, L"Confirm Selected");
            SetWindowTextW(secondaryActionButton_, L"Release Hold");
        } else if (listMode_ == ListMode::PatientAppointments) {
            SetWindowTextW(actionButton_, L"Cancel Selected");
            SetWindowTextW(secondaryActionButton_, L"Start Reschedule");
        } else if (pendingRescheduleAppointmentId_ > 0) {
            SetWindowTextW(actionButton_, L"Apply Reschedule");
            SetWindowTextW(secondaryActionButton_, L"Find Slots");
        } else {
            SetWindowTextW(actionButton_, L"Reserve Selected Slot");
            SetWindowTextW(secondaryActionButton_, L"Find Slots");
        }
        SetWindowTextW(tertiaryActionButton_, L"My Appointments");
        return;
    }

    if (currentUser_.role == "Patient" && patientSection_ == PatientSection::Bills) {
        if (listMode_ == ListMode::PaymentReceipt) {
            SetWindowTextW(actionButton_, L"Return Home");
            SetWindowTextW(secondaryActionButton_, L"My Bills");
            SetWindowTextW(tertiaryActionButton_, L"Pay Another");
        } else {
            SetWindowTextW(actionButton_, L"Submit Payment");
            SetWindowTextW(secondaryActionButton_, L"My Bills");
            SetWindowTextW(tertiaryActionButton_, L"Cancel Payment");
        }
        return;
    }

    if (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Admissions) {
        SetWindowTextW(actionButton_, L"Save Admission");
        SetWindowTextW(secondaryActionButton_, L"Find Patients");
        SetWindowTextW(tertiaryActionButton_, L"Transfer Active");
        return;
    }

    if (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Prescriptions) {
        SetWindowTextW(actionButton_, L"Save Prescription");
        SetWindowTextW(secondaryActionButton_, L"Find Patients");
        SetWindowTextW(tertiaryActionButton_, listMode_ == ListMode::PrescriptionAlternatives
            ? L"Use Alternative"
            : L"Renew Latest");
        return;
    }

    if (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Records) {
        SetWindowTextW(actionButton_, L"Save Record");
        SetWindowTextW(secondaryActionButton_, L"Find Patients");
        SetWindowTextW(tertiaryActionButton_, medicalRecordLockHeld_ ? L"Release Lock" : L"Open Record");
        return;
    }

    if (currentUser_.role == "Pharmacist" && pharmacistSection_ == PharmacistSection::Patients) {
        SetWindowTextW(actionButton_, L"Save Review");
        SetWindowTextW(secondaryActionButton_, L"Find Patients");
        SetWindowTextW(tertiaryActionButton_, L"Show Prescriptions");
        return;
    }

    if (currentUser_.role == "Secretary" && secretarySection_ == SecretarySection::Billing) {
        SetWindowTextW(actionButton_, L"Generate Auto Bill");
        SetWindowTextW(secondaryActionButton_, L"Find Patients");
        SetWindowTextW(tertiaryActionButton_, L"Preview Charges");
        return;
    }

    if (currentUser_.role == "Secretary" && secretarySection_ == SecretarySection::Surgery) {
        SetWindowTextW(actionButton_, pendingSurgeryPlan_.rejected ? L"Log Rejection" : L"Confirm Schedule");
        SetWindowTextW(secondaryActionButton_, L"Find Patients");
        SetWindowTextW(tertiaryActionButton_, L"Preview Schedule");
        return;
    }

    if (currentUser_.role == "Inventory Manager" && inventorySection_ == InventorySection::Orders) {
        SetWindowTextW(actionButton_, listMode_ == ListMode::InventoryOrderPreview ? L"Confirm Order" : L"Review Order");
        SetWindowTextW(secondaryActionButton_, L"Search Catalog");
        SetWindowTextW(tertiaryActionButton_, L"Show Alternatives");
        return;
    }

    if (currentUser_.role == "HR Manager" && hrSection_ == HrSection::Shifts) {
        SetWindowTextW(actionButton_, listMode_ == ListMode::HrSwapCandidates ? L"Confirm Swap" : L"Save Shift");
        SetWindowTextW(secondaryActionButton_, L"Find Swap Options");
        SetWindowTextW(tertiaryActionButton_, pendingHrOverride_ ? L"Apply Override" : L"Override Flow");
    }
}

bool DashboardView::HandlePrimaryWorkflowAction() {
    if (currentUser_.role == "Patient" && patientSection_ == PatientSection::Appointments) {
        return HandlePatientAppointmentPrimary();
    }
    if (currentUser_.role == "Patient" && patientSection_ == PatientSection::Bills) {
        return HandlePatientBillPrimary();
    }
    if (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Admissions) {
        return HandleDoctorAdmissionPrimary();
    }
    if (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Prescriptions) {
        return HandleDoctorPrescriptionPrimary();
    }
    if (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Records) {
        return HandleDoctorRecordPrimary();
    }
    if (currentUser_.role == "Pharmacist" && pharmacistSection_ == PharmacistSection::Patients) {
        return HandlePharmacistPatientPrimary();
    }
    if (currentUser_.role == "Secretary" && secretarySection_ == SecretarySection::Billing) {
        return HandleSecretaryBillingPrimary();
    }
    if (currentUser_.role == "Secretary" && secretarySection_ == SecretarySection::Surgery) {
        return HandleSecretarySurgeryPrimary();
    }
    if (currentUser_.role == "Inventory Manager" && inventorySection_ == InventorySection::Orders) {
        return HandleInventoryOrderPrimary();
    }
    if (currentUser_.role == "HR Manager" && hrSection_ == HrSection::Shifts) {
        return HandleHrShiftPrimary();
    }
    return false;
}

bool DashboardView::HandleSecondaryWorkflowAction() {
    if (currentUser_.role == "Patient" && patientSection_ == PatientSection::Appointments) {
        return HandlePatientAppointmentSecondary();
    }
    if (currentUser_.role == "Patient" && patientSection_ == PatientSection::Bills) {
        return HandlePatientBillSecondary();
    }
    if (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Admissions) {
        return HandleDoctorAdmissionSecondary();
    }
    if (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Prescriptions) {
        return HandleDoctorPrescriptionSecondary();
    }
    if (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Records) {
        return HandleDoctorRecordSecondary();
    }
    if (currentUser_.role == "Pharmacist" && pharmacistSection_ == PharmacistSection::Patients) {
        return HandlePharmacistPatientSecondary();
    }
    if (currentUser_.role == "Secretary" && secretarySection_ == SecretarySection::Billing) {
        return HandleSecretaryBillingSecondary();
    }
    if (currentUser_.role == "Secretary" && secretarySection_ == SecretarySection::Surgery) {
        return HandleSecretarySurgerySecondary();
    }
    if (currentUser_.role == "Inventory Manager" && inventorySection_ == InventorySection::Orders) {
        return HandleInventoryOrderSecondary();
    }
    if (currentUser_.role == "HR Manager" && hrSection_ == HrSection::Shifts) {
        return HandleHrShiftSecondary();
    }
    return false;
}

bool DashboardView::HandleTertiaryWorkflowAction() {
    if (currentUser_.role == "Patient" && patientSection_ == PatientSection::Appointments) {
        return HandlePatientAppointmentTertiary();
    }
    if (currentUser_.role == "Patient" && patientSection_ == PatientSection::Bills) {
        return HandlePatientBillTertiary();
    }
    if (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Admissions) {
        return HandleDoctorAdmissionTertiary();
    }
    if (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Prescriptions) {
        return HandleDoctorPrescriptionTertiary();
    }
    if (currentUser_.role == "Doctor" && doctorSection_ == DoctorSection::Records) {
        return HandleDoctorRecordTertiary();
    }
    if (currentUser_.role == "Pharmacist" && pharmacistSection_ == PharmacistSection::Patients) {
        return HandlePharmacistPatientTertiary();
    }
    if (currentUser_.role == "Secretary" && secretarySection_ == SecretarySection::Billing) {
        return HandleSecretaryBillingTertiary();
    }
    if (currentUser_.role == "Secretary" && secretarySection_ == SecretarySection::Surgery) {
        return HandleSecretarySurgeryTertiary();
    }
    if (currentUser_.role == "Inventory Manager" && inventorySection_ == InventorySection::Orders) {
        return HandleInventoryOrderTertiary();
    }
    if (currentUser_.role == "HR Manager" && hrSection_ == HrSection::Shifts) {
        return HandleHrShiftTertiary();
    }
    return false;
}

DashboardView::ActionConfig DashboardView::GetActionConfigForRole() const {
    if (currentUser_.role == "Patient") {
        if (patientSection_ == PatientSection::Appointments) {
            return {
                L"Manage appointments",
                L"Browse available slots, place a temporary hold, explicitly confirm it, release it, or switch into cancellation/reschedule mode.",
                L"Date",
                L"Specialty",
                L"Doctor filter",
                L"",
                L"",
                L"Reserve Selected Slot",
                L"Find Slots",
                L"My Appointments",
                L"Available slots",
                L"Appointment saved.",
                "patient_appointments",
                true,
                true,
                true,
                true
            };
        }
        if (patientSection_ == PatientSection::Bills) {
            return {
                L"Pay a bill",
                L"Load unpaid bills, submit a payment, and move into a receipt view with a quick route back to the patient home screen.",
                L"Method",
                L"Card / reference",
                L"Amount",
                L"CVV / auth",
                L"",
                L"Submit Payment",
                L"My Bills",
                L"Cancel Payment",
                L"Outstanding bills",
                L"Bill payment saved.",
                "patient_payments",
                true,
                true,
                true,
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
                L"Search for an existing patient or register a new one by providing AMKA and birth date before saving the admission.",
                L"Patient search / new name",
                L"Clinic",
                L"Reason",
                L"AMKA (new patient)",
                L"Birth date (YYYY-MM-DD)",
                L"Save Admission",
                L"Find Patients",
                L"Transfer Active",
                L"Patient results",
                L"Admission saved.",
                "doctor_admissions",
                true,
                true,
                true,
                true
            };
        }
        if (doctorSection_ == DoctorSection::Prescriptions) {
            return {
                L"Issue or renew a prescription",
                L"Search patients, renew the latest prescription, and react to prescription warnings in-place by switching to a safer alternative.",
                L"Patient search",
                L"Medicine",
                L"Dosage",
                L"Frequency",
                L"Instructions",
                L"Save Prescription",
                L"Find Patients",
                L"Renew Latest",
                L"Patient results",
                L"Prescription saved.",
                "doctor_prescriptions",
                true,
                true,
                true,
                true
            };
        }
        return {
            L"Open a medical record",
            L"Search a patient, lock the record for editing when possible, review the full history, and fall back to read-only mode if someone else already has it open.",
            L"Patient search",
            L"Record type",
            L"Note",
            L"",
            L"",
            L"Save Record",
            L"Find Patients",
            L"Open Record",
            L"Record history",
            L"Record note saved.",
            "doctor_records",
            true,
            true,
            true,
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
                L"",
                L"",
                L"Save Dispensing",
                L"",
                L"",
                L"",
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
                L"",
                L"",
                L"Save Stock Update",
                L"",
                L"",
                L"",
                L"Stock update saved.",
                "pharmacist_stock",
                true
            };
        }
        return {
            L"Review patient treatment",
            L"Search a patient, display prescriptions, and save a pharmacist review on the selected prescription.",
            L"Patient search",
            L"Review status",
            L"",
            L"",
            L"",
            L"Save Review",
            L"Find Patients",
            L"Show Prescriptions",
            L"Prescription queue",
            L"Patient review saved.",
            "pharmacist_reviews",
            true,
            true,
            true,
            true
        };
    }
    if (currentUser_.role == "Secretary") {
        if (secretarySection_ == SecretarySection::Billing) {
            return {
                L"Generate an automatic bill",
                L"Search for a patient, preview calculated service charges, apply insurance coverage, and generate the bill.",
                L"Patient search",
                L"",
                L"",
                L"",
                L"",
                L"Generate Auto Bill",
                L"Find Patients",
                L"Preview Charges",
                L"Billing preview",
                L"Billing record saved.",
                "secretary_billing",
                true,
                true,
                true,
                true
            };
        }
        if (secretarySection_ == SecretarySection::Surgery) {
            return {
                L"Schedule a surgery",
                L"Search the patient, preview the matched surgeon, auto-assigned nurse, room availability, and confirm or log the rejection.",
                L"Patient search",
                L"Date",
                L"Time",
                L"Doctor category",
                L"Operating room",
                L"Confirm Schedule",
                L"Find Patients",
                L"Preview Schedule",
                L"Surgery plan",
                L"Surgery schedule saved.",
                "secretary_surgeries",
                true,
                true,
                true,
                true
            };
        }
        return {
            L"Register a patient support task",
            L"Track a patient-facing front-desk task or coordination note.",
            L"Patient",
            L"Task type",
            L"Note",
            L"",
            L"",
            L"Save Patient Task",
            L"",
            L"",
            L"",
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
                L"",
                L"",
                L"Save Inventory",
                L"",
                L"",
                L"",
                L"Inventory update saved.",
                "inventory_status",
                true
            };
        }
        if (inventorySection_ == InventorySection::Orders) {
            return {
                L"Place a restock order",
                L"Search the catalog, review item details, compare alternatives, and confirm the final order before saving it.",
                L"Item / search",
                L"Quantity",
                L"Supplier",
                L"",
                L"",
                L"Review Order",
                L"Search Catalog",
                L"Show Alternatives",
                L"Inventory catalog",
                L"Restock order saved.",
                "inventory_orders",
                true,
                true,
                true,
                true
            };
        }
        return {
            L"Log a stock alert",
            L"Record a low-stock or unavailable item alert.",
            L"Item",
            L"Alert level",
            L"Note",
            L"",
            L"",
            L"Save Alert",
            L"",
            L"",
            L"",
            L"Inventory alert saved.",
            "inventory_alerts",
            true
        };
    }
    if (currentUser_.role == "HR Manager") {
        if (hrSection_ == HrSection::Shifts) {
            return {
                L"Schedule a shift",
                L"Create a timed staffing assignment, search swap candidates, and apply an override with justification when policy conflicts are acknowledged.",
                L"Employee",
                L"Shift date",
                L"Department",
                L"Start time",
                L"End time",
                L"Save Shift",
                L"Find Swap Options",
                L"Override Flow",
                L"Swap candidates",
                L"Shift saved.",
                "hr_shifts",
                true,
                true,
                true,
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
                L"",
                L"",
                L"Save Staff Update",
                L"",
                L"",
                L"",
                L"Staff update saved.",
                "hr_staff_updates",
                true
            };
        }
        return {
            L"Log a schedule conflict",
            L"Record a staffing or leave conflict for review. Use a note like 'Approved leave' to block scheduling on that date.",
            L"Employee",
            L"Conflict date",
            L"Issue",
            L"",
            L"",
            L"Save Conflict",
            L"",
            L"",
            L"",
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
        L"",
        L"",
        L"Save",
        L"",
        L"",
        L"",
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

bool DashboardView::HandlePatientAppointmentPrimary() {
    const std::string payload = GetSelectedPayload();
    if (payload.empty()) {
        SetActionStatus(L"Select a slot or appointment first.");
        return true;
    }

    std::string errorMessage;
    if (payload.rfind("appointment|", 0) == 0) {
        const std::vector<std::string> parts = SplitPayload(payload);
        if (parts.size() < 8) {
            SetActionStatus(L"Select a valid appointment.");
            return true;
        }

        int appointmentId = 0;
        if (!TryParseIntegerValue(parts[1], appointmentId)) {
            SetActionStatus(L"Select a valid appointment.");
            return true;
        }

        const bool reservedAppointment = parts[7] == "reserved";
        const bool actionSucceeded = reservedAppointment
            ? repository_.ConfirmReservedAppointment(currentUser_, appointmentId, errorMessage)
            : repository_.CancelAppointment(currentUser_, appointmentId, errorMessage);
        if (!actionSucceeded) {
            SetActionStatus(ToWide(errorMessage));
            return true;
        }

        pendingRescheduleAppointmentId_ = 0;
        PopulateList(L"My appointments", repository_.ListPatientAppointments(currentUser_));
        listMode_ = ListMode::PatientAppointments;
        SetActionStatus(reservedAppointment ? L"Reservation confirmed." : L"Appointment cancelled.");
        return true;
    }

    const bool actionSucceeded = pendingRescheduleAppointmentId_ > 0
        ? repository_.RescheduleAppointment(currentUser_, pendingRescheduleAppointmentId_, payload, errorMessage)
        : repository_.ReserveAppointmentSlot(currentUser_, payload, errorMessage);
    if (!actionSucceeded) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    if (pendingRescheduleAppointmentId_ > 0) {
        pendingRescheduleAppointmentId_ = 0;
        SetActionStatus(L"Appointment rescheduled.");
    } else {
        SetActionStatus(L"Slot reserved. Confirm it from My Appointments before the hold expires.");
    }
    PopulateList(L"My appointments", repository_.ListPatientAppointments(currentUser_));
    listMode_ = ListMode::PatientAppointments;
    UpdatePatientHistory();
    return true;
}

bool DashboardView::HandlePatientAppointmentSecondary() {
    const std::string selectedPayload = GetSelectedPayload();
    if (listMode_ == ListMode::PatientAppointments && selectedPayload.rfind("appointment|", 0) == 0) {
        const std::vector<std::string> parts = SplitPayload(selectedPayload);
        if (parts.size() >= 8) {
            int appointmentId = 0;
            TryParseIntegerValue(parts[1], appointmentId);
            if (parts[7] == "reserved") {
                std::string errorMessage;
                if (!repository_.ReleaseReservedAppointment(currentUser_, appointmentId, errorMessage)) {
                    SetActionStatus(ToWide(errorMessage));
                    return true;
                }

                PopulateList(L"My appointments", repository_.ListPatientAppointments(currentUser_));
                listMode_ = ListMode::PatientAppointments;
                SetActionStatus(L"Reservation released.");
                return true;
            }

            pendingRescheduleAppointmentId_ = appointmentId;
            if (GetWindowTextString(field1Edit_).empty()) {
                SetWindowTextW(field1Edit_, ToWide(parts[4]).c_str());
            }
            if (GetWindowTextString(field2Edit_).empty()) {
                SetWindowTextW(field2Edit_, ToWide(parts[6]).c_str());
            }
            if (GetWindowTextString(field3Edit_).empty()) {
                SetWindowTextW(field3Edit_, ToWide(parts[3]).c_str());
            }
        }
    }

    const std::wstring date = GetWindowTextString(field1Edit_);
    const std::wstring specialty = GetWindowTextString(field2Edit_);
    const std::wstring doctorFilter = GetWindowTextString(field3Edit_);
    if (date.empty()) {
        SetActionStatus(L"Enter a date first to search for slots.");
        return true;
    }

    const auto items = repository_.FindAppointmentSlots(ToUtf8(date), ToUtf8(specialty), ToUtf8(doctorFilter));
    PopulateList(L"Available slots", items);
    listMode_ = ListMode::AppointmentSlots;
    if (pendingRescheduleAppointmentId_ > 0) {
        SetActionStatus(L"Select a replacement slot and click Apply Reschedule.");
    } else {
        if (items.empty()) {
            const std::string nextDate = repository_.FindNextAvailableAppointmentDate(
                ToUtf8(date),
                ToUtf8(specialty),
                ToUtf8(doctorFilter));
            SetActionStatus(nextDate.empty()
                ? L"No slots found for that search."
                : (L"No slots on that date. Next available date: " + ToWide(nextDate) + L"."));
        } else {
            SetActionStatus(L"Select a slot and click Book Selected Slot.");
        }
    }
    return true;
}

bool DashboardView::HandlePatientAppointmentTertiary() {
    pendingRescheduleAppointmentId_ = 0;
    PopulateList(L"My appointments", repository_.ListPatientAppointments(currentUser_));
    listMode_ = ListMode::PatientAppointments;
    SetActionStatus(L"Select a reservation to confirm or release, or select a confirmed appointment to cancel or reschedule.");
    return true;
}

bool DashboardView::HandlePatientBillSecondary() {
    const auto items = repository_.ListOutstandingBills(currentUser_);
    PopulateList(L"Outstanding bills", items);
    listMode_ = ListMode::PatientBills;
    SetActionStatus(items.empty() ? L"No unpaid bills are available right now." : L"Select a bill, enter payment details, and click Submit Payment.");
    return true;
}

bool DashboardView::HandlePatientBillPrimary() {
    if (listMode_ == ListMode::PaymentReceipt) {
        SetPatientSection(PatientSection::Appointments);
        SetActionStatus(L"Returned to the patient home screen.");
        return true;
    }

    const std::string payload = GetSelectedPayload();
    if (payload.rfind("bill|", 0) != 0) {
        SetActionStatus(L"Select an unpaid bill first.");
        return true;
    }

    const std::vector<std::string> parts = [&]() {
        std::vector<std::string> values;
        std::stringstream ss(payload);
        std::string part;
        while (std::getline(ss, part, '|')) {
            values.push_back(part);
        }
        return values;
    }();
    if (parts.size() < 2) {
        SetActionStatus(L"Select a valid bill first.");
        return true;
    }

    int billId = 0;
    if (!TryParseIntegerValue(parts[1], billId)) {
        SetActionStatus(L"Select a valid bill first.");
        return true;
    }

    std::string summaryMessage;
    std::string errorMessage;
    if (!repository_.ProcessPaymentDetailed(
            currentUser_,
            billId,
            ToUtf8(GetWindowTextString(field1Edit_)),
            ToUtf8(GetWindowTextString(field2Edit_)),
            ToUtf8(GetWindowTextString(field3Edit_)),
            ToUtf8(GetWindowTextString(field4Edit_)),
            summaryMessage,
            errorMessage)) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    ClearActionInputs();
    activeReceiptNumber_.clear();
    const std::string marker = "Receipt ";
    const std::size_t receiptStart = summaryMessage.find(marker);
    if (receiptStart != std::string::npos) {
        const std::size_t valueStart = receiptStart + marker.size();
        const std::size_t valueEnd = summaryMessage.find('.', valueStart);
        activeReceiptNumber_ = summaryMessage.substr(valueStart, valueEnd == std::string::npos
            ? std::string::npos
            : valueEnd - valueStart);
    }

    if (!activeReceiptNumber_.empty() &&
        repository_.LoadPaymentReceipt(currentUser_, activeReceiptNumber_, activeReceipt_, errorMessage) &&
        activeReceipt_.hasValue) {
        std::vector<WorkflowItem> receiptLines;
        receiptLines.push_back({"receipt-number", "Receipt  |  " + activeReceipt_.receiptNumber});
        receiptLines.push_back({"receipt-service", "Service  |  " + activeReceipt_.serviceName});
        receiptLines.push_back({"receipt-amount", "Amount  |  " + std::to_string(activeReceipt_.amount)});
        receiptLines.push_back({"receipt-method", "Method  |  " + activeReceipt_.paymentMethod});
        receiptLines.push_back({"receipt-created", "Paid at  |  " + activeReceipt_.createdAt});
        PopulateList(L"Payment receipt", receiptLines);
        listMode_ = ListMode::PaymentReceipt;
        SetActionStatus(L"Payment complete. Review the receipt or return home.");
    } else {
        PopulateList(L"Outstanding bills", repository_.ListOutstandingBills(currentUser_));
        listMode_ = ListMode::PatientBills;
        SetActionStatus(ToWide(summaryMessage));
    }
    UpdatePatientHistory();
    return true;
}

bool DashboardView::HandlePatientBillTertiary() {
    if (listMode_ == ListMode::PaymentReceipt) {
        ClearActionInputs();
        activeReceipt_ = PaymentReceipt{};
        activeReceiptNumber_.clear();
        PopulateList(L"Outstanding bills", repository_.ListOutstandingBills(currentUser_));
        listMode_ = ListMode::PatientBills;
        SetActionStatus(L"You can submit another payment now.");
        return true;
    }

    ClearActionInputs();
    SetActionStatus(L"Payment cancelled before submission.");
    return true;
}

bool DashboardView::HandleDoctorAdmissionSecondary() {
    const auto items = repository_.SearchPatients(ToUtf8(GetWindowTextString(field1Edit_)));
    PopulateList(L"Patient results", items);
    listMode_ = ListMode::PatientSearch;
    SetActionStatus(items.empty() ? L"No registered patients matched that search. Fill in AMKA and birth date to register a new patient." : L"Select a patient from the list or fill in new-patient details.");
    return true;
}

bool DashboardView::HandleDoctorAdmissionPrimary() {
    const std::string payload = GetSelectedPayload();
    if (payload.rfind("patient|", 0) == 0) {
        const std::vector<std::string> parts = [&]() {
            std::vector<std::string> values;
            std::stringstream ss(payload);
            std::string part;
            while (std::getline(ss, part, '|')) {
                values.push_back(part);
            }
            return values;
        }();
        if (parts.size() >= 2) {
            TryParseIntegerValue(parts[1], selectedPatientUserId_);
        }
    }

    std::string summaryMessage;
    std::string errorMessage;
    if (!repository_.SaveAdmissionDetailed(
            currentUser_,
            selectedPatientUserId_,
            ToUtf8(GetWindowTextString(field1Edit_)),
            ToUtf8(GetWindowTextString(field2Edit_)),
            ToUtf8(GetWindowTextString(field3Edit_)),
            ToUtf8(GetWindowTextString(field4Edit_)),
            ToUtf8(GetWindowTextString(field5Edit_)),
            summaryMessage,
            errorMessage)) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    selectedPatientUserId_ = 0;
    ClearActionInputs();
    ClearList();
    SetActionStatus(ToWide(summaryMessage));
    return true;
}

bool DashboardView::HandleDoctorAdmissionTertiary() {
    const std::string payload = GetSelectedPayload();
    if (payload.rfind("patient|", 0) == 0) {
        const std::vector<std::string> parts = [&]() {
            std::vector<std::string> values;
            std::stringstream ss(payload);
            std::string part;
            while (std::getline(ss, part, '|')) {
                values.push_back(part);
            }
            return values;
        }();
        if (parts.size() >= 2) {
            TryParseIntegerValue(parts[1], selectedPatientUserId_);
        }
    }

    std::string summaryMessage;
    std::string errorMessage;
    if (!repository_.TransferActiveAdmission(
            currentUser_,
            selectedPatientUserId_,
            ToUtf8(GetWindowTextString(field1Edit_)),
            ToUtf8(GetWindowTextString(field2Edit_)),
            summaryMessage,
            errorMessage)) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    selectedPatientUserId_ = 0;
    ClearActionInputs();
    ClearList();
    SetActionStatus(ToWide(summaryMessage));
    return true;
}

bool DashboardView::HandleDoctorPrescriptionSecondary() {
    const auto items = repository_.SearchPatients(ToUtf8(GetWindowTextString(field1Edit_)));
    PopulateList(L"Patient results", items);
    listMode_ = ListMode::PatientSearch;
    SetActionStatus(items.empty() ? L"No patients matched that search." : L"Select a patient from the list.");
    return true;
}

bool DashboardView::HandleDoctorPrescriptionTertiary() {
    if (listMode_ == ListMode::PrescriptionAlternatives) {
        const std::string payload = GetSelectedPayload();
        if (payload.rfind("medalt|", 0) != 0) {
            SetActionStatus(L"Select an alternative medication first.");
            return true;
        }

        const std::vector<std::string> parts = SplitPayload(payload);
        if (parts.size() < 2) {
            SetActionStatus(L"Select an alternative medication first.");
            return true;
        }

        SetWindowTextW(field2Edit_, ToWide(parts[1]).c_str());
        activePrescriptionWarning_ = PrescriptionWarning{};
        ClearList();
        listMode_ = ListMode::None;
        SetActionStatus(L"Alternative medication loaded. Review it and save when ready.");
        return true;
    }

    const std::string payload = GetSelectedPayload();
    if (payload.rfind("patient|", 0) != 0) {
        SetActionStatus(L"Select a patient first.");
        return true;
    }

    const std::vector<std::string> parts = SplitPayload(payload);
    if (parts.size() < 4) {
        SetActionStatus(L"Select a valid patient first.");
        return true;
    }

    int patientId = 0;
    if (!TryParseIntegerValue(parts[1], patientId)) {
        SetActionStatus(L"Select a valid patient first.");
        return true;
    }

    PrescriptionDraft draft;
    std::string errorMessage;
    if (!repository_.LoadLatestPrescriptionDraft(patientId, draft, errorMessage)) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }
    if (!draft.hasValue) {
        SetActionStatus(L"No earlier prescription was found for this patient.");
        return true;
    }

    SetWindowTextW(field2Edit_, ToWide(draft.medicine).c_str());
    SetWindowTextW(field3Edit_, ToWide(draft.dosage).c_str());
    SetWindowTextW(field4Edit_, ToWide(draft.frequency).c_str());
    SetWindowTextW(field5Edit_, ToWide(draft.instructions).c_str());
    selectedPatientUserId_ = patientId;
    SetActionStatus(L"Latest prescription loaded. Review it and save to renew.");
    return true;
}

bool DashboardView::HandleDoctorPrescriptionPrimary() {
    const std::string payload = GetSelectedPayload();
    if (payload.rfind("patient|", 0) == 0) {
        const std::vector<std::string> parts = SplitPayload(payload);
        if (parts.size() >= 2) {
            TryParseIntegerValue(parts[1], selectedPatientUserId_);
        }
    }

    if (selectedPatientUserId_ <= 0) {
        SetActionStatus(L"Search and select a patient first.");
        return true;
    }

    const std::wstring medicine = GetWindowTextString(field2Edit_);
    const std::wstring dosage = GetWindowTextString(field3Edit_);
    const std::wstring frequency = GetWindowTextString(field4Edit_);
    const std::wstring instructions = GetWindowTextString(field5Edit_);
    if (medicine.empty() || dosage.empty() || frequency.empty() || instructions.empty()) {
        SetActionStatus(L"Fill in medicine, dosage, frequency, and instructions.");
        return true;
    }

    activePrescriptionWarning_ = PrescriptionWarning{};
    std::string errorMessage;
    if (!repository_.CheckPrescriptionWarning(
            selectedPatientUserId_,
            ToUtf8(medicine),
            activePrescriptionWarning_,
            errorMessage)) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }
    if (activePrescriptionWarning_.hasWarning) {
        PopulateList(L"Prescription warning", activePrescriptionWarning_.alternatives);
        listMode_ = ListMode::PrescriptionAlternatives;
        SetActionStatus(ToWide(activePrescriptionWarning_.message + " Select an alternative medication or revise the current one."));
        return true;
    }

    if (!repository_.CreatePrescriptionDetailed(
            currentUser_,
            selectedPatientUserId_,
            ToUtf8(medicine),
            ToUtf8(dosage),
            ToUtf8(frequency),
            ToUtf8(instructions),
            errorMessage)) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    activePrescriptionWarning_ = PrescriptionWarning{};
    ClearList();
    listMode_ = ListMode::None;
    SetActionStatus(L"Prescription saved.");
    return true;
}

bool DashboardView::HandleDoctorRecordSecondary() {
    const auto items = repository_.SearchPatients(ToUtf8(GetWindowTextString(field1Edit_)));
    PopulateList(L"Patient results", items);
    listMode_ = ListMode::PatientSearch;
    SetActionStatus(items.empty() ? L"No patients matched that search." : L"Select a patient, then click Open Record.");
    return true;
}

bool DashboardView::HandleDoctorRecordTertiary() {
    if (medicalRecordLockHeld_) {
        std::string errorMessage;
        if (!repository_.ReleaseMedicalRecordLock(currentUser_, selectedPatientUserId_, errorMessage)) {
            SetActionStatus(ToWide(errorMessage));
            return true;
        }

        medicalRecordLockHeld_ = false;
        medicalRecordReadOnly_ = false;
        UpdateRecordEditState();
        SetActionStatus(L"Medical record lock released.");
        return true;
    }

    const std::string payload = GetSelectedPayload();
    if (payload.rfind("patient|", 0) != 0) {
        SetActionStatus(L"Select a patient first.");
        return true;
    }

    const std::vector<std::string> parts = SplitPayload(payload);
    if (parts.size() < 2 || !TryParseIntegerValue(parts[1], selectedPatientUserId_)) {
        SetActionStatus(L"Select a valid patient first.");
        return true;
    }

    MedicalRecordSession session;
    std::string errorMessage;
    if (!repository_.LoadMedicalRecordSession(currentUser_, selectedPatientUserId_, session, errorMessage)) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    medicalRecordReadOnly_ = session.readOnly;
    medicalRecordLockHeld_ = session.lockHeld;
    PopulateList(L"Record history", session.history);
    listMode_ = ListMode::MedicalRecordHistory;
    UpdateRecordEditState();
    SetActionStatus(ToWide(session.statusMessage));
    return true;
}

bool DashboardView::HandleDoctorRecordPrimary() {
    if (selectedPatientUserId_ <= 0) {
        const std::string payload = GetSelectedPayload();
        if (payload.rfind("patient|", 0) == 0) {
            const std::vector<std::string> parts = SplitPayload(payload);
            if (parts.size() >= 2) {
                TryParseIntegerValue(parts[1], selectedPatientUserId_);
            }
        }
    }

    if (selectedPatientUserId_ <= 0) {
        SetActionStatus(L"Search and open a patient record first.");
        return true;
    }
    if (medicalRecordReadOnly_) {
        SetActionStatus(L"This record is open in read-only mode. Release the other lock or open another patient.");
        return true;
    }

    const std::wstring recordType = GetWindowTextString(field2Edit_);
    const std::wstring note = GetWindowTextString(field3Edit_);
    if (recordType.empty() || note.empty()) {
        SetActionStatus(L"Fill in record type and note first.");
        return true;
    }

    std::string errorMessage;
    if (!repository_.SaveMedicalRecordDetailed(
            currentUser_,
            selectedPatientUserId_,
            "",
            ToUtf8(recordType),
            ToUtf8(note),
            errorMessage)) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    MedicalRecordSession session;
    if (repository_.LoadMedicalRecordSession(currentUser_, selectedPatientUserId_, session, errorMessage)) {
        PopulateList(L"Record history", session.history);
        listMode_ = ListMode::MedicalRecordHistory;
        medicalRecordReadOnly_ = session.readOnly;
        medicalRecordLockHeld_ = session.lockHeld;
        UpdateRecordEditState();
    }
    SetWindowTextW(field3Edit_, L"");
    SetActionStatus(L"Medical record note saved.");
    return true;
}

bool DashboardView::HandlePharmacistPatientSecondary() {
    const auto items = repository_.SearchPatients(ToUtf8(GetWindowTextString(field1Edit_)));
    PopulateList(L"Patient results", items);
    listMode_ = ListMode::PatientSearch;
    SetActionStatus(items.empty() ? L"No patients matched that search." : L"Select a patient, then click Show Prescriptions.");
    return true;
}

bool DashboardView::HandlePharmacistPatientTertiary() {
    const std::string payload = GetSelectedPayload();
    if (payload.rfind("patient|", 0) != 0) {
        SetActionStatus(L"Select a patient first.");
        return true;
    }

    const std::vector<std::string> parts = SplitPayload(payload);
    if (parts.size() < 2 || !TryParseIntegerValue(parts[1], selectedPatientUserId_)) {
        SetActionStatus(L"Select a valid patient first.");
        return true;
    }

    const auto items = repository_.ListPatientPrescriptions(selectedPatientUserId_);
    PopulateList(L"Prescription queue", items);
    listMode_ = ListMode::PharmacistPrescriptions;
    SetActionStatus(items.empty() ? L"No prescriptions were found for that patient." : L"Select a prescription and save the pharmacist review.");
    return true;
}

bool DashboardView::HandlePharmacistPatientPrimary() {
    if (selectedPatientUserId_ <= 0) {
        const std::string payload = GetSelectedPayload();
        if (payload.rfind("patient|", 0) == 0) {
            const std::vector<std::string> parts = SplitPayload(payload);
            if (parts.size() >= 2) {
                TryParseIntegerValue(parts[1], selectedPatientUserId_);
            }
        }
    }

    if (selectedPatientUserId_ <= 0) {
        SetActionStatus(L"Search and select a patient first.");
        return true;
    }

    const std::string payload = GetSelectedPayload();
    if (payload.rfind("prescription|", 0) != 0) {
        SetActionStatus(L"Select a prescription first.");
        return true;
    }

    const std::vector<std::string> parts = SplitPayload(payload);
    int prescriptionId = 0;
    if (parts.size() < 2 || !TryParseIntegerValue(parts[1], prescriptionId)) {
        SetActionStatus(L"Select a valid prescription first.");
        return true;
    }

    const std::wstring reviewStatus = GetWindowTextString(field2Edit_);
    if (reviewStatus.empty()) {
        SetActionStatus(L"Enter a review status first.");
        return true;
    }

    std::string summaryMessage;
    std::string errorMessage;
    if (!repository_.SavePharmacyReviewDetailed(
            currentUser_,
            selectedPatientUserId_,
            prescriptionId,
            ToUtf8(reviewStatus),
            summaryMessage,
            errorMessage)) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    PopulateList(L"Prescription queue", repository_.ListPatientPrescriptions(selectedPatientUserId_));
    listMode_ = ListMode::PharmacistPrescriptions;
    SetActionStatus(ToWide(summaryMessage));
    return true;
}

bool DashboardView::HandleSecretaryBillingSecondary() {
    const auto items = repository_.SearchPatients(ToUtf8(GetWindowTextString(field1Edit_)));
    PopulateList(L"Patient results", items);
    listMode_ = ListMode::PatientSearch;
    SetActionStatus(items.empty() ? L"No patients matched that search." : L"Select a patient from the list.");
    return true;
}

bool DashboardView::HandleSecretaryBillingTertiary() {
    const std::string payload = GetSelectedPayload();
    if (payload.rfind("patient|", 0) != 0) {
        SetActionStatus(L"Select a patient first.");
        return true;
    }

    const std::vector<std::string> parts = [&]() {
        std::vector<std::string> values;
        std::stringstream ss(payload);
        std::string part;
        while (std::getline(ss, part, '|')) {
            values.push_back(part);
        }
        return values;
    }();
    if (parts.size() < 2 || !TryParseIntegerValue(parts[1], selectedPatientUserId_)) {
        SetActionStatus(L"Select a valid patient first.");
        return true;
    }

    std::string errorMessage;
    const BillingPreview preview = repository_.BuildBillingPreview(selectedPatientUserId_, errorMessage);
    if (!errorMessage.empty()) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    PopulateList(L"Billing preview", preview.lines);
    listMode_ = ListMode::BillingPreview;
    SetActionStatus(L"Preview loaded. Review the charges and click Generate Auto Bill.");
    return true;
}

bool DashboardView::HandleSecretaryBillingPrimary() {
    if (selectedPatientUserId_ <= 0) {
        const std::string payload = GetSelectedPayload();
        if (payload.rfind("patient|", 0) == 0) {
            const std::vector<std::string> parts = [&]() {
                std::vector<std::string> values;
                std::stringstream ss(payload);
                std::string part;
                while (std::getline(ss, part, '|')) {
                    values.push_back(part);
                }
                return values;
            }();
            if (parts.size() >= 2) {
                TryParseIntegerValue(parts[1], selectedPatientUserId_);
            }
        }
    }

    if (selectedPatientUserId_ <= 0) {
        SetActionStatus(L"Search and select a patient first.");
        return true;
    }

    std::string errorMessage;
    std::string summaryMessage;
    if (!repository_.GenerateAutomaticBill(currentUser_, selectedPatientUserId_, summaryMessage, errorMessage)) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    SetActionStatus(ToWide(summaryMessage));
    return true;
}

bool DashboardView::HandleSecretarySurgerySecondary() {
    const auto items = repository_.SearchPatients(ToUtf8(GetWindowTextString(field1Edit_)));
    PopulateList(L"Patient results", items);
    listMode_ = ListMode::PatientSearch;
    SetActionStatus(items.empty() ? L"No patients matched that search." : L"Select a patient, then preview the surgery schedule.");
    return true;
}

bool DashboardView::HandleSecretarySurgeryTertiary() {
    const std::string payload = GetSelectedPayload();
    if (payload.rfind("patient|", 0) == 0) {
        const std::vector<std::string> parts = SplitPayload(payload);
        if (parts.size() >= 2) {
            TryParseIntegerValue(parts[1], selectedPatientUserId_);
        }
    }

    if (selectedPatientUserId_ <= 0) {
        SetActionStatus(L"Search and select a patient first.");
        return true;
    }

    std::string errorMessage;
    pendingSurgeryPlan_ = repository_.BuildSurgeryPlan(
        selectedPatientUserId_,
        ToUtf8(GetWindowTextString(field2Edit_)),
        ToUtf8(GetWindowTextString(field3Edit_)),
        ToUtf8(GetWindowTextString(field4Edit_)),
        ToUtf8(GetWindowTextString(field5Edit_)),
        errorMessage);
    if (!errorMessage.empty()) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    PopulateList(L"Surgery plan", pendingSurgeryPlan_.lines);
    listMode_ = ListMode::SurgeryPlan;
    UpdateWorkflowButtons();
    SetActionStatus(ToWide(pendingSurgeryPlan_.approved
        ? "Schedule preview is ready. Confirm it to book the room and staff."
        : "The plan was rejected. Review the reason and click Log Rejection if you want to save the rejection."));
    return true;
}

bool DashboardView::HandleSecretarySurgeryPrimary() {
    if (selectedPatientUserId_ <= 0) {
        const std::string payload = GetSelectedPayload();
        if (payload.rfind("patient|", 0) == 0) {
            const std::vector<std::string> parts = SplitPayload(payload);
            if (parts.size() >= 2) {
                TryParseIntegerValue(parts[1], selectedPatientUserId_);
            }
        }
    }

    if (selectedPatientUserId_ <= 0) {
        SetActionStatus(L"Search and select a patient first.");
        return true;
    }
    if (pendingSurgeryPlan_.patientUserId != selectedPatientUserId_) {
        SetActionStatus(L"Preview the surgery plan first.");
        return true;
    }

    std::string summaryMessage;
    std::string errorMessage;
    if (!repository_.ConfirmSurgeryPlan(currentUser_, pendingSurgeryPlan_, summaryMessage, errorMessage)) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    SetActionStatus(ToWide(summaryMessage));
    pendingSurgeryPlan_ = SurgeryPlan{};
    ClearList();
    listMode_ = ListMode::None;
    return true;
}

bool DashboardView::HandleInventoryOrderSecondary() {
    const auto items = repository_.SearchInventoryItems(ToUtf8(GetWindowTextString(field1Edit_)));
    PopulateList(L"Inventory catalog", items);
    listMode_ = ListMode::InventoryCatalog;
    SetActionStatus(items.empty() ? L"No inventory items matched that search." : L"Select an item to review or explore alternatives.");
    return true;
}

bool DashboardView::HandleInventoryOrderTertiary() {
    std::string itemName = ToUtf8(GetWindowTextString(field1Edit_));
    const std::string payload = GetSelectedPayload();
    if (payload.rfind("inventory|", 0) == 0) {
        const std::vector<std::string> parts = SplitPayload(payload);
        if (parts.size() >= 3) {
            itemName = parts[2];
            SetWindowTextW(field1Edit_, ToWide(itemName).c_str());
        }
    }

    const auto items = repository_.FindInventoryOrderAlternatives(itemName, ToUtf8(GetWindowTextString(field3Edit_)));
    PopulateList(L"Inventory alternatives", items);
    listMode_ = ListMode::InventoryAlternatives;
    SetActionStatus(items.empty() ? L"No close alternatives were found for that item." : L"Select an alternative or keep the original item and review the order.");
    return true;
}

bool DashboardView::HandleInventoryOrderPrimary() {
    if (listMode_ == ListMode::InventoryOrderPreview) {
        std::string summaryMessage;
        std::string errorMessage;
        if (!repository_.SaveInventoryOrderDetailed(
                currentUser_,
                ToUtf8(GetWindowTextString(field1Edit_)),
                ToUtf8(GetWindowTextString(field2Edit_)),
                ToUtf8(GetWindowTextString(field3Edit_)),
                pendingInventoryAlternativePayload_,
                summaryMessage,
                errorMessage)) {
            SetActionStatus(ToWide(errorMessage));
            return true;
        }

        pendingInventoryAlternativePayload_.clear();
        ClearList();
        listMode_ = ListMode::None;
        SetActionStatus(ToWide(summaryMessage));
        return true;
    }

    const std::string payload = GetSelectedPayload();
    if (payload.rfind("inventory|", 0) == 0) {
        const std::vector<std::string> parts = SplitPayload(payload);
        if (parts.size() >= 3) {
            SetWindowTextW(field1Edit_, ToWide(parts[2]).c_str());
            pendingInventoryAlternativePayload_.clear();
        }
    } else if (payload.rfind("alternative|", 0) == 0) {
        pendingInventoryAlternativePayload_ = payload;
    }

    std::string errorMessage;
    const InventoryOrderPreview preview = repository_.BuildInventoryOrderPreview(
        ToUtf8(GetWindowTextString(field1Edit_)),
        ToUtf8(GetWindowTextString(field2Edit_)),
        ToUtf8(GetWindowTextString(field3Edit_)),
        pendingInventoryAlternativePayload_,
        errorMessage);
    if (!errorMessage.empty()) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    PopulateList(L"Order preview", preview.lines);
    listMode_ = ListMode::InventoryOrderPreview;
    SetActionStatus(L"Review the order preview and click Confirm Order to save it.");
    return true;
}

bool DashboardView::HandleHrShiftPrimary() {
    if (listMode_ == ListMode::HrSwapCandidates) {
        std::string summaryMessage;
        std::string errorMessage;
        if (!repository_.SwapHrShift(
                currentUser_,
                GetSelectedPayload(),
                ToUtf8(GetWindowTextString(field3Edit_)),
                summaryMessage,
                errorMessage)) {
            SetActionStatus(ToWide(errorMessage));
            return true;
        }

        ClearList();
        listMode_ = ListMode::None;
        SetWindowTextW(field3Label_, actionConfig_.field3Label.c_str());
        SetActionStatus(ToWide(summaryMessage));
        return true;
    }

    const std::wstring employee = GetWindowTextString(field1Edit_);
    const std::wstring shiftDate = GetWindowTextString(field2Edit_);
    const std::wstring department = GetWindowTextString(field3Edit_);
    const std::wstring startTime = GetWindowTextString(field4Edit_);
    const std::wstring endTime = GetWindowTextString(field5Edit_);
    if (employee.empty() || shiftDate.empty() || department.empty() || startTime.empty() || endTime.empty()) {
        SetActionStatus(L"Fill in employee, date, department, start time, and end time.");
        return true;
    }

    std::string summaryMessage;
    std::string errorMessage;
    if (!repository_.SaveHrShiftDetailed(
            currentUser_,
            ToUtf8(employee),
            ToUtf8(shiftDate),
            ToUtf8(startTime),
            ToUtf8(endTime),
            ToUtf8(department),
            "",
            summaryMessage,
            errorMessage)) {
        const std::string normalized = ToLower(errorMessage);
        if (normalized.find("leave") == std::string::npos &&
            normalized.find("rest") == std::string::npos &&
            normalized.find("weekly") == std::string::npos &&
            normalized.find("overlap") == std::string::npos) {
            SetActionStatus(ToWide(errorMessage));
            return true;
        }

        pendingHrOverride_ = true;
        pendingHrEmployee_ = ToUtf8(employee);
        pendingHrDate_ = ToUtf8(shiftDate);
        pendingHrDepartment_ = ToUtf8(department);
        pendingHrStart_ = ToUtf8(startTime);
        pendingHrEnd_ = ToUtf8(endTime);
        SetWindowTextW(field3Label_, L"Justification");
        SetWindowTextW(field3Edit_, L"");
        UpdateWorkflowButtons();
        SetActionStatus(ToWide(errorMessage + " Enter a justification in the third field and click Apply Override if you still need this shift."));
        return true;
    }

    pendingHrOverride_ = false;
    SetWindowTextW(field3Label_, actionConfig_.field3Label.c_str());
    SetActionStatus(ToWide(summaryMessage));
    return true;
}

bool DashboardView::HandleHrShiftSecondary() {
    std::string errorMessage;
    const auto items = repository_.FindShiftSwapCandidates(
        ToUtf8(GetWindowTextString(field1Edit_)),
        ToUtf8(GetWindowTextString(field2Edit_)),
        ToUtf8(GetWindowTextString(field3Edit_)),
        ToUtf8(GetWindowTextString(field4Edit_)),
        ToUtf8(GetWindowTextString(field5Edit_)),
        errorMessage);
    if (!errorMessage.empty()) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    pendingHrEmployee_ = ToUtf8(GetWindowTextString(field1Edit_));
    pendingHrDate_ = ToUtf8(GetWindowTextString(field2Edit_));
    pendingHrDepartment_ = ToUtf8(GetWindowTextString(field3Edit_));
    pendingHrStart_ = ToUtf8(GetWindowTextString(field4Edit_));
    pendingHrEnd_ = ToUtf8(GetWindowTextString(field5Edit_));
    PopulateList(L"Swap candidates", items);
    listMode_ = ListMode::HrSwapCandidates;
    SetWindowTextW(field3Label_, L"Swap justification");
    SetWindowTextW(field3Edit_, L"");
    SetActionStatus(items.empty() ? L"No swap candidates matched that shift." : L"Select a candidate, enter a justification, and click Confirm Swap.");
    return true;
}

bool DashboardView::HandleHrShiftTertiary() {
    if (!pendingHrOverride_) {
        SetActionStatus(L"Try to save a conflicting shift first, then use this button to submit the override justification.");
        return true;
    }

    std::string summaryMessage;
    std::string errorMessage;
    if (!repository_.SaveHrShiftDetailed(
            currentUser_,
            pendingHrEmployee_,
            pendingHrDate_,
            pendingHrStart_,
            pendingHrEnd_,
            pendingHrDepartment_,
            ToUtf8(GetWindowTextString(field3Edit_)),
            summaryMessage,
            errorMessage)) {
        SetActionStatus(ToWide(errorMessage));
        return true;
    }

    pendingHrOverride_ = false;
    SetWindowTextW(field3Label_, actionConfig_.field3Label.c_str());
    SetWindowTextW(field3Edit_, ToWide(pendingHrDepartment_).c_str());
    SetActionStatus(ToWide(summaryMessage));
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
    RECT headerCard = {24, 24, 1084, 228};
    HBRUSH cardBrush = CreateSolidBrush(Theme::kCardBackground);
    HPEN cardPen = CreatePen(PS_SOLID, 1, Theme::kBorder);
    HGDIOBJ oldBrush = SelectObject(hdc, cardBrush);
    HGDIOBJ oldPen = SelectObject(hdc, cardPen);
    RoundRect(hdc, headerCard.left, headerCard.top, headerCard.right, headerCard.bottom, 26, 26);
    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(cardBrush);
    DeleteObject(cardPen);

    RECT actionCard = {24, 418, 1084, 874};
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
