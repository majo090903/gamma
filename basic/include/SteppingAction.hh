// ********************************************************************
// * License and Disclaimer                                           *
// ********************************************************************

#ifndef SteppingAction_h
#define SteppingAction_h 1

#include "G4UserSteppingAction.hh"
#include "G4ios.hh"
#include "globals.hh"

class RunAction;

class SteppingAction : public G4UserSteppingAction
{
public:
  explicit SteppingAction(RunAction*);
  ~SteppingAction() override;

  void UserSteppingAction(const G4Step*) override;

private:
  RunAction* fRunAction;
};
#endif
