#include "CaseRepository.h"

#include "StringUtil.h"

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

    double amount = 0.0;
    if (!TryParseDouble(Trim(amountText), amount) || amount <= 0.0) {
        errorMessage = "Amount must be a positive number.";
        return false;
    }

    sqlite3_stmt* lookup = nullptr;
    if (!database_.Prepare(
            "SELECT status FROM bills WHERE id = ? AND patient_user_id = ? LIMIT 1;",
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

    const char* statusText = reinterpret_cast<const char*>(sqlite3_column_text(lookup, 0));
    if (statusText != nullptr && ToLower(statusText) == "paid") {
        sqlite3_finalize(lookup);
        errorMessage = "This bill is already marked as paid.";
        return false;
    }
    sqlite3_finalize(lookup);

    if (!database_.Execute("BEGIN TRANSACTION;", errorMessage)) {
        return false;
    }

    sqlite3_stmt* insertPayment = nullptr;
    if (!database_.Prepare(
            "INSERT INTO payments("
            "bill_id, patient_user_id, patient_name, payment_method, amount, status"
            ") VALUES (?, ?, ?, ?, ?, 'completed');",
            &insertPayment,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_bind_int(insertPayment, 1, billId);
    sqlite3_bind_int(insertPayment, 2, currentUser.id);
    BindText(insertPayment, 3, currentUser.fullName);
    BindText(insertPayment, 4, Trim(method));
    sqlite3_bind_double(insertPayment, 5, amount);

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

    return database_.Execute("COMMIT;", errorMessage);
}

bool CaseRepository::SaveAdmission(const User& currentUser,
                                   const std::string& patientIdentifier,
                                   const std::string& clinic,
                                   const std::string& reason,
                                   std::string& errorMessage) {
    sqlite3_int64 patientUserId = 0;
    std::string patientName = Trim(patientIdentifier);
    ResolveUserIdentity(patientIdentifier, "Patient", patientUserId, patientName);
    if (patientUserId <= 0) {
        errorMessage = "Patient must match an existing patient account.";
        return false;
    }

    sqlite3_stmt* activeAdmissionCheck = nullptr;
    if (!database_.Prepare(
            "SELECT 1 FROM admissions WHERE patient_user_id = ? AND status = 'active' LIMIT 1;",
            &activeAdmissionCheck,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int64(activeAdmissionCheck, 1, patientUserId);
    if (sqlite3_step(activeAdmissionCheck) == SQLITE_ROW) {
        sqlite3_finalize(activeAdmissionCheck);
        errorMessage = "This patient already has an active admission.";
        return false;
    }
    sqlite3_finalize(activeAdmissionCheck);

    if (!database_.Execute("BEGIN TRANSACTION;", errorMessage)) {
        return false;
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
    BindText(statement, 3, Trim(clinic));
    BindText(statement, 4, Trim(reason));
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
    sqlite3_bind_int64(labRequest, 2, patientUserId);
    BindText(labRequest, 3, patientName);
    sqlite3_bind_int(labRequest, 4, currentUser.id);
    if (!ExecutePrepared(labRequest, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    return database_.Execute("COMMIT;", errorMessage);
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

    sqlite3_stmt* allergyCheck = nullptr;
    if (!database_.Prepare(
            "SELECT allergies FROM patients WHERE user_id = ? LIMIT 1;",
            &allergyCheck,
            errorMessage)) {
        return false;
    }

    sqlite3_bind_int64(allergyCheck, 1, patientUserId);
    if (sqlite3_step(allergyCheck) == SQLITE_ROW) {
        const char* allergies = reinterpret_cast<const char*>(sqlite3_column_text(allergyCheck, 0));
        if (allergies != nullptr && TextContainsInsensitive(allergies, medicine)) {
            sqlite3_finalize(allergyCheck);
            errorMessage = "The patient's profile lists an allergy matching this medicine.";
            return false;
        }
    }
    sqlite3_finalize(allergyCheck);

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO prescriptions("
            "patient_user_id, patient_name, doctor_user_id, medicine, dosage, status"
            ") VALUES (?, ?, ?, ?, ?, 'active');",
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
    sqlite3_bind_int(statement, 3, currentUser.id);
    BindText(statement, 4, Trim(medicine));
    BindText(statement, 5, Trim(dosage));
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
}

bool CaseRepository::SaveMedicalRecord(const User& currentUser,
                                       const std::string& patientIdentifier,
                                       const std::string& recordType,
                                       const std::string& note,
                                       std::string& errorMessage) {
    sqlite3_int64 patientUserId = 0;
    std::string patientName = Trim(patientIdentifier);
    ResolveUserIdentity(patientIdentifier, "Patient", patientUserId, patientName);

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO medical_records("
            "patient_user_id, patient_name, doctor_user_id, record_type, note"
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
    sqlite3_bind_int(statement, 3, currentUser.id);
    BindText(statement, 4, Trim(recordType));
    BindText(statement, 5, Trim(note));
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
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
    const std::string patientName = prescriptionPatientName;

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO pharmacy_patient_reviews("
            "patient_user_id, patient_name, pharmacist_user_id, prescription_reference, review_status"
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
    sqlite3_bind_int(statement, 3, currentUser.id);
    BindText(statement, 4, Trim(prescriptionReference));
    BindText(statement, 5, Trim(status));
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
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
        return false;
    }

    sqlite3_bind_int64(duplicateCheck, 1, patientUserId);
    BindText(duplicateCheck, 2, Trim(surgeryDate));
    if (sqlite3_step(duplicateCheck) == SQLITE_ROW) {
        sqlite3_finalize(duplicateCheck);
        errorMessage = "This patient already has a scheduled surgery for that date.";
        return false;
    }
    sqlite3_finalize(duplicateCheck);

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO surgeries("
            "patient_user_id, patient_name, surgery_date, operating_room, status, created_by_user_id"
            ") VALUES (?, ?, ?, ?, 'scheduled', ?);",
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
    BindText(statement, 3, Trim(surgeryDate));
    BindText(statement, 4, Trim(operatingRoom));
    sqlite3_bind_int(statement, 5, currentUser.id);
    return ExecutePrepared(statement, database_.Handle(), errorMessage);
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

bool CaseRepository::SaveInventoryOrder(const User& currentUser,
                                        const std::string& itemName,
                                        const std::string& quantityText,
                                        const std::string& supplier,
                                        std::string& errorMessage) {
    int quantity = 0;
    if (!TryParseInteger(Trim(quantityText), quantity) || quantity <= 0) {
        errorMessage = "Quantity must be a positive whole number.";
        return false;
    }

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

    BindText(itemUpsert, 1, Trim(itemName));
    if (!ExecutePrepared(itemUpsert, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (!database_.Prepare(
            "INSERT INTO inventory_orders("
            "item_name, quantity, supplier, status, created_by_user_id"
            ") VALUES (?, ?, ?, 'ordered', ?);",
            &statement,
            errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    BindText(statement, 1, Trim(itemName));
    sqlite3_bind_int(statement, 2, quantity);
    BindText(statement, 3, Trim(supplier));
    sqlite3_bind_int(statement, 4, currentUser.id);
    if (!ExecutePrepared(statement, database_.Handle(), errorMessage)) {
        database_.Execute("ROLLBACK;", errorMessage);
        return false;
    }

    return database_.Execute("COMMIT;", errorMessage);
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
