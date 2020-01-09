// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file core/pack/hbonds/HBondGraph_util.hh
/// @brief A collections of methods that are useful for dealing with HBondGraphs. These are all methods that would normally resid in the HBondGraph itself but they need core.4 info and the HBondGraph needs to be in core.3
/// @author Jack Maguire, jackmaguire1444@gmail.com

#ifndef INCLUDED_core_pack_hbonds_HBondGraphUtil_HH
#define INCLUDED_core_pack_hbonds_HBondGraphUtil_HH

#include <core/pack/hbonds/HBondGraphInitializerIG.fwd.hh>
#include <core/pack/interaction_graph/PDInteractionGraph.hh>
#include <core/pack/interaction_graph/InteractionGraphBase.fwd.hh>
#include <core/pack/rotamer_set/RotamerSets.hh>

#include <core/scoring/hbonds/graph/HBondGraph.hh>
#include <core/scoring/hbonds/HBondDatabase.fwd.hh>
#include <core/scoring/hbonds/HBondSet.fwd.hh>

#include <core/conformation/Residue.fwd.hh>
#include <core/conformation/symmetry/SymmetryInfo.fwd.hh>
#include <core/pose/Pose.fwd.hh>

#include <utility/graph/Graph.hh>
#include <utility/pointer/memory.hh>


namespace core {
namespace pack {
namespace hbonds {

///@brief This should be called immediately after creating the graph (and setting the number of nodes if need-be). This adds rotamer_sets info to the nodes for easier lookup later. Technically optional but recommended
void init_node_info( scoring::hbonds::graph::HBondGraph & graph, rotamer_set::RotamerSets const & rotamer_sets );

///@brief This combines the HBondGraph constructor and init_node_info(). The HBondGraph constructor can not take core.4 objects because it is in core.3 (as of the time this was written).
inline scoring::hbonds::graph::HBondGraphOP
create_and_init_hbond_graph( rotamer_set::RotamerSets const & rotamer_sets ){
	scoring::hbonds::graph::HBondGraphOP graph =
		utility::pointer::make_shared< scoring::hbonds::graph::HBondGraph > ( rotamer_sets.nrotamers() );
	init_node_info( * graph, rotamer_sets );
	return graph;
}

///@brief Construct atom level hbond graph, initialize the node information, score rotamers to create hbonds. This will not work perfectly if you are using symmetry because it does not score one-body interactions, but can still provide a good template
scoring::hbonds::graph::HBondGraphOP
create_init_and_create_edges_for_hbond_graph(
	rotamer_set::RotamerSetsOP rotamer_sets,
	scoring::ScoreFunction const & sfxn,
	pose::Pose const & pose,
	Real hydrogen_bond_threshold,
	Real clash_threshold,
	Real hbond_energy_threshold_for_satisfaction = -0.25f, // should this be the same as hydrogen_bond_threshold?
	bool include_backbone_at_atom_level = false
);

///@brief Utility function used by other functions in this file. Given that an edge exists between two nodes, find all of the hbonds between those two residues. It does not matter which order resA and resB are. Output hbonds are put in the HBondSet provided. This should not be called before the graph is populated with edges (see HBondGraphInitializerIG) - nothing will crash if this is called with no edges but it just does not make sense.
void find_hbonds_in_residue_pair(
	conformation::Residue const & resA,
	conformation::Residue const & resB,
	scoring::hbonds::HBondDatabase const & database,
	utility::graph::Graph const & tenA_neighbor_graph,
	scoring::hbonds::HBondSet & set //output
);

///@brief Iterate through edges and calls determine_atom_level_edge_info(). skip_edges_with_degree_zero does not evaluate edges that have no incident edges and can not be part of a hydrogen bond network. symm_info needs to be != 0 if and only if you want to evaluate symmetry.
void determine_atom_level_edge_info_for_all_edges(
	scoring::hbonds::graph::HBondGraph & hb_graph,
	rotamer_set::RotamerSets const & rotamer_sets,
	scoring::hbonds::HBondDatabase const & database,
	utility::graph::Graph const & tenA_neighbor_graph,
	pose::Pose const & pose,
	bool skip_edges_with_degree_zero = false,//do not waste time on edges that can not be part of hbond networks
	Real hbond_energy_threshold_for_satisfaction = -0.25,
	conformation::symmetry::SymmetryInfoCOP symm_info = 0
);

///@brief Add information regarding the atoms participating in hbonds (see find_hbonds_in_residue_pair() ). symm_info needs to be != 0 if and only if you want to evaluate symmetry.
void determine_atom_level_edge_info(
	scoring::hbonds::graph::HBondEdge & hb_edge,
	rotamer_set::RotamerSets const & rotamer_sets,
	scoring::hbonds::HBondDatabase const & database,
	utility::graph::Graph const & tenA_neighbor_graph,
	pose::Pose const & pose,
	Real hbond_energy_threshold_for_satisfaction = -0.25,
	conformation::symmetry::SymmetryInfoCOP symm_info = 0
);

///@brief Calls determine_atom_level_node_info() for all nodes. skip_nodes_with_no_edges is recommended to be set to true but depends on your protocol. The default is false for safety purposes.
void determine_atom_level_node_info_for_all_nodes(
	scoring::hbonds::graph::HBondGraph & hb_graph,
	rotamer_set::RotamerSets const & rotamer_sets,
	utility::vector1< bool > const & include_these_resids,
	bool skip_nodes_with_no_edges = false,//do not waste time on nodes that can not form hbonds
	bool include_backbone = false // original use-case (HBNet) didn't include backbone
);

///@brief Store atom information for every node with a corresponding resid set to true in include_these_resids. include_these_resids was originally meant to protray "resid_is_buried" so that we only store atom info for buried residues. I don't care what you use this for so I gave it a more generalized name.
void determine_atom_level_node_info(
	scoring::hbonds::graph::HBondNode & node,
	rotamer_set::RotamerSets const & rotamer_sets,
	utility::vector1< bool > const & include_these_resids,
	bool include_backbone = false // original use-case (HBNet) didn't include backbone
);

///@brief CALL determine_atom_level_node_info() OR determine_atom_level_node_info_for_all_nodes() BEFORE CALLING THIS! After adding atom info to nodes, calling this function will result in removing atom info for atoms that hydrogen bond with the backbone or fixed sidechains.
void find_satisfying_interactions_with_background(
	scoring::hbonds::graph::HBondNode & node,
	rotamer_set::RotamerSets const & rotamer_sets,
	utility::graph::Graph const & packer_neighbor_graph,
	pose::Pose const & poly_ala_pose,
	Real hbond_energy_threshold_for_satisfaction = -0.25
);

/// @brief If you only care about hbond networks, then you might not care about edges that have no incident edges. This function deletes those edges.
void delete_edges_with_degree_zero( scoring::hbonds::graph::HBondGraph & hb_graph );

/// @brief Construct an HBondGraph from a partial rotsets (like you might see during packing)
/// @detail BB-BB hbonds are only included for the first residue. This means that prolines are not
///         handled correctly. If proline is the first resdiue at a position and other residues
///         are being packed at that position, any hbonds to the other Ns will not be reported.
///   If one wishes to have BB-BB hbonds for all pairs, enable all 4 hbond terms for
///   scorefxn_sc and leave scorefxn_bb as a nullptr (or a blank scorefxn)
scoring::hbonds::graph::HBondGraphOP
hbond_graph_from_partial_rotsets(
	pose::Pose const & pose_in,
	pack::rotamer_set::RotamerSets const & original_rotsets,
	scoring::ScoreFunctionOP const & scorefxn_sc, // Only hbond_sc_bb and hbond_sc
	scoring::ScoreFunctionOP const & scorefxn_bb, // Only hbond_lr_bb and hbond_sr_bb
	pack::rotamer_set::RotamerSetsOP & complete_rotsets_out,
	utility::vector1<bool> & position_had_rotset,
	float minimum_hb_cut = 0
);

} //hbonds
} //pack
} //core

#endif
