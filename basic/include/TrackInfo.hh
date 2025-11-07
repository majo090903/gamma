// ********************************************************************
// * License and Disclaimer                                           *
// ********************************************************************

#ifndef TrackInfo_h
#define TrackInfo_h 1

#include "globals.hh"
#include "G4VUserTrackInformation.hh"

class TrackInfo : public G4VUserTrackInformation
{
public:
  TrackInfo();
  ~TrackInfo() override = default;

  void SetScattered(G4bool value = true) { fHasScattered = value; }
  G4bool HasScattered() const { return fHasScattered; }

private:
  G4bool fHasScattered;
};

#endif
