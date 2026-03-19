#pragma once

#include "Database.h"
#include "User.h"

#include <string>
#include <vector>

struct WorkflowItem {
    std::string payload;
    std::string displayText;
};

struct PrescriptionDraft {
    int prescriptionId = 0;
    std::string medicine;
    std::string dosage;
    std::string frequency;
    std::string instructions;
    bool hasValue = false;
};

struct BillingPreview {
    std::vector<WorkflowItem> lines;
    double totalAmount = 0.0;
    double insuranceCoverage = 0.0;
    double patientPayable = 0.0;
};

struct PaymentReceipt {
    int paymentId = 0;
    int billId = 0;
    std::string receiptNumber;
    std::string patientName;
    std::string serviceName;
    std::string paymentMethod;
    double amount = 0.0;
    std::string createdAt;
    bool hasValue = false;
};

struct PrescriptionWarning {
    bool hasWarning = false;
    std::string message;
    std::vector<WorkflowItem> alternatives;
};

struct MedicalRecordSession {
    std::vector<WorkflowItem> history;
    bool readOnly = false;
    bool lockHeld = false;
    std::string lockOwner;
    std::string statusMessage;
};

struct InventoryOrderPreview {
    std::vector<WorkflowItem> lines;
    std::string itemName;
    std::string supplier;
    std::string chosenAlternative;
    int quantity = 0;
    bool hasAlternative = false;
};

struct SurgeryPlan {
    bool approved = false;
    bool rejected = false;
    int patientUserId = 0;
    std::string patientName;
    std::string surgeryDate;
    std::string surgeryTime;
    std::string doctorCategory;
    int surgeonUserId = 0;
    std::string surgeonName;
    std::string nurseName;
    std::string roomName;
    int durationMinutes = 120;
    std::string rejectionReason;
    std::vector<WorkflowItem> lines;
};

class CaseRepository {
public:
    explicit CaseRepository(std::string databasePath);

    bool IsAvailable() const;
    const std::string& InitializationError() const;

    bool SaveAction(const std::string& storageKey,
                    const User& currentUser,
                    const std::string& field1,
                    const std::string& field2,
                    const std::string& field3,
                    std::string& errorMessage);

    std::vector<std::wstring> LoadRecentPatientHistory(const User& currentUser) const;
    std::vector<WorkflowItem> FindAppointmentSlots(const std::string& appointmentDate,
                                                   const std::string& specialty,
                                                   const std::string& doctorFilter) const;
    std::string FindNextAvailableAppointmentDate(const std::string& appointmentDate,
                                                 const std::string& specialty,
                                                 const std::string& doctorFilter) const;
    std::vector<WorkflowItem> ListPatientAppointments(const User& currentUser) const;
    std::vector<WorkflowItem> ListOutstandingBills(const User& currentUser) const;
    bool ReserveAppointmentSlot(const User& currentUser, const std::string& slotPayload, std::string& errorMessage);
    bool ConfirmReservedAppointment(const User& currentUser, int appointmentId, std::string& errorMessage);
    bool ReleaseReservedAppointment(const User& currentUser, int appointmentId, std::string& errorMessage);
    bool BookAppointmentSlot(const User& currentUser, const std::string& slotPayload, std::string& errorMessage);
    bool CancelAppointment(const User& currentUser, int appointmentId, std::string& errorMessage);
    bool RescheduleAppointment(const User& currentUser,
                               int appointmentId,
                               const std::string& slotPayload,
                               std::string& errorMessage);
    std::vector<WorkflowItem> SearchPatients(const std::string& filter) const;
    bool CreatePrescriptionDetailed(const User& currentUser,
                                    int patientUserId,
                                    const std::string& medicine,
                                    const std::string& dosage,
                                    const std::string& frequency,
                                    const std::string& instructions,
                                    std::string& errorMessage);
    bool LoadLatestPrescriptionDraft(int patientUserId,
                                     PrescriptionDraft& draft,
                                     std::string& errorMessage) const;
    BillingPreview BuildBillingPreview(int patientUserId, std::string& errorMessage) const;
    bool GenerateAutomaticBill(const User& currentUser,
                               int patientUserId,
                               std::string& summaryMessage,
                               std::string& errorMessage);
    bool ProcessPaymentDetailed(const User& currentUser,
                                int billId,
                                const std::string& method,
                                const std::string& accountReference,
                                const std::string& amountText,
                                const std::string& authorizationCode,
                                std::string& summaryMessage,
                                std::string& errorMessage);
    bool LoadPaymentReceipt(const User& currentUser,
                            const std::string& receiptNumber,
                            PaymentReceipt& receipt,
                            std::string& errorMessage) const;
    bool SaveAdmissionDetailed(const User& currentUser,
                               int selectedPatientUserId,
                               const std::string& patientIdentifier,
                               const std::string& clinic,
                               const std::string& reason,
                               const std::string& amka,
                               const std::string& dateOfBirth,
                               std::string& summaryMessage,
                               std::string& errorMessage);
    bool TransferActiveAdmission(const User& currentUser,
                                 int selectedPatientUserId,
                                 const std::string& patientIdentifier,
                                 const std::string& newClinic,
                                 std::string& summaryMessage,
                                 std::string& errorMessage);
    bool SaveHrShiftDetailed(const User& currentUser,
                             const std::string& employeeName,
                             const std::string& shiftDate,
                             const std::string& startTime,
                             const std::string& endTime,
                             const std::string& department,
                             const std::string& overrideJustification,
                             std::string& summaryMessage,
                             std::string& errorMessage);
    bool CheckPrescriptionWarning(int patientUserId,
                                  const std::string& medicine,
                                  PrescriptionWarning& warning,
                                  std::string& errorMessage) const;
    bool SaveMedicalRecordDetailed(const User& currentUser,
                                   int patientUserId,
                                   const std::string& patientName,
                                   const std::string& recordType,
                                   const std::string& note,
                                   std::string& errorMessage);
    bool LoadMedicalRecordSession(const User& currentUser,
                                  int patientUserId,
                                  MedicalRecordSession& session,
                                  std::string& errorMessage);
    bool ReleaseMedicalRecordLock(const User& currentUser,
                                  int patientUserId,
                                  std::string& errorMessage);
    std::vector<WorkflowItem> ListPatientPrescriptions(int patientUserId) const;
    bool SavePharmacyReviewDetailed(const User& currentUser,
                                    int patientUserId,
                                    int prescriptionId,
                                    const std::string& status,
                                    std::string& summaryMessage,
                                    std::string& errorMessage);
    std::vector<WorkflowItem> SearchInventoryItems(const std::string& filter) const;
    std::vector<WorkflowItem> FindInventoryOrderAlternatives(const std::string& itemName,
                                                             const std::string& supplier) const;
    InventoryOrderPreview BuildInventoryOrderPreview(const std::string& itemName,
                                                     const std::string& quantityText,
                                                     const std::string& supplier,
                                                     const std::string& alternativePayload,
                                                     std::string& errorMessage) const;
    bool SaveInventoryOrderDetailed(const User& currentUser,
                                    const std::string& itemName,
                                    const std::string& quantityText,
                                    const std::string& supplier,
                                    const std::string& alternativePayload,
                                    std::string& summaryMessage,
                                    std::string& errorMessage);
    SurgeryPlan BuildSurgeryPlan(int patientUserId,
                                 const std::string& surgeryDate,
                                 const std::string& surgeryTime,
                                 const std::string& doctorCategory,
                                 const std::string& operatingRoom,
                                 std::string& errorMessage) const;
    bool ConfirmSurgeryPlan(const User& currentUser,
                            const SurgeryPlan& plan,
                            std::string& summaryMessage,
                            std::string& errorMessage);
    std::vector<WorkflowItem> FindShiftSwapCandidates(const std::string& employeeName,
                                                      const std::string& shiftDate,
                                                      const std::string& department,
                                                      const std::string& startTime,
                                                      const std::string& endTime,
                                                      std::string& errorMessage) const;
    bool SwapHrShift(const User& currentUser,
                     const std::string& swapPayload,
                     const std::string& justification,
                     std::string& summaryMessage,
                     std::string& errorMessage);

private:
    void ReleaseExpiredAppointmentReservations() const;
    void CleanupExpiredMedicalRecordLocks() const;
    bool ResolveUserIdentity(const std::string& identifier,
                             const std::string& expectedRole,
                             sqlite3_int64& userId,
                             std::string& fullName) const;
    bool SaveAppointment(const User& currentUser,
                         const std::string& appointmentDate,
                         const std::string& specialty,
                         const std::string& doctorIdentifier,
                         std::string& errorMessage);
    bool SavePayment(const User& currentUser,
                     const std::string& billReference,
                     const std::string& method,
                     const std::string& amountText,
                     std::string& errorMessage);
    bool SaveAdmission(const User& currentUser,
                       const std::string& patientIdentifier,
                       const std::string& clinic,
                       const std::string& reason,
                       std::string& errorMessage);
    bool SavePrescription(const User& currentUser,
                          const std::string& patientIdentifier,
                          const std::string& medicine,
                          const std::string& dosage,
                          std::string& errorMessage);
    bool SaveMedicalRecord(const User& currentUser,
                           const std::string& patientIdentifier,
                           const std::string& recordType,
                           const std::string& note,
                           std::string& errorMessage);
    bool SaveMedicationDispensing(const User& currentUser,
                                  const std::string& patientIdentifier,
                                  const std::string& medicine,
                                  const std::string& quantityText,
                                  std::string& errorMessage);
    bool SaveInventorySnapshot(const User& currentUser,
                               const std::string& itemName,
                               const std::string& quantityText,
                               const std::string& location,
                               std::string& errorMessage);
    bool SavePharmacyReview(const User& currentUser,
                            const std::string& patientIdentifier,
                            const std::string& prescriptionReference,
                            const std::string& status,
                            std::string& errorMessage);
    bool SaveBill(const User& currentUser,
                  const std::string& patientIdentifier,
                  const std::string& service,
                  const std::string& amountText,
                  std::string& errorMessage);
    bool SaveSurgery(const User& currentUser,
                     const std::string& patientIdentifier,
                     const std::string& surgeryDate,
                     const std::string& operatingRoom,
                     std::string& errorMessage);
    bool SavePatientSupportTask(const User& currentUser,
                                const std::string& patientIdentifier,
                                const std::string& taskType,
                                const std::string& note,
                                std::string& errorMessage);
    bool SaveInventoryOrder(const User& currentUser,
                            const std::string& itemName,
                            const std::string& quantityText,
                            const std::string& supplier,
                            std::string& errorMessage);
    bool SaveInventoryAlert(const User& currentUser,
                            const std::string& itemName,
                            const std::string& alertLevel,
                            const std::string& note,
                            std::string& errorMessage);
    bool SaveHrShift(const User& currentUser,
                     const std::string& employeeName,
                     const std::string& shiftDate,
                     const std::string& department,
                     std::string& errorMessage);
    bool SaveHrStaffUpdate(const User& currentUser,
                           const std::string& employeeName,
                           const std::string& roleName,
                           const std::string& department,
                           std::string& errorMessage);
    bool SaveHrConflict(const User& currentUser,
                        const std::string& employeeName,
                        const std::string& conflictDate,
                        const std::string& issue,
                        std::string& errorMessage);

    Database database_;
    std::string initializationError_;
};
