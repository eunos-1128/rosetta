#!/usr/bin/python

# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org. Questions about this can be
# (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

## @file   /GUIs/tests/Test_General_Functions
## @brief  PyRosetta Toolkit Test script
## @author Jared Adolf-Bryfogle (jadolfbr@gmail.com)

#Python Imports
import os
import sys
rel = "../"
sys.path.append(os.path.abspath(rel))
                
                
#Toolkit Imports
from modules.tools import general_tools

#Rosetta Imports
from rosetta import *
rosetta.init()

p = pose_from_pdb(os.path.dirname(os.path.abspath(__file__))+"/data/2j88.pdb")

print "Testing atom distance measure: "
print repr(general_tools.getDist(p, 5, 10, "CA", "CA"))

print "Getting platform: "
print repr(general_tools.getOS())

print "Test region conversion functions"
loop_string = "24:42:L"
loops_as_strings = []; loops_as_strings.append(loop_string)
regions = general_tools.loops_as_strings_to_regions(loops_as_strings)
print str(regions)
region = general_tools.loop_string_to_region(loop_string)
print str(region)

#Renameandsave untested.

print "Complete."


    