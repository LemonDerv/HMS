#include "CaseRepository.h"

#include "StringUtil.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstring>
#include <cstdio>

namespace {
sqlite3_destructor_type SqliteTransient() {
    return SQLITE_TRANSIENT;
}

bool BindText(sqlite3_stmt* statement, int parameterIndex, const std::string& value) {
    return sqlite3_bind_text(statement, parameterIndex, value.c_str(), -1, SqliteTransient()) == SQLITE_OK;
}

bool TryParseInteger(const std::string& text, int& value) {
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

bool TryParseDouble(const std::string& text, double& value) {
    try {
        std::size_t parsedLength = 0;
        const double parsedValue = std::stod(text, &parsedLength);
        if (parsedLength != text.size()) {
            return false;
        }
        value = parsedValue;
        return true;
    } catch (...) {
        return false;
    }
}

bool TextContainsInsensitive(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) {
        return false;
    }

    return ToLower(haystack).find(ToLower(needle)) != std::string::npos;
}

std::vector<std::string> SplitText(const std::string& text, char delimiter) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : text) {
        if (ch == delimiter) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

bool ParseTimeOfDay(const std::string& timeText, int& hour, int& minute) {
    if (timeText.size() != 5 || timeText[2] != ':') {
        return false;
    }

    int parsedHour = 0;
    int parsedMinute = 0;
    if (!TryParseInteger(timeText.substr(0, 2), parsedHour) || !TryParseInteger(timeText.substr(3, 2), parsedMinute)) {
        return false;
    }
    if (parsedHour < 0 || parsedHour > 23 || parsedMinute < 0 || parsedMinute > 59) {
        return false;
    }

    hour = parsedHour;
    minute = parsedMinute;
    return true;
}

bool ParseDateTime(const std::string& dateText, const std::string& timeText, std::time_t& value, bool endCanRollOver) {
    if (dateText.size() != 10 || dateText[4] != '-' || dateText[7] != '-') {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    if (!TryParseInteger(dateText.substr(0, 4), year) ||
        !TryParseInteger(dateText.substr(5, 2), month) ||
        !TryParseInteger(dateText.substr(8, 2), day) ||
        !ParseTimeOfDay(timeText, hour, minute)) {
        return false;
    }

    std::tm tmValue = {};
    tmValue.tm_year = year - 1900;
    tmValue.tm_mon = month - 1;
    tmValue.tm_mday = day;
    tmValue.tm_hour = hour;
    tmValue.tm_min = minute;
    tmValue.tm_sec = 0;
    tmValue.tm_isdst = -1;
    value = std::mktime(&tmValue);
    if (value == static_cast<std::time_t>(-1)) {
        return false;
    }

    if (endCanRollOver) {
        int startHour = 0;
        int startMinute = 0;
        if (ParseTimeOfDay("00:00", startHour, startMinute)) {
            (void)startHour;
            (void)startMinute;
        }
    }

    return true;
}

double GetInsuranceRate(const std::string& provider) {
    const std::string normalized = ToLower(Trim(provider));
    if (normalized.empty()) {
        return 0.0;
    }
    if (normalized.find("gold") != std::string::npos || normalized.find("premium") != std::string::npos) {
        return 0.70;
    }
    if (normalized.find("public") != std::string::npos || normalized.find("national") != std::string::npos) {
        return 0.40;
    }
    if (normalized.find("basic") != std::string::npos) {
        return 0.25;
    }
    return 0.20;
}

bool ParseCalendarDate(const std::string& dateText, std::time_t& value) {
    if (dateText.size() != 10 || dateText[4] != '-' || dateText[7] != '-') {
        return false;
    }

    int year = 0;
    int month = 0;
    int day = 0;
    if (!TryParseInteger(dateText.substr(0, 4), year) ||
        !TryParseInteger(dateText.substr(5, 2), month) ||
        !TryParseInteger(dateText.substr(8, 2), day)) {
        return false;
    }

    std::tm tmValue = {};
    tmValue.tm_year = year - 1900;
    tmValue.tm_mon = month - 1;
    tmValue.tm_mday = day;
    tmValue.tm_hour = 12;
    tmValue.tm_isdst = -1;
    value = std::mktime(&tmValue);
    return value != static_cast<std::time_t>(-1);
}

std::string FormatCalendarDate(std::time_t value) {
    std::tm tmValue = {};
    localtime_s(&tmValue, &value);
    char buffer[11] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tmValue);
    return buffer;
}

std::string FormatWeekStamp(std::time_t value) {
    std::tm tmValue = {};
    localtime_s(&tmValue, &value);
    char buffer[9] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%W", &tmValue);
    return buffer;
}

std::string NormalizeDigits(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (char ch : text) {
        if (ch >= '0' && ch <= '9') {
            result.push_back(ch);
        }
    }
    return result;
}

bool MedicinesInteract(const std::string& leftMedicine,
                       const std::string& rightMedicine,
                       std::string& message) {
    struct InteractionRule {
        const char* left;
        const char* right;
        const char* message;
    };

    static const InteractionRule kRules[] = {
        {"warfarin", "aspirin", "Warfarin and aspirin can sharply increase bleeding risk."},
        {"warfarin", "ibuprofen", "Warfarin and ibuprofen can sharply increase bleeding risk."},
        {"nitroglycerin", "sildenafil", "Nitroglycerin and sildenafil should not be prescribed together."},
        {"clarithromycin", "simvastatin", "Clarithromycin can dangerously raise simvastatin levels."},
        {"tramadol", "sertraline", "Tramadol and sertraline can increase serotonin syndrome risk."}
    };

    const std::string normalizedLeft = ToLower(Trim(leftMedicine));
    const std::string normalizedRight = ToLower(Trim(rightMedicine));
    if (normalizedLeft.empty() || normalizedRight.empty()) {
        return false;
    }

    for (const auto& rule : kRules) {
        const bool matchesForward =
            normalizedLeft.find(rule.left) != std::string::npos &&
            normalizedRight.find(rule.right) != std::string::npos;
        const bool matchesReverse =
            normalizedLeft.find(rule.right) != std::string::npos &&
            normalizedRight.find(rule.left) != std::string::npos;
        if (matchesForward || matchesReverse) {
            message = rule.message;
            return true;
        }
    }

    return false;
}

std::string GenerateReceiptNumber(int billId) {
    const std::time_t now = std::time(nullptr);
    std::tm tmValue = {};
    localtime_s(&tmValue, &now);
    char timestamp[15] = {};
    std::strftime(timestamp, sizeof(timestamp), "%y%m%d%H%M%S", &tmValue);
    return "RCT-" + std::to_string(billId) + "-" + timestamp;
}

std::string FormatAmount(double amount) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.2f", amount);
    return buffer;
}

std::vector<std::string> MedicationAlternativesFor(const std::string& medicine) {
    const std::string normalized = ToLower(Trim(medicine));
    if (normalized.find("aspirin") != std::string::npos) {
        return {"acetaminophen", "clopidogrel"};
    }
    if (normalized.find("ibuprofen") != std::string::npos) {
        return {"acetaminophen", "naproxen"};
    }
    if (normalized.find("warfarin") != std::string::npos) {
        return {"apixaban", "rivaroxaban"};
    }
    if (normalized.find("nitroglycerin") != std::string::npos) {
        return {"isosorbide mononitrate", "amlodipine"};
    }
    if (normalized.find("clarithromycin") != std::string::npos) {
        return {"azithromycin", "doxycycline"};
    }
    if (normalized.find("tramadol") != std::string::npos) {
        return {"acetaminophen", "naproxen"};
    }
    return {"acetaminophen", "cetirizine", "omeprazole"};
}

int SurgeryDurationMinutesFor(const std::string& doctorCategory) {
    const std::string normalized = ToLower(Trim(doctorCategory));
    if (normalized.find("card") != std::string::npos) {
        return 180;
    }
    if (normalized.find("neuro") != std::string::npos) {
        return 210;
    }
    if (normalized.find("orth") != std::string::npos) {
        return 150;
    }
    if (normalized.find("ent") != std::string::npos || normalized.find("ear") != std::string::npos) {
        return 90;
    }
    return 120;
}

bool TimeRangesOverlap(const std::string& dateText,
                       const std::string& startTime,
                       int durationMinutes,
                       const std::string& otherDateText,
                       const std::string& otherStartTime,
                       int otherDurationMinutes) {
    std::time_t start = {};
    std::time_t otherStart = {};
    if (!ParseDateTime(dateText, startTime, start, false) ||
        !ParseDateTime(otherDateText, otherStartTime, otherStart, false)) {
        return false;
    }

    const std::time_t end = start + static_cast<std::time_t>(std::max(durationMinutes, 1)) * 60;
    const std::time_t otherEnd = otherStart + static_cast<std::time_t>(std::max(otherDurationMinutes, 1)) * 60;
    return start < otherEnd && otherStart < end;
}

bool DoctorCategoryMatches(const std::string& specialization, const std::string& requestedCategory) {
    const std::string normalizedSpecialization = ToLower(Trim(specialization));
    const std::string normalizedCategory = ToLower(Trim(requestedCategory));
    if (normalizedCategory.empty()) {
        return true;
    }
    if (normalizedSpecialization.empty()) {
        return false;
    }
    return normalizedSpecialization.find(normalizedCategory) != std::string::npos ||
           normalizedCategory.find(normalizedSpecialization) != std::string::npos;
}

std::string FirstToken(const std::string& text) {
    const std::string trimmed = Trim(text);
    const std::size_t spaceIndex = trimmed.find(' ');
    return spaceIndex == std::string::npos ? trimmed : trimmed.substr(0, spaceIndex);
}

bool LooksLikeApprovedLeave(const std::string& issue) {
    const std::string normalized = ToLower(Trim(issue));
    return normalized.find("approved leave") != std::string::npos ||
           normalized.find("leave approved") != std::string::npos ||
           normalized.find("annual leave") != std::string::npos ||
           normalized.find("medical leave") != std::string::npos;
}

bool ExecutePrepared(sqlite3_stmt* statement, sqlite3* databaseHandle, std::string& errorMessage) {
    const int result = sqlite3_step(statement);
    if (result != SQLITE_DONE) {
        errorMessage = sqlite3_errmsg(databaseHandle);
        sqlite3_finalize(statement);
        return false;
    }

    sqlite3_finalize(statement);
    return true;
}
}

CaseRepository::CaseRepository(std::string databasePath) : database_(std::move(databasePath)) {
    if (!database_.Open(initializationError_)) {
        return;
    }

    database_.InitializeSchema(initializationError_);
}

bool CaseRepository::IsAvailable() const {
    return database_.IsOpen() && initializationError_.empty();
}

const std::string& CaseRepository::InitializationError() const {
    return initializationError_;
}

bool CaseRepository::SaveAction(const std::string& storageKey,
                               const User& currentUser,
                               const std::string& field1,
                               const std::string& field2,
                               const std::string& field3,
                               std::string& errorMessage) {
    if (!IsAvailable()) {
        errorMessage = initializationError_.empty() ? "The SQLite database is not available." : initializationError_;
        return false;
    }

    if (storageKey == "patient_appointments") {
        return SaveAppointment(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "patient_payments") {
        return SavePayment(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "doctor_admissions") {
        return SaveAdmission(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "doctor_prescriptions") {
        return SavePrescription(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "doctor_records") {
        return SaveMedicalRecord(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "pharmacist_dispensing") {
        return SaveMedicationDispensing(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "pharmacist_stock") {
        return SaveInventorySnapshot(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "pharmacist_reviews") {
        return SavePharmacyReview(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "secretary_billing") {
        return SaveBill(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "secretary_surgeries") {
        return SaveSurgery(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "secretary_patient_tasks") {
        return SavePatientSupportTask(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "inventory_status") {
        return SaveInventorySnapshot(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "inventory_orders") {
        return SaveInventoryOrder(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "inventory_alerts") {
        return SaveInventoryAlert(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "hr_shifts") {
        return SaveHrShift(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "hr_staff_updates") {
        return SaveHrStaffUpdate(currentUser, field1, field2, field3, errorMessage);
    }
    if (storageKey == "hr_conflicts") {
        return SaveHrConflict(currentUser, field1, field2, field3, errorMessage);
    }

    errorMessage = "This dashboard action is not mapped to a SQLite table yet.";
    return false;
}

std::vector<std::wstring> CaseRepository::LoadRecentPatientHistory(const User& currentUser) const {
    std::vector<std::wstring> history;
    if (!IsAvailable() || currentUser.id <= 0) {
        return history;
    }

    sqlite3_stmt* statement = nullptr;
    std::string errorMessage;
    const std::string sql =
        "SELECT label, details "
        "FROM ("
        "   SELECT 'Appointment' AS label, "
        "          appointment_date || ' | ' || specialty || ' | ' || COALESCE(doctor_name, 'No doctor') AS details, "
        "          created_at "
        "   FROM appointments "
        "   WHERE patient_user_id = ? "
        "   UNION ALL "
        "   SELECT 'Bill' AS label, "
        "          service_name || ' | ' || printf('%.2f', amount) || ' | ' || status AS details, "
        "          created_at "
        "   FROM bills "
        "   WHERE patient_user_id = ? "
        "   UNION ALL "
        "   SELECT 'Payment' AS label, "
        "          payment_method || ' | ' || printf('%.2f', amount) || ' | ' || status AS details, "
        "          created_at "
        "   FROM payments "
        "   WHERE patient_user_id = ? "
        "   UNION ALL "
        "   SELECT 'Prescription' AS label, "
        "          medicine || ' | ' || dosage || ' | ' || status AS details, "
        "          created_at "
        "   FROM prescriptions "
        "   WHERE patient_user_id = ? "
        "   UNION ALL "
        "   SELECT 'Admission' AS label, "
        "          clinic || ' | ' || reason || ' | ' || status AS details, "
        "          created_at "
        "   FROM admissions "
        "   WHERE patient_user_id = ? "
        "   UNION ALL "
        "   SELECT 'Dispensing' AS label, "
        "          medicine || ' | qty ' || quantity || ' | ' || status AS details, "
        "          created_at "
        "   FROM medication_dispensing "
        "   WHERE patient_user_id = ? "
        ") "
        "ORDER BY created_at DESC "
        "LIMIT 8;";

    if (!database_.Prepare(sql, &statement, errorMessage)) {
        return history;
    }

    sqlite3_bind_int(statement, 1, currentUser.id);
    sqlite3_bind_int(statement, 2, currentUser.id);
    sqlite3_bind_int(statement, 3, currentUser.id);
    sqlite3_bind_int(statement, 4, currentUser.id);
    sqlite3_bind_int(statement, 5, currentUser.id);
    sqlite3_bind_int(statement, 6, currentUser.id);

    while (sqlite3_step(statement) == SQLITE_ROW) {
        const char* label = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        const char* details = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        history.push_back(ToWide(label != nullptr ? label : "") + L": " + ToWide(details != nullptr ? details : ""));
    }

    sqlite3_finalize(statement);
    return history;
}

void CaseRepository::ReleaseExpiredAppointmentReservations() const {
    if (!IsAvailable()) {
        return;
    }

    std::string ignoredError;
    database_.Execute(
        "UPDATE appointments "
        "SET status = 'released', reservation_expires_at = NULL "
        "WHERE status = 'reserved' "
        "  AND reservation_expires_at IS NOT NULL "
        "  AND datetime(reservation_expires_at) <= datetime('now', 'localtime');",
        ignoredError);
}

void CaseRepository::CleanupExpiredMedicalRecordLocks() const {
    if (!IsAvailable()) {
        return;
    }

    std::string ignoredError;
    database_.Execute(
        "DELETE FROM medical_record_locks "
        "WHERE datetime(expires_at) <= datetime('now', 'localtime');",
        ignoredError);
}

std::vector<WorkflowItem> CaseRepository::FindAppointmentSlots(const std::string& appointmentDate,
                                                               const std::string& specialty,
                                                               const std::string& doctorFilter) const {
    std::vector<WorkflowItem> items;
    if (!IsAvailable() || Trim(appointmentDate).empty()) {
        return items;
    }

    ReleaseExpiredAppointmentReservations();

    sqlite3_stmt* doctors = nullptr;
    std::string errorMessage;
    if (!database_.Prepare(
            "SELECT users.id, users.full_name, COALESCE(staff_profiles.specialization, '') "
            "FROM users "
            "LEFT JOIN staff_profiles ON staff_profiles.user_id = users.id "
            "WHERE users.role = 'Doctor' "
            "  AND (? = '' OR users.username LIKE '%' || ? || '%' COLLATE NOCASE "
            "       OR users.full_name LIKE '%' || ? || '%' COLLATE NOCASE) "
            "ORDER BY users.full_name ASC;",
            &doctors,
            errorMessage)) {
        return items;
    }

    const std::string doctorSearch = Trim(doctorFilter);
    BindText(doctors, 1, doctorSearch);
    BindText(doctors, 2, doctorSearch);
    BindText(doctors, 3, doctorSearch);

    sqlite3_stmt* bookedCheck = nullptr;
    if (!database_.Prepare(
            "SELECT 1 "
            "FROM appointments "
            "WHERE doctor_user_id = ? "
            "  AND appointment_date = ? "
            "  AND appointment_time = ? "
            "  AND (status IN ('scheduled', 'confirmed') "
            "       OR (status = 'reserved' "
            "           AND reservation_expires_at IS NOT NULL "
            "           AND datetime(reservation_expires_at) > datetime('now', 'localtime'))) "
            "LIMIT 1;",
            &bookedCheck,
            errorMessage)) {
        sqlite3_finalize(doctors);
        return items;
    }

    const char* slots[] = {"09:00", "10:00", "11:00", "14:00", "15:00", "16:00"};
    const std::string requestedSpecialty = Trim(specialty);

    while (sqlite3_step(doctors) == SQLITE_ROW) {
        const int doctorId = sqlite3_column_int(doctors, 0);
        const char* doctorNameText = reinterpret_cast<const char*>(sqlite3_column_text(doctors, 1));
        const char* specializationText = reinterpret_cast<const char*>(sqlite3_column_text(doctors, 2));
        const std::string doctorName = doctorNameText != nullptr ? doctorNameText : "";
        const std::string specialization = specializationText != nullptr ? specializationText : "";

        if (!requestedSpecialty.empty() && !specialization.empty() && ToLower(specialization) != ToLower(requestedSpecialty)) {
            continue;
        }

        for (const char* slot : slots) {
            sqlite3_reset(bookedCheck);
            sqlite3_clear_bindings(bookedCheck);
            sqlite3_bind_int(bookedCheck, 1, doctorId);
            BindText(bookedCheck, 2, appointmentDate);
            BindText(bookedCheck, 3, slot);
            if (sqlite3_step(bookedCheck) == SQLITE_ROW) {
                continue;
            }

            WorkflowItem item;
            item.payload = "slot|" + std::to_string(doctorId) + "|" + doctorName + "|" + appointmentDate + "|" + slot + "|" + requestedSpecialty;
            item.displayText = appointmentDate + "  " + slot + "  |  " + doctorName +
                (requestedSpecialty.empty() ? "" : "  |  " + requestedSpecialty);
            items.push_back(item);
        }
    }

    sqlite3_finalize(bookedCheck);
    sqlite3_finalize(doctors);
    return items;
}

std::string CaseRepository::FindNextAvailableAppointmentDate(const std::string& appointmentDate,
                                                             const std::string& specialty,
                                                             const std::string& doctorFilter) const {
    std::time_t startDate = {};
    if (!ParseCalendarDate(Trim(appointmentDate), startDate)) {
        return "";
    }

    constexpr int kLookAheadDays = 21;
    for (int dayOffset = 1; dayOffset <= kLookAheadDays; ++dayOffset) {
        const std::time_t candidate = startDate + static_cast<std::time_t>(dayOffset) * 24 * 60 * 60;
        const std::string candidateDate = FormatCalendarDate(candidate);
        if (!FindAppointmentSlots(candidateDate, specialty, doctorFilter).empty()) {
            return candidateDate;
        }
    }

    return "";
}

std::vector<WorkflowItem> CaseRepository::ListPatientAppointments(const User& currentUser) const {
    std::vector<WorkflowItem> items;
    if (!IsAvailable() || currentUser.id <= 0) {
        return items;
    }

    ReleaseExpiredAppointmentReservations();

    sqlite3_stmt* statement = nullptr;
    std::string errorMessage;
    if (!database_.Prepare(
            "SELECT id, COALESCE(doctor_user_id, 0), COALESCE(doctor_name, ''), appointment_date, "
            "       COALESCE(appointment_time, ''), specialty, status, COALESCE(reservation_expires_at, '') "
            "FROM appointments "
            "WHERE patient_user_id = ? "
            "ORDER BY appointment_date DESC, appointment_time DESC, id DESC;",
            &statement,
            errorMessage)) {
        return items;
    }

    sqlite3_bind_int(statement, 1, currentUser.id);
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const int appointmentId = sqlite3_column_int(statement, 0);
        const int doctorUserId = sqlite3_column_int(statement, 1);
        const char* doctorNameText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        const char* dateText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
        const char* timeText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
        const char* specialtyText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));
        const char* statusText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 6));
        const char* reservationExpiresText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 7));

        WorkflowItem item;
        item.payload =
            "appointment|" + std::to_string(appointmentId) + "|" + std::to_string(doctorUserId) + "|" +
            (doctorNameText != nullptr ? doctorNameText : "") + "|" +
            (dateText != nullptr ? dateText : "") + "|" +
            (timeText != nullptr ? timeText : "") + "|" +
            (specialtyText != nullptr ? specialtyText : "") + "|" +
            (statusText != nullptr ? statusText : "") + "|" +
            (reservationExpiresText != nullptr ? reservationExpiresText : "");
        item.displayText =
            std::string(dateText != nullptr ? dateText : "") + "  " +
            std::string(timeText != nullptr ? timeText : "") + "  |  " +
            std::string(doctorNameText != nullptr ? doctorNameText : "Unassigned") + "  |  " +
            std::string(specialtyText != nullptr ? specialtyText : "") + "  |  " +
            std::string(statusText != nullptr ? statusText : "") +
            ((statusText != nullptr && std::string(statusText) == "reserved" && reservationExpiresText != nullptr &&
              std::strlen(reservationExpiresText) > 0)
                ? " until " + std::string(reservationExpiresText)
                : "");
        items.push_back(item);
    }

    sqlite3_finalize(statement);
    return items;
}

std::vector<WorkflowItem> CaseRepository::ListOutstandingBills(const User& currentUser) const {
    std::vector<WorkflowItem> items;
    if (!IsAvailable() || currentUser.id <= 0) {
        return items;
    }

    sqlite3_stmt* statement = nullptr;
    std::string errorMessage;
    if (!database_.Prepare(
            "SELECT id, service_name, amount, insurance_coverage, status "
            "FROM bills "
            "WHERE patient_user_id = ? AND status = 'unpaid' "
            "ORDER BY created_at DESC, id DESC;",
            &statement,
            errorMessage)) {
        return items;
    }

    sqlite3_bind_int(statement, 1, currentUser.id);
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const int billId = sqlite3_column_int(statement, 0);
        const char* serviceText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        const double totalAmount = sqlite3_column_double(statement, 2);
        const double insuranceCoverage = sqlite3_column_double(statement, 3);
        const char* statusText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
        const double patientPayable = std::max(0.0, totalAmount - insuranceCoverage);

        WorkflowItem item;
        item.payload =
            "bill|" + std::to_string(billId) + "|" +
            (serviceText != nullptr ? serviceText : "") + "|" +
            std::to_string(patientPayable) + "|" +
            std::to_string(totalAmount) + "|" +
            std::to_string(insuranceCoverage) + "|" +
            (statusText != nullptr ? statusText : "");
        item.displayText =
            "Bill #" + std::to_string(billId) + "  |  " +
            std::string(serviceText != nullptr ? serviceText : "") +
            "  |  Total " + std::to_string(totalAmount) +
            "  |  Covered " + std::to_string(insuranceCoverage) +
            "  |  Pay " + std::to_string(patientPayable);
        items.push_back(item);
    }

    sqlite3_finalize(statement);
    return items;
}

bool CaseRepository::ReserveAppointmentSlot(const User& currentUser,
                                            const std::string& slotPayload,
                                            std::string& errorMessage) {
    const std::vector<std::string> parts = SplitText(slotPayload, '|');
    if (parts.size() < 6 || parts[0] != "slot") {
        errorMessage = "Please select a valid appointment slot.";
        return false;
    }

    ReleaseExpiredAppointmentReservations();

    int doctorUserId = 0;
    if (!TryParseInteger(parts[1], doctorUserId) || doctorUserId <= 0) {
        errorMessage = "Invalid doctor selection.";
        return false;
    }

    sqlite3_stmt* duplicateCheck = nullptr;
    if (!database_.Prepare(
            "SELECT 1 "
            "FROM appointments "
            "WHERE patient_user_id = ? "
            "  AND doctor_user_id = ? "
            "  AND appointment_date = ? "
            "  AND appointment_time = ? "
            "  AND (status IN ('scheduled', 'confirmed') "
            "       OR (status = 'reserved' "
            "           AND reservation_expires_at IS NOT NULL "
            "           AND datetime(reservation_expires_at) > datetime('now', 'localtime'))) "
            "LIMIT 1;",
            &duplicateCheck,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(duplicateCheck, 1, currentUser.id);
    sqlite3_bind_int(duplicateCheck, 2, doctorUserId);
    BindText(duplicateCheck, 3, parts[3]);
    BindText(duplicateCheck, 4, parts[4]);
    if (sqlite3_step(duplicateCheck) == SQLITE_ROW) {
        sqlite3_finalize(duplicateCheck);
        errorMessage = "This slot is already booked for the current patient.";
        return false;
    }
    sqlite3_finalize(duplicateCheck);

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO appointments("
            "patient_user_id, patient_name, specialty, doctor_user_id, doctor_name, appointment_date, appointment_time, status, reservation_expires_at"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, 'reserved', datetime('now', 'localtime', '+10 minutes'));",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(statement, 1, currentUser.id);
    BindText(statement, 2, currentUser.fullName);
    BindText(statement, 3, parts[5]);
    sqlite3_bind_int(statement, 4, doctorUserId);
    BindText(statement, 5, parts[2]);
    BindText(statement, 6, parts[3]);
    BindText(statement, 7, parts[4]);
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
}

bool CaseRepository::ConfirmReservedAppointment(const User& currentUser,
                                                int appointmentId,
                                                std::string& errorMessage) {
    ReleaseExpiredAppointmentReservations();

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "UPDATE appointments "
            "SET status = 'confirmed', reservation_expires_at = NULL "
            "WHERE id = ? "
            "  AND patient_user_id = ? "
            "  AND status = 'reserved' "
            "  AND reservation_expires_at IS NOT NULL "
            "  AND datetime(reservation_expires_at) > datetime('now', 'localtime');",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(statement, 1, appointmentId);
    sqlite3_bind_int(statement, 2, currentUser.id);
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        return false;
    }

    if (sqlite3_changes(database_.Handle()) == 0) {
        errorMessage = "The selected reservation could not be confirmed.";
        return false;
    }
    return true;
}

bool CaseRepository::ReleaseReservedAppointment(const User& currentUser,
                                                int appointmentId,
                                                std::string& errorMessage) {
    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "UPDATE appointments "
            "SET status = 'released', reservation_expires_at = NULL "
            "WHERE id = ? AND patient_user_id = ? AND status = 'reserved';",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(statement, 1, appointmentId);
    sqlite3_bind_int(statement, 2, currentUser.id);
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        return false;
    }

    if (sqlite3_changes(database_.Handle()) == 0) {
        errorMessage = "The selected reservation could not be released.";
        return false;
    }
    return true;
}

bool CaseRepository::BookAppointmentSlot(const User& currentUser, const std::string& slotPayload, std::string& errorMessage) {
    const std::vector<std::string> parts = SplitText(slotPayload, '|');
    if (parts.size() < 6 || parts[0] != "slot") {
        errorMessage = "Please select a valid appointment slot.";
        return false;
    }

    ReleaseExpiredAppointmentReservations();

    int doctorUserId = 0;
    if (!TryParseInteger(parts[1], doctorUserId) || doctorUserId <= 0) {
        errorMessage = "Invalid doctor selection.";
        return false;
    }

    sqlite3_stmt* duplicateCheck = nullptr;
    if (!database_.Prepare(
            "SELECT 1 "
            "FROM appointments "
            "WHERE patient_user_id = ? "
            "  AND doctor_user_id = ? "
            "  AND appointment_date = ? "
            "  AND appointment_time = ? "
            "  AND (status IN ('scheduled', 'confirmed') "
            "       OR (status = 'reserved' "
            "           AND reservation_expires_at IS NOT NULL "
            "           AND datetime(reservation_expires_at) > datetime('now', 'localtime'))) "
            "LIMIT 1;",
            &duplicateCheck,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(duplicateCheck, 1, currentUser.id);
    sqlite3_bind_int(duplicateCheck, 2, doctorUserId);
    BindText(duplicateCheck, 3, parts[3]);
    BindText(duplicateCheck, 4, parts[4]);
    if (sqlite3_step(duplicateCheck) == SQLITE_ROW) {
        sqlite3_finalize(duplicateCheck);
        errorMessage = "This slot is already booked for the current patient.";
        return false;
    }
    sqlite3_finalize(duplicateCheck);

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO appointments("
            "patient_user_id, patient_name, specialty, doctor_user_id, doctor_name, appointment_date, appointment_time, status, reservation_expires_at"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, 'confirmed', NULL);",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(statement, 1, currentUser.id);
    BindText(statement, 2, currentUser.fullName);
    BindText(statement, 3, parts[5]);
    sqlite3_bind_int(statement, 4, doctorUserId);
    BindText(statement, 5, parts[2]);
    BindText(statement, 6, parts[3]);
    BindText(statement, 7, parts[4]);
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
}

bool CaseRepository::CancelAppointment(const User& currentUser, int appointmentId, std::string& errorMessage) {
    ReleaseExpiredAppointmentReservations();

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "UPDATE appointments "
            "SET status = 'cancelled', reservation_expires_at = NULL "
            "WHERE id = ? AND patient_user_id = ? AND status <> 'cancelled';",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(statement, 1, appointmentId);
    sqlite3_bind_int(statement, 2, currentUser.id);
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        return false;
    }

    if (sqlite3_changes(database_.Handle()) == 0) {
        errorMessage = "The selected appointment could not be cancelled.";
        return false;
    }
    return true;
}

bool CaseRepository::RescheduleAppointment(const User& currentUser,
                                          int appointmentId,
                                          const std::string& slotPayload,
                                          std::string& errorMessage) {
    const std::vector<std::string> parts = SplitText(slotPayload, '|');
    if (parts.size() < 6 || parts[0] != "slot") {
        errorMessage = "Please select a valid replacement slot.";
        return false;
    }

    int doctorUserId = 0;
    if (!TryParseInteger(parts[1], doctorUserId) || doctorUserId <= 0) {
        errorMessage = "Invalid doctor selection.";
        return false;
    }

    ReleaseExpiredAppointmentReservations();

    sqlite3_stmt* slotCheck = nullptr;
    if (!database_.Prepare(
            "SELECT 1 "
            "FROM appointments "
            "WHERE doctor_user_id = ? "
            "  AND appointment_date = ? "
            "  AND appointment_time = ? "
            "  AND id <> ? "
            "  AND (status IN ('scheduled', 'confirmed') "
            "       OR (status = 'reserved' "
            "           AND reservation_expires_at IS NOT NULL "
            "           AND datetime(reservation_expires_at) > datetime('now', 'localtime'))) "
            "LIMIT 1;",
            &slotCheck,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(slotCheck, 1, doctorUserId);
    BindText(slotCheck, 2, parts[3]);
    BindText(slotCheck, 3, parts[4]);
    sqlite3_bind_int(slotCheck, 4, appointmentId);
    if (sqlite3_step(slotCheck) == SQLITE_ROW) {
        sqlite3_finalize(slotCheck);
        errorMessage = "That replacement slot is no longer available.";
        return false;
    }
    sqlite3_finalize(slotCheck);

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "UPDATE appointments "
            "SET doctor_user_id = ?, doctor_name = ?, appointment_date = ?, appointment_time = ?, specialty = ?, status = 'confirmed', reservation_expires_at = NULL "
            "WHERE id = ? AND patient_user_id = ?;",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(statement, 1, doctorUserId);
    BindText(statement, 2, parts[2]);
    BindText(statement, 3, parts[3]);
    BindText(statement, 4, parts[4]);
    BindText(statement, 5, parts[5]);
    sqlite3_bind_int(statement, 6, appointmentId);
    sqlite3_bind_int(statement, 7, currentUser.id);
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        return false;
    }

    if (sqlite3_changes(database_.Handle()) == 0) {
        errorMessage = "The selected appointment could not be rescheduled.";
        return false;
    }
    return true;
}

std::vector<WorkflowItem> CaseRepository::SearchPatients(const std::string& filter) const {
    std::vector<WorkflowItem> items;
    if (!IsAvailable()) {
        return items;
    }

    sqlite3_stmt* statement = nullptr;
    std::string errorMessage;
    if (!database_.Prepare(
            "SELECT users.id, users.full_name, users.username, COALESCE(patients.amka, '') "
            "FROM users "
            "LEFT JOIN patients ON patients.user_id = users.id "
            "WHERE role = 'Patient' "
            "  AND (? = '' OR users.username LIKE '%' || ? || '%' COLLATE NOCASE "
            "       OR users.full_name LIKE '%' || ? || '%' COLLATE NOCASE "
            "       OR patients.amka LIKE '%' || ? || '%' COLLATE NOCASE) "
            "ORDER BY users.full_name ASC;",
            &statement,
            errorMessage)) {
        return items;
    }

    const std::string search = Trim(filter);
    BindText(statement, 1, search);
    BindText(statement, 2, search);
    BindText(statement, 3, search);
    BindText(statement, 4, search);
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const int userId = sqlite3_column_int(statement, 0);
        const char* fullNameText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        const char* usernameText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        const char* amkaText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
        const std::string fullName = fullNameText != nullptr ? fullNameText : "";
        const std::string username = usernameText != nullptr ? usernameText : "";
        const std::string amka = amkaText != nullptr ? amkaText : "";

        WorkflowItem item;
        item.payload = "patient|" + std::to_string(userId) + "|" + fullName + "|" + username;
        item.displayText = fullName + "  (" + username + ")" + (amka.empty() ? "" : "  |  AMKA " + amka);
        items.push_back(item);
    }

    sqlite3_finalize(statement);
    return items;
}

bool CaseRepository::CheckPrescriptionWarning(int patientUserId,
                                              const std::string& medicine,
                                              PrescriptionWarning& warning,
                                              std::string& errorMessage) const {
    warning = PrescriptionWarning{};
    if (patientUserId <= 0) {
        errorMessage = "Please select a valid patient first.";
        return false;
    }

    sqlite3_stmt* patientCheck = nullptr;
    if (!database_.Prepare(
            "SELECT COALESCE(allergies, '') FROM patients WHERE user_id = ? LIMIT 1;",
            &patientCheck,
            errorMessage)) {
        return false;
    }

    std::string allergies;
    sqlite3_bind_int(patientCheck, 1, patientUserId);
    if (sqlite3_step(patientCheck) == SQLITE_ROW) {
        const char* allergyText = reinterpret_cast<const char*>(sqlite3_column_text(patientCheck, 0));
        allergies = allergyText != nullptr ? allergyText : "";
    }
    sqlite3_finalize(patientCheck);

    std::vector<std::string> activeMedicines;
    sqlite3_stmt* interactionCheck = nullptr;
    if (!database_.Prepare(
            "SELECT medicine "
            "FROM prescriptions "
            "WHERE patient_user_id = ? "
            "  AND status = 'active';",
            &interactionCheck,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(interactionCheck, 1, patientUserId);
    while (sqlite3_step(interactionCheck) == SQLITE_ROW) {
        const char* activeMedicineText = reinterpret_cast<const char*>(sqlite3_column_text(interactionCheck, 0));
        activeMedicines.push_back(activeMedicineText != nullptr ? activeMedicineText : "");
    }
    sqlite3_finalize(interactionCheck);

    if (!allergies.empty() && TextContainsInsensitive(allergies, medicine)) {
        warning.hasWarning = true;
        warning.message = "The patient's profile lists an allergy matching this medicine.";
    }

    if (!warning.hasWarning) {
        for (const std::string& activeMedicine : activeMedicines) {
            std::string interactionMessage;
            if (MedicinesInteract(medicine, activeMedicine, interactionMessage)) {
                warning.hasWarning = true;
                warning.message = interactionMessage;
                break;
            }
        }
    }

    if (!warning.hasWarning) {
        sqlite3_stmt* monthlyLimitCheck = nullptr;
        if (!database_.Prepare(
                "SELECT COUNT(*) "
                "FROM prescriptions "
                "WHERE patient_user_id = ? "
                "  AND medicine = ? COLLATE NOCASE "
                "  AND strftime('%Y-%m', created_at) = strftime('%Y-%m', 'now', 'localtime');",
                &monthlyLimitCheck,
                errorMessage)) {
            return false;
        }

        sqlite3_bind_int(monthlyLimitCheck, 1, patientUserId);
        BindText(monthlyLimitCheck, 2, Trim(medicine));
        if (sqlite3_step(monthlyLimitCheck) == SQLITE_ROW && sqlite3_column_int(monthlyLimitCheck, 0) >= 3) {
            warning.hasWarning = true;
            warning.message = "This patient has already reached the monthly prescribing limit for this medicine.";
        }
        sqlite3_finalize(monthlyLimitCheck);
    }

    if (!warning.hasWarning) {
        return true;
    }

    for (const std::string& alternativeMedicine : MedicationAlternativesFor(medicine)) {
        if (TextContainsInsensitive(allergies, alternativeMedicine)) {
            continue;
        }

        bool interacts = false;
        for (const std::string& activeMedicine : activeMedicines) {
            std::string interactionMessage;
            if (MedicinesInteract(alternativeMedicine, activeMedicine, interactionMessage)) {
                interacts = true;
                break;
            }
        }
        if (interacts) {
            continue;
        }

        WorkflowItem item;
        item.payload = "medalt|" + alternativeMedicine;
        item.displayText = alternativeMedicine + "  |  safer alternative";
        warning.alternatives.push_back(item);
    }

    return true;
}

bool CaseRepository::CreatePrescriptionDetailed(const User& currentUser,
                                                int patientUserId,
                                                const std::string& medicine,
                                                const std::string& dosage,
                                                const std::string& frequency,
                                                const std::string& instructions,
                                                std::string& errorMessage) {
    sqlite3_stmt* patientLookup = nullptr;
    if (!database_.Prepare(
            "SELECT full_name FROM users WHERE id = ? AND role = 'Patient' LIMIT 1;",
            &patientLookup,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(patientLookup, 1, patientUserId);
    if (sqlite3_step(patientLookup) != SQLITE_ROW) {
        sqlite3_finalize(patientLookup);
        errorMessage = "Please select a valid patient first.";
        return false;
    }

    const char* patientNameText = reinterpret_cast<const char*>(sqlite3_column_text(patientLookup, 0));
    const std::string patientName = patientNameText != nullptr ? patientNameText : "";
    sqlite3_finalize(patientLookup);

    PrescriptionWarning warning;
    if (!CheckPrescriptionWarning(patientUserId, medicine, warning, errorMessage)) {
        return false;
    }
    if (warning.hasWarning) {
        errorMessage = warning.message;
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO prescriptions("
            "patient_user_id, patient_name, doctor_user_id, medicine, dosage, frequency, instructions, status"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, 'active');",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(statement, 1, patientUserId);
    BindText(statement, 2, patientName);
    sqlite3_bind_int(statement, 3, currentUser.id);
    BindText(statement, 4, Trim(medicine));
    BindText(statement, 5, Trim(dosage));
    BindText(statement, 6, Trim(frequency));
    BindText(statement, 7, Trim(instructions));
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
}

bool CaseRepository::LoadLatestPrescriptionDraft(int patientUserId,
                                                 PrescriptionDraft& draft,
                                                 std::string& errorMessage) const {
    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "SELECT id, medicine, dosage, COALESCE(frequency, ''), COALESCE(instructions, '') "
            "FROM prescriptions "
            "WHERE patient_user_id = ? "
            "ORDER BY created_at DESC, id DESC "
            "LIMIT 1;",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(statement, 1, patientUserId);
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        draft = PrescriptionDraft{};
        return true;
    }

    draft.prescriptionId = sqlite3_column_int(statement, 0);
    draft.medicine = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
    draft.dosage = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
    draft.frequency = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
    draft.instructions = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
    draft.hasValue = true;
    sqlite3_finalize(statement);
    return true;
}

std::vector<WorkflowItem> CaseRepository::ListPatientPrescriptions(int patientUserId) const {
    std::vector<WorkflowItem> items;
    if (!IsAvailable() || patientUserId <= 0) {
        return items;
    }

    sqlite3_stmt* statement = nullptr;
    std::string errorMessage;
    if (!database_.Prepare(
            "SELECT id, medicine, dosage, COALESCE(frequency, ''), COALESCE(instructions, ''), status "
            "FROM prescriptions "
            "WHERE patient_user_id = ? "
            "ORDER BY CASE WHEN status = 'active' THEN 0 ELSE 1 END, created_at DESC, id DESC;",
            &statement,
            errorMessage)) {
        return items;
    }

    sqlite3_bind_int(statement, 1, patientUserId);
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const int prescriptionId = sqlite3_column_int(statement, 0);
        const char* medicineText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        const char* dosageText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        const char* frequencyText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
        const char* instructionsText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
        const char* statusText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));

        WorkflowItem item;
        item.payload =
            "prescription|" + std::to_string(prescriptionId) + "|" +
            (medicineText != nullptr ? medicineText : "") + "|" +
            (dosageText != nullptr ? dosageText : "") + "|" +
            (frequencyText != nullptr ? frequencyText : "") + "|" +
            (instructionsText != nullptr ? instructionsText : "") + "|" +
            (statusText != nullptr ? statusText : "");
        item.displayText =
            "Rx #" + std::to_string(prescriptionId) + "  |  " +
            std::string(medicineText != nullptr ? medicineText : "") + "  |  " +
            std::string(dosageText != nullptr ? dosageText : "") + "  |  " +
            std::string(frequencyText != nullptr ? frequencyText : "") + "  |  " +
            std::string(statusText != nullptr ? statusText : "");
        items.push_back(item);
    }

    sqlite3_finalize(statement);
    return items;
}

BillingPreview CaseRepository::BuildBillingPreview(int patientUserId, std::string& errorMessage) const {
    BillingPreview preview;
    if (!IsAvailable() || patientUserId <= 0) {
        errorMessage = "Please select a valid patient.";
        return preview;
    }

    auto appendAmountLine = [&](const std::string& label, const std::string& description, double amount) {
        if (amount <= 0.0) {
            return;
        }
        WorkflowItem item;
        item.payload = label;
        item.displayText = description + "  |  " + std::to_string(amount);
        preview.lines.push_back(item);
        preview.totalAmount += amount;
    };

    sqlite3_stmt* countStatement = nullptr;
    if (!database_.Prepare(
            "SELECT COUNT(*) FROM appointments WHERE patient_user_id = ? AND status IN ('scheduled', 'confirmed');",
            &countStatement,
            errorMessage)) {
        return preview;
    }
    sqlite3_bind_int(countStatement, 1, patientUserId);
    if (sqlite3_step(countStatement) == SQLITE_ROW) {
        appendAmountLine("appointments", "Appointments x" + std::to_string(sqlite3_column_int(countStatement, 0)),
                         sqlite3_column_int(countStatement, 0) * 80.0);
    }
    sqlite3_finalize(countStatement);

    if (!database_.Prepare(
            "SELECT COUNT(*) FROM admissions WHERE patient_user_id = ?;",
            &countStatement,
            errorMessage)) {
        return preview;
    }
    sqlite3_bind_int(countStatement, 1, patientUserId);
    if (sqlite3_step(countStatement) == SQLITE_ROW) {
        appendAmountLine("admissions", "Admissions x" + std::to_string(sqlite3_column_int(countStatement, 0)),
                         sqlite3_column_int(countStatement, 0) * 650.0);
    }
    sqlite3_finalize(countStatement);

    if (!database_.Prepare(
            "SELECT COALESCE(SUM(quantity), 0) FROM medication_dispensing WHERE patient_user_id = ?;",
            &countStatement,
            errorMessage)) {
        return preview;
    }
    sqlite3_bind_int(countStatement, 1, patientUserId);
    if (sqlite3_step(countStatement) == SQLITE_ROW) {
        appendAmountLine("dispensing", "Dispensed medication units x" + std::to_string(sqlite3_column_int(countStatement, 0)),
                         sqlite3_column_int(countStatement, 0) * 12.0);
    }
    sqlite3_finalize(countStatement);

    if (!database_.Prepare(
            "SELECT COUNT(*) FROM surgeries WHERE patient_user_id = ?;",
            &countStatement,
            errorMessage)) {
        return preview;
    }
    sqlite3_bind_int(countStatement, 1, patientUserId);
    if (sqlite3_step(countStatement) == SQLITE_ROW) {
        appendAmountLine("surgeries", "Surgeries x" + std::to_string(sqlite3_column_int(countStatement, 0)),
                         sqlite3_column_int(countStatement, 0) * 2500.0);
    }
    sqlite3_finalize(countStatement);

    sqlite3_stmt* insuranceLookup = nullptr;
    if (!database_.Prepare(
            "SELECT COALESCE(insurance_provider, '') FROM patients WHERE user_id = ? LIMIT 1;",
            &insuranceLookup,
            errorMessage)) {
        return preview;
    }
    sqlite3_bind_int(insuranceLookup, 1, patientUserId);
    std::string insuranceProvider;
    if (sqlite3_step(insuranceLookup) == SQLITE_ROW) {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(insuranceLookup, 0));
        insuranceProvider = text != nullptr ? text : "";
    }
    sqlite3_finalize(insuranceLookup);

    const double insuranceRate = GetInsuranceRate(insuranceProvider);
    preview.insuranceCoverage = preview.totalAmount * insuranceRate;
    preview.patientPayable = preview.totalAmount - preview.insuranceCoverage;

    if (!insuranceProvider.empty()) {
        WorkflowItem insuranceItem;
        insuranceItem.payload = "insurance";
        insuranceItem.displayText = "Insurance provider: " + insuranceProvider + "  |  coverage " +
            std::to_string(static_cast<int>(insuranceRate * 100)) + "%";
        preview.lines.push_back(insuranceItem);
    }

    WorkflowItem totalItem;
    totalItem.payload = "totals";
    totalItem.displayText = "Total: " + std::to_string(preview.totalAmount) +
        "  |  Covered: " + std::to_string(preview.insuranceCoverage) +
        "  |  Patient pays: " + std::to_string(preview.patientPayable);
    preview.lines.push_back(totalItem);
    return preview;
}

bool CaseRepository::GenerateAutomaticBill(const User& currentUser,
                                           int patientUserId,
                                           std::string& summaryMessage,
                                           std::string& errorMessage) {
    BillingPreview preview = BuildBillingPreview(patientUserId, errorMessage);
    if (!errorMessage.empty()) {
        return false;
    }
    if (preview.totalAmount <= 0.0) {
        errorMessage = "No billable patient services were found yet.";
        return false;
    }

    sqlite3_stmt* patientLookup = nullptr;
    if (!database_.Prepare(
            "SELECT full_name FROM users WHERE id = ? AND role = 'Patient' LIMIT 1;",
            &patientLookup,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(patientLookup, 1, patientUserId);
    if (sqlite3_step(patientLookup) != SQLITE_ROW) {
        sqlite3_finalize(patientLookup);
        errorMessage = "Selected patient was not found.";
        return false;
    }

    const char* patientNameText = reinterpret_cast<const char*>(sqlite3_column_text(patientLookup, 0));
    const std::string patientName = patientNameText != nullptr ? patientNameText : "";
    sqlite3_finalize(patientLookup);

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO bills("
            "patient_user_id, patient_name, service_name, amount, insurance_coverage, status, created_by_user_id"
            ") VALUES (?, ?, 'Automatic services bundle', ?, ?, 'unpaid', ?);",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(statement, 1, patientUserId);
    BindText(statement, 2, patientName);
    sqlite3_bind_double(statement, 3, preview.totalAmount);
    sqlite3_bind_double(statement, 4, preview.insuranceCoverage);
    sqlite3_bind_int(statement, 5, currentUser.id);
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        return false;
    }

    summaryMessage =
        "Automatic bill created. Total " + std::to_string(preview.totalAmount) +
        ", insurance " + std::to_string(preview.insuranceCoverage) +
        ", patient pays " + std::to_string(preview.patientPayable) + ".";
    return true;
}

bool CaseRepository::SaveHrShiftDetailed(const User& currentUser,
                                         const std::string& employeeName,
                                         const std::string& shiftDate,
                                         const std::string& startTime,
                                         const std::string& endTime,
                                         const std::string& department,
                                         const std::string& overrideJustification,
                                         std::string& summaryMessage,
                                         std::string& errorMessage) {
    const std::string requestedEmployee = Trim(employeeName);
    const std::string requestedDate = Trim(shiftDate);
    const std::string requestedDepartment = Trim(department);
    const std::string requestedStart = Trim(startTime);
    const std::string requestedEnd = Trim(endTime);
    const std::string requestedOverride = Trim(overrideJustification);
    if (requestedEmployee.empty() || requestedDate.empty() || requestedDepartment.empty() ||
        requestedStart.empty() || requestedEnd.empty()) {
        errorMessage = "Employee, date, department, start time, and end time are required.";
        return false;
    }

    std::time_t newStart = {};
    std::time_t newEnd = {};
    if (!ParseDateTime(requestedDate, requestedStart, newStart, false) ||
        !ParseDateTime(requestedDate, requestedEnd, newEnd, false)) {
        errorMessage = "Shift date and times must use YYYY-MM-DD and HH:MM formats.";
        return false;
    }
    if (newEnd <= newStart) {
        newEnd += 24 * 60 * 60;
    }

    const auto logConflict = [&](const std::string& issue) {
        sqlite3_stmt* conflictInsert = nullptr;
        std::string ignoredError;
        if (!database_.Prepare(
                "INSERT INTO hr_conflicts(employee_name, conflict_date, issue, created_by_user_id) "
                "VALUES (?, ?, ?, ?);",
                &conflictInsert,
                ignoredError)) {
            return;
        }

        BindText(conflictInsert, 1, requestedEmployee);
        BindText(conflictInsert, 2, requestedDate);
        BindText(conflictInsert, 3, issue);
        sqlite3_bind_int(conflictInsert, 4, currentUser.id);
        ExecutePrepared(conflictInsert, database_.Handle(), ignoredError);
    };

    sqlite3_stmt* leaveCheck = nullptr;
    if (!database_.Prepare(
            "SELECT issue "
            "FROM hr_conflicts "
            "WHERE employee_name = ? COLLATE NOCASE "
            "  AND conflict_date = ?;",
            &leaveCheck,
            errorMessage)) {
        return false;
    }

    BindText(leaveCheck, 1, requestedEmployee);
    BindText(leaveCheck, 2, requestedDate);
    while (sqlite3_step(leaveCheck) == SQLITE_ROW) {
        const char* issueText = reinterpret_cast<const char*>(sqlite3_column_text(leaveCheck, 0));
        const std::string issue = issueText != nullptr ? issueText : "";
        if (LooksLikeApprovedLeave(issue)) {
            sqlite3_finalize(leaveCheck);
            if (requestedOverride.empty()) {
                logConflict("Shift blocked because the employee already has approved leave.");
                errorMessage = "This employee has approved leave on the selected date.";
                return false;
            }
            logConflict("Shift override used despite approved leave. Reason: " + requestedOverride);
            break;
        }
    }
    sqlite3_finalize(leaveCheck);

    sqlite3_stmt* existing = nullptr;
    if (!database_.Prepare(
            "SELECT shift_date, COALESCE(start_time, ''), COALESCE(end_time, '') "
            "FROM hr_shifts "
            "WHERE employee_name = ? COLLATE NOCASE;",
            &existing,
            errorMessage)) {
        return false;
    }

    BindText(existing, 1, requestedEmployee);
    double weeklyHours = 0.0;
    const std::string requestedWeek = FormatWeekStamp(newStart);
    while (sqlite3_step(existing) == SQLITE_ROW) {
        const char* dateText = reinterpret_cast<const char*>(sqlite3_column_text(existing, 0));
        const char* startText = reinterpret_cast<const char*>(sqlite3_column_text(existing, 1));
        const char* endText = reinterpret_cast<const char*>(sqlite3_column_text(existing, 2));
        const std::string existingDate = dateText != nullptr ? dateText : "";
        const std::string existingStartText = startText != nullptr ? startText : "";
        const std::string existingEndText = endText != nullptr ? endText : "";
        if (existingStartText.empty() || existingEndText.empty()) {
            continue;
        }

        std::time_t existingStart = {};
        std::time_t existingEnd = {};
        if (!ParseDateTime(existingDate, existingStartText, existingStart, false) ||
            !ParseDateTime(existingDate, existingEndText, existingEnd, false)) {
            continue;
        }
        if (existingEnd <= existingStart) {
            existingEnd += 24 * 60 * 60;
        }

        const std::time_t earlierEnd = existingEnd < newStart ? existingEnd : newEnd;
        const std::time_t laterStart = existingEnd < newStart ? newStart : existingStart;

        if ((newStart < existingEnd && existingStart < newEnd) ||
            std::difftime(laterStart, earlierEnd) < 11.0 * 60.0 * 60.0) {
            sqlite3_finalize(existing);
            if (requestedOverride.empty()) {
                logConflict("Shift blocked because it overlaps or breaks the 11-hour rest rule.");
                errorMessage = "This shift breaks the 11-hour legal rest rule or overlaps an existing shift.";
                return false;
            }
            logConflict("Shift override used for overlap/rest-rule conflict. Reason: " + requestedOverride);
            break;
        }

        if (FormatWeekStamp(existingStart) == requestedWeek) {
            weeklyHours += std::difftime(existingEnd, existingStart) / 3600.0;
        }
    }
    sqlite3_finalize(existing);

    weeklyHours += std::difftime(newEnd, newStart) / 3600.0;
    if (weeklyHours > 48.0) {
        if (requestedOverride.empty()) {
            logConflict("Shift blocked because it would exceed the weekly hour limit.");
            errorMessage = "This shift would push the employee above the weekly hour limit.";
            return false;
        }
        logConflict("Shift override used for weekly-hour-limit conflict. Reason: " + requestedOverride);
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO hr_shifts(employee_name, shift_date, start_time, end_time, department, override_reason, created_by_user_id) "
            "VALUES (?, ?, ?, ?, ?, ?, ?);",
            &statement,
            errorMessage)) {
        return false;
    }

    BindText(statement, 1, requestedEmployee);
    BindText(statement, 2, requestedDate);
    BindText(statement, 3, requestedStart);
    BindText(statement, 4, requestedEnd);
    BindText(statement, 5, requestedDepartment);
    if (!requestedOverride.empty()) {
        BindText(statement, 6, requestedOverride);
    } else {
        sqlite3_bind_null(statement, 6);
    }
    sqlite3_bind_int(statement, 7, currentUser.id);
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        return false;
    }

    summaryMessage = requestedOverride.empty()
        ? "Shift saved."
        : "Shift saved with override justification.";
    return true;
}

std::vector<WorkflowItem> CaseRepository::FindShiftSwapCandidates(const std::string& employeeName,
                                                                  const std::string& shiftDate,
                                                                  const std::string& department,
                                                                  const std::string& startTime,
                                                                  const std::string& endTime,
                                                                  std::string& errorMessage) const {
    std::vector<WorkflowItem> items;
    sqlite3_stmt* baseShiftLookup = nullptr;
    if (!database_.Prepare(
            "SELECT id "
            "FROM hr_shifts "
            "WHERE employee_name = ? COLLATE NOCASE "
            "  AND shift_date = ? "
            "  AND department = ? COLLATE NOCASE "
            "  AND COALESCE(start_time, '') = ? "
            "  AND COALESCE(end_time, '') = ? "
            "LIMIT 1;",
            &baseShiftLookup,
            errorMessage)) {
        return items;
    }

    BindText(baseShiftLookup, 1, Trim(employeeName));
    BindText(baseShiftLookup, 2, Trim(shiftDate));
    BindText(baseShiftLookup, 3, Trim(department));
    BindText(baseShiftLookup, 4, Trim(startTime));
    BindText(baseShiftLookup, 5, Trim(endTime));
    if (sqlite3_step(baseShiftLookup) != SQLITE_ROW) {
        sqlite3_finalize(baseShiftLookup);
        errorMessage = "Create or locate the original shift before searching for swap candidates.";
        return items;
    }
    const int baseShiftId = sqlite3_column_int(baseShiftLookup, 0);
    sqlite3_finalize(baseShiftLookup);

    sqlite3_stmt* candidateLookup = nullptr;
    if (!database_.Prepare(
            "SELECT id, employee_name, COALESCE(start_time, ''), COALESCE(end_time, '') "
            "FROM hr_shifts "
            "WHERE shift_date = ? "
            "  AND department = ? COLLATE NOCASE "
            "  AND employee_name <> ? COLLATE NOCASE "
            "ORDER BY employee_name ASC;",
            &candidateLookup,
            errorMessage)) {
        return items;
    }

    BindText(candidateLookup, 1, Trim(shiftDate));
    BindText(candidateLookup, 2, Trim(department));
    BindText(candidateLookup, 3, Trim(employeeName));
    while (sqlite3_step(candidateLookup) == SQLITE_ROW) {
        const int candidateShiftId = sqlite3_column_int(candidateLookup, 0);
        const char* candidateNameText = reinterpret_cast<const char*>(sqlite3_column_text(candidateLookup, 1));
        const char* candidateStartText = reinterpret_cast<const char*>(sqlite3_column_text(candidateLookup, 2));
        const char* candidateEndText = reinterpret_cast<const char*>(sqlite3_column_text(candidateLookup, 3));

        WorkflowItem item;
        item.payload =
            "swap|" + std::to_string(baseShiftId) + "|" + std::to_string(candidateShiftId) + "|" +
            Trim(employeeName) + "|" +
            std::string(candidateNameText != nullptr ? candidateNameText : "") + "|" +
            Trim(shiftDate) + "|" +
            Trim(department);
        item.displayText =
            std::string(candidateNameText != nullptr ? candidateNameText : "") + "  |  " +
            std::string(candidateStartText != nullptr ? candidateStartText : "") + "-" +
            std::string(candidateEndText != nullptr ? candidateEndText : "") + "  |  " +
            Trim(department);
        items.push_back(item);
    }
    sqlite3_finalize(candidateLookup);
    return items;
}

bool CaseRepository::SwapHrShift(const User& currentUser,
                                 const std::string& swapPayload,
                                 const std::string& justification,
                                 std::string& summaryMessage,
                                 std::string& errorMessage) {
    const std::vector<std::string> parts = SplitText(swapPayload, '|');
    if (parts.size() < 7 || parts[0] != "swap") {
        errorMessage = "Select a valid swap candidate first.";
        return false;
    }
    if (Trim(justification).empty()) {
        errorMessage = "Provide a swap justification first.";
        return false;
    }

    int baseShiftId = 0;
    int candidateShiftId = 0;
    if (!TryParseInteger(parts[1], baseShiftId) || !TryParseInteger(parts[2], candidateShiftId) ||
        baseShiftId <= 0 || candidateShiftId <= 0) {
        errorMessage = "Swap selection is invalid.";
        return false;
    }

    const std::string fromEmployee = parts[3];
    const std::string toEmployee = parts[4];
    const std::string shiftDate = parts[5];

    if (!database_.Execute("BEGIN TRANSACTION;", errorMessage)) {
        return false;
    }

    sqlite3_stmt* updateBase = nullptr;
    if (!database_.Prepare(
            "UPDATE hr_shifts SET employee_name = ?, override_reason = ? WHERE id = ?;",
            &updateBase,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }
    BindText(updateBase, 1, toEmployee);
    BindText(updateBase, 2, "Swap: " + Trim(justification));
    sqlite3_bind_int(updateBase, 3, baseShiftId);
    if (!ExecutePrepared(updateBase, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_stmt* updateCandidate = nullptr;
    if (!database_.Prepare(
            "UPDATE hr_shifts SET employee_name = ?, override_reason = ? WHERE id = ?;",
            &updateCandidate,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }
    BindText(updateCandidate, 1, fromEmployee);
    BindText(updateCandidate, 2, "Swap: " + Trim(justification));
    sqlite3_bind_int(updateCandidate, 3, candidateShiftId);
    if (!ExecutePrepared(updateCandidate, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_stmt* logSwap = nullptr;
    if (!database_.Prepare(
            "INSERT INTO hr_shift_swaps("
            "from_shift_id, to_shift_id, from_employee_name, to_employee_name, shift_date, justification, created_by_user_id"
            ") VALUES (?, ?, ?, ?, ?, ?, ?);",
            &logSwap,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }
    sqlite3_bind_int(logSwap, 1, baseShiftId);
    sqlite3_bind_int(logSwap, 2, candidateShiftId);
    BindText(logSwap, 3, fromEmployee);
    BindText(logSwap, 4, toEmployee);
    BindText(logSwap, 5, shiftDate);
    BindText(logSwap, 6, Trim(justification));
    sqlite3_bind_int(logSwap, 7, currentUser.id);
    if (!ExecutePrepared(logSwap, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    if (!database_.Execute("COMMIT;", errorMessage)) {
        return false;
    }

    summaryMessage = "Shift swap completed between " + fromEmployee + " and " + toEmployee + ".";
    return true;
}

bool CaseRepository::ResolveUserIdentity(const std::string& identifier,
                                         const std::string& expectedRole,
                                         sqlite3_int64& userId,
                                         std::string& fullName) const {
    userId = 0;
    fullName = Trim(identifier);
    if (!IsAvailable() || fullName.empty()) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    std::string errorMessage;
    if (!database_.Prepare(
            "SELECT id, full_name "
            "FROM users "
            "WHERE role = ? "
            "  AND (username = ? COLLATE NOCASE OR full_name = ? COLLATE NOCASE) "
            "LIMIT 1;",
            &statement,
            errorMessage)) {
        return false;
    }

    BindText(statement, 1, expectedRole);
    BindText(statement, 2, fullName);
    BindText(statement, 3, fullName);

    if (sqlite3_step(statement) == SQLITE_ROW) {
        userId = sqlite3_column_int64(statement, 0);
        const char* resolvedName = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        if (resolvedName != nullptr) {
            fullName = resolvedName;
        }
        sqlite3_finalize(statement);
        return true;
    }

    sqlite3_finalize(statement);
    return false;
}

bool CaseRepository::SaveAppointment(const User& currentUser,
                                     const std::string& appointmentDate,
                                     const std::string& specialty,
                                     const std::string& doctorIdentifier,
                                     std::string& errorMessage) {
    sqlite3_int64 doctorUserId = 0;
    std::string doctorName = Trim(doctorIdentifier);
    ResolveUserIdentity(doctorIdentifier, "Doctor", doctorUserId, doctorName);
    if (doctorUserId <= 0) {
        errorMessage = "Doctor must match an existing doctor account.";
        return false;
    }

    sqlite3_stmt* duplicateCheck = nullptr;
    if (!database_.Prepare(
            "SELECT 1 "
            "FROM appointments "
            "WHERE patient_user_id = ? "
            "  AND doctor_user_id = ? "
            "  AND appointment_date = ? "
            "  AND status IN ('scheduled', 'confirmed') "
            "LIMIT 1;",
            &duplicateCheck,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(duplicateCheck, 1, currentUser.id);
    sqlite3_bind_int64(duplicateCheck, 2, doctorUserId);
    BindText(duplicateCheck, 3, Trim(appointmentDate));
    if (sqlite3_step(duplicateCheck) == SQLITE_ROW) {
        sqlite3_finalize(duplicateCheck);
        errorMessage = "An appointment with this doctor already exists for that date.";
        return false;
    }
    sqlite3_finalize(duplicateCheck);

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO appointments("
            "patient_user_id, patient_name, specialty, doctor_user_id, doctor_name, appointment_date, status"
            ") VALUES (?, ?, ?, ?, ?, ?, 'scheduled');",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(statement, 1, currentUser.id);
    BindText(statement, 2, currentUser.fullName);
    BindText(statement, 3, Trim(specialty));
    if (doctorUserId > 0) {
        sqlite3_bind_int64(statement, 4, doctorUserId);
    } else {
        sqlite3_bind_null(statement, 4);
    }
    BindText(statement, 5, doctorName);
    BindText(statement, 6, Trim(appointmentDate));
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
}

bool CaseRepository::SavePayment(const User& currentUser,
                                 const std::string& billReference,
                                 const std::string& method,
                                 const std::string& amountText,
                                 std::string& errorMessage) {
    int billId = 0;
    if (!TryParseInteger(Trim(billReference), billId)) {
        errorMessage = "Bill ID must be a number.";
        return false;
    }

    std::string summaryMessage;
    return ProcessPaymentDetailed(
        currentUser,
        billId,
        method,
        "",
        amountText,
        "",
        summaryMessage,
        errorMessage);
}

bool CaseRepository::ProcessPaymentDetailed(const User& currentUser,
                                           int billId,
                                           const std::string& method,
                                           const std::string& accountReference,
                                           const std::string& amountText,
                                           const std::string& authorizationCode,
                                           std::string& summaryMessage,
                                           std::string& errorMessage) {
    if (billId <= 0) {
        errorMessage = "Select a valid bill first.";
        return false;
    }

    const std::string paymentMethod = Trim(method);
    if (paymentMethod.empty()) {
        errorMessage = "Choose a payment method first.";
        return false;
    }

    const std::string normalizedMethod = ToLower(paymentMethod);
    const std::string normalizedReference = NormalizeDigits(accountReference);
    const std::string normalizedAuthorization = NormalizeDigits(authorizationCode);

    if (normalizedMethod == "card" || normalizedMethod == "credit card" || normalizedMethod == "debit card") {
        if (normalizedReference.size() != 16 || normalizedAuthorization.size() != 3) {
            errorMessage = "Card details were rejected. Check the card number and CVV.";
            return false;
        }
    } else if (normalizedMethod == "bank transfer" || normalizedMethod == "transfer") {
        if (Trim(accountReference).size() < 8) {
            errorMessage = "Bank transfer payments require a reference or account number.";
            return false;
        }
    }

    sqlite3_stmt* lookup = nullptr;
    if (!database_.Prepare(
            "SELECT service_name, amount, insurance_coverage, status "
            "FROM bills WHERE id = ? AND patient_user_id = ? LIMIT 1;",
            &lookup,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(lookup, 1, billId);
    sqlite3_bind_int(lookup, 2, currentUser.id);

    const int lookupResult = sqlite3_step(lookup);
    if (lookupResult != SQLITE_ROW) {
        sqlite3_finalize(lookup);
        errorMessage = "No bill with this ID was found for the current patient.";
        return false;
    }

    const char* serviceText = reinterpret_cast<const char*>(sqlite3_column_text(lookup, 0));
    const double billAmount = sqlite3_column_double(lookup, 1);
    const double insuranceCoverage = sqlite3_column_double(lookup, 2);
    const char* statusText = reinterpret_cast<const char*>(sqlite3_column_text(lookup, 3));
    if (statusText != nullptr && ToLower(statusText) == "paid") {
        sqlite3_finalize(lookup);
        errorMessage = "This bill is already marked as paid.";
        return false;
    }

    const double patientPayable = std::max(0.0, billAmount - insuranceCoverage);
    double amount = patientPayable;
    if (!Trim(amountText).empty()) {
        if (!TryParseDouble(Trim(amountText), amount) || amount <= 0.0) {
            sqlite3_finalize(lookup);
            errorMessage = "Amount must be a positive number.";
            return false;
        }
    }

    if (patientPayable <= 0.0) {
        sqlite3_finalize(lookup);
        errorMessage = "This bill no longer has an outstanding patient balance.";
        return false;
    }

    if (std::fabs(amount - patientPayable) > 0.01) {
        sqlite3_finalize(lookup);
        errorMessage = "Payment amount must match the patient balance shown for the selected bill.";
        return false;
    }

    const std::string serviceName = serviceText != nullptr ? serviceText : "bill";
    sqlite3_finalize(lookup);

    if (!database_.Execute("BEGIN TRANSACTION;", errorMessage)) {
        return false;
    }

    sqlite3_stmt* insertPayment = nullptr;
    if (!database_.Prepare(
            "INSERT INTO payments("
            "bill_id, patient_user_id, patient_name, payment_method, amount, status, receipt_number, authorization_reference"
            ") VALUES (?, ?, ?, ?, ?, 'completed', ?, ?);",
            &insertPayment,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    const std::string receiptNumber = GenerateReceiptNumber(billId);
    std::string storedMethod = paymentMethod;
    if ((normalizedMethod == "card" || normalizedMethod == "credit card" || normalizedMethod == "debit card") &&
        normalizedReference.size() >= 4) {
        storedMethod += " ****" + normalizedReference.substr(normalizedReference.size() - 4);
    } else if (!Trim(accountReference).empty()) {
        storedMethod += " (" + Trim(accountReference) + ")";
    }

    sqlite3_bind_int(insertPayment, 1, billId);
    sqlite3_bind_int(insertPayment, 2, currentUser.id);
    BindText(insertPayment, 3, currentUser.fullName);
    BindText(insertPayment, 4, storedMethod);
    sqlite3_bind_double(insertPayment, 5, amount);
    BindText(insertPayment, 6, receiptNumber);
    BindText(insertPayment, 7, Trim(accountReference).empty() ? Trim(authorizationCode) : Trim(accountReference));

    if (!ExecutePrepared(insertPayment, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_stmt* updateBill = nullptr;
    if (!database_.Prepare("UPDATE bills SET status = 'paid' WHERE id = ?;", &updateBill, errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_bind_int(updateBill, 1, billId);
    if (!ExecutePrepared(updateBill, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    if (!database_.Execute("COMMIT;", errorMessage)) {
        return false;
    }

    summaryMessage =
        "Payment completed for " + serviceName +
        ". Receipt " + receiptNumber +
        ". Patient paid " + FormatAmount(amount) + ".";
    return true;
}

bool CaseRepository::LoadPaymentReceipt(const User& currentUser,
                                        const std::string& receiptNumber,
                                        PaymentReceipt& receipt,
                                        std::string& errorMessage) const {
    receipt = PaymentReceipt{};
    if (Trim(receiptNumber).empty()) {
        errorMessage = "Receipt number is missing.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "SELECT payments.id, payments.bill_id, payments.receipt_number, payments.patient_name, "
            "       COALESCE(bills.service_name, 'Paid bill'), payments.payment_method, payments.amount, payments.created_at "
            "FROM payments "
            "LEFT JOIN bills ON bills.id = payments.bill_id "
            "WHERE payments.patient_user_id = ? "
            "  AND payments.receipt_number = ? "
            "LIMIT 1;",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(statement, 1, currentUser.id);
    BindText(statement, 2, Trim(receiptNumber));
    if (sqlite3_step(statement) != SQLITE_ROW) {
        sqlite3_finalize(statement);
        errorMessage = "The requested receipt was not found.";
        return false;
    }

    receipt.paymentId = sqlite3_column_int(statement, 0);
    receipt.billId = sqlite3_column_int(statement, 1);
    const char* receiptText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
    const char* patientText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
    const char* serviceText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
    const char* methodText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));
    const char* createdAtText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 7));
    receipt.receiptNumber = receiptText != nullptr ? receiptText : "";
    receipt.patientName = patientText != nullptr ? patientText : "";
    receipt.serviceName = serviceText != nullptr ? serviceText : "";
    receipt.paymentMethod = methodText != nullptr ? methodText : "";
    receipt.amount = sqlite3_column_double(statement, 6);
    receipt.createdAt = createdAtText != nullptr ? createdAtText : "";
    receipt.hasValue = true;
    sqlite3_finalize(statement);
    return true;
}

bool CaseRepository::SaveAdmission(const User& currentUser,
                                   const std::string& patientIdentifier,
                                   const std::string& clinic,
                                   const std::string& reason,
                                   std::string& errorMessage) {
    std::string summaryMessage;
    return SaveAdmissionDetailed(
        currentUser,
        0,
        patientIdentifier,
        clinic,
        reason,
        "",
        "",
        summaryMessage,
        errorMessage);
}

bool CaseRepository::SaveAdmissionDetailed(const User& currentUser,
                                           int selectedPatientUserId,
                                           const std::string& patientIdentifier,
                                           const std::string& clinic,
                                           const std::string& reason,
                                           const std::string& amka,
                                           const std::string& dateOfBirth,
                                           std::string& summaryMessage,
                                           std::string& errorMessage) {
    const std::string requestedClinic = Trim(clinic);
    const std::string requestedReason = Trim(reason);
    std::string patientName = Trim(patientIdentifier);
    std::string patientAmka = Trim(amka);
    std::string birthDate = Trim(dateOfBirth);
    sqlite3_int64 patientUserId = selectedPatientUserId > 0 ? selectedPatientUserId : 0;
    bool createdNewPatient = false;

    if (requestedClinic.empty()) {
        errorMessage = "Clinic is required.";
        return false;
    }
    if (requestedReason.empty()) {
        errorMessage = "Reason for admission is required.";
        return false;
    }

    if (patientUserId > 0) {
        sqlite3_stmt* patientLookup = nullptr;
        if (!database_.Prepare(
                "SELECT full_name FROM users WHERE id = ? AND role = 'Patient' LIMIT 1;",
                &patientLookup,
                errorMessage)) {
            return false;
        }

        sqlite3_bind_int64(patientLookup, 1, patientUserId);
        if (sqlite3_step(patientLookup) != SQLITE_ROW) {
            sqlite3_finalize(patientLookup);
            errorMessage = "Select a valid patient first.";
            return false;
        }

        const char* patientNameText = reinterpret_cast<const char*>(sqlite3_column_text(patientLookup, 0));
        patientName = patientNameText != nullptr ? patientNameText : patientName;
        sqlite3_finalize(patientLookup);
    } else {
        ResolveUserIdentity(patientIdentifier, "Patient", patientUserId, patientName);
    }

    if (patientUserId <= 0 && !patientAmka.empty()) {
        sqlite3_stmt* patientRecordLookup = nullptr;
        if (!database_.Prepare(
                "SELECT COALESCE(user_id, 0), patient_name, COALESCE(date_of_birth, '') "
                "FROM patients WHERE amka = ? LIMIT 1;",
                &patientRecordLookup,
                errorMessage)) {
            return false;
        }

        BindText(patientRecordLookup, 1, patientAmka);
        if (sqlite3_step(patientRecordLookup) == SQLITE_ROW) {
            const sqlite3_int64 matchedUserId = sqlite3_column_int64(patientRecordLookup, 0);
            const char* patientNameText = reinterpret_cast<const char*>(sqlite3_column_text(patientRecordLookup, 1));
            const char* birthDateText = reinterpret_cast<const char*>(sqlite3_column_text(patientRecordLookup, 2));
            if (matchedUserId > 0) {
                patientUserId = matchedUserId;
            }
            if (patientName.empty() && patientNameText != nullptr) {
                patientName = patientNameText;
            }
            if (birthDate.empty() && birthDateText != nullptr) {
                birthDate = birthDateText;
            }
        }
        sqlite3_finalize(patientRecordLookup);
    }

    if (patientName.empty()) {
        errorMessage = "Enter or select a patient first.";
        return false;
    }

    if (patientUserId <= 0) {
        std::time_t parsedBirthDate = {};
        if (patientAmka.empty() || birthDate.empty()) {
            errorMessage = "Registering a new patient requires AMKA and birth date.";
            return false;
        }
        if (!ParseCalendarDate(birthDate, parsedBirthDate)) {
            errorMessage = "Birth date must use YYYY-MM-DD.";
            return false;
        }
        createdNewPatient = true;
    }

    sqlite3_stmt* activeAdmissionCheck = nullptr;
    if (patientUserId > 0) {
        if (!database_.Prepare(
                "SELECT clinic FROM admissions WHERE patient_user_id = ? AND status = 'active' LIMIT 1;",
                &activeAdmissionCheck,
                errorMessage)) {
            return false;
        }
        sqlite3_bind_int64(activeAdmissionCheck, 1, patientUserId);
    } else {
        if (!database_.Prepare(
                "SELECT clinic "
                "FROM admissions "
                "WHERE patient_user_id IS NULL "
                "  AND patient_name = ? COLLATE NOCASE "
                "  AND status = 'active' "
                "LIMIT 1;",
                &activeAdmissionCheck,
                errorMessage)) {
            return false;
        }
        BindText(activeAdmissionCheck, 1, patientName);
    }

    if (sqlite3_step(activeAdmissionCheck) == SQLITE_ROW) {
        const char* clinicText = reinterpret_cast<const char*>(sqlite3_column_text(activeAdmissionCheck, 0));
        const std::string activeClinic = clinicText != nullptr ? clinicText : "";
        sqlite3_finalize(activeAdmissionCheck);
        errorMessage = activeClinic.empty()
            ? "This patient already has an active admission. Use Transfer Active instead."
            : "This patient is already admitted to " + activeClinic + ". Use Transfer Active instead.";
        return false;
    }
    sqlite3_finalize(activeAdmissionCheck);

    if (!database_.Execute("BEGIN TRANSACTION;", errorMessage)) {
        return false;
    }

    if (patientUserId <= 0) {
        sqlite3_stmt* patientInsert = nullptr;
        if (!database_.Prepare(
                "INSERT INTO patients(user_id, patient_name, amka, date_of_birth, current_status) "
                "VALUES (NULL, ?, ?, ?, 'registered') "
                "ON CONFLICT(amka) DO UPDATE SET "
                "patient_name = excluded.patient_name, "
                "date_of_birth = excluded.date_of_birth, "
                "current_status = 'registered';",
                &patientInsert,
                errorMessage)) {
            database_.Execute("ROLLBACK;", errorMessage);
            return false;
        }

        BindText(patientInsert, 1, patientName);
        BindText(patientInsert, 2, patientAmka);
        BindText(patientInsert, 3, birthDate);
        if (!ExecutePrepared(patientInsert, database_.Handle(), errorMessage)) {
            database_.Execute("ROLLBACK;", errorMessage);
            return false;
        }
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO admissions("
            "patient_user_id, patient_name, clinic, reason, status, doctor_user_id"
            ") VALUES (?, ?, ?, ?, 'active', ?);",
            &statement,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    if (patientUserId > 0) {
        sqlite3_bind_int64(statement, 1, patientUserId);
    } else {
        sqlite3_bind_null(statement, 1);
    }
    BindText(statement, 2, patientName);
    BindText(statement, 3, requestedClinic);
    BindText(statement, 4, requestedReason);
    sqlite3_bind_int(statement, 5, currentUser.id);
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_stmt* labRequest = nullptr;
    if (!database_.Prepare(
            "INSERT INTO lab_requests("
            "admission_id, patient_user_id, patient_name, requested_by_user_id, request_type, status"
            ") VALUES (?, ?, ?, ?, 'Initial admission panel', 'pending');",
            &labRequest,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_bind_int64(labRequest, 1, database_.LastInsertRowId());
    if (patientUserId > 0) {
        sqlite3_bind_int64(labRequest, 2, patientUserId);
    } else {
        sqlite3_bind_null(labRequest, 2);
    }
    BindText(labRequest, 3, patientName);
    sqlite3_bind_int(labRequest, 4, currentUser.id);
    if (!ExecutePrepared(labRequest, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_stmt* statusUpdate = nullptr;
    if (patientUserId > 0) {
        if (database_.Prepare(
                "UPDATE patients SET current_status = 'admitted' WHERE user_id = ?;",
                &statusUpdate,
                errorMessage)) {
            sqlite3_bind_int64(statusUpdate, 1, patientUserId);
            if (!ExecutePrepared(statusUpdate, database_.Handle(), errorMessage)) {
                database_.Execute("ROLLBACK;", errorMessage);
                return false;
            }
        } else {
            database_.Execute("ROLLBACK;", errorMessage);
            return false;
        }
    } else if (!patientAmka.empty()) {
        if (database_.Prepare(
                "UPDATE patients SET current_status = 'admitted' WHERE amka = ?;",
                &statusUpdate,
                errorMessage)) {
            BindText(statusUpdate, 1, patientAmka);
            if (!ExecutePrepared(statusUpdate, database_.Handle(), errorMessage)) {
                database_.Execute("ROLLBACK;", errorMessage);
                return false;
            }
        } else {
            database_.Execute("ROLLBACK;", errorMessage);
            return false;
        }
    }

    if (!database_.Execute("COMMIT;", errorMessage)) {
        return false;
    }

    summaryMessage = createdNewPatient
        ? "New patient registered, admission saved, and initial lab request created."
        : "Admission saved and initial lab request created.";
    return true;
}

bool CaseRepository::TransferActiveAdmission(const User&,
                                             int selectedPatientUserId,
                                             const std::string& patientIdentifier,
                                             const std::string& newClinic,
                                             std::string& summaryMessage,
                                             std::string& errorMessage) {
    const std::string requestedClinic = Trim(newClinic);
    if (requestedClinic.empty()) {
        errorMessage = "Choose the new clinic first.";
        return false;
    }

    sqlite3_int64 patientUserId = selectedPatientUserId > 0 ? selectedPatientUserId : 0;
    std::string patientName = Trim(patientIdentifier);
    if (patientUserId <= 0) {
        ResolveUserIdentity(patientIdentifier, "Patient", patientUserId, patientName);
    }

    sqlite3_stmt* activeAdmissionLookup = nullptr;
    if (patientUserId > 0) {
        if (!database_.Prepare(
                "SELECT id, clinic "
                "FROM admissions "
                "WHERE patient_user_id = ? AND status = 'active' "
                "LIMIT 1;",
                &activeAdmissionLookup,
                errorMessage)) {
            return false;
        }
        sqlite3_bind_int64(activeAdmissionLookup, 1, patientUserId);
    } else {
        if (patientName.empty()) {
            errorMessage = "Select an admitted patient first.";
            return false;
        }
        if (!database_.Prepare(
                "SELECT id, clinic "
                "FROM admissions "
                "WHERE patient_user_id IS NULL "
                "  AND patient_name = ? COLLATE NOCASE "
                "  AND status = 'active' "
                "LIMIT 1;",
                &activeAdmissionLookup,
                errorMessage)) {
            return false;
        }
        BindText(activeAdmissionLookup, 1, patientName);
    }

    if (sqlite3_step(activeAdmissionLookup) != SQLITE_ROW) {
        sqlite3_finalize(activeAdmissionLookup);
        errorMessage = "No active admission was found for this patient.";
        return false;
    }

    const int admissionId = sqlite3_column_int(activeAdmissionLookup, 0);
    const char* currentClinicText = reinterpret_cast<const char*>(sqlite3_column_text(activeAdmissionLookup, 1));
    const std::string currentClinic = currentClinicText != nullptr ? currentClinicText : "";
    sqlite3_finalize(activeAdmissionLookup);

    if (ToLower(currentClinic) == ToLower(requestedClinic)) {
        errorMessage = "The patient is already assigned to that clinic.";
        return false;
    }

    sqlite3_stmt* transferStatement = nullptr;
    if (!database_.Prepare(
            "UPDATE admissions SET clinic = ? WHERE id = ?;",
            &transferStatement,
            errorMessage)) {
        return false;
    }

    BindText(transferStatement, 1, requestedClinic);
    sqlite3_bind_int(transferStatement, 2, admissionId);
    if (!ExecutePrepared(transferStatement, database_.Handle(), errorMessage)) {
        return false;
    }

    summaryMessage = "Active admission transferred from " + currentClinic + " to " + requestedClinic + ".";
    return true;
}

bool CaseRepository::SavePrescription(const User& currentUser,
                                      const std::string& patientIdentifier,
                                      const std::string& medicine,
                                      const std::string& dosage,
                                      std::string& errorMessage) {
    sqlite3_int64 patientUserId = 0;
    std::string patientName = Trim(patientIdentifier);
    ResolveUserIdentity(patientIdentifier, "Patient", patientUserId, patientName);
    if (patientUserId <= 0) {
        errorMessage = "Patient must match an existing patient account.";
        return false;
    }

    std::string unusedFrequency;
    std::string unusedInstructions;
    return CreatePrescriptionDetailed(
        currentUser,
        static_cast<int>(patientUserId),
        medicine,
        dosage,
        unusedFrequency,
        unusedInstructions,
        errorMessage);
}

bool CaseRepository::SaveMedicalRecordDetailed(const User& currentUser,
                                               int patientUserId,
                                               const std::string& patientNameValue,
                                               const std::string& recordType,
                                               const std::string& note,
                                               std::string& errorMessage) {
    std::string patientName = Trim(patientNameValue);
    sqlite3_int64 resolvedPatientUserId = patientUserId;
    if (resolvedPatientUserId <= 0) {
        ResolveUserIdentity(patientNameValue, "Patient", resolvedPatientUserId, patientName);
    }

    if (resolvedPatientUserId > 0) {
        CleanupExpiredMedicalRecordLocks();

        sqlite3_stmt* lockCheck = nullptr;
        if (!database_.Prepare(
                "SELECT locked_by_user_id "
                "FROM medical_record_locks "
                "WHERE patient_user_id = ? "
                "LIMIT 1;",
                &lockCheck,
                errorMessage)) {
            return false;
        }

        sqlite3_bind_int64(lockCheck, 1, resolvedPatientUserId);
        if (sqlite3_step(lockCheck) != SQLITE_ROW || sqlite3_column_int(lockCheck, 0) != currentUser.id) {
            sqlite3_finalize(lockCheck);
            errorMessage = "This medical record is open in read-only mode. Open it for editing before saving.";
            return false;
        }
        sqlite3_finalize(lockCheck);
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO medical_records("
            "patient_user_id, patient_name, doctor_user_id, record_type, note"
            ") VALUES (?, ?, ?, ?, ?);",
            &statement,
            errorMessage)) {
        return false;
    }

    if (resolvedPatientUserId > 0) {
        sqlite3_bind_int64(statement, 1, resolvedPatientUserId);
    } else {
        sqlite3_bind_null(statement, 1);
    }
    BindText(statement, 2, patientName);
    sqlite3_bind_int(statement, 3, currentUser.id);
    BindText(statement, 4, Trim(recordType));
    BindText(statement, 5, Trim(note));
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
}

bool CaseRepository::LoadMedicalRecordSession(const User& currentUser,
                                              int patientUserId,
                                              MedicalRecordSession& session,
                                              std::string& errorMessage) {
    session = MedicalRecordSession{};
    if (patientUserId <= 0) {
        errorMessage = "Select a patient first.";
        return false;
    }

    CleanupExpiredMedicalRecordLocks();

    sqlite3_stmt* patientLookup = nullptr;
    if (!database_.Prepare(
            "SELECT full_name FROM users WHERE id = ? AND role = 'Patient' LIMIT 1;",
            &patientLookup,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(patientLookup, 1, patientUserId);
    if (sqlite3_step(patientLookup) != SQLITE_ROW) {
        sqlite3_finalize(patientLookup);
        errorMessage = "The selected patient could not be found.";
        return false;
    }

    const char* patientNameText = reinterpret_cast<const char*>(sqlite3_column_text(patientLookup, 0));
    const std::string patientName = patientNameText != nullptr ? patientNameText : "";
    sqlite3_finalize(patientLookup);

    sqlite3_stmt* lockLookup = nullptr;
    if (!database_.Prepare(
            "SELECT locked_by_user_id, locked_by_name "
            "FROM medical_record_locks "
            "WHERE patient_user_id = ? "
            "LIMIT 1;",
            &lockLookup,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(lockLookup, 1, patientUserId);
    const int lockResult = sqlite3_step(lockLookup);
    if (lockResult == SQLITE_ROW) {
        const int lockedByUserId = sqlite3_column_int(lockLookup, 0);
        const char* lockedByNameText = reinterpret_cast<const char*>(sqlite3_column_text(lockLookup, 1));
        session.lockOwner = lockedByNameText != nullptr ? lockedByNameText : "";
        sqlite3_finalize(lockLookup);

        if (lockedByUserId == currentUser.id) {
            sqlite3_stmt* extendLock = nullptr;
            if (!database_.Prepare(
                    "UPDATE medical_record_locks "
                    "SET expires_at = datetime('now', 'localtime', '+10 minutes') "
                    "WHERE patient_user_id = ? AND locked_by_user_id = ?;",
                    &extendLock,
                    errorMessage)) {
                return false;
            }

            sqlite3_bind_int(extendLock, 1, patientUserId);
            sqlite3_bind_int(extendLock, 2, currentUser.id);
            if (!ExecutePrepared(extendLock, database_.Handle(), errorMessage)) {
                return false;
            }
            session.lockHeld = true;
            session.statusMessage = "Record opened for editing.";
        } else {
            session.readOnly = true;
            session.statusMessage = "Opened in read-only mode. Locked by " + session.lockOwner + ".";
        }
    } else {
        sqlite3_finalize(lockLookup);

        sqlite3_stmt* insertLock = nullptr;
        if (!database_.Prepare(
                "INSERT INTO medical_record_locks(patient_user_id, locked_by_user_id, locked_by_name, expires_at) "
                "VALUES (?, ?, ?, datetime('now', 'localtime', '+10 minutes'));",
                &insertLock,
                errorMessage)) {
            return false;
        }

        sqlite3_bind_int(insertLock, 1, patientUserId);
        sqlite3_bind_int(insertLock, 2, currentUser.id);
        BindText(insertLock, 3, currentUser.fullName);
        if (!ExecutePrepared(insertLock, database_.Handle(), errorMessage)) {
            return false;
        }

        session.lockHeld = true;
        session.lockOwner = currentUser.fullName;
        session.statusMessage = "Record opened for editing.";
    }

    sqlite3_stmt* historyLookup = nullptr;
    if (!database_.Prepare(
            "SELECT medical_records.id, COALESCE(medical_records.record_type, ''), COALESCE(medical_records.note, ''), "
            "       COALESCE(medical_records.created_at, ''), COALESCE(users.full_name, 'Unknown doctor') "
            "FROM medical_records "
            "LEFT JOIN users ON users.id = medical_records.doctor_user_id "
            "WHERE medical_records.patient_user_id = ? "
            "ORDER BY medical_records.created_at DESC, medical_records.id DESC;",
            &historyLookup,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(historyLookup, 1, patientUserId);
    while (sqlite3_step(historyLookup) == SQLITE_ROW) {
        const int recordId = sqlite3_column_int(historyLookup, 0);
        const char* typeText = reinterpret_cast<const char*>(sqlite3_column_text(historyLookup, 1));
        const char* noteText = reinterpret_cast<const char*>(sqlite3_column_text(historyLookup, 2));
        const char* createdAtText = reinterpret_cast<const char*>(sqlite3_column_text(historyLookup, 3));
        const char* doctorText = reinterpret_cast<const char*>(sqlite3_column_text(historyLookup, 4));

        WorkflowItem item;
        item.payload = "record|" + std::to_string(recordId);
        item.displayText =
            std::string(createdAtText != nullptr ? createdAtText : "") + "  |  " +
            std::string(typeText != nullptr ? typeText : "") + "  |  " +
            std::string(doctorText != nullptr ? doctorText : "") + "  |  " +
            std::string(noteText != nullptr ? noteText : "");
        session.history.push_back(item);
    }
    sqlite3_finalize(historyLookup);

    if (session.history.empty()) {
        WorkflowItem item;
        item.payload = "record|none";
        item.displayText = patientName + "  |  No medical record history yet.";
        session.history.push_back(item);
    }

    return true;
}

bool CaseRepository::ReleaseMedicalRecordLock(const User& currentUser,
                                              int patientUserId,
                                              std::string& errorMessage) {
    if (patientUserId <= 0) {
        errorMessage = "Select a patient first.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "DELETE FROM medical_record_locks "
            "WHERE patient_user_id = ? AND locked_by_user_id = ?;",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(statement, 1, patientUserId);
    sqlite3_bind_int(statement, 2, currentUser.id);
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        return false;
    }

    if (sqlite3_changes(database_.Handle()) == 0) {
        errorMessage = "There is no editable lock to release for this record.";
        return false;
    }
    return true;
}

bool CaseRepository::SaveMedicalRecord(const User& currentUser,
                                       const std::string& patientIdentifier,
                                       const std::string& recordType,
                                       const std::string& note,
                                       std::string& errorMessage) {
    sqlite3_int64 patientUserId = 0;
    std::string patientName = Trim(patientIdentifier);
    ResolveUserIdentity(patientIdentifier, "Patient", patientUserId, patientName);
    return SaveMedicalRecordDetailed(
        currentUser,
        static_cast<int>(patientUserId),
        patientName,
        recordType,
        note,
        errorMessage);
}

bool CaseRepository::SaveMedicationDispensing(const User& currentUser,
                                              const std::string& patientIdentifier,
                                              const std::string& medicine,
                                              const std::string& quantityText,
                                              std::string& errorMessage) {
    int quantity = 0;
    if (!TryParseInteger(Trim(quantityText), quantity) || quantity <= 0) {
        errorMessage = "Quantity must be a positive whole number.";
        return false;
    }

    sqlite3_int64 patientUserId = 0;
    std::string patientName = Trim(patientIdentifier);
    ResolveUserIdentity(patientIdentifier, "Patient", patientUserId, patientName);
    if (patientUserId <= 0) {
        errorMessage = "Patient must match an existing patient account.";
        return false;
    }

    sqlite3_stmt* prescriptionCheck = nullptr;
    if (!database_.Prepare(
            "SELECT id "
            "FROM prescriptions "
            "WHERE patient_user_id = ? "
            "  AND medicine = ? COLLATE NOCASE "
            "  AND status = 'active' "
            "LIMIT 1;",
            &prescriptionCheck,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int64(prescriptionCheck, 1, patientUserId);
    BindText(prescriptionCheck, 2, Trim(medicine));
    if (sqlite3_step(prescriptionCheck) != SQLITE_ROW) {
        sqlite3_finalize(prescriptionCheck);
        errorMessage = "No active prescription for this medicine was found for the selected patient.";
        return false;
    }
    sqlite3_finalize(prescriptionCheck);

    sqlite3_stmt* stockCheck = nullptr;
    if (!database_.Prepare(
            "SELECT quantity FROM inventory_items WHERE item_name = ? COLLATE NOCASE LIMIT 1;",
            &stockCheck,
            errorMessage)) {
        return false;
    }

    BindText(stockCheck, 1, Trim(medicine));
    const int stockResult = sqlite3_step(stockCheck);
    if (stockResult != SQLITE_ROW) {
        sqlite3_finalize(stockCheck);
        errorMessage = "This medicine is not currently registered in inventory.";
        return false;
    }

    const int availableQuantity = sqlite3_column_int(stockCheck, 0);
    sqlite3_finalize(stockCheck);
    if (availableQuantity < quantity) {
        errorMessage = "There is not enough stock to complete this dispensing.";
        return false;
    }

    const int remainingQuantity = availableQuantity - quantity;
    if (!database_.Execute("BEGIN TRANSACTION;", errorMessage)) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO medication_dispensing("
            "patient_user_id, patient_name, pharmacist_user_id, medicine, quantity, status"
            ") VALUES (?, ?, ?, ?, ?, 'completed');",
            &statement,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    if (patientUserId > 0) {
        sqlite3_bind_int64(statement, 1, patientUserId);
    } else {
        sqlite3_bind_null(statement, 1);
    }
    BindText(statement, 2, patientName);
    sqlite3_bind_int(statement, 3, currentUser.id);
    BindText(statement, 4, Trim(medicine));
    sqlite3_bind_int(statement, 5, quantity);
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_stmt* stockUpdate = nullptr;
    if (!database_.Prepare(
            "UPDATE inventory_items "
            "SET quantity = ?, status = CASE WHEN ? = 0 THEN 'low' ELSE 'available' END "
            "WHERE item_name = ? COLLATE NOCASE;",
            &stockUpdate,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_bind_int(stockUpdate, 1, remainingQuantity);
    sqlite3_bind_int(stockUpdate, 2, remainingQuantity);
    BindText(stockUpdate, 3, Trim(medicine));
    if (!ExecutePrepared(stockUpdate, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    if (remainingQuantity == 0) {
        sqlite3_stmt* alertInsert = nullptr;
        if (!database_.Prepare(
                "INSERT INTO inventory_alerts(item_name, alert_level, note, created_by_user_id) "
                "VALUES (?, 'critical', 'Stock depleted during dispensing.', ?);",
                &alertInsert,
                errorMessage)) {
            database_.Execute("ROLLBACK;", errorMessage);
            return false;
        }

        BindText(alertInsert, 1, Trim(medicine));
        sqlite3_bind_int(alertInsert, 2, currentUser.id);
        if (!ExecutePrepared(alertInsert, database_.Handle(), errorMessage)) {
            database_.Execute("ROLLBACK;", errorMessage);
            return false;
        }
    }

    return database_.Execute("COMMIT;", errorMessage);
}

bool CaseRepository::SavePharmacyReviewDetailed(const User& currentUser,
                                                int patientUserId,
                                                int prescriptionId,
                                                const std::string& status,
                                                std::string& summaryMessage,
                                                std::string& errorMessage) {
    if (patientUserId <= 0) {
        errorMessage = "Select a patient first.";
        return false;
    }
    if (prescriptionId <= 0) {
        errorMessage = "Select a prescription first.";
        return false;
    }

    sqlite3_stmt* lookup = nullptr;
    if (!database_.Prepare(
            "SELECT patient_name "
            "FROM prescriptions "
            "WHERE id = ? AND patient_user_id = ? "
            "LIMIT 1;",
            &lookup,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(lookup, 1, prescriptionId);
    sqlite3_bind_int(lookup, 2, patientUserId);
    if (sqlite3_step(lookup) != SQLITE_ROW) {
        sqlite3_finalize(lookup);
        errorMessage = "The selected prescription does not belong to the selected patient.";
        return false;
    }

    const char* patientNameText = reinterpret_cast<const char*>(sqlite3_column_text(lookup, 0));
    const std::string patientName = patientNameText != nullptr ? patientNameText : "";
    sqlite3_finalize(lookup);

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO pharmacy_patient_reviews("
            "patient_user_id, patient_name, pharmacist_user_id, prescription_reference, review_status"
            ") VALUES (?, ?, ?, ?, ?);",
            &statement,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(statement, 1, patientUserId);
    BindText(statement, 2, patientName);
    sqlite3_bind_int(statement, 3, currentUser.id);
    BindText(statement, 4, std::to_string(prescriptionId));
    BindText(statement, 5, Trim(status));
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        return false;
    }

    summaryMessage = "Pharmacy review saved for Rx #" + std::to_string(prescriptionId) + ".";
    return true;
}

bool CaseRepository::SaveInventorySnapshot(const User&,
                                           const std::string& itemName,
                                           const std::string& quantityText,
                                           const std::string& location,
                                           std::string& errorMessage) {
    int quantity = 0;
    if (!TryParseInteger(Trim(quantityText), quantity) || quantity < 0) {
        errorMessage = "Available quantity must be a non-negative whole number.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO inventory_items(item_name, category, quantity, location, status) "
            "VALUES (?, 'General', ?, ?, CASE WHEN ? = 0 THEN 'low' ELSE 'available' END) "
            "ON CONFLICT(item_name) DO UPDATE SET "
            "quantity = excluded.quantity, "
            "location = excluded.location, "
            "status = CASE WHEN excluded.quantity = 0 THEN 'low' ELSE 'available' END;",
            &statement,
            errorMessage)) {
        return false;
    }

    BindText(statement, 1, Trim(itemName));
    sqlite3_bind_int(statement, 2, quantity);
    BindText(statement, 3, Trim(location));
    sqlite3_bind_int(statement, 4, quantity);
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
}

bool CaseRepository::SavePharmacyReview(const User& currentUser,
                                        const std::string& patientIdentifier,
                                        const std::string& prescriptionReference,
                                        const std::string& status,
                                        std::string& errorMessage) {
    int prescriptionId = 0;
    if (!TryParseInteger(Trim(prescriptionReference), prescriptionId) || prescriptionId <= 0) {
        errorMessage = "Prescription ID must be a positive whole number.";
        return false;
    }

    sqlite3_int64 requestedPatientUserId = 0;
    std::string requestedPatientName = Trim(patientIdentifier);
    ResolveUserIdentity(patientIdentifier, "Patient", requestedPatientUserId, requestedPatientName);

    sqlite3_stmt* prescriptionLookup = nullptr;
    if (!database_.Prepare(
            "SELECT patient_user_id, patient_name "
            "FROM prescriptions "
            "WHERE id = ? "
            "LIMIT 1;",
            &prescriptionLookup,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int(prescriptionLookup, 1, prescriptionId);
    if (sqlite3_step(prescriptionLookup) != SQLITE_ROW) {
        sqlite3_finalize(prescriptionLookup);
        errorMessage = "Prescription ID was not found.";
        return false;
    }

    const sqlite3_int64 prescriptionPatientUserId = sqlite3_column_type(prescriptionLookup, 0) == SQLITE_NULL
        ? 0
        : sqlite3_column_int64(prescriptionLookup, 0);
    const char* resolvedPatientNameText = reinterpret_cast<const char*>(sqlite3_column_text(prescriptionLookup, 1));
    const std::string prescriptionPatientName = resolvedPatientNameText != nullptr ? resolvedPatientNameText : requestedPatientName;
    sqlite3_finalize(prescriptionLookup);

    if (requestedPatientUserId > 0 && prescriptionPatientUserId > 0 && requestedPatientUserId != prescriptionPatientUserId) {
        errorMessage = "The selected patient does not match this prescription.";
        return false;
    }

    const sqlite3_int64 patientUserId = prescriptionPatientUserId > 0 ? prescriptionPatientUserId : requestedPatientUserId;
    if (patientUserId <= 0) {
        errorMessage = "The selected prescription is not linked to a patient.";
        return false;
    }

    std::string summaryMessage;
    return SavePharmacyReviewDetailed(
        currentUser,
        static_cast<int>(patientUserId),
        prescriptionId,
        status,
        summaryMessage,
        errorMessage);
}

bool CaseRepository::SaveBill(const User& currentUser,
                              const std::string& patientIdentifier,
                              const std::string& service,
                              const std::string& amountText,
                              std::string& errorMessage) {
    double amount = 0.0;
    if (!TryParseDouble(Trim(amountText), amount) || amount <= 0.0) {
        errorMessage = "Amount must be a positive number.";
        return false;
    }

    sqlite3_int64 patientUserId = 0;
    std::string patientName = Trim(patientIdentifier);
    ResolveUserIdentity(patientIdentifier, "Patient", patientUserId, patientName);
    if (patientUserId <= 0) {
        errorMessage = "Patient must match an existing patient account.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO bills("
            "patient_user_id, patient_name, service_name, amount, status, created_by_user_id"
            ") VALUES (?, ?, ?, ?, 'unpaid', ?);",
            &statement,
            errorMessage)) {
        return false;
    }

    if (patientUserId > 0) {
        sqlite3_bind_int64(statement, 1, patientUserId);
    } else {
        sqlite3_bind_null(statement, 1);
    }
    BindText(statement, 2, patientName);
    BindText(statement, 3, Trim(service));
    sqlite3_bind_double(statement, 4, amount);
    sqlite3_bind_int(statement, 5, currentUser.id);
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
}

SurgeryPlan CaseRepository::BuildSurgeryPlan(int patientUserId,
                                             const std::string& surgeryDate,
                                             const std::string& surgeryTime,
                                             const std::string& doctorCategory,
                                             const std::string& operatingRoom,
                                             std::string& errorMessage) const {
    SurgeryPlan plan;
    plan.patientUserId = patientUserId;
    plan.surgeryDate = Trim(surgeryDate);
    plan.surgeryTime = Trim(surgeryTime);
    plan.doctorCategory = Trim(doctorCategory);
    plan.roomName = Trim(operatingRoom);
    plan.durationMinutes = SurgeryDurationMinutesFor(plan.doctorCategory);

    if (plan.patientUserId <= 0) {
        errorMessage = "Select a patient first.";
        return plan;
    }
    if (plan.surgeryDate.empty() || plan.surgeryTime.empty() || plan.doctorCategory.empty() || plan.roomName.empty()) {
        errorMessage = "Date, time, doctor category, and room are required.";
        return plan;
    }

    sqlite3_stmt* patientLookup = nullptr;
    if (!database_.Prepare(
            "SELECT full_name FROM users WHERE id = ? AND role = 'Patient' LIMIT 1;",
            &patientLookup,
            errorMessage)) {
        return plan;
    }

    sqlite3_bind_int(patientLookup, 1, patientUserId);
    if (sqlite3_step(patientLookup) != SQLITE_ROW) {
        sqlite3_finalize(patientLookup);
        errorMessage = "The selected patient could not be found.";
        return plan;
    }
    const char* patientNameText = reinterpret_cast<const char*>(sqlite3_column_text(patientLookup, 0));
    plan.patientName = patientNameText != nullptr ? patientNameText : "";
    sqlite3_finalize(patientLookup);

    std::string rejectionReason;

    sqlite3_stmt* duplicateCheck = nullptr;
    if (!database_.Prepare(
            "SELECT 1 "
            "FROM surgeries "
            "WHERE patient_user_id = ? "
            "  AND surgery_date = ? "
            "  AND status = 'scheduled' "
            "LIMIT 1;",
            &duplicateCheck,
            errorMessage)) {
        return plan;
    }
    sqlite3_bind_int(duplicateCheck, 1, patientUserId);
    BindText(duplicateCheck, 2, plan.surgeryDate);
    if (sqlite3_step(duplicateCheck) == SQLITE_ROW) {
        rejectionReason = "This patient already has a scheduled surgery on that date.";
    }
    sqlite3_finalize(duplicateCheck);

    if (rejectionReason.empty()) {
        sqlite3_stmt* roomCheck = nullptr;
        if (!database_.Prepare(
                "SELECT COALESCE(surgery_time, ''), COALESCE(duration_minutes, 120) "
                "FROM surgeries "
                "WHERE operating_room = ? "
                "  AND surgery_date = ? "
                "  AND status = 'scheduled';",
                &roomCheck,
                errorMessage)) {
            return plan;
        }

        BindText(roomCheck, 1, plan.roomName);
        BindText(roomCheck, 2, plan.surgeryDate);
        while (sqlite3_step(roomCheck) == SQLITE_ROW) {
            const char* otherTimeText = reinterpret_cast<const char*>(sqlite3_column_text(roomCheck, 0));
            const int otherDuration = sqlite3_column_int(roomCheck, 1);
            if (otherTimeText != nullptr &&
                TimeRangesOverlap(plan.surgeryDate, plan.surgeryTime, plan.durationMinutes,
                                  plan.surgeryDate, otherTimeText, otherDuration)) {
                rejectionReason = "The requested operating room is not available at that time.";
                break;
            }
        }
        sqlite3_finalize(roomCheck);
    }

    if (rejectionReason.empty()) {
        sqlite3_stmt* surgeonLookup = nullptr;
        if (!database_.Prepare(
                "SELECT users.id, users.full_name, COALESCE(staff_profiles.specialization, '') "
                "FROM users "
                "LEFT JOIN staff_profiles ON staff_profiles.user_id = users.id "
                "WHERE users.role = 'Doctor' "
                "ORDER BY users.full_name ASC;",
                &surgeonLookup,
                errorMessage)) {
            return plan;
        }

        while (sqlite3_step(surgeonLookup) == SQLITE_ROW) {
            const int surgeonUserId = sqlite3_column_int(surgeonLookup, 0);
            const char* surgeonNameText = reinterpret_cast<const char*>(sqlite3_column_text(surgeonLookup, 1));
            const char* specializationText = reinterpret_cast<const char*>(sqlite3_column_text(surgeonLookup, 2));
            const std::string surgeonName = surgeonNameText != nullptr ? surgeonNameText : "";
            const std::string specialization = specializationText != nullptr ? specializationText : "";
            if (!DoctorCategoryMatches(specialization, plan.doctorCategory)) {
                continue;
            }

            bool available = true;
            sqlite3_stmt* surgeonConflict = nullptr;
            if (!database_.Prepare(
                    "SELECT COALESCE(surgery_time, ''), COALESCE(duration_minutes, 120) "
                    "FROM surgeries "
                    "WHERE surgeon_user_id = ? "
                    "  AND surgery_date = ? "
                    "  AND status = 'scheduled';",
                    &surgeonConflict,
                    errorMessage)) {
                sqlite3_finalize(surgeonLookup);
                return plan;
            }

            sqlite3_bind_int(surgeonConflict, 1, surgeonUserId);
            BindText(surgeonConflict, 2, plan.surgeryDate);
            while (sqlite3_step(surgeonConflict) == SQLITE_ROW) {
                const char* otherTimeText = reinterpret_cast<const char*>(sqlite3_column_text(surgeonConflict, 0));
                const int otherDuration = sqlite3_column_int(surgeonConflict, 1);
                if (otherTimeText != nullptr &&
                    TimeRangesOverlap(plan.surgeryDate, plan.surgeryTime, plan.durationMinutes,
                                      plan.surgeryDate, otherTimeText, otherDuration)) {
                    available = false;
                    break;
                }
            }
            sqlite3_finalize(surgeonConflict);

            if (available) {
                plan.surgeonUserId = surgeonUserId;
                plan.surgeonName = surgeonName;
                break;
            }
        }
        sqlite3_finalize(surgeonLookup);

        if (plan.surgeonUserId <= 0) {
            rejectionReason = "No available doctor matched the requested category for that time.";
        }
    }

    if (rejectionReason.empty()) {
        sqlite3_stmt* nurseLookup = nullptr;
        if (!database_.Prepare(
                "SELECT COALESCE(users.full_name, ''), COALESCE(staff_profiles.department, ''), COALESCE(staff_profiles.role_name, '') "
                "FROM staff_profiles "
                "LEFT JOIN users ON users.id = staff_profiles.user_id "
                "WHERE lower(COALESCE(staff_profiles.role_name, '')) LIKE '%nurse%' "
                "ORDER BY users.full_name ASC;",
                &nurseLookup,
                errorMessage)) {
            return plan;
        }

        while (sqlite3_step(nurseLookup) == SQLITE_ROW) {
            const char* nurseNameText = reinterpret_cast<const char*>(sqlite3_column_text(nurseLookup, 0));
            const char* departmentText = reinterpret_cast<const char*>(sqlite3_column_text(nurseLookup, 1));
            const std::string nurseName = nurseNameText != nullptr ? nurseNameText : "";
            const std::string department = departmentText != nullptr ? departmentText : "";
            if (nurseName.empty()) {
                continue;
            }

            bool onShift = department.empty();
            sqlite3_stmt* shiftLookup = nullptr;
            if (!database_.Prepare(
                    "SELECT COALESCE(start_time, ''), COALESCE(end_time, '') "
                    "FROM hr_shifts "
                    "WHERE employee_name = ? COLLATE NOCASE "
                    "  AND shift_date = ?;",
                    &shiftLookup,
                    errorMessage)) {
                sqlite3_finalize(nurseLookup);
                return plan;
            }

            BindText(shiftLookup, 1, nurseName);
            BindText(shiftLookup, 2, plan.surgeryDate);
            while (sqlite3_step(shiftLookup) == SQLITE_ROW) {
                const char* startText = reinterpret_cast<const char*>(sqlite3_column_text(shiftLookup, 0));
                const char* endText = reinterpret_cast<const char*>(sqlite3_column_text(shiftLookup, 1));
                if (startText == nullptr || endText == nullptr || std::strlen(startText) == 0 || std::strlen(endText) == 0) {
                    onShift = true;
                    break;
                }

                std::time_t shiftStart = {};
                std::time_t shiftEnd = {};
                std::time_t surgeryStart = {};
                if (!ParseDateTime(plan.surgeryDate, startText, shiftStart, false) ||
                    !ParseDateTime(plan.surgeryDate, endText, shiftEnd, false) ||
                    !ParseDateTime(plan.surgeryDate, plan.surgeryTime, surgeryStart, false)) {
                    continue;
                }
                if (shiftEnd <= shiftStart) {
                    shiftEnd += 24 * 60 * 60;
                }
                const std::time_t surgeryEnd = surgeryStart + static_cast<std::time_t>(plan.durationMinutes) * 60;
                if (shiftStart <= surgeryStart && shiftEnd >= surgeryEnd) {
                    onShift = true;
                    break;
                }
            }
            sqlite3_finalize(shiftLookup);

            if (!onShift && !department.empty() && ToLower(department).find("surgery") == std::string::npos) {
                continue;
            }

            bool available = true;
            sqlite3_stmt* nurseConflict = nullptr;
            if (!database_.Prepare(
                    "SELECT COALESCE(surgery_time, ''), COALESCE(duration_minutes, 120) "
                    "FROM surgeries "
                    "WHERE nurse_name = ? COLLATE NOCASE "
                    "  AND surgery_date = ? "
                    "  AND status = 'scheduled';",
                    &nurseConflict,
                    errorMessage)) {
                sqlite3_finalize(nurseLookup);
                return plan;
            }

            BindText(nurseConflict, 1, nurseName);
            BindText(nurseConflict, 2, plan.surgeryDate);
            while (sqlite3_step(nurseConflict) == SQLITE_ROW) {
                const char* otherTimeText = reinterpret_cast<const char*>(sqlite3_column_text(nurseConflict, 0));
                const int otherDuration = sqlite3_column_int(nurseConflict, 1);
                if (otherTimeText != nullptr &&
                    TimeRangesOverlap(plan.surgeryDate, plan.surgeryTime, plan.durationMinutes,
                                      plan.surgeryDate, otherTimeText, otherDuration)) {
                    available = false;
                    break;
                }
            }
            sqlite3_finalize(nurseConflict);

            if (available) {
                plan.nurseName = nurseName;
                break;
            }
        }
        sqlite3_finalize(nurseLookup);

        if (plan.nurseName.empty()) {
            rejectionReason = "No on-shift nurse was available for the requested surgery time.";
        }
    }

    plan.rejected = !rejectionReason.empty();
    plan.approved = !plan.rejected;
    plan.rejectionReason = rejectionReason;

    auto appendLine = [&](const std::string& label, const std::string& value) {
        WorkflowItem item;
        item.payload = label;
        item.displayText = label + "  |  " + value;
        plan.lines.push_back(item);
    };

    appendLine("Patient", plan.patientName);
    appendLine("Date", plan.surgeryDate);
    appendLine("Time", plan.surgeryTime);
    appendLine("Category", plan.doctorCategory);
    appendLine("Duration", std::to_string(plan.durationMinutes) + " minutes");
    appendLine("Room", plan.roomName);
    if (plan.approved) {
        appendLine("Assigned surgeon", plan.surgeonName);
        appendLine("Assigned nurse", plan.nurseName);
        appendLine("Plan status", "Ready to schedule");
    } else {
        appendLine("Plan status", "Rejected");
        appendLine("Reason", plan.rejectionReason);
    }

    return plan;
}

bool CaseRepository::ConfirmSurgeryPlan(const User& currentUser,
                                        const SurgeryPlan& requestedPlan,
                                        std::string& summaryMessage,
                                        std::string& errorMessage) {
    SurgeryPlan plan = BuildSurgeryPlan(
        requestedPlan.patientUserId,
        requestedPlan.surgeryDate,
        requestedPlan.surgeryTime,
        requestedPlan.doctorCategory,
        requestedPlan.roomName,
        errorMessage);
    if (!errorMessage.empty()) {
        return false;
    }

    if (!database_.Execute("BEGIN TRANSACTION;", errorMessage)) {
        return false;
    }

    sqlite3_stmt* roomUpsert = nullptr;
    if (!database_.Prepare(
            "INSERT INTO operating_rooms(room_name, status) "
            "VALUES (?, 'available') "
            "ON CONFLICT(room_name) DO NOTHING;",
            &roomUpsert,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }
    BindText(roomUpsert, 1, plan.roomName);
    if (!ExecutePrepared(roomUpsert, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO surgeries("
            "patient_user_id, patient_name, surgery_date, surgery_time, duration_minutes, doctor_category, "
            "surgeon_user_id, surgeon_name, nurse_name, operating_room, status, rejection_reason, created_by_user_id"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
            &statement,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_bind_int(statement, 1, plan.patientUserId);
    BindText(statement, 2, plan.patientName);
    BindText(statement, 3, plan.surgeryDate);
    BindText(statement, 4, plan.surgeryTime);
    sqlite3_bind_int(statement, 5, plan.durationMinutes);
    BindText(statement, 6, plan.doctorCategory);
    if (plan.approved && plan.surgeonUserId > 0) {
        sqlite3_bind_int(statement, 7, plan.surgeonUserId);
    } else {
        sqlite3_bind_null(statement, 7);
    }
    BindText(statement, 8, plan.surgeonName);
    BindText(statement, 9, plan.nurseName);
    BindText(statement, 10, plan.roomName);
    BindText(statement, 11, plan.approved ? "scheduled" : "rejected");
    if (plan.rejected) {
        BindText(statement, 12, plan.rejectionReason);
    } else {
        sqlite3_bind_null(statement, 12);
    }
    sqlite3_bind_int(statement, 13, currentUser.id);
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    const int surgeryId = static_cast<int>(database_.LastInsertRowId());
    if (plan.approved) {
        sqlite3_stmt* roomUpdate = nullptr;
        if (!database_.Prepare(
                "UPDATE operating_rooms "
                "SET status = 'reserved', reserved_for_surgery_id = ?, updated_at = CURRENT_TIMESTAMP "
                "WHERE room_name = ?;",
                &roomUpdate,
                errorMessage)) {
            database_.Execute("ROLLBACK;", errorMessage);
            return false;
        }

        sqlite3_bind_int(roomUpdate, 1, surgeryId);
        BindText(roomUpdate, 2, plan.roomName);
        if (!ExecutePrepared(roomUpdate, database_.Handle(), errorMessage)) {
            database_.Execute("ROLLBACK;", errorMessage);
            return false;
        }
    }

    if (!database_.Execute("COMMIT;", errorMessage)) {
        return false;
    }

    if (plan.approved) {
        summaryMessage =
            "Surgery scheduled for " + plan.patientName +
            " with " + plan.surgeonName +
            " and nurse " + plan.nurseName + ".";
    } else {
        summaryMessage = "Surgery request rejected and logged: " + plan.rejectionReason;
    }
    return true;
}

bool CaseRepository::SaveSurgery(const User& currentUser,
                                 const std::string& patientIdentifier,
                                 const std::string& surgeryDate,
                                 const std::string& operatingRoom,
                                 std::string& errorMessage) {
    sqlite3_int64 patientUserId = 0;
    std::string patientName = Trim(patientIdentifier);
    ResolveUserIdentity(patientIdentifier, "Patient", patientUserId, patientName);
    if (patientUserId <= 0) {
        errorMessage = "Patient must match an existing patient account.";
        return false;
    }

    SurgeryPlan plan = BuildSurgeryPlan(
        static_cast<int>(patientUserId),
        surgeryDate,
        "09:00",
        "General Surgery",
        operatingRoom,
        errorMessage);
    if (!errorMessage.empty()) {
        return false;
    }

    std::string summaryMessage;
    return ConfirmSurgeryPlan(currentUser, plan, summaryMessage, errorMessage);
}

bool CaseRepository::SavePatientSupportTask(const User& currentUser,
                                            const std::string& patientIdentifier,
                                            const std::string& taskType,
                                            const std::string& note,
                                            std::string& errorMessage) {
    sqlite3_int64 patientUserId = 0;
    std::string patientName = Trim(patientIdentifier);
    ResolveUserIdentity(patientIdentifier, "Patient", patientUserId, patientName);
    if (patientUserId <= 0) {
        errorMessage = "Patient must match an existing patient account.";
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO patient_support_tasks("
            "patient_user_id, patient_name, task_type, note, created_by_user_id"
            ") VALUES (?, ?, ?, ?, ?);",
            &statement,
            errorMessage)) {
        return false;
    }

    if (patientUserId > 0) {
        sqlite3_bind_int64(statement, 1, patientUserId);
    } else {
        sqlite3_bind_null(statement, 1);
    }
    BindText(statement, 2, patientName);
    BindText(statement, 3, Trim(taskType));
    BindText(statement, 4, Trim(note));
    sqlite3_bind_int(statement, 5, currentUser.id);
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
}

std::vector<WorkflowItem> CaseRepository::SearchInventoryItems(const std::string& filter) const {
    std::vector<WorkflowItem> items;
    if (!IsAvailable()) {
        return items;
    }

    sqlite3_stmt* statement = nullptr;
    std::string errorMessage;
    if (!database_.Prepare(
            "SELECT id, item_name, category, quantity, COALESCE(location, ''), status "
            "FROM inventory_items "
            "WHERE (? = '' OR item_name LIKE '%' || ? || '%' COLLATE NOCASE "
            "       OR category LIKE '%' || ? || '%' COLLATE NOCASE "
            "       OR location LIKE '%' || ? || '%' COLLATE NOCASE) "
            "ORDER BY item_name ASC;",
            &statement,
            errorMessage)) {
        return items;
    }

    const std::string search = Trim(filter);
    BindText(statement, 1, search);
    BindText(statement, 2, search);
    BindText(statement, 3, search);
    BindText(statement, 4, search);
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const int itemId = sqlite3_column_int(statement, 0);
        const char* itemNameText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        const char* categoryText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 2));
        const int quantity = sqlite3_column_int(statement, 3);
        const char* locationText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));
        const char* statusText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 5));

        WorkflowItem item;
        item.payload =
            "inventory|" + std::to_string(itemId) + "|" +
            (itemNameText != nullptr ? itemNameText : "") + "|" +
            (categoryText != nullptr ? categoryText : "") + "|" +
            std::to_string(quantity) + "|" +
            (locationText != nullptr ? locationText : "") + "|" +
            (statusText != nullptr ? statusText : "");
        item.displayText =
            std::string(itemNameText != nullptr ? itemNameText : "") + "  |  " +
            std::string(categoryText != nullptr ? categoryText : "") + "  |  qty " +
            std::to_string(quantity) + "  |  " +
            std::string(locationText != nullptr ? locationText : "") + "  |  " +
            std::string(statusText != nullptr ? statusText : "");
        items.push_back(item);
    }

    sqlite3_finalize(statement);
    return items;
}

std::vector<WorkflowItem> CaseRepository::FindInventoryOrderAlternatives(const std::string& itemName,
                                                                         const std::string& supplier) const {
    std::vector<WorkflowItem> items;
    if (!IsAvailable() || Trim(itemName).empty()) {
        return items;
    }

    std::string category;
    sqlite3_stmt* categoryLookup = nullptr;
    std::string errorMessage;
    if (database_.Prepare(
            "SELECT COALESCE(category, '') "
            "FROM inventory_items "
            "WHERE item_name = ? COLLATE NOCASE "
            "LIMIT 1;",
            &categoryLookup,
            errorMessage)) {
        BindText(categoryLookup, 1, Trim(itemName));
        if (sqlite3_step(categoryLookup) == SQLITE_ROW) {
            const char* categoryText = reinterpret_cast<const char*>(sqlite3_column_text(categoryLookup, 0));
            category = categoryText != nullptr ? categoryText : "";
        }
        sqlite3_finalize(categoryLookup);
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "SELECT item_name, category, quantity, COALESCE(location, ''), status "
            "FROM inventory_items "
            "WHERE item_name <> ? COLLATE NOCASE "
            "  AND ((? <> '' AND category = ? COLLATE NOCASE) "
            "       OR item_name LIKE '%' || ? || '%' COLLATE NOCASE) "
            "ORDER BY quantity DESC, item_name ASC "
            "LIMIT 8;",
            &statement,
            errorMessage)) {
        return items;
    }

    const std::string searchToken = FirstToken(itemName);
    BindText(statement, 1, Trim(itemName));
    BindText(statement, 2, category);
    BindText(statement, 3, category);
    BindText(statement, 4, searchToken);
    while (sqlite3_step(statement) == SQLITE_ROW) {
        const char* altNameText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 0));
        const char* categoryText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 1));
        const int quantity = sqlite3_column_int(statement, 2);
        const char* locationText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 3));
        const char* statusText = reinterpret_cast<const char*>(sqlite3_column_text(statement, 4));

        WorkflowItem item;
        item.payload =
            "alternative|" + std::string(altNameText != nullptr ? altNameText : "") + "|" +
            std::string(categoryText != nullptr ? categoryText : "") + "|" +
            std::to_string(quantity) + "|" +
            std::string(locationText != nullptr ? locationText : "") + "|" +
            std::string(statusText != nullptr ? statusText : "");
        item.displayText =
            std::string(altNameText != nullptr ? altNameText : "") + "  |  " +
            std::string(categoryText != nullptr ? categoryText : "") + "  |  qty " +
            std::to_string(quantity) + "  |  " +
            std::string(locationText != nullptr ? locationText : "") + "  |  " +
            std::string(statusText != nullptr ? statusText : "");
        items.push_back(item);
    }
    sqlite3_finalize(statement);

    if (items.empty() && !Trim(supplier).empty()) {
        WorkflowItem item;
        item.payload = "alternative|" + Trim(itemName) + "|supplier|" + Trim(supplier) + "|0|planned";
        item.displayText = Trim(itemName) + "  |  keep original request with supplier " + Trim(supplier);
        items.push_back(item);
    }

    return items;
}

InventoryOrderPreview CaseRepository::BuildInventoryOrderPreview(const std::string& itemName,
                                                                 const std::string& quantityText,
                                                                 const std::string& supplier,
                                                                 const std::string& alternativePayload,
                                                                 std::string& errorMessage) const {
    InventoryOrderPreview preview;
    preview.itemName = Trim(itemName);
    preview.supplier = Trim(supplier);
    if (preview.itemName.empty()) {
        errorMessage = "Choose an item first.";
        return preview;
    }
    if (!TryParseInteger(Trim(quantityText), preview.quantity) || preview.quantity <= 0) {
        errorMessage = "Quantity must be a positive whole number.";
        return preview;
    }

    std::string orderedItemName = preview.itemName;
    const std::vector<std::string> alternativeParts = SplitText(alternativePayload, '|');
    if (alternativeParts.size() >= 2 && alternativeParts[0] == "alternative") {
        orderedItemName = Trim(alternativeParts[1]);
        preview.chosenAlternative = orderedItemName;
        preview.hasAlternative = !orderedItemName.empty() && !TextContainsInsensitive(preview.itemName, orderedItemName);
    }

    int currentQuantity = 0;
    std::string location;
    std::string category = "General";
    sqlite3_stmt* stockLookup = nullptr;
    if (database_.Prepare(
            "SELECT COALESCE(category, 'General'), quantity, COALESCE(location, '') "
            "FROM inventory_items "
            "WHERE item_name = ? COLLATE NOCASE "
            "LIMIT 1;",
            &stockLookup,
            errorMessage)) {
        BindText(stockLookup, 1, orderedItemName);
        if (sqlite3_step(stockLookup) == SQLITE_ROW) {
            const char* categoryText = reinterpret_cast<const char*>(sqlite3_column_text(stockLookup, 0));
            const char* locationText = reinterpret_cast<const char*>(sqlite3_column_text(stockLookup, 2));
            category = categoryText != nullptr ? categoryText : "General";
            currentQuantity = sqlite3_column_int(stockLookup, 1);
            location = locationText != nullptr ? locationText : "";
        }
        sqlite3_finalize(stockLookup);
    } else {
        errorMessage.clear();
    }

    auto appendLine = [&](const std::string& label, const std::string& value) {
        WorkflowItem item;
        item.payload = label;
        item.displayText = label + "  |  " + value;
        preview.lines.push_back(item);
    };

    appendLine("Requested item", preview.itemName);
    appendLine("Ordered item", orderedItemName);
    appendLine("Quantity", std::to_string(preview.quantity));
    appendLine("Supplier", preview.supplier.empty() ? "Not specified" : preview.supplier);
    appendLine("Category", category);
    appendLine("Current stock", std::to_string(currentQuantity));
    if (!location.empty()) {
        appendLine("Location", location);
    }
    if (preview.hasAlternative) {
        appendLine("Alternative", "Using " + orderedItemName + " in place of " + preview.itemName);
    }

    return preview;
}

bool CaseRepository::SaveInventoryOrderDetailed(const User& currentUser,
                                                const std::string& itemName,
                                                const std::string& quantityText,
                                                const std::string& supplier,
                                                const std::string& alternativePayload,
                                                std::string& summaryMessage,
                                                std::string& errorMessage) {
    InventoryOrderPreview preview = BuildInventoryOrderPreview(itemName, quantityText, supplier, alternativePayload, errorMessage);
    if (!errorMessage.empty()) {
        return false;
    }

    const std::string orderedItemName = preview.hasAlternative ? preview.chosenAlternative : preview.itemName;
    if (!database_.Execute("BEGIN TRANSACTION;", errorMessage)) {
        return false;
    }

    sqlite3_stmt* itemUpsert = nullptr;
    if (!database_.Prepare(
            "INSERT INTO inventory_items(item_name, category, quantity, status) "
            "VALUES (?, 'General', 0, 'available') "
            "ON CONFLICT(item_name) DO NOTHING;",
            &itemUpsert,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    BindText(itemUpsert, 1, orderedItemName);
    if (!ExecutePrepared(itemUpsert, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    int itemId = 0;
    sqlite3_stmt* itemLookup = nullptr;
    if (!database_.Prepare(
            "SELECT id FROM inventory_items WHERE item_name = ? COLLATE NOCASE LIMIT 1;",
            &itemLookup,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    BindText(itemLookup, 1, orderedItemName);
    if (sqlite3_step(itemLookup) == SQLITE_ROW) {
        itemId = sqlite3_column_int(itemLookup, 0);
    }
    sqlite3_finalize(itemLookup);

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO inventory_orders("
            "item_id, item_name, quantity, supplier, alternative_item_name, justification, status, created_by_user_id"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?);",
            &statement,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    if (itemId > 0) {
        sqlite3_bind_int(statement, 1, itemId);
    } else {
        sqlite3_bind_null(statement, 1);
    }
    BindText(statement, 2, preview.itemName);
    sqlite3_bind_int(statement, 3, preview.quantity);
    BindText(statement, 4, preview.supplier);
    if (preview.hasAlternative) {
        BindText(statement, 5, preview.chosenAlternative);
    } else {
        sqlite3_bind_null(statement, 5);
    }
    BindText(statement, 6, preview.hasAlternative
        ? "Alternative item selected after catalog review."
        : "Confirmed after inventory order preview.");
    BindText(statement, 7, preview.hasAlternative ? "alternative-ordered" : "ordered");
    sqlite3_bind_int(statement, 8, currentUser.id);
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    if (!database_.Execute("COMMIT;", errorMessage)) {
        return false;
    }

    summaryMessage =
        "Inventory order confirmed for " + orderedItemName +
        " x" + std::to_string(preview.quantity) +
        (preview.supplier.empty() ? "." : " from " + preview.supplier + ".");
    return true;
}

bool CaseRepository::SaveInventoryOrder(const User& currentUser,
                                        const std::string& itemName,
                                        const std::string& quantityText,
                                        const std::string& supplier,
                                        std::string& errorMessage) {
    std::string summaryMessage;
    return SaveInventoryOrderDetailed(
        currentUser,
        itemName,
        quantityText,
        supplier,
        "",
        summaryMessage,
        errorMessage);
}

bool CaseRepository::SaveInventoryAlert(const User& currentUser,
                                        const std::string& itemName,
                                        const std::string& alertLevel,
                                        const std::string& note,
                                        std::string& errorMessage) {
    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO inventory_alerts("
            "item_name, alert_level, note, created_by_user_id"
            ") VALUES (?, ?, ?, ?);",
            &statement,
            errorMessage)) {
        return false;
    }

    BindText(statement, 1, Trim(itemName));
    BindText(statement, 2, Trim(alertLevel));
    BindText(statement, 3, Trim(note));
    sqlite3_bind_int(statement, 4, currentUser.id);
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
}

bool CaseRepository::SaveHrShift(const User& currentUser,
                                 const std::string& employeeName,
                                 const std::string& shiftDate,
                                 const std::string& department,
                                 std::string& errorMessage) {
    sqlite3_stmt* duplicateCheck = nullptr;
    if (!database_.Prepare(
            "SELECT 1 "
            "FROM hr_shifts "
            "WHERE employee_name = ? COLLATE NOCASE "
            "  AND shift_date = ? "
            "LIMIT 1;",
            &duplicateCheck,
            errorMessage)) {
        return false;
    }

    BindText(duplicateCheck, 1, Trim(employeeName));
    BindText(duplicateCheck, 2, Trim(shiftDate));
    if (sqlite3_step(duplicateCheck) == SQLITE_ROW) {
        sqlite3_finalize(duplicateCheck);

        sqlite3_stmt* conflictInsert = nullptr;
        if (database_.Prepare(
                "INSERT INTO hr_conflicts(employee_name, conflict_date, issue, created_by_user_id) "
                "VALUES (?, ?, 'Duplicate shift request for the same day.', ?);",
                &conflictInsert,
                errorMessage)) {
            BindText(conflictInsert, 1, Trim(employeeName));
            BindText(conflictInsert, 2, Trim(shiftDate));
            sqlite3_bind_int(conflictInsert, 3, currentUser.id);
            ExecutePrepared(conflictInsert, database_.Handle(), errorMessage);
        }

        errorMessage = "This employee already has a shift on that date.";
        return false;
    }
    sqlite3_finalize(duplicateCheck);

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO hr_shifts(employee_name, shift_date, department, created_by_user_id) "
            "VALUES (?, ?, ?, ?);",
            &statement,
            errorMessage)) {
        return false;
    }

    BindText(statement, 1, Trim(employeeName));
    BindText(statement, 2, Trim(shiftDate));
    BindText(statement, 3, Trim(department));
    sqlite3_bind_int(statement, 4, currentUser.id);
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
}

bool CaseRepository::SaveHrStaffUpdate(const User& currentUser,
                                       const std::string& employeeName,
                                       const std::string& roleName,
                                       const std::string& department,
                                       std::string& errorMessage) {
    if (!database_.Execute("BEGIN TRANSACTION;", errorMessage)) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO hr_staff_updates(employee_name, role_name, department, created_by_user_id) "
            "VALUES (?, ?, ?, ?);",
            &statement,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    BindText(statement, 1, Trim(employeeName));
    BindText(statement, 2, Trim(roleName));
    BindText(statement, 3, Trim(department));
    sqlite3_bind_int(statement, 4, currentUser.id);
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_stmt* userLookup = nullptr;
    if (!database_.Prepare(
            "SELECT id "
            "FROM users "
            "WHERE role <> 'Patient' "
            "  AND (username = ? COLLATE NOCASE OR full_name = ? COLLATE NOCASE) "
            "LIMIT 1;",
            &userLookup,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    BindText(userLookup, 1, Trim(employeeName));
    BindText(userLookup, 2, Trim(employeeName));
    const int lookupResult = sqlite3_step(userLookup);
    const sqlite3_int64 staffUserId = lookupResult == SQLITE_ROW ? sqlite3_column_int64(userLookup, 0) : 0;
    sqlite3_finalize(userLookup);

    if (staffUserId > 0) {
        sqlite3_stmt* profileUpdate = nullptr;
        if (!database_.Prepare(
                "UPDATE staff_profiles SET role_name = ?, department = ? WHERE user_id = ?;",
                &profileUpdate,
                errorMessage)) {
            database_.Execute("ROLLBACK;", errorMessage);
            return false;
        }

        BindText(profileUpdate, 1, Trim(roleName));
        BindText(profileUpdate, 2, Trim(department));
        sqlite3_bind_int64(profileUpdate, 3, staffUserId);
        if (!ExecutePrepared(profileUpdate, database_.Handle(), errorMessage)) {
            database_.Execute("ROLLBACK;", errorMessage);
            return false;
        }
    }

    return database_.Execute("COMMIT;", errorMessage);
}

bool CaseRepository::SaveHrConflict(const User& currentUser,
                                    const std::string& employeeName,
                                    const std::string& conflictDate,
                                    const std::string& issue,
                                    std::string& errorMessage) {
    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO hr_conflicts(employee_name, conflict_date, issue, created_by_user_id) "
            "VALUES (?, ?, ?, ?);",
            &statement,
            errorMessage)) {
        return false;
    }

    BindText(statement, 1, Trim(employeeName));
    BindText(statement, 2, Trim(conflictDate));
    BindText(statement, 3, Trim(issue));
    sqlite3_bind_int(statement, 4, currentUser.id);
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
}
