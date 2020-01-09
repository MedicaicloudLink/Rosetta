// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/antibody/SnugDockProtocol.hh
/// @brief Dock and antigen to an antibody while optimizing the rigid body orientation of the VH and VL chains and performing CDR loop minimization.
/// @details
///
///
/// @author Jianqing Xu ( xubest@gmail.com )
/// @author Brian D. Weitzner ( brian.weitzner@gmail.com )


#ifndef INCLUDED_protocols_antibody_snugdock_SnugDockProtocol_HH
#define INCLUDED_protocols_antibody_snugdock_SnugDockProtocol_HH

// Unit headers
#include <protocols/antibody/snugdock/SnugDockProtocol.fwd.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/antibody/AntibodyEnum.hh>
#include <protocols/loops/Loops.hh>

// Package headers
#include <protocols/antibody/AntibodyInfo.fwd.hh>

// Project headers
#include <core/conformation/Residue.fwd.hh>
#include <core/pose/PDBInfo.fwd.hh>
#include <core/kinematics/FoldTree.fwd.hh>
#include <core/pack/task/TaskFactory.fwd.hh>
#include <protocols/docking/DockingProtocol.fwd.hh>
#include <protocols/loop_modeling/LoopProtocol.fwd.hh>
#include <protocols/loop_modeler/LoopModeler.fwd.hh>

// Utility headers
#include <utility/vector1.fwd.hh>

// C++ headers
#include <iostream>
#include <set>

namespace protocols {
namespace antibody {
namespace snugdock {

class SnugDockProtocol: public moves::Mover {
public: // boiler plate / virtuals
	// default constructor
	SnugDockProtocol();

	// copy constructor
	SnugDockProtocol( SnugDockProtocol const & rhs );

	// assignment operator
	SnugDockProtocol & operator=( SnugDockProtocol const & rhs );

	// destructor
	virtual ~SnugDockProtocol();

	void apply( Pose & ) override;
	std::string get_name() const override;

	protocols::moves::MoverOP clone() const override;
	protocols::moves::MoverOP fresh_instance() const override;

	/// @brief This mover retains state such that a fresh version is needed if the input Pose is about to change
	bool reinitialize_for_new_input() const override;

	/// @brief Associates relevant options with the SnugDockProtocol class
	static void register_options();

	// Accessors for auto kink-constraint options
	bool auto_generate_kink_constraint() const { return auto_generate_kink_constraint_; }
	void auto_generate_kink_constraint( bool const setting ) { auto_generate_kink_constraint_ = setting; }

	bool high_res_kink_constraint() const { return high_res_kink_constraint_; }
	void high_res_kink_constraint( bool const setting ) { high_res_kink_constraint_ = setting; }


public:
	void show( std::ostream & out=std::cout ) const override;
	friend std::ostream & operator<<(std::ostream& out, SnugDockProtocol const & snugdockprotocol );
	// TODO: Move to AntibodyInfo() and generalize
	core::pack::task::TaskFactoryOP repack_tf_from_residue_sets(Pose const & pose, std::set< core::Size > const & ab_residues, std::set< core::Size > const & ag_residues);
	void setup_ab_ag_foldtree( Pose & pose, AntibodyInfoOP antibody_info );
	core::conformation::ResidueOP place_VRT_at_residue_COM( Pose const & pose, utility::vector1<bool> residues);
	core::conformation::ResidueOP place_VRT_at_residue_COM( Pose const & pose, core::Size start, core::Size stop);


private: // methods
	void setup_objects( Pose & pose );
	void setup_loop_refinement_movers();
	void update_fold_tree_from_bool_vector();
	void init();
	void init_for_equal_operator_and_copy_constructor( SnugDockProtocol & lhs, SnugDockProtocol const & rhs);
	void init_from_options();
	void set_default();

	docking::DockingProtocolOP docking() const;
	// return KIC LoopModeler with some defaults (to avoid code duplication)
	protocols::loop_modeler::LoopModelerOP refine_loop() const;

private: // data
	AntibodyInfoOP antibody_info_;
	core::pose::PDBInfoOP pdb_info_;
	std::map<protocols::antibody::CDRNameEnum,loops::Loop> CDR_loops_;
	bool ab_has_light_chain_;
	core::Size vh_vl_jump_;
	// sets are necessary for interface calculation
	std::set< core::Size > ab_residues_;
	std::set< core::Size > ag_residues_;
	core::pack::task::TaskFactoryOP tf_;

	// Movers
	protocols::loop_modeler::LoopModelerOP low_res_refine_cdr_h2_;
	protocols::loop_modeler::LoopModelerOP low_res_refine_cdr_h3_;
	mutable docking::DockingProtocolOP docking_;

	// auto kink-constraint options
	bool auto_generate_kink_constraint_;
	bool high_res_kink_constraint_;


}; // class SnugDockProtocol

} // namespace snugdock
} // namespace antibody
} // namespace protocols

#endif // INCLUDED_protocols_antibody_snugdock_SnugDockProtocol_HH
