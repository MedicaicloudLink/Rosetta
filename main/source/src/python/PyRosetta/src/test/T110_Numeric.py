# :noTabs=true:
# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org. Questions about this can be
# (c) addressed to University of Washington CoMotion, email: license@uw.edu.

## @author Sergey Lyskov

from __future__ import print_function

import pyrosetta
import pyrosetta.rosetta as rosetta

pyrosetta.init(extra_options = "-constant_seed")  # WARNING: option '-constant_seed' is for testing only! MAKE SURE TO REMOVE IT IN PRODUCTION RUNS!!!!!
import os; os.chdir('.test.output')


I = rosetta.numeric.xyzMatrix_double_t.identity()
V = rosetta.numeric.xyzVector_double_t()

a = I * V


m = rosetta.numeric.xyzMatrix_double_t.identity()
m = m * m


m.xx = 1;  m.xy = 2;  m.xz = 3;
m.yx = 4;  m.yy = 5;  m.yz = 6;
m.zx = 7;  m.zy = 8;  m.zz = 9;

assert( m.xx == 1 );  assert( m.xy == 2 );  assert( m.xz == 3 );
assert( m.yx == 4 );  assert( m.yy == 5 );  assert( m.yz == 6 );
assert( m.zx == 7 );  assert( m.zy == 8 );  assert( m.zz == 9 );
