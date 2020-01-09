// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/simple_moves/DisulfideInsertionMover.hh
/// @brief Header file for DisulfideInsertionMover
/// @author Orly Marcu ( orly.marcu@mail.huji.ac.il )
/// @date Jan. 12, 2015

#ifndef INCLUDED_protocols_simple_moves_DisulfideInsertionMover_hh
#define INCLUDED_protocols_simple_moves_DisulfideInsertionMover_hh

// unit headers
#include <protocols/simple_moves/DisulfideInsertionMover.fwd.hh>

// protocols headers
#include <protocols/moves/Mover.hh>

// core headers
#include <core/pose/Pose.fwd.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/select/movemap/MoveMapFactory.fwd.hh>

// utility headers
#include <utility/tag/Tag.fwd.hh>

namespace protocols {
namespace simple_moves {

enum DisulfideCyclizationViability {
	DCV_NOT_CYCLIZABLE,
	DCV_CYCLIZABLE,
	DCV_ALREADY_CYCLIZED
};

/// @details a mover that given a receptor peptide pose mutates the peptides edge residues to cysteins,
///           if needed, and enforces disulfide bonding by constrained minimization of the bond and the interaction
class DisulfideInsertionMover : public protocols::moves::Mover {
public:

	//empty ctor
	DisulfideInsertionMover();

	// cctor
	DisulfideInsertionMover( DisulfideInsertionMover const & );

	// dtor
	~DisulfideInsertionMover() override= default;

	// clone
	protocols::moves::MoverOP clone() const override;

	// mover interface
	void apply( core::pose::Pose & pose ) override;

	/// @brief checks if two residues in a pose are near enough in space
	///         and geometrically similar enough to existing disulfide forming cys

	DisulfideCyclizationViability determine_cyclization_viability(
		core::pose::Pose const & partner_pose, core::Size const n_cyd_position, core::Size c_cyd_position);

	// RosettaScripts implementation
	void parse_my_tag( utility::tag::TagCOP tag,
		basic::datacache::DataMap & data,
		protocols::filters::Filters_map const & filters,
		protocols::moves::Movers_map const & movers,
		core::pose::Pose const & pose ) override;

	void set_scorefxn ( core::scoring::ScoreFunctionOP const score_function) { scorefxn_ = score_function; }
	core::scoring::ScoreFunctionOP get_scorefxn() const { return scorefxn_;}

	void set_movemap_factory (core::select::movemap::MoveMapFactoryCOP const mm) {movemap_factory_ = mm;}
	core::select::movemap::MoveMapFactoryCOP get_movemap_factory() const {return movemap_factory_;}

	//void set_movemap (core::kinematics::MoveMapOP const mm) {movemap_ = mm;}
	//core::kinematics::MoveMapOP get_movemap() {return movemap_;}

	void set_peptide_chain(core::Size const value) { peptide_chain_num_ = value; }
	core::Size get_peptide_chain () const {return peptide_chain_num_;}

	void set_n_cyd_seqpos(core::Size const n_cyd_seqpos) { n_cyd_seqpos_ = n_cyd_seqpos; }
	void set_c_cyd_seqpos(core::Size const c_cyd_seqpos) { c_cyd_seqpos_ = c_cyd_seqpos; }
	core::Size get_n_cyd_seqpos() const { return n_cyd_seqpos_; }
	core::Size get_c_cyd_seqpos() const { return c_cyd_seqpos_; }

	void set_constraint_weight(core::Real const value) { constraint_weight_ = value; }
	core::Real get_constraint_weight() const { return constraint_weight_; }

	void set_max_dslf_pot(core::Real const value) {max_dslf_pot_ = value; }
	core::Real get_max_dslf_pot () {return max_dslf_pot_; }

	void set_max_dslf_energy(core::Real const value) {max_dslf_energy_ = value; }
	core::Real get_max_dslf_energy () {return max_dslf_energy_; }

	std::string
	get_name() const override;

	static
	std::string
	mover_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );


private:
	/// @brief adds angle, dihedral angle and atom-pair constraints to the pose
	///     Based on code by Nir London
	static void setup_constraints(core::pose::Pose & peptide_receptor_pose,
		core::conformation::Residue lower_cys,
		core::conformation::Residue upper_cys,
		core::Size n_cyd_two_chain_position,
		core::Size c_cyd_two_chain_position);

	/// @brief the chain number for the chain to introduce the Cys mutations to
	core::Size peptide_chain_num_;

	/// @brief the score function to use in all operations; if constraint_weight > 0
	///        this function may be modified to include constraints.
	core::scoring::ScoreFunctionOP scorefxn_;

	/// @brief the movemap for minimization upon call to rebuild_disulfide() (after mutation)
	core::select::movemap::MoveMapFactoryCOP movemap_factory_;
	///// @brief the movemap for minimization upon call to rebuild_disulfide() (after mutation)
	//core::kinematics::MoveMapOP movemap_;

	/// @brief together with c_cyd_seqpos_, the residue numbers to be mutated into Cys
	///        and modeled as having a disulfide bond.
	core::Size n_cyd_seqpos_;
	core::Size c_cyd_seqpos_;

	/// @brief when > 0, applies angle, dihedral angle and atom-pair constraints
	///        using this weight in the score function, to the newly formed disulfide
	///        bond before repacking and minimization.
	core::Real constraint_weight_;

	/// @brief the maximal allowed value of rot-trans deviation from database dslf bond geometry
	/// when comparing two residues to potentially be mutated to cys to form a disulfide
	core::Real max_dslf_pot_;

	/// @brief the maximal allowed value of change in disulfide energy for the generated disulfide bonded peptide
	core::Real max_dslf_energy_;

	/// @brief A multiplier for the miniumum distance cutoff when detecting whether two positinos could possibly form
	/// a disulfide.  Default 1.0.
	core::Real min_dist_multiplier_;

	/// @brief A multiplier for the muximum distance cutoff when detecting whether two positinos could possibly form
	/// a disulfide.  Default 1.0.
	core::Real max_dist_multiplier_;

};

}//simple_moves
}//protocols

#endif //INCLUDED_protocols_simple_moves_DisulfideInsertionMover_HH

