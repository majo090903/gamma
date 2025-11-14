// ********************************************************************
// * License and Disclaimer                                           *
// ********************************************************************

#include "RunAction.hh"

#include "DetectorConstruction.hh"
#include "ProcessesCount.hh"

#include "G4EmCalculator.hh"
#include "G4Material.hh"
#include "G4PhysicalConstants.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
  constexpr G4double kEpsilon = 1.e-12;

  bool GetEnvFlag(const char* name, bool defaultValue)
  {
    const char* value = std::getenv(name);
    if (!value) {
      return defaultValue;
    }
    std::string token(value);
    std::transform(token.begin(), token.end(), token.begin(), [](unsigned char c) { return std::tolower(c); });
    if (token == "1" || token == "true" || token == "on" || token == "yes") {
      return true;
    }
    if (token == "0" || token == "false" || token == "off" || token == "no") {
      return false;
    }
    return defaultValue;
  }

  G4int GetEnvInt(const char* name, G4int defaultValue)
  {
    const char* value = std::getenv(name);
    if (!value) {
      return defaultValue;
    }
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (end == value) {
      return defaultValue;
    }
    return static_cast<G4int>(parsed);
  }
}

RunAction::RunAction(DetectorConstruction* detector)
  : G4UserRunAction(),
    ProcCounter(nullptr),
    fDetector(detector),
    fInjected(0),
    fDetectedUncollided(0),
    fDetectedScattered(0),
    fIncidentEnergy(0.),
    fTransmittedEnergyUncollided(0.),
    fTransmittedEnergyTotal(0.),
    fDepositedEnergyFoil(0.),
    fDepositedEnergyBacking(0.),
    fDepositedEnergyOther(0.),
    fReferenceLoaded(false),
    fReferenceAvailable(false),
    fReferenceSource(""),
    fUseLogInterpolation(GetEnvFlag("W_USE_LOG_INTERP", false)),
    fPreferCalculatorMuTr(GetEnvFlag("W_USE_MU_TR_CALC", true)),
    fComptonIntegrationSteps(512)
{
  const G4int requestedSteps = GetEnvInt("W_COMPTON_STEPS", fComptonIntegrationSteps);
  const G4int evenSteps = (requestedSteps % 2 == 0) ? requestedSteps : requestedSteps + 1;
  fComptonIntegrationSteps = std::max(32, evenSteps);
}

RunAction::~RunAction()
{
  WriteSummaryFile();
  delete ProcCounter;
}

void RunAction::BeginOfRunAction(const G4Run* run)
{
  G4cout << "Run " << run->GetRunID() << " starts ..." << G4endl;

  if (ProcCounter) {
    delete ProcCounter;
  }
  ProcCounter = new ProcessesCount;

  fInjected = 0;
  fDetectedUncollided = 0;
  fDetectedScattered = 0;
  fIncidentEnergy = 0.;
  fTransmittedEnergyUncollided = 0.;
  fTransmittedEnergyTotal = 0.;
  fDepositedEnergyFoil = 0.;
  fDepositedEnergyBacking = 0.;
  fDepositedEnergyOther = 0.;
}

void RunAction::EndOfRunAction(const G4Run* run)
{
  const G4int numberOfEvents = run->GetNumberOfEvent();
  if (numberOfEvents == 0) {
    return;
  }

  const G4double transmitted = static_cast<G4double>(fDetectedUncollided + fDetectedScattered);
  const G4double injected    = static_cast<G4double>(fInjected);

  G4double transmissionCountsRaw = 0.;
  if (injected > 0.) {
    transmissionCountsRaw = static_cast<G4double>(fDetectedUncollided) / injected;
  }
  if (transmissionCountsRaw > 0.995) {
    G4cout << " [advice] T>0.995: increase primaries (/gps/number or /run/beamOn) or slightly increase foil thickness to reduce counting noise." << G4endl;
  }
  if (transmissionCountsRaw < 0.005 && injected > 0.) {
    G4cout << " [advice] T<0.005: decrease foil thickness or increase primaries." << G4endl;
  }

  const G4double clampLow = 1.e-9;
  const G4double clampHigh = 1. - 1.e-9;
  G4double transmissionCounts = std::clamp(transmissionCountsRaw, clampLow, clampHigh);
  G4int clampFlag = 0;
  if (transmissionCountsRaw <= clampLow) {
    clampFlag = 1;
  } else if (transmissionCountsRaw >= clampHigh) {
    clampFlag = 2;
  }
  if (std::abs(transmissionCountsRaw - transmissionCounts) > 1.e-12) {
    G4cout << " [RunAction] Warning: transmission fraction " << transmissionCountsRaw
           << " clamped to " << transmissionCounts
           << ". Consider increasing statistics or foil thickness." << G4endl;
  }
  G4double sigma_T_counts = 0.;
  if (injected > 0.) {
    const G4double p = std::clamp(transmissionCountsRaw, 0.0, 1.0);
    sigma_T_counts = std::sqrt(p * std::max(0.0, 1.0 - p) / injected);
  }
  const G4double transmissionScattered = (injected > 0.) ? static_cast<G4double>(fDetectedScattered) / injected : 0.;

  auto* material = fDetector->GetMaterial();
  auto* backingMaterial = fDetector->GetBackingMaterial();
  const G4double density = material ? material->GetDensity() : 0.;
  const G4double density_g_cm3 = density / (g/cm3);
  const G4double backingDensity = backingMaterial ? backingMaterial->GetDensity() : density;
  const G4double backing_density_g_cm3 = backingDensity / (g/cm3);
  const G4String backingMaterialName = backingMaterial ? backingMaterial->GetName() : "";

  const G4double thickness = fDetector->GetFoilThickness();  // Geant4 length (mm)
  const G4double thickness_mm = thickness / mm;
  const G4double thickness_nm = thickness / nm;
  const G4double thickness_cm = thickness / cm;
  const G4double backing_thickness = fDetector->GetBackingThickness();
  const G4double backing_thickness_um = backing_thickness / um;
  const G4double backing_thickness_mm = backing_thickness / mm;
  const G4double backing_thickness_cm = backing_thickness / cm;

  const G4double worldHalf_cm = fDetector->GetWorldHalfLength() / cm;

  G4cout << " Beam energy (keV)      : ";
  G4double primaryEnergy = 0.;
  if (injected > 0. && fIncidentEnergy > 0.) {
    primaryEnergy = fIncidentEnergy / injected;
  }
  const G4double energy_keV = primaryEnergy / keV;
  G4cout << energy_keV << G4endl;
  if (std::abs(energy_keV - 1000.0) < 1.e-3) {
    G4cout << " [sentinel] energy = 1000 keV (== 1 MeV). Units consistent." << G4endl;
  }

  G4cout << " Foil thickness (nm)    : " << thickness_nm << G4endl;
  G4cout << " Backing thickness (µm) : " << backing_thickness_um << G4endl;
  G4cout << " Target density (g/cm3) : " << density_g_cm3 << G4endl;

  G4double mu_counts_per_mm = 0.;
  if (thickness_mm > 0.) {
    mu_counts_per_mm = -std::log(transmissionCounts) / thickness_mm;
  }
  const G4double mu_counts_cm2_g = (density_g_cm3 > 0.) ? (mu_counts_per_mm * 10.0) / density_g_cm3 : 0.;
  G4double sigma_mu_counts_cm2_g = 0.;
  if (thickness_mm > 0. && transmissionCountsRaw > 0. && density_g_cm3 > 0.) {
    sigma_mu_counts_cm2_g = sigma_T_counts * 10.0 / (density_g_cm3 * thickness_mm * std::max(transmissionCountsRaw, 1.e-12));
  }

  const G4double E_trans_unc_keV = fTransmittedEnergyUncollided / keV;
  const G4double E_trans_tot_keV = fTransmittedEnergyTotal / keV;
  const G4double E_dep_foil_keV = fDepositedEnergyFoil / keV;
  const G4double E_dep_backing_keV = fDepositedEnergyBacking / keV;
  const G4double E_dep_other_keV = fDepositedEnergyOther / keV;
  const G4double E_dep_slab_keV = E_dep_foil_keV + E_dep_backing_keV;
  const G4double E_dep_total_keV = E_dep_slab_keV + E_dep_other_keV;

  const G4double fluence_energy_keV = energy_keV * injected;
  G4double T_energy_unc = 0.;
  if (fluence_energy_keV > 0.) {
    T_energy_unc = E_trans_unc_keV / fluence_energy_keV;
  }

  G4double absorbedFraction = 0.;
  if (fluence_energy_keV > 0.) {
    absorbedFraction = E_dep_foil_keV / fluence_energy_keV;
  }
  absorbedFraction = std::clamp(absorbedFraction, 0.0, 1.0);
  G4double absorbedFractionSlab = 0.;
  if (fluence_energy_keV > 0.) {
    absorbedFractionSlab = E_dep_slab_keV / fluence_energy_keV;
  }
  absorbedFractionSlab = std::clamp(absorbedFractionSlab, 0.0, 1.0);

  const G4double mass_path_foil_g_cm2 = density_g_cm3 * (thickness_mm / 10.0);
  const G4double mass_path_backing_g_cm2 = backing_density_g_cm3 * (backing_thickness_mm / 10.0);
  const G4double mass_path_slab_g_cm2 = mass_path_foil_g_cm2 + mass_path_backing_g_cm2;
  G4double mu_en_raw_foil_cm2_g = 0.;
  if (fluence_energy_keV > 0. && mass_path_foil_g_cm2 > 0.) {
    mu_en_raw_foil_cm2_g = (E_dep_foil_keV / fluence_energy_keV) / mass_path_foil_g_cm2;
  }
  G4double mu_en_raw_slab_cm2_g = 0.;
  if (fluence_energy_keV > 0. && mass_path_slab_g_cm2 > 0.) {
    mu_en_raw_slab_cm2_g = (E_dep_slab_keV / fluence_energy_keV) / mass_path_slab_g_cm2;
  }
  const G4double mu_en_raw_foil_per_mm = (density_g_cm3 > 0.)
                                           ? (mu_en_raw_foil_cm2_g * density_g_cm3 / 10.0)
                                           : 0.0;
  const G4double slabThickness_cm = thickness_cm + backing_thickness_cm;
  G4double rho_eff_slab = 0.;
  if (slabThickness_cm > 0. && mass_path_slab_g_cm2 > 0.) {
    rho_eff_slab = mass_path_slab_g_cm2 / slabThickness_cm;
  }
  const G4double mu_en_raw_slab_per_mm = (rho_eff_slab > 0.)
                                           ? (mu_en_raw_slab_cm2_g * rho_eff_slab / 10.0)
                                           : 0.0;

  G4double mu_en_per_mm = mu_en_raw_foil_per_mm;
  G4double mu_en_cm2_g  = mu_en_raw_foil_cm2_g;
  const G4double mu_en_raw_per_mm = mu_en_raw_foil_per_mm;
  const G4double mu_en_raw_cm2_g  = mu_en_raw_foil_cm2_g;

  G4double mu_eff_per_mm = 0.;
  if (absorbedFraction > 0. && absorbedFraction < 1.) {
    mu_eff_per_mm = -std::log(std::max(1. - absorbedFraction, kEpsilon)) / thickness_mm;
  }
  const G4double mu_eff_cm2_g = (density_g_cm3 > 0.) ? (mu_eff_per_mm * 10.0) / density_g_cm3 : 0.;

  G4double totalEnergyFraction = 0.;
  if (fluence_energy_keV > 0.) {
    totalEnergyFraction = E_trans_tot_keV / fluence_energy_keV;
  }
  totalEnergyFraction = std::clamp(totalEnergyFraction, 0.0, 1.0);

  // ------------------------------------------------------------------
  // Geant4 calculator diagnostics
  // ------------------------------------------------------------------
  G4double mu_calc_per_mm = 0.;
  G4double mu_calc_cm2_g = 0.;
  G4double mu_tr_per_mm = 0.;
  G4double mu_tr_cm2_g = 0.;
  G4double mu_en_cpe_per_mm = mu_en_per_mm;
  G4double mu_en_cpe_cm2_g  = mu_en_cm2_g;
  G4double delta_mu_en_cpe_percent = 0.;
  G4double delta_mu_counts_vs_calc_percent = 0.;
  G4double delta_mu_en_cpe_vs_mu_tr_percent = 0.;
  if (material && primaryEnergy > 0.) {
    G4EmCalculator calculator;
    const G4String materialName = material->GetName();
    const G4double mu_phot = calculator.ComputeCrossSectionPerVolume(primaryEnergy, "gamma", "phot", materialName);
    const G4double mu_compt = calculator.ComputeCrossSectionPerVolume(primaryEnergy, "gamma", "compt", materialName);
    const G4double mu_rayl = calculator.ComputeCrossSectionPerVolume(primaryEnergy, "gamma", "Rayl", materialName);
    const G4double mu_conv = calculator.ComputeCrossSectionPerVolume(primaryEnergy, "gamma", "conv", materialName);
    const G4double mu_convIoni = calculator.ComputeCrossSectionPerVolume(primaryEnergy, "gamma", "convIoni", materialName);

    mu_calc_per_mm = mu_phot + mu_compt + mu_rayl + mu_conv + mu_convIoni;
    if (density_g_cm3 > 0.) {
      mu_calc_cm2_g = (mu_calc_per_mm * cm) / density_g_cm3;
    }

    const G4double comptonRetention = ComputeComptonRetentionFraction(primaryEnergy);
    const G4double comptonTransferFraction = std::max(0.0, 1.0 - comptonRetention);
    const G4double pairFraction = (primaryEnergy > 2.0 * electron_mass_c2)
                                    ? (primaryEnergy - 2.0 * electron_mass_c2) / primaryEnergy
                                    : 0.0;

    mu_tr_per_mm = mu_phot + (mu_conv + mu_convIoni) * pairFraction + mu_compt * comptonTransferFraction;
    if (density_g_cm3 > 0.) {
      mu_tr_cm2_g = (mu_tr_per_mm * cm) / density_g_cm3;
    }
  }

  // ------------------------------------------------------------------
  // Reference comparison (NIST/XCOM)
  // ------------------------------------------------------------------
  G4double mu_ref_cm2_g = 0.;
  G4double mu_ref_en_cm2_g = 0.;
  G4double delta_mu_percent = 0.;
  G4double delta_mu_en_percent = 0.;
  const G4bool hasReference = InterpolateReference(energy_keV, mu_ref_cm2_g, mu_ref_en_cm2_g);
  if (hasReference && mu_ref_cm2_g > 0.) {
    delta_mu_percent = (mu_counts_cm2_g - mu_ref_cm2_g) / mu_ref_cm2_g * 100.0;
  }
  if (hasReference && mu_ref_en_cm2_g > 0.) {
    delta_mu_en_percent = (mu_en_raw_cm2_g - mu_ref_en_cm2_g) / mu_ref_en_cm2_g * 100.0;
  }

  if (hasReference && mu_ref_cm2_g > 0. && mu_ref_en_cm2_g > 0.) {
    const G4double refRatio = mu_ref_en_cm2_g / mu_ref_cm2_g;
    mu_en_cpe_cm2_g  = mu_counts_cm2_g * refRatio;
    mu_en_cpe_per_mm = mu_counts_per_mm * refRatio;
  } else if (fPreferCalculatorMuTr && mu_tr_cm2_g > 0.) {
    mu_en_cpe_cm2_g  = mu_tr_cm2_g;
    mu_en_cpe_per_mm = mu_tr_per_mm;
  }
  if (hasReference && mu_ref_en_cm2_g > 0.) {
    delta_mu_en_cpe_percent = (mu_en_cpe_cm2_g - mu_ref_en_cm2_g) / mu_ref_en_cm2_g * 100.0;
  }
  if (mu_calc_cm2_g > 0.) {
    delta_mu_counts_vs_calc_percent = (mu_counts_cm2_g - mu_calc_cm2_g) / mu_calc_cm2_g * 100.0;
  }
  if (mu_tr_cm2_g > 0.) {
    delta_mu_en_cpe_vs_mu_tr_percent = (mu_en_cpe_cm2_g - mu_tr_cm2_g) / mu_tr_cm2_g * 100.0;
  }
  mu_en_per_mm = mu_en_cpe_per_mm;
  mu_en_cm2_g  = mu_en_cpe_cm2_g;

  // ------------------------------------------------------------------
  // Logging
  // ------------------------------------------------------------------
  G4cout << " Injected primaries    : " << fInjected << G4endl;
  G4cout << " Transmitted (total)   : " << static_cast<G4int>(transmitted) << G4endl;
  G4cout << "   - uncollided        : " << fDetectedUncollided << G4endl;
  G4cout << "   - scattered         : " << fDetectedScattered << G4endl;
  G4cout << " Transmission (counts) : " << transmissionCountsRaw << " (sigma " << sigma_T_counts << ")" << G4endl;
  if (clampFlag != 0) {
    G4cout << "   used (clamped)     : " << transmissionCounts << " [flag " << clampFlag << "]" << G4endl;
  }
  G4cout << " Transmission (scatt.) : " << transmissionScattered << G4endl;
  G4cout << " mu (counts) [1/mm]    : " << mu_counts_per_mm << G4endl;
  G4cout << " mu/rho (counts) [cm2/g]: " << mu_counts_cm2_g << " (sigma " << sigma_mu_counts_cm2_g << ")" << G4endl;
  G4cout << " Energy frac (uncoll.) : " << T_energy_unc << G4endl;
  G4cout << " mu_eff [1/mm]         : " << mu_eff_per_mm << G4endl;
  G4cout << " mu_eff/rho [cm2/g]    : " << mu_eff_cm2_g << G4endl;
  G4cout << " mu_en [1/mm]          : " << mu_en_per_mm << G4endl;
  G4cout << " mu_en/rho (raw foil) [cm2/g] : " << mu_en_raw_cm2_g << G4endl;
  G4cout << " mu_en/rho (raw slab) [cm2/g] : " << mu_en_raw_slab_cm2_g << G4endl;
  G4cout << " mu_en/rho (CPE) [cm2/g]: " << mu_en_cpe_cm2_g
         << " (Δ " << delta_mu_en_cpe_percent << " % vs NIST)" << G4endl;
  G4cout << " Absorbed fraction (foil) : " << absorbedFraction << G4endl;
  G4cout << " Absorbed fraction (slab) : " << absorbedFractionSlab << G4endl;

  if (mu_calc_cm2_g > 0.) {
    G4cout << " [diag] mu/rho (trans vs G4 calc) : "
           << mu_counts_cm2_g << " vs " << mu_calc_cm2_g
           << " (Δ = " << delta_mu_counts_vs_calc_percent << " %)" << G4endl;
  }
  if (mu_tr_cm2_g > 0.) {
    G4cout << " [diag] mu_en/rho vs mu_tr/rho   : "
           << mu_en_cm2_g << " vs " << mu_tr_cm2_g
           << " (Δ = " << delta_mu_en_cpe_vs_mu_tr_percent << " %)" << G4endl;
  }
  if (hasReference) {
    G4cout << " [nist] mu/rho reference [cm2/g] : " << mu_ref_cm2_g
           << " (Δ = " << delta_mu_percent << " %)" << G4endl;
    G4cout << " [nist] μ_en/rho reference      : " << mu_ref_en_cm2_g
           << " (Δ raw = " << delta_mu_en_percent << " % | Δ CPE = " << delta_mu_en_cpe_percent << " %)" << G4endl;
  } else if (fReferenceLoaded && !fReferenceAvailable && run->GetRunID() == 0) {
    G4cout << " [nist] Reference CSV not loaded (set W_MU_REFERENCE_CSV or place nist_reference.csv)." << G4endl;
  }
  G4cout << "------------------------------------------------------------" << G4endl;

  SummaryRow row{};
  row.runID                  = run->GetRunID();
  row.worldHalf_cm           = worldHalf_cm;
  row.thickness_nm           = thickness_nm;
  row.backingThickness_um    = backing_thickness_um;
  row.density_g_cm3          = density_g_cm3;
  row.energy_keV             = energy_keV;
  row.totalInjected          = injected;
  row.transmittedUncollided  = fDetectedUncollided;
  row.transmittedScattered   = fDetectedScattered;
  row.transmittedTotal       = transmitted;
  row.T_counts               = transmissionCountsRaw;
  row.T_counts_scattered     = transmissionScattered;
  row.T_counts_clamped       = transmissionCounts;
  row.clamp_flag             = clampFlag;
  row.mu_counts_per_mm       = mu_counts_per_mm;
  row.mu_counts_cm2_g        = mu_counts_cm2_g;
  row.mu_calc_cm2_g          = mu_calc_cm2_g;
  row.mu_tr_cm2_g            = mu_tr_cm2_g;
  row.mu_ref_cm2_g           = hasReference ? mu_ref_cm2_g : 0.;
  row.mu_en_ref_cm2_g        = hasReference ? mu_ref_en_cm2_g : 0.;
  row.delta_mu_percent       = delta_mu_percent;
  row.delta_mu_en_percent    = delta_mu_en_percent;
  row.sigma_T_counts         = sigma_T_counts;
  row.sigma_mu_counts_cm2_g  = sigma_mu_counts_cm2_g;
  row.mu_calc_per_mm         = mu_calc_per_mm;
  row.mu_tr_per_mm           = mu_tr_per_mm;
  row.mu_en_cpe_per_mm       = mu_en_cpe_per_mm;
  row.mu_en_cpe_cm2_g        = mu_en_cpe_cm2_g;
  row.delta_mu_en_cpe_percent = delta_mu_en_cpe_percent;
  row.delta_mu_counts_vs_calc_percent = delta_mu_counts_vs_calc_percent;
  row.delta_mu_en_cpe_vs_mu_tr_percent = delta_mu_en_cpe_vs_mu_tr_percent;
  row.absorbedFraction       = absorbedFraction;
  row.absorbedFractionSlab   = absorbedFractionSlab;
  row.mu_en_per_mm           = mu_en_per_mm;
  row.mu_en_cm2_g            = mu_en_cm2_g;
  row.mu_en_raw_per_mm       = mu_en_raw_per_mm;
  row.mu_en_raw_cm2_g        = mu_en_raw_cm2_g;
  row.mu_en_raw_slab_per_mm  = mu_en_raw_slab_per_mm;
  row.mu_en_raw_slab_cm2_g   = mu_en_raw_slab_cm2_g;
  row.mu_eff_per_mm          = mu_eff_per_mm;
  row.mu_eff_cm2_g           = mu_eff_cm2_g;
  row.E_trans_unc_keV        = E_trans_unc_keV;
  row.E_trans_tot_keV        = E_trans_tot_keV;
  row.E_abs_keV              = E_dep_foil_keV;
  row.E_abs_backing_keV      = E_dep_backing_keV;
  row.E_abs_slab_keV         = E_dep_slab_keV;
  row.E_abs_other_keV        = E_dep_other_keV;
  row.T_energy_tot           = totalEnergyFraction;
  row.T_energy_unc           = T_energy_unc;
  row.backingThickness_um    = backing_thickness_um;
  row.backingMaterial        = backingMaterialName;

  fSummaryRows.push_back(row);
}

void RunAction::CountProcesses(G4String procName)
{
  if (!ProcCounter) {
    ProcCounter = new ProcessesCount;
  }

  size_t nbProc = ProcCounter->size();
  size_t i = 0;
  while ((i < nbProc) && ((*ProcCounter)[i]->GetName() != procName)) {
    ++i;
  }
  if (i == nbProc) {
    ProcCounter->push_back(new OneProcessCount(procName));
  }
  (*ProcCounter)[i]->Count();
}

void RunAction::CountInjection()
{
  ++fInjected;
}

void RunAction::RecordTransmission(G4double energy, G4double /*cosZ*/, G4bool scattered)
{
  if (scattered) {
    ++fDetectedScattered;
  } else {
    ++fDetectedUncollided;
    fTransmittedEnergyUncollided += energy;
  }
  fTransmittedEnergyTotal += energy;
}

void RunAction::AddIncidentEnergy(G4double energy)
{
  fIncidentEnergy += energy;
}

void RunAction::AddFoilDepositedEnergy(G4double energy)
{
  fDepositedEnergyFoil += energy;
}

void RunAction::AddBackingDepositedEnergy(G4double energy)
{
  fDepositedEnergyBacking += energy;
}

void RunAction::AddOtherDepositedEnergy(G4double energy)
{
  fDepositedEnergyOther += energy;
}

bool RunAction::EnsureReferenceDataLoaded() const
{
  if (fReferenceLoaded) {
    return fReferenceAvailable;
  }

  fReferenceLoaded = true;
  const char* envPath = std::getenv("W_MU_REFERENCE_CSV");
  std::vector<G4String> candidates;
  if (envPath && *envPath) {
    candidates.emplace_back(envPath);
  }
  candidates.emplace_back("nist_reference.csv");
  candidates.emplace_back("../nist_reference.csv");

  for (const auto& path : candidates) {
    std::ifstream in(path);
    if (!in) {
      continue;
    }

    std::vector<ReferenceDatum> table;
    std::string line;
    std::getline(in, line); // skip header
    while (std::getline(in, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      std::stringstream ss(line);
      std::string item;
      std::vector<double> fields;
      while (std::getline(ss, item, ',')) {
        try {
          fields.push_back(std::stod(item));
        } catch (...) {
          fields.clear();
          break;
        }
      }
      if (fields.size() >= 3) {
        ReferenceDatum datum{};
        datum.energy_keV = fields[0];
        datum.mu_cm2_g = fields[1];
        datum.mu_en_cm2_g = fields[2];
        table.push_back(datum);
      }
    }

    if (!table.empty()) {
      std::sort(table.begin(), table.end(), [](const ReferenceDatum& lhs, const ReferenceDatum& rhs) {
        return lhs.energy_keV < rhs.energy_keV;
      });
      fReferenceData = std::move(table);
      fReferenceAvailable = true;
      fReferenceSource = path;
      G4cout << " [nist] Loaded reference data from " << fReferenceSource << G4endl;
      return true;
    }
    G4cout << " [nist] Reference file " << path << " contained no data" << G4endl;
  }

  fReferenceAvailable = false;
  fReferenceSource = "";
  return false;
}

bool RunAction::InterpolateReference(G4double energy_keV,
                                     G4double& mu_cm2_g,
                                     G4double& mu_en_cm2_g) const
{
  mu_cm2_g = 0.;
  mu_en_cm2_g = 0.;
  if (!EnsureReferenceDataLoaded() || !fReferenceAvailable || fReferenceData.empty()) {
    return false;
  }

  if (energy_keV <= fReferenceData.front().energy_keV) {
    mu_cm2_g = fReferenceData.front().mu_cm2_g;
    mu_en_cm2_g = fReferenceData.front().mu_en_cm2_g;
    return true;
  }

  if (energy_keV >= fReferenceData.back().energy_keV) {
    mu_cm2_g = fReferenceData.back().mu_cm2_g;
    mu_en_cm2_g = fReferenceData.back().mu_en_cm2_g;
    return true;
  }

  const auto upper = std::lower_bound(
    fReferenceData.begin(), fReferenceData.end(), energy_keV,
    [](const ReferenceDatum& datum, G4double value) { return datum.energy_keV < value; });

  if (upper == fReferenceData.begin() || upper == fReferenceData.end()) {
    return false;
  }

  const auto lower = upper - 1;
  if (fUseLogInterpolation &&
      lower->mu_cm2_g > 0. && upper->mu_cm2_g > 0. &&
      lower->mu_en_cm2_g > 0. && upper->mu_en_cm2_g > 0.) {
    const G4double logE = std::log(energy_keV);
    const G4double logE1 = std::log(lower->energy_keV);
    const G4double logE2 = std::log(upper->energy_keV);
    const G4double weight = (logE2 - logE1) > 0. ? (logE - logE1) / (logE2 - logE1) : 0.;
    const G4double logMu1 = std::log(lower->mu_cm2_g);
    const G4double logMu2 = std::log(upper->mu_cm2_g);
    const G4double logMuEn1 = std::log(lower->mu_en_cm2_g);
    const G4double logMuEn2 = std::log(upper->mu_en_cm2_g);
    mu_cm2_g = std::exp(logMu1 + weight * (logMu2 - logMu1));
    mu_en_cm2_g = std::exp(logMuEn1 + weight * (logMuEn2 - logMuEn1));
  } else {
    const G4double span = upper->energy_keV - lower->energy_keV;
    const G4double fraction = (span > 0.) ? (energy_keV - lower->energy_keV) / span : 0.;
    mu_cm2_g = lower->mu_cm2_g + fraction * (upper->mu_cm2_g - lower->mu_cm2_g);
    mu_en_cm2_g = lower->mu_en_cm2_g + fraction * (upper->mu_en_cm2_g - lower->mu_en_cm2_g);
  }
  return true;
}

G4double RunAction::ComputeComptonRetentionFraction(G4double energy) const
{
  if (energy <= 0.) {
    return 1.0;
  }

  const G4double alpha = energy / electron_mass_c2;
  const G4int stepCount = std::max(2, fComptonIntegrationSteps);
  const std::size_t steps = static_cast<std::size_t>((stepCount % 2 == 0) ? stepCount : stepCount + 1);
  const G4double dcos = 2.0 / static_cast<G4double>(steps);
  const G4double prefactor = twopi * classic_electr_radius * classic_electr_radius * 0.5;

  auto integrand = [alpha, prefactor](G4double cosTheta) {
    const G4double energyRatio = 1.0 / (1.0 + alpha * (1.0 - cosTheta));
    const G4double sin2Theta = 1.0 - cosTheta * cosTheta;
    const G4double term = energyRatio + 1.0 / energyRatio - sin2Theta;
    return prefactor * energyRatio * energyRatio * term;
  };

  G4double total = 0.0;
  G4double weighted = 0.0;
  for (std::size_t i = 0; i <= steps; ++i) {
    const G4double cosTheta = -1.0 + i * dcos;
    const G4double weight = (i == 0 || i == steps) ? 1.0 : (i % 2 == 0 ? 2.0 : 4.0);
    const G4double diffXS = integrand(cosTheta);
    const G4double energyRatio = 1.0 / (1.0 + alpha * (1.0 - cosTheta));
    total += weight * diffXS;
    weighted += weight * diffXS * energyRatio;
  }

  total *= dcos / 3.0;
  weighted *= dcos / 3.0;
  if (total <= 0.) {
    return 1.0;
  }
  return weighted / total;
}

void RunAction::WriteSummaryFile() const
{
  if (fSummaryRows.empty()) {
    return;
  }

  const std::string filename = "transmission_summary.csv";
  std::ifstream headerCheck(filename);
  const bool fileExists = headerCheck.good();
  headerCheck.close();

  std::ofstream out(filename, std::ios::out | std::ios::app);
  if (!out) {
    G4cerr << "[RunAction] Failed to open " << filename << " for writing" << G4endl;
    return;
  }

  if (!fileExists) {
    out << "run_id,world_half_cm,thickness_nm,backing_thickness_um,backing_material,density_g_cm3,E_keV,";
    out << "N_injected,N_uncollided,N_scattered,N_trans_total,T_counts,T_counts_scattered,T_counts_clamped,clamp_flag,";
    out << "mu_counts_per_mm,mu_counts_cm2_g,mu_calc_per_mm,mu_calc_cm2_g,";
    out << "mu_tr_per_mm,mu_tr_cm2_g,mu_ref_cm2_g,mu_en_ref_cm2_g,";
    out << "delta_mu_percent,delta_mu_en_percent,sigma_T_counts,sigma_mu_counts_cm2_g,";
    out << "mu_en_cpe_per_mm,mu_en_cpe_cm2_g,delta_mu_en_cpe_percent,delta_mu_counts_vs_mu_calc_percent,delta_mu_en_cpe_vs_mu_tr_percent,";
    out << "absorbed_fraction,absorbed_fraction_slab,mu_en_per_mm,mu_en_cm2_g,mu_en_raw_per_mm,mu_en_raw_cm2_g,";
    out << "mu_en_raw_slab_per_mm,mu_en_raw_slab_cm2_g,mu_eff_per_mm,mu_eff_cm2_g,";
    out << "E_trans_unc_keV,E_trans_tot_keV,E_abs_keV,E_abs_backing_keV,E_abs_slab_keV,E_abs_other_keV,T_energy_unc,T_energy_tot" << '\n';
  }

  out.setf(std::ios::scientific);
  out << std::setprecision(10);

  for (const auto& row : fSummaryRows) {
    out << row.runID << ','
        << row.worldHalf_cm << ','
        << row.thickness_nm << ','
        << row.backingThickness_um << ','
        << row.backingMaterial << ','
        << row.density_g_cm3 << ','
        << row.energy_keV << ','
        << row.totalInjected << ','
        << row.transmittedUncollided << ','
        << row.transmittedScattered << ','
        << row.transmittedTotal << ','
        << row.T_counts << ','
        << row.T_counts_scattered << ','
        << row.T_counts_clamped << ','
        << row.clamp_flag << ','
        << row.mu_counts_per_mm << ','
        << row.mu_counts_cm2_g << ','
        << row.mu_calc_per_mm << ','
        << row.mu_calc_cm2_g << ','
        << row.mu_tr_per_mm << ','
        << row.mu_tr_cm2_g << ','
        << row.mu_ref_cm2_g << ','
        << row.mu_en_ref_cm2_g << ','
        << row.delta_mu_percent << ','
        << row.delta_mu_en_percent << ','
        << row.sigma_T_counts << ','
        << row.sigma_mu_counts_cm2_g << ','
        << row.mu_en_cpe_per_mm << ','
        << row.mu_en_cpe_cm2_g << ','
        << row.delta_mu_en_cpe_percent << ','
        << row.delta_mu_counts_vs_calc_percent << ','
        << row.delta_mu_en_cpe_vs_mu_tr_percent << ','
        << row.absorbedFraction << ','
        << row.absorbedFractionSlab << ','
        << row.mu_en_per_mm << ','
        << row.mu_en_cm2_g << ','
        << row.mu_en_raw_per_mm << ','
        << row.mu_en_raw_cm2_g << ','
        << row.mu_en_raw_slab_per_mm << ','
        << row.mu_en_raw_slab_cm2_g << ','
        << row.mu_eff_per_mm << ','
        << row.mu_eff_cm2_g << ','
        << row.E_trans_unc_keV << ','
        << row.E_trans_tot_keV << ','
        << row.E_abs_keV << ','
        << row.E_abs_backing_keV << ','
        << row.E_abs_slab_keV << ','
        << row.E_abs_other_keV << ','
        << row.T_energy_unc << ','
        << row.T_energy_tot << '\n';
  }
}
