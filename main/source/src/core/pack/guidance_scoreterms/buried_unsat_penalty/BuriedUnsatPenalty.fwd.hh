// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file core/pack/guidance_scoreterms/buried_unsat_penalty/BuriedUnsatPenalty.fwd.hh
/// @brief Forward declarations for an EnergyMethod that gives a penalty for buried unsatisfied hydrogen bond donors and acceptors.
/// @details This energy method is inherently not pairwise decomposible.  However, it is intended for very rapid calculation,
/// and has been designed to plug into Alex Ford's modifications to the packer that permit it to work with non-pairwise scoring
/// terms.
/// @note This was updated on 21 September 2018 to ensure that, during a packing trajectorym, only buried unsatisfied groups that are (a) on packable
/// rotamers, or (b) that are on non-packable rotamers, but which can hydrogen-bond to at least one packable rotamer are the only ones that count towards
/// the penalty.  Without this, "native" buried unsats that are unreachable in a packing problem contribute unreasonably to the total score.  (Thanks to
/// Kale Kundert for identifying the problem.)
/// @author Vikram K. Mulligan (vmullig@uw.edu).


#ifndef INCLUDED_core_pack_guidance_scoreterms_buried_unsat_penalty_BuriedUnsatPenalty_fwd_hh
#define INCLUDED_core_pack_guidance_scoreterms_buried_unsat_penalty_BuriedUnsatPenalty_fwd_hh

// Utility headers
#include <utility/pointer/owning_ptr.hh>

namespace core {
namespace pack {
namespace guidance_scoreterms {
namespace buried_unsat_penalty {

class BuriedUnsatPenalty;

typedef utility::pointer::shared_ptr< BuriedUnsatPenalty > BuriedUnsatPenaltyOP;
typedef utility::pointer::shared_ptr< BuriedUnsatPenalty const > BuriedUnsatPenaltyCOP;


} // buried_unsat_penalty
} // guidance_scoreterms
} // pack
} // core


#endif
