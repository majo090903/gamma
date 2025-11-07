#!/usr/bin/python

import os
import math
from array import *

#-----fluka---------------
def GiveMeDensity (mat):
  if mat == 'BLOOD':          density=1.06
  if mat == 'BONECOMP':       density=1.85
  if mat == 'LUNG':           density=1.05
  if mat == 'EYE-LENS':       density=1.1
  if mat == 'EYELENS':       density=1.1
  if mat == 'Adipose_':       density=0.92
  if mat == 'TISSUEIC':       density=1.00
  if mat == 'BRAIN':          density=1.039
  if mat == 'MuscleEl':       density=1.04
  if mat == 'SKIN_EL':        density=1.1

  return density
#-------------------------


"""
material = ['BLOOD','BONECOMP','LUNG','EYELENS','Adipose_','TISSUEIC','BRAIN','MuscleEl','SKIN_EL']
energy   = [60, 80, 150, 400, 500, 600, 1000, 1250, 1500, 2000]
"""

material = ['BLOOD']
energy   = [60, 80]
event    = 1000
cycle    = 100

# create a macro template
macrotemplate="""#attenuation macro
/control/verbose 2
/run/verbose 2
/mygeom/setup/detectorMaterial {tmaterial}
/testem/phys/addPhysics standard
/run/initialize
/testem/phys/setCuts 1 nm
#/process/eLoss/binsDEDX 1000
/gps/particle gamma
/gps/energy {tenergy} keV
/gps/direction 0 0 1
/gps/position 0 0 -1 cm
/random/setSavingFlag 1
/random/saveThisRun
/random/resetEngineFrom currentEvent.rndm	
/run/beamOn {tevent}
"""
os.system('rm -f results.data')
fw = open('results.data','a')

for fmaterial in material:
  for fenergy in energy:

      # generate macro for this run
      fmac = fmaterial +'_'+ str(fenergy) + 'keV'
      f=open(fmac + '.mac','w')
      macfile=macrotemplate.format(tmaterial=fmaterial, tenergy=fenergy, tevent=event)
      f.write(macfile)
      f.close()

      # start cycle for this run
      count = 0
      sumattenu = 0
      vattenu = array('f')
      os.system('rm -f ' + finp +'_results.data')
      fww = open(fmac +'_results.data','a')

      while count < cycle:
        os.system('./attenuation ' + fmac + '.mac' )
        fr = open('rn.out','r')      
        content = fr.read()
        nenter  = int(content.split(' ')[0])
        nfwexit = int(content.split(' ')[1])
        nbwexit = int(content.split(' ')[2])
        attenu  = float(content.split(' ')[3])
        #unit    = str(content.split(' ')[3])
        #if unit == "cm2/g":    attenu = attenu * 100
        sumattenu += attenu
        vattenu.append(attenu)
        fww.write( fmac + '\t' + str(nenter) + '\t' + str(nfwexit) + '\t' + str(nbwexit) + '\t' + str(attenu) + '\n' )
        count = count + 1
        os.system('rm -f rn.out')
      
      # when cycle ends, evalute standart deviation for whole cycles
      sdattenu = 0
      count = 0
      avattenu=sumattenu/cycle
      while count < cycle:
        sdattenu = math.pow( (vattenu[count] - avattenu) , 2) / ((cycle) * (cycle-1))
        count = count + 1 
      sdattenu = math.sqrt( sdattenu)
      fw.write( fmac + '\t ' + str(avattenu) + ' +- ' + str(sdattenu)  + '\n' )

# end of runMe.py clear unncessary files
os.system('rm -f *mac *.out')       



#        os.system('mv attenuation.dat ' + fmac + '_' + str(count) + '.data')
#        fr = open( fmac + '_' + str(count) + '.data','r')
#os.system('mv gamma.root ' + fmac + '.root')
#os.system('mv *.root results/gamma')
#os.system('mv *.txt results')



