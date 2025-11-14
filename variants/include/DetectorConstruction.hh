
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

/**********************************************************************
     Version :  Geant4.10.0.p02
     Created :  25/10/2014
     Author  :  Ercan Pilicer
     Company :  Uludag University
**********************************************************************/

#ifndef DetectorConstruction_H
#define DetectorConstruction_H 1

#include "G4VUserDetectorConstruction.hh"
#include "G4ThreeVector.hh"
#include "G4VPhysicalVolume.hh"
#include "G4Material.hh"
#include "G4String.hh"
#include "globals.hh"

class G4Box;
class G4VPhysicalVolume;
class G4LogicalVolume;
class DetectorMessenger;
class G4Material;

class DetectorConstruction : public G4VUserDetectorConstruction
{
public:
  DetectorConstruction();
  ~DetectorConstruction() override;

public:

  G4VPhysicalVolume* Construct() override;

  void SetFoilThickness(G4double value);
  void SetWorldHalfLength(G4double value);
  void SetBackingThickness(G4double value);
  void SetBackingMaterial(const G4String& name);

  void UpdateGeometry();
  G4Material* GetFoilMaterial() const { return fFoilMaterial; }
  G4Material* GetBackingMaterial() const { return fBackingMaterial; }
  G4String GetBackingMaterialName() const;
  G4Material* GetMaterial() { return fFoilMaterial; }
  G4double GetFoilThickness() const { return fFoilThickness; }
  G4double GetWorldHalfLength() const { return fWorldHalfLength; }
  G4double GetBackingThickness() const { return fBackingThickness; }

private:

  void DefineMaterials();
  G4VPhysicalVolume* ConstructVolumes();
  void BuildFoilRegion(G4LogicalVolume* foilLogical);
  void BuildBackingRegion(G4LogicalVolume* backingLogical);

  G4Box*             fWorldSolid;
  G4LogicalVolume*   fWorldLogical;
  G4VPhysicalVolume* fWorldPhysical;

  G4Box*             fFoilSolid;
  G4LogicalVolume*   fFoilLogical;
  G4VPhysicalVolume* fFoilPhysical;

  G4Box*             fBackingSolid;
  G4LogicalVolume*   fBackingLogical;
  G4VPhysicalVolume* fBackingPhysical;

  DetectorMessenger* fMessenger;

  G4Material*        fFoilMaterial;
  G4Material*        fBackingMaterial;
  G4Material*        fVacuumMaterial;

  G4double           fWorldHalfLength;
  G4double           fFoilThickness;
  G4double           fBackingThickness;
};
#endif
