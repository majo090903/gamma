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

#include "SteppingAction.hh"

#include "RunAction.hh"
#include "TrackInfo.hh"

#include "G4ParticleDefinition.hh"
#include "G4Step.hh"
#include "G4StepPoint.hh"
#include "G4Track.hh"
#include "G4VProcess.hh"

SteppingAction::SteppingAction(RunAction* run)
  : G4UserSteppingAction(),
    fRunAction(run)
{}

SteppingAction::~SteppingAction() = default;

void SteppingAction::UserSteppingAction(const G4Step* step)
{
  auto* track = step->GetTrack();
  const auto* postPoint = step->GetPostStepPoint();
  const auto* process = postPoint->GetProcessDefinedStep();
  if (process) {
    fRunAction->CountProcesses(process->GetProcessName());
  }

  const auto* particle = track->GetDefinition();
  if (!particle || particle->GetParticleName() != "gamma") {
    return;
  }

  auto* info = static_cast<TrackInfo*>(track->GetUserInformation());
  if (!info) {
    info = new TrackInfo();
    track->SetUserInformation(info);
  }

  if (track->GetTrackID() != 1) {
    info->SetScattered();
  }

  if (process && process->GetProcessName() != "Transportation") {
    info->SetScattered();
  }

  const auto* prePoint = step->GetPreStepPoint();
  if (!prePoint) {
    return;
  }

  const auto* preVolume = prePoint->GetPhysicalVolume();
  const auto* postVolume = postPoint->GetPhysicalVolume();
  if (!preVolume || !postVolume) {
    return;
  }
  const auto preName = preVolume->GetName();

  const auto edep = step->GetTotalEnergyDeposit();
  if (edep > 0.) {
    if (preName == "Foil") {
      fRunAction->AddFoilDepositedEnergy(edep);
    } else if (preName == "Backing") {
      fRunAction->AddBackingDepositedEnergy(edep);
    } else {
      fRunAction->AddOtherDepositedEnergy(edep);
    }
  }

  if (prePoint->GetStepStatus() != fGeomBoundary || postPoint->GetStepStatus() != fGeomBoundary) {
    return;
  }

  const auto postName = postVolume->GetName();
  const G4bool fromFoilToBacking = (preName == "Foil" && postName == "Backing");
  const G4bool fromFoilToWorld   = (preName == "Foil" && postName == "World");
  const G4bool fromBackingToWorld = (preName == "Backing" && postName == "World");

  G4bool recordTransmission = false;
  G4bool stopTrack = false;

  if (fromFoilToBacking) {
    if (!info->TransmissionLogged()) {
      recordTransmission = true;
    }
  } else if (fromFoilToWorld) {
    if (!info->TransmissionLogged()) {
      recordTransmission = true;
    }
    stopTrack = true;
  } else if (fromBackingToWorld) {
    if (!info->TransmissionLogged()) {
      recordTransmission = true;
    }
    stopTrack = true;
  } else {
    return;
  }

  const auto& direction = postPoint->GetMomentumDirection();
  if (direction.z() <= 0.) {
    return;
  }

  if (recordTransmission) {
    const auto energy = postPoint->GetKineticEnergy();
    const G4bool scattered = info->HasScattered();
    fRunAction->RecordTransmission(energy, direction.z(), scattered);
    info->SetTransmissionLogged();
  }

  if (stopTrack) {
    track->SetTrackStatus(fStopAndKill);
  }
}
