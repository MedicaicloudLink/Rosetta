// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/scoring/methods/LK_BallEnergy.hh
/// @brief  Orientation dependent variant of the LK Solvation using
/// @author Phil Bradley


#ifndef INCLUDED_core_scoring_methods_LK_BALLENERGY_HH
#define INCLUDED_core_scoring_methods_LK_BALLENERGY_HH

// Unit Headers
#include <core/scoring/lkball/LK_BallEnergy.fwd.hh>
#include <core/scoring/lkball/LK_BallInfo.hh>
#include <core/scoring/lkball/lkbtrie/LKBAtom.hh>
#include <core/scoring/lkball/lkbtrie/LKBTrie.fwd.hh>
#include <core/scoring/lkball/lkbtrie/LKBTrieEvaluator.hh>
#include <core/scoring/trie/TrieCountPairBase.hh>

#include <core/scoring/etable/count_pair/types.hh>

// Package headers
#include <core/conformation/Atom.fwd.hh>
#include <core/conformation/RotamerSetBase.fwd.hh>
#include <core/scoring/methods/ContextIndependentTwoBodyEnergy.hh>
#include <core/scoring/methods/EnergyMethodOptions.fwd.hh>
#include <core/scoring/etable/Etable.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/scoring/MinimizationData.hh>
#include <core/scoring/hbonds/types.hh>

// Project headers
#include <core/pose/Pose.fwd.hh>

// Utility headers
#include <ObjexxFCL/FArray3D.hh>
#include <utility/vector1.hh>

#ifdef    SERIALIZATION
#include <cereal/types/polymorphic.fwd.hpp>
#endif // SERIALIZATION

namespace core {
namespace scoring {
namespace lkball {

struct ScoredBridgingWater {
	ScoredBridgingWater( core::Real lkbr_uncpl_score_in,
		core::Real lkbr_score_in,
		numeric::xyzVector< core::Real > position_in ) :
		lkbr_uncpl_score(lkbr_uncpl_score_in), lkbr_score(lkbr_score_in), position(position_in) {}
	core::Real lkbr_uncpl_score;
	core::Real lkbr_score;
	numeric::xyzVector< core::Real > position;
};


class LK_BallEnergy : public methods::ContextIndependentTwoBodyEnergy {
public:
	typedef methods::ContextIndependentTwoBodyEnergy  parent;
public:
	/// convenience typedefs
	typedef chemical::ResidueType ResidueType;
	typedef utility::vector1< Size > Sizes;
	typedef utility::vector1< Vector > Vectors;


public:

	LK_BallEnergy( methods::EnergyMethodOptions const & options );


	/// clone
	methods::EnergyMethodOP
	clone() const override;

	LK_BallEnergy( LK_BallEnergy const & src );

	void
	setup_for_packing(
		pose::Pose & pose,
		utility::vector1< bool > const &,
		utility::vector1< bool > const & ) const override;

	void
	setup_for_scoring( pose::Pose & pose, ScoreFunction const & ) const override;

	void
	prepare_rotamers_for_packing(
		pose::Pose const & pose,
		conformation::RotamerSetBase & rotamer_set
	) const override;

	void
	update_residue_for_packing(
		pose::Pose &,
		Size resid ) const override;

	void
	setup_for_derivatives(
		pose::Pose & pose,
		ScoreFunction const & scfxn
	) const override;

	/// helper function for outside use
	Real
	calculate_lk_desolvation_of_single_atom_by_residue(
		Size const atom1,
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2
	);

	Real
	calculate_lk_desolvation_of_single_atom_by_residue_no_count_pair(
		Size const atom1,
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2
	);

	void
	calculate_lk_ball_atom_energies(
		Size const atom1,
		conformation::Residue const & rsd1,
		Size atom1_n_attached_waters,
		Size atom1_water_offset,
		WaterCoords const & rsd1_waters,
		Size const atom2,
		conformation::Residue const & rsd2,
		Real & lk_desolvation_of_atom1_by_atom2,
		Real & lk_ball_desolvation_of_atom1_by_atom2 // includes lk-fraction
	) const;

	void
	calculate_lk_ball_atom_energies_cp(
		Size const atom1,
		conformation::Residue const & rsd1,
		Size atom1_n_attached_waters,
		Size atom1_water_offset,
		WaterCoords const & atom1_waters,
		Size const atom2,
		conformation::Residue const & rsd2,
		etable::count_pair::CPCrossoverBehavior const & cp_crossover,
		Real & lk_desolvation_of_atom1_by_atom2,
		Real & lk_ball_desolvation_of_atom1_by_atom2 // includes lk-fraction
	) const;

	// helper
	Real
	get_lk_fractional_contribution_for_single_water(
		Vector const & atom2_xyz,
		Size const atom2_type,
		Vector const & atom1_water
	) const;

	void
	eval_desolvation_derivs_no_count_pair(
		Real const d2,
		Size const atom1,
		conformation::Residue const & rsd1,
		Size const atom2,
		conformation::Residue const & rsd2,
		Real & atom1_lk_desolvation_by_atom2_deriv,
		Real & atom2_lk_desolvation_by_atom1_deriv
	);

	void
	eval_residue_pair_derivatives(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		ResSingleMinimizationData const &,
		ResSingleMinimizationData const &,
		ResPairMinimizationData const & min_data,
		pose::Pose const & pose, // provides context
		EnergyMap const & weights,
		utility::vector1< DerivVectorPair > & r1_atom_derivs,
		utility::vector1< DerivVectorPair > & r2_atom_derivs
	) const override;


	void
	residue_pair_energy(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		pose::Pose const & pose,
		ScoreFunction const & sfxn,
		EnergyMap & emap
	) const override;

	/// @brief Let the energy-method consumers (e.g. the packer) use bounding-sphere logic for
	/// deciding whether bb/bb, bb/sc, and sc/sc energies should be evaluated.
	bool
	divides_backbone_and_sidechain_energetics() const override;

	/// @brief The sum bbE(r1,r2) + bs(r1,r2) + bs(r2,r1) + ss(r1,r2) must equal rpe(r1,r2).
	/// This function evaluates only the energies of bb/bb interactions.
	void
	backbone_backbone_energy(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		pose::Pose const & pose,
		ScoreFunction const & sfxn,
		EnergyMap & emap
	) const override;

	/// @brief The sum bbE(r1,r2) + bs(r1,r2) + bs(r2,r1) + ss(r1,r2) must equal rpe(r1,r2).
	/// This function evaluates only the energies of bb/sc interactions; that is, the backbone
	/// from residue 1 with the sidechain of residue 2
	void
	backbone_sidechain_energy(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		pose::Pose const & pose,
		ScoreFunction const & sfxn,
		EnergyMap & emap
	) const override;

	/// @brief The sum bbE(r1,r2) + bs(r1,r2) + bs(r2,r1) + ss(r1,r2) must equal rpe(r1,r2).
	/// This function evaluates only the energies of sc/sc interactions.
	void
	sidechain_sidechain_energy(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		pose::Pose const & pose,
		ScoreFunction const & sfxn,
		EnergyMap & emap
	) const override;


	void
	residue_pair_energy(
		conformation::Residue const & rsd1,
		LKB_ResidueInfo const & rsd1_info,
		conformation::Residue const & rsd2,
		LKB_ResidueInfo const & rsd2_info,
		ScoreFunction const &sf,
		EnergyMap & emap
	) const;


	void
	accumulate_single_atom_contributions(
		Size atom1_n_attached_waters,
		Size atom1_water_offset,
		WaterCoords const & rsd1_waters,
		AtomWeights const & atom1_wts,
		conformation::Residue const & rsd1,
		Size const atom2_type_index,
		Vector const & atom2_xyz,
		Real const lk_desolvation_of_atom1_by_atom2,
		EnergyMap & emap
	) const;

	void
	setup_for_minimizing_for_residue(
		conformation::Residue const & rsd,
		pose::Pose const & pose,
		ScoreFunction const & scorefxn,
		kinematics::MinimizerMapBase const & min_map,
		basic::datacache::BasicDataCache & res_data_cache,
		ResSingleMinimizationData & resdata
	) const override;


	void
	setup_for_minimizing_for_residue_pair(
		conformation::Residue const & rsd1,
		conformation::Residue const & rsd2,
		pose::Pose const &,
		ScoreFunction const & scorefxn,
		kinematics::MinimizerMapBase const & min_map,
		ResSingleMinimizationData const & res1data,
		ResSingleMinimizationData const & res2data,
		ResPairMinimizationData & pairdata
	) const override;

	bool
	minimize_in_whole_structure_context( pose::Pose const & ) const override;

	bool
	requires_a_setup_for_scoring_for_residue_opportunity_during_regular_scoring( pose::Pose const & pose ) const override;

	using TwoBodyEnergy::setup_for_scoring_for_residue;

	void
	setup_for_scoring_for_residue(
		conformation::Residue const & rsd,
		pose::Pose const & pose,
		ScoreFunction const & sfxn,
		basic::datacache::BasicDataCache & residue_data_cache
	) const override;

	bool
	requires_a_setup_for_derivatives_for_residue_opportunity( pose::Pose const &  ) const override;

	void
	setup_for_derivatives_for_residue(
		conformation::Residue const & rsd,
		pose::Pose const & pose,
		ScoreFunction const & sfxn,
		ResSingleMinimizationData & min_data,
		basic::datacache::BasicDataCache & residue_data_cache
	) const override;

	bool
	defines_intrares_energy( EnergyMap const & /*weights*/ ) const override { return false; }

	void
	eval_intrares_energy(
		conformation::Residue const &,
		pose::Pose const &,
		ScoreFunction const &,
		EnergyMap &
	) const override {}

	void
	finalize_total_energy(
		pose::Pose & pose,
		ScoreFunction const &,
		EnergyMap & totals
	) const override;

	void
	eval_atom_derivative(
		id::AtomID const & id,
		pose::Pose const & pose,
		kinematics::DomainMap const &, // domain_map,
		ScoreFunction const & sfxn,
		EnergyMap const & weights,
		Vector & F1,
		Vector & F2
	) const override;

	Distance
	atomic_interaction_cutoff() const override;


	void indicate_required_context_graphs( utility::vector1< bool > & context_graphs_required ) const override;


	Real
	eval_lk_fraction( Real const d2_delta, Real const width ) const;


	Real
	eval_d_lk_fraction_dr_over_r( Real const d2_delta, Real const width ) const;

	Real
	get_lk_fractional_contribution(
		Vector const & atom2_xyz,
		Size const atom2_type_index,
		Size atom1_n_attached_waters,
		Size atom1_water_offset,
		WaterCoords const & rsd1_waters,
		WaterDerivContributions & d_weighted_d2_d_di,  // per water contribution
		Real & weighted_water_dis2
	) const;


	Real
	get_lk_fractional_contribution(
		Vector const & atom2_xyz,
		Size const atom2_type_index,
		Size const atom1_n_attached_waters,
		Size const atom1_water_offset,
		WaterCoords const & rsd1_waters
	) const;

	Real
	get_lkbr_fractional_contribution(
		Vector const & atom1_base,
		Vector const & atom2_base,
		Size atom1_n_attached_waters,
		Size atom2_n_attached_waters,
		Size atom1_water_offset,
		Size atom2_water_offset,
		WaterCoords const & rsd1_waters,
		WaterCoords const & rsd2_waters
	) const;

	Real
	get_lkbr_fractional_contribution_noangle(
		Size atom1_n_attached_waters,
		Size atom2_n_attached_waters,
		Size atom1_water_offset,
		Size atom2_water_offset,
		WaterCoords const & rsd1_waters,
		WaterCoords const & rsd2_waters
	) const;

	Real
	get_lkbr_fractional_contribution(
		Vector const & atom1_base,
		Vector const & atom2_base,
		Size atom1_n_attached_waters,
		Size atom2_n_attached_waters,
		Size atom1_water_offset,
		Size atom2_water_offset,
		WaterCoords const & rsd1_waters,
		WaterCoords const & rsd2_waters,
		WaterDerivVectors & d_weighted_d2_d_di1,  // per water1 contribution
		Real & weighted_d2_water_delta,
		Real & pointterm_lkbr,
		Real & angleterm_lkbr,
		Real & d_angleterm_lkbr_dr,
		bool compute_derivs=true
	) const;

	void
	evaluate_rotamer_pair_energies(
		conformation::RotamerSetBase const & set1,
		conformation::RotamerSetBase const & set2,
		pose::Pose const & pose,
		ScoreFunction const & sfxn,
		EnergyMap const & weights,
		ObjexxFCL::FArray2D< core::PackerEnergy > & energy_table
	) const override;


	void
	evaluate_rotamer_background_energies(
		conformation::RotamerSetBase const & set,
		conformation::Residue const & residue,
		pose::Pose const & pose,
		ScoreFunction const & sfxn,
		EnergyMap const & weights,
		utility::vector1< core::PackerEnergy > & energy_vector
	) const override;

	void
	sum_deriv_contributions_for_heavyatom_pair_one_way(
		Size const heavyatom1,
		conformation::Residue const & rsd1,
		LKB_ResidueInfo const & rsd1_info,
		Size const heavyatom2,
		conformation::Residue const & rsd2,
		LKB_ResidueInfo const & rsd2_info,
		EnergyMap const & weights,
		Real const weight_factor,
		Real const d2,
		utility::vector1< DerivVectorPair > & r1_at_derivs,
		utility::vector1< DerivVectorPair > & r2_at_derivs
	) const;

	void
	sum_deriv_contributions_for_heavyatom_pair(
		Real const d2,
		Size const heavyatom1,
		conformation::Residue const & rsd1,
		LKB_ResidueInfo const & rsd1_info,
		Size const heavyatom2,
		conformation::Residue const & rsd2,
		LKB_ResidueInfo const & rsd2_info,
		pose::Pose const &,
		EnergyMap const & weights,
		Real const cp_weight,
		utility::vector1< DerivVectorPair > & r1_at_derivs,
		utility::vector1< DerivVectorPair > & r2_at_derivs
	) const;

	void
	setup_d2_bounds();

	etable::Etable const & etable() const;

	ObjexxFCL::FArray3D< Real > const & solv1() const { return solv1_; }
	ObjexxFCL::FArray3D< Real > const & solv2() const { return solv2_; }

	inline
	Real
	etable_bins_per_A2() const
	{
		return etable_bins_per_A2_;
	}

	inline
	Real
	lkb_max_dis() const
	{
		return lkb_max_dis_;
	}

	inline
	Real
	lkb_max_dis2() const
	{
		return lkb_max_dis2_;
	}

	inline
	Real
	fasol_max_dis2() const
	{
		return fasol_max_dis2_;
	}

	inline
	bool
	slim_etable() const
	{
		return slim_etable_;
	}

	// get lkball parameters
	inline core::Real get_ramp_width_A2() const { return ramp_width_A2_; }
	inline core::Real get_overlap_width_A2() const { return overlap_width_A2_; }
	inline core::Real get_multi_water_fade() const { return multi_water_fade_; }
	inline core::Real get_lkbridge_angle_widthscale() const { return lkbridge_angle_widthscale_; }
	inline core::Real get_overlap_target_len2() const { return overlap_target_len2_; }
	inline core::Real get_overlap_gap2() const { return overlap_gap2_; }
	inline core::Real get_d2_low( core::Size idx ) const { return d2_low_[idx]; }

private:

	lkbtrie::LKBRotamerTrieOP
	create_rotamer_trie(
		conformation::RotamerSetBase const & rotset
	) const;

	lkbtrie::LKBRotamerTrieOP
	create_rotamer_trie(
		conformation::Residue const & res
	) const;

	trie::TrieCountPairBaseOP
	get_count_pair_function_trie(
		conformation::RotamerSetBase const & set1,
		conformation::RotamerSetBase const & set2,
		pose::Pose const & pose,
		ScoreFunction const & sfxn
	) const;


	trie::TrieCountPairBaseOP
	get_count_pair_function_trie(
		conformation::Residue const & res1,
		conformation::Residue const & res2,
		trie::RotamerTrieBaseCOP trie1,
		trie::RotamerTrieBaseCOP trie2,
		pose::Pose const & pose,
		ScoreFunction const & sfxn
	) const;

	/////////////////////////////////////////////////////////////////////////////
	// data

	etable::EtableCOP etable_;

	/// these guys are taken from the etable
	ObjexxFCL::FArray3D< Real > const & solv1_;
	ObjexxFCL::FArray3D< Real > const & solv2_;

	ObjexxFCL::FArray3D< Real > const & dsolv1_;

	Real const etable_bins_per_A2_;
	Real lkb_max_dis_, lkb_max_dis2_;
	Real fasol_max_dis2_;
	bool const slim_etable_;
	//bool const use_intra_dna_cp_crossover_4_;

	// controls the shape of the potential:
	//    ramp_width_A2: fade between water distance and heavyatom distance
	//    overlap_width_A2: fade in water-water overlap
	//    multi_water_fade: "softness" of soft-max
	Real ramp_width_A2_, overlap_width_A2_, multi_water_fade_;
	Real lkbridge_angle_widthscale_, overlap_target_len2_, overlap_gap2_;

	utility::vector1< Real > d2_low_;
	utility::vector1< bool > atom_type_is_charged_;

	utility::vector1< Real > lk_ball_prefactor_;

public:
	core::Size version() const override;

};

}
}
}

#ifdef    SERIALIZATION
CEREAL_FORCE_DYNAMIC_INIT( core_scoring_lkball_LKBallEnergy )
#endif // SERIALIZATION


#endif // INCLUDED_core_scoring_methods_LK_BallEnergy_HH
