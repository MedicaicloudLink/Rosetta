// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/moves/INCLUDED_protocols_simple_moves_SliceToMiniProteinMover_fwd_hh
/// @brief This mover chops a big protein into miniProteins
/// @author TJ Brunette (tjbrunette@gmail.com)

#ifndef INCLUDED_protocols_pose_creation_SliceToMiniProteinMover_fwd_hh
#define INCLUDED_protocols_pose_creation_SliceToMiniProteinMover_fwd_hh

#include <protocols/moves/MoverCreator.hh>

namespace protocols {
namespace pose_creation {

class SliceToMiniProteinMover;

typedef utility::pointer::shared_ptr< SliceToMiniProteinMover > SliceToMiniProteinMoverOP;
typedef utility::pointer::shared_ptr< SliceToMiniProteinMover const > SliceToMiniProteinMoverCOP;

} // pose_creation
} // protocols

#endif // INCLUDED_protocols_simple_moves_MergePdbMoverCreator_fwd_hh

