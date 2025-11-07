
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************

/**********************************************************************
     Version :  Geant4.10.0.p02
     Created :  25/10/2014
     Author  :  Ercan Pilicer
     Company :  Uludag University
**********************************************************************/

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
  RunAction(DetectorConstruction*);
  ~RunAction();

public:
  void BeginOfRunAction(const G4Run*);
  void EndOfRunAction(const G4Run* );
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
    G4double backingThickness_um;
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
