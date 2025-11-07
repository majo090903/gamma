// ********************************************************************
// * License and Disclaimer                                           *
// ********************************************************************

#ifndef ProcessesCount_HH
#define ProcessesCount_HH

#include "globals.hh"
#include <vector>

class OneProcessCount
{
public:
  explicit OneProcessCount(G4String name) : Name(name), Counter(0) {}
  ~OneProcessCount() = default;

  G4String GetName() { return Name; }
  G4int    GetCounter() { return Counter; }
  void     Count() { Counter++; }

private:
  G4String Name;
  G4int    Counter;
};

using ProcessesCount = std::vector<OneProcessCount*>;

#endif
