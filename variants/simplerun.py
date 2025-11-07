#!/usr/bin/python

import os

count = 0
while count < 10:
	os.system('./attenuation run.mac' )
	os.system('mv particles.txt particle_' + str(count) + '.txt')
	os.system('mv event_info.txt event_' + str(count) + '.txt')
	count = count + 1

os.system('mv particle_*.txt results/particle')
os.system('mv event_*.txt results/event')				

