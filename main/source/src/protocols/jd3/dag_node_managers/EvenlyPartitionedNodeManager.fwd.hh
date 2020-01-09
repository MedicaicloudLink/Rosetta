// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/jd3/dag_node_managers/EvenlyPartitionedNodeManager.fwd.hh
/// @brief
/// @detailed
/// @author Jack Maguire, jackmaguire1444@gmail.com


#ifndef INCLUDED_protocols_jd3_dag_node_managers_EvenlyPartitionedNodeManager_FWD_HH
#define INCLUDED_protocols_jd3_dag_node_managers_EvenlyPartitionedNodeManager_FWD_HH

#include <utility/pointer/owning_ptr.hh>

namespace protocols {
namespace jd3 {
namespace dag_node_managers {

class EvenlyPartitionedNodeManager;
typedef utility::pointer::shared_ptr< EvenlyPartitionedNodeManager > EvenlyPartitionedNodeManagerOP;
typedef utility::pointer::shared_ptr< EvenlyPartitionedNodeManager const > EvenlyPartitionedNodeManagerCOP;

}
} //jd3
} //protocols

#endif
