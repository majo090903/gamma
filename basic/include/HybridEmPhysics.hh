// ********************************************************************
// * License and Disclaimer                                           *
// ********************************************************************

#ifndef HybridEmPhysics_hh
#define HybridEmPhysics_hh

#include "G4VPhysicsConstructor.hh"
#include "G4SystemOfUnits.hh"

class G4EmStandardPhysics_option4;

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
