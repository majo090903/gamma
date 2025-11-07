// ********************************************************************
// * License and Disclaimer                                           *
// ********************************************************************

#ifndef RunAction_h
#define RunAction_h 1

#include "G4UserRunAction.hh"
#include "ProcessesCount.hh"

#include "G4RunManager.hh"
#include "globals.hh"

#include <vector>
#include <string>

class G4Run;
class DetectorConstruction;

class RunAction : public G4UserRunAction
{
public:
  explicit RunAction(DetectorConstruction*);
  ~RunAction() override;

  void BeginOfRunAction(const G4Run*) override;
  void EndOfRunAction(const G4Run*) override;
  void CountProcesses(G4String);

  void CountInjection();
  void RecordTransmission(G4double energy, G4double cosZ, G4bool scattered);
  void AddIncidentEnergy(G4double energy);
  void AddDepositedEnergy(G4double energy);

private:
  struct SummaryRow {
    G4int runID;
    G4double worldHalf_cm;
    G4double thickness_nm;
    G4double density_g_cm3;
    G4double energy_keV;
    G4double totalInjected;
    G4double transmittedUncollided;
    G4double transmittedTotal;
    G4double T_counts;
    G4double mu_counts_per_mm;
    G4double mu_counts_cm2_g;
    G4double mu_calc_cm2_g;
    G4double mu_tr_cm2_g;
    G4double mu_ref_cm2_g;
    G4double mu_en_ref_cm2_g;
    G4double delta_mu_percent;
    G4double delta_mu_en_percent;
    G4double sigma_T_counts;
    G4double sigma_mu_counts_cm2_g;
    G4double mu_calc_per_mm;
    G4double mu_tr_per_mm;
    G4double mu_en_cpe_per_mm;
    G4double mu_en_cpe_cm2_g;
    G4double delta_mu_en_cpe_percent;
    G4double absorbedFraction;
    G4double mu_en_per_mm;
    G4double mu_en_cm2_g;
    G4double mu_en_raw_per_mm;
    G4double mu_en_raw_cm2_g;
    G4double mu_eff_per_mm;
    G4double mu_eff_cm2_g;
    G4double E_trans_unc_keV;
    G4double E_trans_tot_keV;
    G4double E_abs_keV;
    G4double T_energy_tot;
  };

  void WriteSummaryFile() const;
  bool EnsureReferenceDataLoaded() const;
  bool InterpolateReference(G4double energy_keV,
                            G4double& mu_cm2_g,
                            G4double& mu_en_cm2_g) const;
  G4double ComputeComptonRetentionFraction(G4double energy) const;

  ProcessesCount*       ProcCounter;
  DetectorConstruction* fDetector;

  G4int    fInjected;
  G4int    fDetectedUncollided;
  G4int    fDetectedScattered;
  G4double fIncidentEnergy;
  G4double fTransmittedEnergyUncollided;
  G4double fTransmittedEnergyTotal;
  G4double fDepositedEnergy;

  std::vector<SummaryRow> fSummaryRows;

  struct ReferenceDatum {
    G4double energy_keV;
    G4double mu_cm2_g;
    G4double mu_en_cm2_g;
  };

  mutable bool fReferenceLoaded;
  mutable bool fReferenceAvailable;
  mutable G4String fReferenceSource;
  mutable std::vector<ReferenceDatum> fReferenceData;
};
#endif
