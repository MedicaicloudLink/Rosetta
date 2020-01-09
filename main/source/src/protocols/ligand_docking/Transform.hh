// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   src/protocols/ligand_docking/Transform.hh
/// @author Sam DeLuca

#ifndef INCLUDED_protocols_ligand_docking_Transform_hh
#define INCLUDED_protocols_ligand_docking_Transform_hh

#include <protocols/moves/Mover.hh>
#include <protocols/ligand_docking/Transform.fwd.hh>
#include <protocols/qsar/scoring_grid/GridSet.fwd.hh>

#include <core/conformation/Residue.fwd.hh>
#include <core/conformation/UltraLightResidue.fwd.hh>
#include <core/kinematics/Jump.fwd.hh>
#include <utility/io/ozstream.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <utility/vector1.hh>


namespace protocols {
namespace ligand_docking {


struct Transform_info{ // including default values

public:
	std::string chain = "";
	core::Size chain_id = 0;
	core::Size jump_id = 0;
	core::Real move_distance = 0.0;
	core::Real box_size = 0.0;
	core::Real angle = 0.0;
	core::Real rmsd = 0.0;
	core::Size cycles = 0;
	core::Real temperature = 0.0;
	core::Size repeats = 1;
	Transform_info() = default;
	Transform_info(Transform_info const & ) = default;
};

class Transform: public protocols::moves::Mover
{
public:
	Transform();
	Transform(
		qsar::scoring_grid::GridSetCOP grid_prototype,
		std::string const & chain,
		core::Real const & box_size,
		core::Real const & move_distance,
		core::Real const & angle,
		core::Size const & cycles,
		core::Real const & temp
	);
	Transform(Transform const & other);
	~Transform() override;
	protocols::moves::MoverOP clone() const override;
	protocols::moves::MoverOP fresh_instance() const override;

	void parse_my_tag(
		utility::tag::TagCOP tag,
		basic::datacache::DataMap & data_map,
		protocols::filters::Filters_map const &,
		protocols::moves::Movers_map const &,
		core::pose::Pose const & pose
	) override;

	void apply(core::pose::Pose & pose) override;

	std::string
	get_name() const override;

	static
	std::string
	mover_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );


	/// @brief Performa a randomization of the ligand residue, translating by some random value within a uniform distribution with a max of distance,
	/// and rotating around a random axis with a uniformly random angle of between -angle/2 and +angle/2 (in degrees). Also randomizes starting conformer if available
	void randomize_ligand(core::conformation::UltraLightResidue & residue, core::Real distance, core::Real angle);

	/// @brief translate and rotate a random value by the distances specified in the Transform_info object, using a gaussian distribution
	void transform_ligand(core::conformation::UltraLightResidue & residue);

	/// @brief setup conformers for use
	void setup_conformers(core::pose::Pose & pose, core::Size begin);

	/// @brief Check that conformers are safely within the grids.
	/// Returns true if good, false if bad.
	bool check_conformers(core::conformation::UltraLightResidue & starting_residue) const;

	/// @brief Return the recommended minimum size of the ligand grids for this particular setup.
	/// @details - success_rate is the MC success rate. Will be bumped to a reasonable minimum
	core::Real recommended_grid_size( core::Real success_rate = -1 ) const;

	/// @brief Return the recommended minimum box size for this particular setup.
	/// @details - success_rate is the MC success rate. Will be bumped to a reasonable minimum
	core::Real recommended_box_size( core::Real success_rate = -1 ) const;

	/// @brief randomly change the ligand conformation
	void change_conformer(core::conformation::UltraLightResidue & residue);

	/// @brief output the ligand residues to a pdb file
	void dump_conformer(core::conformation::UltraLightResidue & residue, utility::io::ozstream & output);

	/// @brief return true if the rmsd is within the specified cutoff
	bool check_rmsd(core::conformation::UltraLightResidue const & start, core::conformation::UltraLightResidue const& current) const;

	/// @brief check ligand still inside the grid
	bool check_grid(core::conformation::UltraLightResidue & ligand_residue, core::Real distance = 0);

	/// @brief Calculate unconstrained score for given ligand
	core::Real score_ligand(core::conformation::UltraLightResidue & ligand_residue);

	/// @brief score constraints by updating pose and applying score_function_
	core::Real score_constraints(core::pose::Pose & pose, core::conformation::UltraLightResidue & residue, core::scoring::ScoreFunctionOP & sfxn);

	/// @brief Generate all extra grids
	void make_multi_pose_grids(core::Vector center);

	/// @brief copy the ligand into the desired receptor model and update conformation using the best model
	core::Real convert_to_full_pose(core::pose::Pose & pose, core::conformation::UltraLightResidue & residue);

private:
	/// @brief Estimate how much the ligand will travel during the MC translation
	/// @details - success_rate is the MC success rate. Will be bumped to a reasonable minimum
	core::Real estimate_mc_travel( core::Real success_rate = -1 ) const;

private:
	Transform_info transform_info_;
	utility::vector1<std::pair<std::string, qsar::scoring_grid::GridSetCOP>> grid_sets_;
	// std::map<std::string, core::Real> grid_set_weights_;
	std::map<std::string, core::pose::Pose> grid_set_poses_;
	utility::vector1< core::conformation::UltraLightResidueOP > ligand_conformers_;
	protocols::qsar::scoring_grid::GridSetCOP grid_set_prototype_;
	bool optimize_until_score_is_negative_ = false;
	bool output_sampled_space_ = false;
	bool check_rmsd_ = false;
	bool use_conformers_ = true;
	bool use_constraints_ = false;
	bool use_main_model_ = false;
	std::string ensemble_proteins_ = "";
	std::string cst_fa_file_ = "";
	core::Real cst_fa_weight_ = 1.0;
	std::string sampled_space_file_;
	core::Real initial_perturb_ = 0.0;
	core::Real initial_angle_perturb_ = -360.0;

};

}
}

#endif /* TRANSFORM_HH_ */
