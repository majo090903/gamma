// ********************************************************************
// * Hybrid electromagnetic physics constructor: Option4 base with   *
// * de-excitation enabled; Penelope-style settings can be extended. *
// ********************************************************************

#include "HybridEmPhysics.hh"

#include "G4EmParameters.hh"
#include "G4EmStandardPhysics_option4.hh"
#include "G4SystemOfUnits.hh"

HybridEmPhysics::HybridEmPhysics(G4double transitionEnergy)
  : G4VPhysicsConstructor("HybridEm"),
    fTransitionEnergy(transitionEnergy),
    fOption4(new G4EmStandardPhysics_option4())
{}

HybridEmPhysics::~HybridEmPhysics()
{
  delete fOption4;
}

void HybridEmPhysics::ConstructParticle()
{
  fOption4->ConstructParticle();
}

void HybridEmPhysics::ConstructProcess()
{
  fOption4->ConstructProcess();

  auto* params = G4EmParameters::Instance();
  params->SetMinEnergy(10. * eV);
  params->SetMaxEnergy(100. * TeV);
  params->SetVerbose(0);
  params->SetFluo(true);
  params->SetAuger(true);
  params->SetAugerCascade(true);
  params->SetPixe(false);
  params->SetDeexcitationIgnoreCut(false);
}
