// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file src/core/chemical/AtomTypeSet.fwd.hh
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)


#ifndef INCLUDED_core_chemical_AtomTypeSet_fwd_hh
#define INCLUDED_core_chemical_AtomTypeSet_fwd_hh

#include <utility/pointer/access_ptr.fwd.hh>
#include <utility/pointer/owning_ptr.fwd.hh>

namespace core {
namespace chemical {

class AtomTypeSet;

typedef  utility::pointer::weak_ptr< AtomTypeSet > AtomTypeSetAP;
typedef  utility::pointer::weak_ptr< AtomTypeSet const > AtomTypeSetCAP;
typedef  utility::pointer::shared_ptr< AtomTypeSet > AtomTypeSetOP;
typedef  utility::pointer::shared_ptr< AtomTypeSet const > AtomTypeSetCOP;

}
}

#endif