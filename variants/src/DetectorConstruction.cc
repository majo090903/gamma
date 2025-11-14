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

#include "DetectorConstruction.hh"
#include "DetectorMessenger.hh"

#include "G4Box.hh"
#include "G4LogicalVolume.hh"
#include "G4PVPlacement.hh"
#include "G4RunManager.hh"
#include "G4ThreeVector.hh"

#include "G4GeometryManager.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4PhysicalVolumeStore.hh"
#include "G4Region.hh"
#include "G4RegionStore.hh"
#include "G4SolidStore.hh"

#include "G4Material.hh"
#include "G4NistManager.hh"
#include "G4ProductionCuts.hh"
#include "G4SystemOfUnits.hh"
#include "G4UnitsTable.hh"
#include "G4VisAttributes.hh"
#include "G4Colour.hh"

DetectorConstruction::DetectorConstruction()
  : fWorldSolid(nullptr),
    fWorldLogical(nullptr),
    fWorldPhysical(nullptr),
    fFoilSolid(nullptr),
    fFoilLogical(nullptr),
    fFoilPhysical(nullptr),
    fBackingSolid(nullptr),
    fBackingLogical(nullptr),
    fBackingPhysical(nullptr),
    fMessenger(nullptr),
    fFoilMaterial(nullptr),
    fBackingMaterial(nullptr),
    fVacuumMaterial(nullptr),
    fWorldHalfLength(5.0 * cm),
    fFoilThickness(250.0 * nm),
    fBackingThickness(0.05 * mm)
{
  DefineMaterials();
  fMessenger = new DetectorMessenger(this);
}

DetectorConstruction::~DetectorConstruction()
{
  delete fMessenger;
}

void DetectorConstruction::DefineMaterials()
{
  auto* nist = G4NistManager::Instance();
  fFoilMaterial = nist->FindOrBuildMaterial("G4_W");
  fBackingMaterial = fFoilMaterial;
  fVacuumMaterial = nist->FindOrBuildMaterial("G4_Galactic");
}

G4VPhysicalVolume* DetectorConstruction::Construct()
{
  return ConstructVolumes();
}

void DetectorConstruction::SetFoilThickness(G4double value)
{
  if (value <= 0.) {
    G4cout << "[DetectorConstruction] Ignoring non-positive foil thickness: "
           << value / nm << " nm" << G4endl;
    return;
  }

  if (value >= 2.0 * fWorldHalfLength) {
    G4cout << "[DetectorConstruction] Requested foil thickness exceeds world size. "
           << "Increase world half-length first." << G4endl;
    return;
  }

  fFoilThickness = value;
  UpdateGeometry();
}

void DetectorConstruction::SetWorldHalfLength(G4double value)
{
  if (value <= 0.) {
    G4cout << "[DetectorConstruction] Ignoring non-positive world half-length: "
           << value / mm << " mm" << G4endl;
    return;
  }

  if (value <= 0.5 * fFoilThickness) {
    G4cout << "[DetectorConstruction] World half-length must exceed half the foil thickness."
           << G4endl;
    return;
  }

  fWorldHalfLength = value;
  UpdateGeometry();
}

void DetectorConstruction::SetBackingThickness(G4double value)
{
  if (value < 0.) {
    G4cout << "[DetectorConstruction] Ignoring negative backing thickness: "
           << value / mm << " mm" << G4endl;
    return;
  }

  if (value >= 2.0 * fWorldHalfLength) {
    G4cout << "[DetectorConstruction] Requested backing thickness exceeds world size. "
           << "Increase world half-length first." << G4endl;
    return;
  }

  fBackingThickness = value;
  UpdateGeometry();
}

void DetectorConstruction::SetBackingMaterial(const G4String& name)
{
  if (name.empty()) {
    G4cout << "[DetectorConstruction] Ignoring empty backing material request." << G4endl;
    return;
  }

  auto* material = G4Material::GetMaterial(name, false);
  if (!material) {
    auto* nist = G4NistManager::Instance();
    material = nist->FindOrBuildMaterial(name, false);
  }

  if (!material) {
    G4cout << "[DetectorConstruction] Backing material '" << name << "' not found." << G4endl;
    return;
  }

  fBackingMaterial = material;
  G4cout << "[DetectorConstruction] Backing material set to " << material->GetName() << G4endl;
  UpdateGeometry();
}

G4String DetectorConstruction::GetBackingMaterialName() const
{
  if (fBackingMaterial) {
    return fBackingMaterial->GetName();
  }
  if (fFoilMaterial) {
    return fFoilMaterial->GetName();
  }
  return "";
}

void DetectorConstruction::UpdateGeometry()
{
  if (auto* runManager = G4RunManager::GetRunManager()) {
    runManager->DefineWorldVolume(ConstructVolumes(), true);
  }
}

void DetectorConstruction::BuildFoilRegion(G4LogicalVolume* foilLogical)
{
  auto* regionStore = G4RegionStore::GetInstance();
  if (auto* existing = regionStore->GetRegion("FoilRegion", false)) {
    regionStore->DeRegister(existing);
    delete existing;
  }

  auto* region = new G4Region("FoilRegion");
  auto* cuts = new G4ProductionCuts();
  const G4double cutValue = 20.0 * nm;
  cuts->SetProductionCut(cutValue, idxG4GammaCut);
  cuts->SetProductionCut(cutValue, idxG4ElectronCut);
  cuts->SetProductionCut(cutValue, idxG4PositronCut);
  region->SetProductionCuts(cuts);
  region->AddRootLogicalVolume(foilLogical);
}

void DetectorConstruction::BuildBackingRegion(G4LogicalVolume* backingLogical)
{
  if (!backingLogical) {
    return;
  }

  auto* regionStore = G4RegionStore::GetInstance();
  if (auto* existing = regionStore->GetRegion("BackingRegion", false)) {
    regionStore->DeRegister(existing);
    delete existing;
  }

  auto* region = new G4Region("BackingRegion");
  auto* cuts = new G4ProductionCuts();
  const G4double cutValue = 20.0 * nm;
  cuts->SetProductionCut(cutValue, idxG4GammaCut);
  cuts->SetProductionCut(cutValue, idxG4ElectronCut);
  cuts->SetProductionCut(cutValue, idxG4PositronCut);
  region->SetProductionCuts(cuts);
  region->AddRootLogicalVolume(backingLogical);
}

G4VPhysicalVolume* DetectorConstruction::ConstructVolumes()
{
  G4GeometryManager::GetInstance()->OpenGeometry();
  G4PhysicalVolumeStore::GetInstance()->Clean();
  G4LogicalVolumeStore::GetInstance()->Clean();
  G4SolidStore::GetInstance()->Clean();

  fWorldSolid = new G4Box("World", fWorldHalfLength, fWorldHalfLength, fWorldHalfLength);
  fWorldLogical = new G4LogicalVolume(fWorldSolid, fVacuumMaterial, "World");
  fWorldPhysical = new G4PVPlacement(nullptr,
                                     G4ThreeVector(),
                                     fWorldLogical,
                                     "World",
                                     nullptr,
                                     false,
                                     0);

  const G4double foilHalfXY = 0.8 * fWorldHalfLength;
  fFoilSolid = new G4Box("Foil", foilHalfXY, foilHalfXY, 0.5 * fFoilThickness);
  fFoilLogical = new G4LogicalVolume(fFoilSolid, fFoilMaterial, "Foil");
  fFoilPhysical = new G4PVPlacement(nullptr,
                                    G4ThreeVector(),
                                    fFoilLogical,
                                    "Foil",
                                    fWorldLogical,
                                    false,
                                    0);

  if (fBackingThickness > 0.) {
    fBackingSolid = new G4Box("Backing", foilHalfXY, foilHalfXY, 0.5 * fBackingThickness);
    auto* backingMaterial = fBackingMaterial ? fBackingMaterial : fFoilMaterial;
    fBackingLogical = new G4LogicalVolume(fBackingSolid, backingMaterial, "Backing");
    const G4double zPlacement = 0.5 * (fFoilThickness + fBackingThickness);
    fBackingPhysical = new G4PVPlacement(nullptr,
                                         G4ThreeVector(0., 0., zPlacement),
                                         fBackingLogical,
                                         "Backing",
                                         fWorldLogical,
                                         false,
                                         0);
  } else {
    fBackingSolid = nullptr;
    fBackingLogical = nullptr;
    fBackingPhysical = nullptr;
  }

  BuildFoilRegion(fFoilLogical);
  BuildBackingRegion(fBackingLogical);

  auto* worldVis = new G4VisAttributes();
  worldVis->SetVisibility(false);
  fWorldLogical->SetVisAttributes(worldVis);

  auto* foilVis = new G4VisAttributes(G4Colour(0.4, 0.4, 0.8));
  foilVis->SetForceSolid(true);
  fFoilLogical->SetVisAttributes(foilVis);

  if (fBackingLogical) {
    auto* backingVis = new G4VisAttributes(G4Colour(0.2, 0.6, 0.2));
    backingVis->SetForceSolid(true);
    fBackingLogical->SetVisAttributes(backingVis);
  }

  G4cout << "------------------------------------------------------------" << G4endl;
  G4cout << " World half-length : " << G4BestUnit(fWorldHalfLength, "Length") << G4endl;
  G4cout << " Foil thickness    : " << G4BestUnit(fFoilThickness, "Length") << G4endl;
  G4cout << " Foil material     : " << fFoilMaterial->GetName() << G4endl;
  G4cout << " Backing thickness : " << G4BestUnit(fBackingThickness, "Length") << G4endl;
  if (fBackingLogical && fBackingLogical->GetMaterial()) {
    G4cout << " Backing material  : " << fBackingLogical->GetMaterial()->GetName() << G4endl;
  }
  G4cout << "------------------------------------------------------------" << G4endl;
  G4cout << "(Info) e-/e+ lines in the following 'Table of registered couples' report"
            " secondary transport thresholds, not additional primary beams." << G4endl;

  return fWorldPhysical;
}
