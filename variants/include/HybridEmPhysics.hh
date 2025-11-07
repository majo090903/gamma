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

#ifndef HybridEmPhysics_hh
#define HybridEmPhysics_hh

#include "G4VPhysicsConstructor.hh"
#include "G4SystemOfUnits.hh"

class G4EmStandardPhysics_option4;

// Hybrid electromagnetic physics: Penelope models below a configurable
// transition energy, Option4 models above. Atomic de-excitation remains enabled.
class HybridEmPhysics : public G4VPhysicsConstructor
{
public:
  explicit HybridEmPhysics(G4double transitionEnergy = 200. * keV);
  ~HybridEmPhysics() override;

  void ConstructParticle() override;
  void ConstructProcess() override;

private:
  HybridEmPhysics(const HybridEmPhysics&) = delete;
  HybridEmPhysics& operator=(const HybridEmPhysics&) = delete;

  G4double fTransitionEnergy;
  G4EmStandardPhysics_option4* fOption4;
};

#endif
