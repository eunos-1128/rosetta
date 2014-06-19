#!/usr/bin/env bash
# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org. Questions about this can be
# (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

#First make sure that all the options files are properly built.
#./update_options.sh

# This doesn't work, passes too many arguments and errors out
#ctags `find src/core -name "*.cc" -or -name "*.hh"`
#ctags --append `find src/protocols -name "*.cc" -or -name "*.hh"`
#ctags --append `find src/devel -name "*.cc" -or -name "*.hh"`
#ctags --append `find src/utility -name "*.cc" -or -name "*.hh"`
#ctags --append `find src/numeric -name "*.cc" -or -name "*.hh"`

# The -L flag requires Exuberant Ctags
find ./src/{apps,core,devel,numeric,protocols,utility} test -name "*.cc" -o -name "*.hh" > tagged_files.txt
ctags -L tagged_files.txt
cscope -bqk -i tagged_files.txt
