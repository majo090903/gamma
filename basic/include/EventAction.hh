// ********************************************************************
// * License and Disclaimer                                           *
// ********************************************************************

#ifndef EventAction_h
#define EventAction_h 1

#include "G4UserEventAction.hh"
#include "globals.hh"

class RunAction;
class G4Event;

class EventAction : public G4UserEventAction
{
public:
  explicit EventAction(RunAction*);
  ~EventAction() override;

  void BeginOfEventAction(const G4Event*) override;
  void EndOfEventAction(const G4Event*) override;

private:
  RunAction* runAct;
};

#endif
