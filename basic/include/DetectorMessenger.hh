// ********************************************************************
// * License and Disclaimer                                           *
// ********************************************************************

#ifndef DetectorMessenger_h
#define DetectorMessenger_h 1

#include "globals.hh"
#include "G4UImessenger.hh"

class DetectorConstruction;
class G4UIdirectory;
#include "G4UIcmdWithADoubleAndUnit.hh"

class DetectorMessenger: public G4UImessenger
{
public:
  explicit DetectorMessenger(DetectorConstruction*);
  ~DetectorMessenger() override;

  void SetNewValue(G4UIcommand*, G4String) override;

private:
  DetectorConstruction*       fDetector;

  G4UIdirectory*              fRootDir;
  G4UIdirectory*              fSetupDir;
  G4UIcmdWithADoubleAndUnit*  fFoilThicknessCmd;
  G4UIcmdWithADoubleAndUnit*  fWorldHalfCmd;
};
#endif
