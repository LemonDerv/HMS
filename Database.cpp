#include "Database.h"

namespace {
const char* kSchemaSql = R"SQL(
PRAGMA foreign_keys = ON;
PRAGMA journal_mode = WAL;

CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    full_name TEXT NOT NULL,
    username TEXT NOT NULL UNIQUE COLLATE NOCASE,
    password TEXT NOT NULL,
    role TEXT NOT NULL CHECK(role IN ('Patient', 'Doctor', 'Pharmacist', 'Secretary', 'Inventory Manager', 'HR Manager')),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS patients (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER UNIQUE,
    patient_name TEXT NOT NULL,
    amka TEXT UNIQUE,
    date_of_birth TEXT,
    insurance_provider TEXT,
    allergies TEXT,
    current_status TEXT NOT NULL DEFAULT 'registered',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS staff_profiles (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER UNIQUE NOT NULL,
    role_name TEXT NOT NULL,
    department TEXT,
    specialization TEXT,
    employee_code TEXT UNIQUE,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS appointments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    patient_user_id INTEGER,
    patient_name TEXT NOT NULL,
    specialty TEXT NOT NULL,
    doctor_user_id INTEGER,
    doctor_name TEXT,
    appointment_date TEXT NOT NULL,
    appointment_time TEXT,
    status TEXT NOT NULL DEFAULT 'scheduled',
    notes TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(patient_user_id) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY(doctor_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS bills (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    patient_user_id INTEGER,
    patient_name TEXT NOT NULL,
    service_name TEXT NOT NULL,
    amount REAL NOT NULL,
    insurance_coverage REAL NOT NULL DEFAULT 0,
    status TEXT NOT NULL DEFAULT 'unpaid',
    created_by_user_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(patient_user_id) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY(created_by_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS payments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    bill_id INTEGER,
    patient_user_id INTEGER,
    patient_name TEXT NOT NULL,
    payment_method TEXT NOT NULL,
    amount REAL NOT NULL,
    status TEXT NOT NULL DEFAULT 'completed',
    receipt_number TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(bill_id) REFERENCES bills(id) ON DELETE SET NULL,
    FOREIGN KEY(patient_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS admissions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    patient_user_id INTEGER,
    patient_name TEXT NOT NULL,
    clinic TEXT NOT NULL,
    reason TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'active',
    doctor_user_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(patient_user_id) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY(doctor_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS lab_requests (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    admission_id INTEGER,
    patient_user_id INTEGER,
    patient_name TEXT NOT NULL,
    requested_by_user_id INTEGER,
    request_type TEXT NOT NULL,
    status TEXT NOT NULL DEFAULT 'pending',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(admission_id) REFERENCES admissions(id) ON DELETE SET NULL,
    FOREIGN KEY(patient_user_id) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY(requested_by_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS prescriptions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    patient_user_id INTEGER,
    patient_name TEXT NOT NULL,
    doctor_user_id INTEGER,
    medicine TEXT NOT NULL,
    dosage TEXT NOT NULL,
    frequency TEXT,
    instructions TEXT,
    status TEXT NOT NULL DEFAULT 'active',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(patient_user_id) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY(doctor_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS medical_records (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    patient_user_id INTEGER,
    patient_name TEXT NOT NULL,
    doctor_user_id INTEGER,
    record_type TEXT NOT NULL,
    note TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(patient_user_id) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY(doctor_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS medication_dispensing (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    patient_user_id INTEGER,
    patient_name TEXT NOT NULL,
    pharmacist_user_id INTEGER,
    medicine TEXT NOT NULL,
    quantity INTEGER NOT NULL,
    status TEXT NOT NULL DEFAULT 'completed',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(patient_user_id) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY(pharmacist_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS pharmacy_patient_reviews (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    patient_user_id INTEGER,
    patient_name TEXT NOT NULL,
    pharmacist_user_id INTEGER,
    prescription_reference TEXT NOT NULL,
    review_status TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(patient_user_id) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY(pharmacist_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS inventory_items (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    item_name TEXT NOT NULL UNIQUE,
    category TEXT NOT NULL,
    quantity INTEGER NOT NULL DEFAULT 0,
    reorder_level INTEGER NOT NULL DEFAULT 0,
    location TEXT,
    status TEXT NOT NULL DEFAULT 'available',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS inventory_orders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id INTEGER,
    item_name TEXT NOT NULL,
    quantity INTEGER NOT NULL,
    supplier TEXT,
    status TEXT NOT NULL DEFAULT 'ordered',
    created_by_user_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(item_id) REFERENCES inventory_items(id) ON DELETE SET NULL,
    FOREIGN KEY(created_by_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS inventory_alerts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    item_id INTEGER,
    item_name TEXT NOT NULL,
    alert_level TEXT NOT NULL,
    note TEXT,
    created_by_user_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(item_id) REFERENCES inventory_items(id) ON DELETE SET NULL,
    FOREIGN KEY(created_by_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS surgeries (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    patient_user_id INTEGER,
    patient_name TEXT NOT NULL,
    surgery_date TEXT NOT NULL,
    operating_room TEXT,
    status TEXT NOT NULL DEFAULT 'scheduled',
    created_by_user_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(patient_user_id) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY(created_by_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS patient_support_tasks (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    patient_user_id INTEGER,
    patient_name TEXT NOT NULL,
    task_type TEXT NOT NULL,
    note TEXT NOT NULL,
    created_by_user_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(patient_user_id) REFERENCES users(id) ON DELETE SET NULL,
    FOREIGN KEY(created_by_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS hr_shifts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    employee_name TEXT NOT NULL,
    shift_date TEXT NOT NULL,
    department TEXT NOT NULL,
    created_by_user_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(created_by_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS hr_staff_updates (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    employee_name TEXT NOT NULL,
    role_name TEXT NOT NULL,
    department TEXT NOT NULL,
    created_by_user_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(created_by_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS hr_conflicts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    employee_name TEXT NOT NULL,
    conflict_date TEXT NOT NULL,
    issue TEXT NOT NULL,
    created_by_user_id INTEGER,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(created_by_user_id) REFERENCES users(id) ON DELETE SET NULL
);

CREATE INDEX IF NOT EXISTS idx_users_username ON users(username);
CREATE INDEX IF NOT EXISTS idx_appointments_patient_user ON appointments(patient_user_id);
CREATE INDEX IF NOT EXISTS idx_bills_patient_user ON bills(patient_user_id);
CREATE INDEX IF NOT EXISTS idx_payments_bill_id ON payments(bill_id);
CREATE INDEX IF NOT EXISTS idx_prescriptions_patient_user ON prescriptions(patient_user_id);
CREATE INDEX IF NOT EXISTS idx_admissions_patient_user ON admissions(patient_user_id);
CREATE INDEX IF NOT EXISTS idx_inventory_orders_item_name ON inventory_orders(item_name);
CREATE INDEX IF NOT EXISTS idx_pharmacy_reviews_patient_user ON pharmacy_patient_reviews(patient_user_id);
)SQL";
}

Database::Database(std::string path) : path_(std::move(path)) {}

Database::~Database() {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::Open(std::string& errorMessage) {
    if (db_ != nullptr) {
        return true;
    }

    const int openResult = sqlite3_open(path_.c_str(), &db_);
    if (openResult != SQLITE_OK) {
        errorMessage = db_ != nullptr ? sqlite3_errmsg(db_) : "Unable to open SQLite database.";
        if (db_ != nullptr) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    sqlite3_extended_result_codes(db_, 1);
    sqlite3_busy_timeout(db_, 3000);
    return true;
}

bool Database::IsOpen() const {
    return db_ != nullptr;
}

bool Database::Execute(const std::string& sql, std::string& errorMessage) const {
    if (db_ == nullptr) {
        errorMessage = "Database connection is not open.";
        return false;
    }

    char* sqliteError = nullptr;
    const int execResult = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &sqliteError);
    if (execResult != SQLITE_OK) {
        errorMessage = sqliteError != nullptr ? sqliteError : sqlite3_errmsg(db_);
        sqlite3_free(sqliteError);
        return false;
    }

    return true;
}

bool Database::Prepare(const std::string& sql, sqlite3_stmt** statement, std::string& errorMessage) const {
    if (db_ == nullptr) {
        errorMessage = "Database connection is not open.";
        return false;
    }

    const int prepareResult = sqlite3_prepare_v2(db_, sql.c_str(), -1, statement, nullptr);
    if (prepareResult != SQLITE_OK) {
        errorMessage = sqlite3_errmsg(db_);
        return false;
    }

    return true;
}

bool Database::InitializeSchema(std::string& errorMessage) const {
    return Execute(kSchemaSql, errorMessage);
}

sqlite3* Database::Handle() const {
    return db_;
}

sqlite3_int64 Database::LastInsertRowId() const {
    return sqlite3_last_insert_rowid(db_);
}

const std::string& Database::Path() const {
    return path_;
}
