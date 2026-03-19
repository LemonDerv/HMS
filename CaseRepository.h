#pragma once

#include "Database.h"
#include "User.h"

#include <string>
#include <vector>

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

private:
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
