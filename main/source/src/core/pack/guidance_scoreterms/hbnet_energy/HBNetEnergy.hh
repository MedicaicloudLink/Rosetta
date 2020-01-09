// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file core/pack/guidance_scoreterms/hbnet_energy/HBNetEnergy.hh
/// @brief Headers for an EnergyMethod that gives a bonus for hydrogen bond networks, which ramps nonlinearly with the size of the
/// networks.
/// @details This energy method is inherently not pairwise decomposible.  However, it is intended for very rapid calculation,
/// and has been designed to plug into Alex Ford's modifications to the packer that permit it to work with non-pairwise scoring
/// terms.  It has also been modified to permit sub-regions of a pose to be scored.
/// @author Vikram K. Mulligan (vmullig@uw.edu).

#ifndef INCLUDED_core_pack_guidance_scoreterms_hbnet_energy_HBNetEnergy_hh
#define INCLUDED_core_pack_guidance_scoreterms_hbnet_energy_HBNetEnergy_hh

// Unit headers
#include <core/pack/guidance_scoreterms/hbnet_energy/HBNetEnergy.fwd.hh>

// Package headers
#include <core/scoring/annealing/ResidueArrayAnnealableEnergy.hh>
#include <core/scoring/methods/EnergyMethod.hh>
#include <core/scoring/methods/EnergyMethodOptions.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/scoring/EnergyMap.fwd.hh>
#include <core/scoring/methods/WholeStructureEnergy.hh>
#include <core/conformation/Residue.fwd.hh>
#include <core/conformation/symmetry/SymmetryInfo.fwd.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/chemical/ResidueProperty.hh>
#include <core/scoring/TenANeighborGraph.hh>

// Utility headers

// Project headers
#include <core/types.hh>
#include <map>
#include <string>
#include <utility/vector1.hh>

namespace core {
namespace pack {
namespace guidance_scoreterms {
namespace hbnet_energy {

/// @brief Allowed types of HBNetEnergy rampings.
/// @details If you add to this list, update the HBNetEnergy::hbnet_energy_ramping_string_from_enum() function.
enum HBNetEnergyRamping {
	HBNetEnergyRampQuadratic=1, //keep this first
	HBNetEnergyRampLinear,
	HBNetEnergyRampLogarithmic,
	HBNetEnergyRampSquareRoot,
	HBNetEnergyRampINVALID, //keep this last
	HBNetEnergyRamp_end_of_list = HBNetEnergyRampINVALID //keep this last
};

/// @brief HBNetEnergy, an EnergyMethod that gives a bonus for hydrogen bond networks, which ramps nonlinearly with the size of the
/// networks. This class is derived from base class WholeStructureEnergy, which is meaningful only on entire structures.
/// These EnergyMethods do all of their work in the "finalize_total_energy" section of scorefunction evaluation.
class HBNetEnergy : public core::scoring::methods::WholeStructureEnergy, public core::scoring::annealing::ResidueArrayAnnealableEnergy {
public:
	typedef core::scoring::methods::WholeStructureEnergy parent1;
	typedef core::scoring::annealing::ResidueArrayAnnealableEnergy parent2;

public:

	/// @brief Options constructor.
	///
	HBNetEnergy( core::scoring::methods::EnergyMethodOptions const &options );

	/// @brief Default destructor.
	///
	virtual ~HBNetEnergy();

	/// @brief Clone: create a copy of this object, and return an owning pointer
	/// to the copy.
	core::scoring::methods::EnergyMethodOP clone() const override;

	/// @brief HBNetEnergy is context-independent and thus indicates that no context graphs need to be maintained by
	/// class Energies.
	void indicate_required_context_graphs( utility::vector1< bool > &context_graphs_required ) const override;

	/// @brief HBNetEnergy is version 1.0 right now.
	///
	core::Size version() const override;

	/// @brief Actually calculate the total energy
	/// @details Called by the scoring machinery.  The update_residue_neighbors() function of the pose
	/// must be called first.
	void finalize_total_energy( core::pose::Pose & pose, core::scoring::ScoreFunction const &, core::scoring::EnergyMap & totals ) const override;

	/// @brief Calculate the total energy given a vector of const owning pointers to residues.
	/// @details Called directly by the ResidueArrayAnnealingEvaluator during packer runs.
	core::Real calculate_energy(
		utility::vector1< core::conformation::ResidueCOP > const &resvect,
		utility::vector1< core::Size > const & rotamer_ids,
		core::Size const substitution_position = 0
	) const override;

	/// @brief What to do when a substitution that was considered is accepted.
	/// @author Vikram K. Mulligan (vmullig@uw.edu).
	void commit_considered_substitution() override;

	/// @brief Get a summary of all loaded data.
	///
	void report() const;

	/// @brief Cache data from the pose in this EnergyMethod in anticipation of scoring.
	///
	void set_up_residuearrayannealableenergy_for_packing ( core::pose::Pose &pose, core::pack::rotamer_set::RotamerSets const &rotamersets, core::scoring::ScoreFunction const &sfxn) override;

	/// @brief Disable this scoreterm during minimization trajectory.
	void setup_for_minimizing( core::pose::Pose & pose, core::scoring::ScoreFunction const & sfxn, core::kinematics::MinimizerMapBase const & minmap) const override;

	/// @brief Re-enable this scoreterm after a minimization trajectory.
	void finalize_after_minimizing( core::pose::Pose & pose) const override;

	/// @brief Set the way that HBNetEnergy scales with network size,
	/// by string.
	void set_hbnet_energy_ramping ( std::string const &ramping_string );

	/// @brief Set the way that HBNetEnergy scales with network size,
	/// by enum.
	void set_hbnet_energy_ramping ( HBNetEnergyRamping const ramping_enum );

	/// @brief Given a string for an HBNetEnergyRamping type, return the corresponding enum.
	/// @details Returns HBNetEnergyRampINVALID if the string isn't recognized.
	HBNetEnergyRamping hbnet_energy_ramping_enum_from_string( std::string const &ramping_string ) const;

	/// @brief Given an enum for an HBNetEnergyRamping type, return the corresponding string.
	/// @details Returns "INVALID" if the string isn't recognized.
	std::string hbnet_energy_ramping_string_from_enum( HBNetEnergyRamping const ramping_enum ) const;

	/// @brief Get the maximum network size, beyond which there is no bonus for making a network bigger.
	/// @details A value of "0" (the default) means no max.
	inline core::Size max_network_size() const { return max_network_size_; }

	/// @brief Set the maximum network size, beyond which there is no bonus for making a network bigger.
	/// @details A value of "0" (the default) means no max.
	void max_network_size( core::Size const setting );

private:

	/******************
	Private functions:
	******************/

	/// @brief Initializes the neighbor_graph_ and hbonds_graph_ objects, given a pose.
	///
	void initialize_graphs(core::pose::Pose const &pose) const;

	/// @brief Set up the bb_hbonds_graph_ object.
	/// @details If X and Y share an edge, then X is bb-bb hydrogen bonded to Y, or to a residue that is covalently bonded to Y,
	/// or a residue covalently bonded to X is bb-bb hydrogen bonded to Y.
	void initialize_bb_hbonds_graph( core::pose::Pose const &pose ) const;

	/// @brief Determines whether a hydrogen bond exists between res1 and res2.
	/// @details Ignores backbone-backbone hydrogen bonds.  Also, this is directional: it considers donors in res1 to acceptors in res2.
	/// Call twice for bidirectional hbonding.
	/// @note For now, this is just a distance check between donors and acceptors, but it could be updated to properly call the hbond scoring
	/// machinery.
	bool has_hbond( core::conformation::Residue const &res1, core::conformation::Residue const &res2, core::Size const res1_index, core::Size const res2_index ) const;

	/// @brief Determines whether a pair of residues has a close backbone hydrogen bond connection.
	/// @details Returns true if (a) the residues have a bb-bb hbond, (b) res1 has a bb-bb hbond to a residue that is covalently bonded
	/// to res2, (c) res2 has a bb-bb hbond to a residue that is covalently bonded to res1, or (d) if res1 and res2 are directly covalently bonded.
	/// @note This only considers res1 (and its neighbors) as donor and res2 (and its neighbors) as acceptor.  Call twice to check two-way hydrogen bonds.
	bool has_bb_hbond_connection( core::pose::Pose const &pose, core::conformation::Residue const &res1, core::conformation::Residue const &res2 ) const;

	/// @brief Determines whether a pair of residues has a direct backbone hydrogen bond connection.
	/// @details Returns true if the residues have a bb-bb hbond.
	/// @note This only considers res1 as donor and res2 as acceptor.  Call twice to check two-way hydrogen bonds.
	bool has_bb_hbond( core::conformation::Residue const &res1, core::conformation::Residue const &res2 ) const;

	/// @brief Given the size of a connected component in a graph, return a value to pass to the bonus function accumulator.
	/// @details The value returned by this function is more POSITIVE for bigger bonuses; it should be SUBTRACTED from the accumulator
	/// to yield a score that gets more negative with greater favourability.
	core::Real bonus_function( core::Size count_in ) const;

	/// @brief Given the index of an asymmetric node in the hbonds_graph_ object, drop all of the edges for all corresponding symmetric nodes.
	/// @details Assumes symm_info_ points to something.  Check symm_info != nullptr before calling this function.
	void drop_all_edges_for_symmetric_nodes( core::Size const asymm_node ) const;

	/// @brief Given residue indices node1 and node2 in the asymmetric unit, add an edge between the corresponding nodes in all symmetric units.
	/// @details Assumes symm_info_ points to something.  Check symm_info != nullptr before calling this function.
	void symmetrize_hbonds_graph( core::Size const node1, core::Size const node2, utility::vector1< core::conformation::ResidueCOP > const & resvect ) const;

	/******************
	Private variables:
	******************/

	/// @brief Is this term disabled?
	/// @details This term disables itself during minimization trajectories.
	mutable bool disabled_;

	/// @brief The neighbor graph for the pose being scored, extracted from the pose by the setup_residuearrayannealableenergy_for_packing function
	/// and/or by the finalize_total_energy function.
	/// @details The update_residue_neighbors() method of the pose must be called first.
	mutable core::scoring::TenANeighborGraph neighbour_graph_;

	/// @brief A graph object in which nodes are residues and edges indicate that the residues are hydrogen-bonded.
	/// @details Updated on-the-fly during packing.
	mutable core::scoring::TenANeighborGraph hbonds_graph_;

	/// @brief A graph object in which nodes are residues and edges indicate that the residues are hydrogen-bonded.
	/// @details This one represents the last accepted move in the simulated annealing search.
	mutable core::scoring::TenANeighborGraph hbonds_graph_last_accepted_;

	/// @brief A graph object in which nodes are residues and edges indicate close backbone-backbone hydrogen bonding.
	/// @details If X and Y share an edge, then X is bb-bb hydrogen bonded to Y, or to a residue that is covalently bonded to Y,
	/// or a residue covalently bonded to X is bb-bb hydrogen bonded to Y.  (Or X and Y are covalently bonded, in which case they
	/// are also given an edge.)
	mutable core::scoring::TenANeighborGraph bb_hbonds_graph_;

	/// @brief Information about the symmetric state of the pose.  Will be nullptr if the pose is not symmetric.
	mutable core::conformation::symmetry::SymmetryInfoCOP symm_info_;

	/// @brief How does the energy scale with size of network?
	/// @details Defaults to quadratic.  Can be logarithmic, square root, etc.
	HBNetEnergyRamping ramping_type_;

	/// @brief What is the maximum network size, beyond which we no longer get a bonus?
	/// @details Defaults to 0 (no limit).  Values greater than 0 impose a limit.
	core::Size max_network_size_;


};

} // hbnet_energy
} // guidance_scoreterms
} // pack
} // core


#endif // INCLUDED_core_scoring_EtableEnergy_HH
