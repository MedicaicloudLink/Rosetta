// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/rna/denovo/movers/RNA_DeNovoMasterMover.fwd.hh
/// @brief
/// @detailed
/// @author Rhiju Das, rhiju@stanford.edu


#ifndef INCLUDED_protocols_rna_denovo_movers_RNA_DeNovoMasterMover_FWD_HH
#define INCLUDED_protocols_rna_denovo_movers_RNA_DeNovoMasterMover_FWD_HH

#include <utility/pointer/owning_ptr.hh>

namespace protocols {
namespace rna {
namespace denovo {
namespace movers {

class RNA_DeNovoMasterMover;
typedef utility::pointer::shared_ptr< RNA_DeNovoMasterMover > RNA_DeNovoMasterMoverOP;
typedef utility::pointer::shared_ptr< RNA_DeNovoMasterMover const > RNA_DeNovoMasterMoverCOP;

} //movers
} //denovo
} //rna
} //protocols

#endif
