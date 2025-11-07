// ********************************************************************
// * License and Disclaimer                                           *
// ********************************************************************

#ifndef PhysicsListMessenger_h
#define PhysicsListMessenger_h 1

#include "G4UImessenger.hh"
#include "globals.hh"

class PhysicsList;
class G4UIdirectory;
class G4UIcmdWithADoubleAndUnit;
class G4UIcmdWithAString;

class PhysicsListMessenger : public G4UImessenger
{
public:
  explicit PhysicsListMessenger(PhysicsList*);
  ~PhysicsListMessenger() override;

  void SetNewValue(G4UIcommand*, G4String) override;

private:
  PhysicsList*               fPhysicsList;

  G4UIdirectory*             fPhysDir;
  G4UIcmdWithADoubleAndUnit* fGammaCutCmd;
  G4UIcmdWithADoubleAndUnit* fElectCutCmd;
  G4UIcmdWithADoubleAndUnit* fProtoCutCmd;
  G4UIcmdWithADoubleAndUnit* fAllCutCmd;
  G4UIcmdWithAString*        fListCmd;
};

#endif
