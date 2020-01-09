// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/task_operations/StoreCompoundTaskMover.cc
/// @brief  Combine tasks using boolean logic for residues that are packable or designable, as
/// well as for residue specific AA sets, and store the resulting task in the pose's cacheable data.
/// @author Jacob Bale (balej@uw.edu)

#ifndef INCLUDED_protocols_task_operations_StoreCompoundTaskMover_fwd_hh
#define INCLUDED_protocols_task_operations_StoreCompoundTaskMover_fwd_hh

#include <utility/pointer/owning_ptr.hh>

namespace protocols {
namespace task_operations {

class StoreCompoundTaskMover;
typedef utility::pointer::shared_ptr< StoreCompoundTaskMover > StoreCompoundTaskMoverOP;
typedef utility::pointer::shared_ptr< StoreCompoundTaskMover const > StoreCompoundTaskMoverCOP;

enum boolean_operations {
	AND=1,
	OR,
	XOR,
	NOR,
	NAND,
	ORNOT,
	ANDNOT,
	NOT
};

} // task_operations
} // protocols

#endif
