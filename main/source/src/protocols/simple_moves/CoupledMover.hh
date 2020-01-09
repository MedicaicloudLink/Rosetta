// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/simple_moves/CoupledMover.hh
/// @brief definition of CoupledMover class and functions
/// @author Noah Ollikainen (nollikai@gmail.com)
/// @author Anum Glasgow (anumazam@gmail.com)
/// @author Amanda Loshbaugh (aloshbau@gmail.com)

#ifndef INCLUDED_protocols_simple_moves_CoupledMover_hh
#define INCLUDED_protocols_simple_moves_CoupledMover_hh

// Unit headers
#include <protocols/simple_moves/CoupledMover.fwd.hh>

// Project headers
#include <core/types.hh>
#include <core/pose/Pose.fwd.hh>
#include <protocols/moves/Mover.fwd.hh>
#include <protocols/kinematic_closure/KicMover.hh>
#include <protocols/simple_moves/ShortBackrubMover.hh>
#include <protocols/minimization_packing/BoltzmannRotamerMover.hh>
#include <protocols/rigid/RigidBodyMover.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/pack/task/PackerTask.hh>

// Parser headers
#include <basic/datacache/DataMap.fwd.hh>
#include <utility/tag/Tag.fwd.hh>

#include <utility/vector1.hh>


namespace protocols {
namespace simple_moves {

/// @brief This mover is called by CoupledMovesProtocol to make a single coupled move.
/// @details The CoupledMover apply function makes a single coupled move as follows:
/// (1) Backbone move -- As of March 2018, there are three options: backrub, kinematic closure with walking perturber, or kinematic closure with fragment perturber.
/// (2) Side chain move -- The three residues at the center of the perturbed backbone are sampled by BoltzmannRotamerMover.
/// (3) Repack -- 10A shell around the residues from step 2 is repacked.
class CoupledMover : public protocols::moves::Mover {

public:

	typedef protocols::moves::MoverOP MoverOP;

	// default constructor
	CoupledMover();

	/// @brief constructor that sets input pose, score function and packer task
	CoupledMover(
		core::pose::PoseOP pose,
		core::scoring::ScoreFunctionOP score_fxn,
		core::pack::task::PackerTaskOP packer_task
	);

	/// @brief constructor that sets input pose, score function, packer task and ligand residue number
	CoupledMover(
		core::pose::PoseOP pose,
		core::scoring::ScoreFunctionOP score_fxn,
		core::pack::task::PackerTaskOP packer_task,
		core::Size ligand_resnum
	);

	/// @brief copy constructor
	CoupledMover( CoupledMover const & rval );

	/// @brief destructor
	~CoupledMover() override;

	/// @brief clone this object
	protocols::moves::MoverOP clone() const override;

	/// @brief create this type of object
	protocols::moves::MoverOP fresh_instance() const override;

	void apply( core::pose::Pose & pose ) override;

	// setters
	void set_resnum( core::Size resnum );
	void set_randomize_resnum( bool randomize_resnum );
	void set_fix_backbone( bool fix_backbone );
	void set_rotation_std_dev( core::Real rotation_std_dev );
	void set_uniform_backrub( bool uniform_backrub );
	void set_input_pose( core::pose::PoseCOP pose ) override;
	void set_temperature( core::Real temperature );
	void set_bias_sampling( bool bias_sampling );
	void set_bump_check( bool bump_check );
	void set_ligand_resnum( core::Size ligand_resnum, core::pose::PoseCOP pose  );
	void set_ligand_jump_id( core::Size ligand_jump_id );
	void set_ligand_weight( core::Real ligand_weight );
	void set_rotation_magnitude( core::Real rotation_magnitude );
	void set_translation_magnitude( core::Real translation_magnitude );
	void set_short_backrub_mover( protocols::simple_moves::ShortBackrubMoverOP short_backrub_mover );
	void set_boltzmann_rotamer_mover( protocols::minimization_packing::BoltzmannRotamerMoverOP boltzmann_rotamer_mover );
	void set_rigid_body_mover( protocols::rigid::RigidBodyPerturbMoverOP rigid_body_mover );
	void set_score_fxn( core::scoring::ScoreFunctionOP score_fxn );
	void set_packer_task( core::pack::task::PackerTaskOP packer_task );
	void set_loop_size( core::Size loop_size );
	void set_perturber( kinematic_closure::perturbers::PerturberOP perturber );
	void set_backbone_mover( std::string const & backbone_mover );
	void set_repack_neighborhood ( bool repack_neighborhood );

	// getters
	core::Size get_resnum() const;
	bool get_randomize_resnum() const;
	bool get_fix_backbone() const;
	core::Real get_rotation_std_dev() const;
	bool get_uniform_backrub() const;
	core::Real get_temperature() const;
	bool get_bias_sampling() const;
	bool get_bump_check() const;
	core::Size get_ligand_resnum() const;
	core::Size get_ligand_jump_id() const;
	core::Real get_ligand_weight() const;
	core::Real get_rotation_magnitude() const;
	core::Real get_translation_magnitude() const;
	protocols::simple_moves::ShortBackrubMoverOP get_short_backrub_mover() const;
	protocols::minimization_packing::BoltzmannRotamerMoverOP get_boltzmann_rotamer_mover() const;
	protocols::rigid::RigidBodyPerturbMoverOP get_rigid_body_mover() const;
	core::scoring::ScoreFunctionOP get_score_fxn() const;
	core::pack::task::PackerTaskOP get_packer_task() const;
	core::Size get_loop_size() const;
	kinematic_closure::perturbers::PerturberOP get_perturber() const;
	const std::string & get_backbone_mover() const;
	const bool & get_repack_neighborhood() const;

	std::string
	get_name() const override;

	static
	std::string
	mover_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );


private:

	/// @brief residue number specifying the residue for the next move
	core::Size resnum_;

	/// @brief if true, choose a random residue for the next move
	bool randomize_resnum_;

	/// @brief if true, do not perform backbone moves
	bool fix_backbone_;

	/// @brief standard deviation of rotation angle (degrees) used for ShortBackrubMover
	core::Real rotation_std_dev_;

	/// @brief if true, sample rotation angle from a uniform distribution from -20 to 20
	bool uniform_backrub_;

	/// @brief kT value used for Boltzmann probability calculation
	core::Real temperature_;

	/// @brief if true, bias rotamer selection based on energy
	bool bias_sampling_;

	/// @brief if true, use bump check when generating rotamers
	bool bump_check_;

	/// @brief residue number specifying the ligand (set to zero if there is no ligand)
	core::Size ligand_resnum_;

	/// @brief jump id specifying the ligand jump (set to zero if there is no ligand)
	core::Size ligand_jump_id_;

	/// @brief weight of interaction between resnum_ and ligand_resnum_
	core::Real ligand_weight_;

	/// @brief the magnitude of ligand rotation used by the RigidBodyMover (in degrees)
	core::Real rotation_magnitude_;

	/// @brief the magnitude of ligand translation used by the RigidBodyMover (in angstroms)
	core::Real translation_magnitude_;

	/// @brief mover used for backbone moves when option set to backrub
	protocols::simple_moves::ShortBackrubMoverOP short_backrub_mover_;

	/// @brief mover used for backbone moves when option set to kinematic closure
	protocols::kinematic_closure::KicMoverOP fullatom_kic_mover_;//This seems to be completely unused

	/// @brief mover used for side-chain moves
	protocols::minimization_packing::BoltzmannRotamerMoverOP boltzmann_rotamer_mover_;

	/// @brief mover used for rigid body rotation and translation
	protocols::rigid::RigidBodyPerturbMoverOP rigid_body_mover_;

	/// @brief score function needed for the BoltzmannRotamerMover
	core::scoring::ScoreFunctionOP score_fxn_;

	/// @brief task factory needed to generate packer task
	core::pack::task::PackerTaskOP packer_task_;

	/// @brief the size of the loop for KIC move. Loop is center residue + and - this number.
	core::Size loop_size_;

	/// @brief Which perturber to use during kinematic closure (KIC). Currently the options are fragment or walking perturber. Walking perturber adjusts torsions by degrees, the magnitude of which can be set by walking_perturber_magnitude option in CoupledMovesProtocol.
	kinematic_closure::perturbers::PerturberOP perturber_;

	/// @brief type of backbone move, e.g. backrub or kic
	std::string backbone_mover_;

	/// @brief After the backbone move and rotamer move, repack sidechains within 5A of the design residue. Default false for legacy behavior.
	bool repack_neighborhood_;

};  // class CoupledMover

} // moves
} // protocols

#endif
