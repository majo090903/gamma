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

#include "DetectorMessenger.hh"

#include "DetectorConstruction.hh"

#include "G4SystemOfUnits.hh"
#include "G4UIdirectory.hh"

DetectorMessenger::DetectorMessenger(DetectorConstruction* detector)
  : fDetector(detector),
    fRootDir(nullptr),
    fSetupDir(nullptr),
    fFoilThicknessCmd(nullptr),
    fWorldHalfCmd(nullptr)
{
  fRootDir = new G4UIdirectory("/det/");
  fRootDir->SetGuidance("Detector configuration commands");

  fSetupDir = new G4UIdirectory("/det/setup/");
  fSetupDir->SetGuidance("Setup control");

  fFoilThicknessCmd =
    new G4UIcmdWithADoubleAndUnit("/det/setWThickness", this);
  fFoilThicknessCmd->SetGuidance("Set tungsten foil thickness");
  fFoilThicknessCmd->SetParameterName("Thickness", false);
  fFoilThicknessCmd->SetUnitCategory("Length");
  fFoilThicknessCmd->SetDefaultUnit("nm");
  fFoilThicknessCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fWorldHalfCmd =
    new G4UIcmdWithADoubleAndUnit("/det/setWorldHalf", this);
  fWorldHalfCmd->SetGuidance("Set world half-length (same for x,y,z)");
  fWorldHalfCmd->SetParameterName("HalfLength", false);
  fWorldHalfCmd->SetUnitCategory("Length");
  fWorldHalfCmd->SetDefaultUnit("cm");
  fWorldHalfCmd->AvailableForStates(G4State_PreInit, G4State_Idle);

  fBackingThicknessCmd =
    new G4UIcmdWithADoubleAndUnit("/det/setBackingThickness", this);
  fBackingThicknessCmd->SetGuidance("Set thickness of the downstream backing layer (0 disables it)");
  fBackingThicknessCmd->SetParameterName("Backing", false);
  fBackingThicknessCmd->SetUnitCategory("Length");
  fBackingThicknessCmd->SetDefaultUnit("mm");
  fBackingThicknessCmd->AvailableForStates(G4State_PreInit, G4State_Idle);
}

DetectorMessenger::~DetectorMessenger()
{
  delete fFoilThicknessCmd;
  delete fWorldHalfCmd;
  delete fBackingThicknessCmd;
  delete fSetupDir;
  delete fRootDir;
}

void DetectorMessenger::SetNewValue(G4UIcommand* command, G4String newValue)
{
  if (command == fFoilThicknessCmd) {
    fDetector->SetFoilThickness(fFoilThicknessCmd->GetNewDoubleValue(newValue));
  } else if (command == fWorldHalfCmd) {
    fDetector->SetWorldHalfLength(fWorldHalfCmd->GetNewDoubleValue(newValue));
  } else if (command == fBackingThicknessCmd) {
    fDetector->SetBackingThickness(fBackingThicknessCmd->GetNewDoubleValue(newValue));
  }
}
