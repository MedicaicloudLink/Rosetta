// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file core/scoring/aa_composition_energy/AACompositionEnergy.hh
/// @brief Headers for an EnergyMethod that penalizes deviation from a desired amino acid composition.
/// @details This energy method is inherently not pairwise decomposible.  However, it is intended for very rapid calculation,
/// and has been designed to plug into Alex Ford's modifications to the packer that permit it to work with non-pairwise scoring
/// terms.  It has also been modified to permit sub-regions of a pose to be scored.
/// @author Vikram K. Mulligan (vmullig@uw.edu).

#ifndef INCLUDED_core_scoring_aa_composition_energy_AACompositionEnergy_hh
#define INCLUDED_core_scoring_aa_composition_energy_AACompositionEnergy_hh

// Unit headers
#include <core/scoring/aa_composition_energy/AACompositionEnergy.fwd.hh>
#include <core/scoring/aa_composition_energy/AACompositionEnergySetup.fwd.hh>
#include <core/scoring/aa_composition_energy/AACompositionEnergySetup.hh>

// Package headers
#include <core/scoring/annealing/ResidueArrayAnnealableEnergy.hh>
#include <core/scoring/methods/EnergyMethod.hh>
#include <core/scoring/methods/EnergyMethodOptions.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/scoring/EnergyMap.fwd.hh>
#include <core/scoring/methods/WholeStructureEnergy.hh>
#include <core/conformation/Residue.fwd.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/chemical/ResidueProperty.hh>
#include <core/select/residue_selector/ResidueSelector.fwd.hh>

// Project headers
#include <core/types.hh>
#include <map>
#include <string>
#include <utility/vector1.hh>


namespace core {
namespace scoring {
namespace aa_composition_energy {

/// @brief AACompositionEnergy, an energy function to penalize stretches of the same residue,
/// derived from base class for EnergyMethods, which are meaningful only on entire structures.
/// These EnergyMethods do all of their work in the "finalize_total_energy" section of score
/// function evaluation.
class AACompositionEnergy : public core::scoring::methods::WholeStructureEnergy, public core::scoring::annealing::ResidueArrayAnnealableEnergy {
public:
	typedef core::scoring::methods::WholeStructureEnergy parent1;
	typedef core::scoring::annealing::ResidueArrayAnnealableEnergy parent2;

public:

	/// @brief Options constructor.
	///
	AACompositionEnergy( core::scoring::methods::EnergyMethodOptions const &options );

	/// @brief Copy constructor.
	/// @details Note that there's no deep-copying here.
	AACompositionEnergy( AACompositionEnergy const &src );

	/// @brief Default destructor.
	///
	virtual ~AACompositionEnergy();

	/// @brief Clone: create a copy of this object, and return an owning pointer
	/// to the copy.
	core::scoring::methods::EnergyMethodOP clone() const override;

	/// @brief AACompositionEnergy is context-independent and thus indicates that no context graphs need to be maintained by
	/// class Energies.
	void indicate_required_context_graphs( utility::vector1< bool > &context_graphs_required ) const override;

	/// @brief AACompositionEnergy is version 1.0 right now.
	///
	core::Size version() const override;

	/// @brief Actually calculate the total energy
	/// @details Called by the scoring machinery.
	void finalize_total_energy( core::pose::Pose & pose, ScoreFunction const &, EnergyMap & totals ) const override;

	/// @brief Calculate the total energy given a vector of const owning pointers to residues.
	/// @details Called directly by the ResidueArrayAnnealingEvaluator during packer runs.
	core::Real calculate_energy(
		utility::vector1< core::conformation::ResidueCOP > const &resvect,
		utility::vector1< core::Size > const & rotamer_ids,
		core::Size const substitution_position = 0
	) const override;

	/// @brief Calculate the total energy given a vector of const owning pointers to residues, vectors of AACompositionEnergySetup objects, and vectors of masks.
	/// @details Called by finalize_total_energy() and during packer runs.  Requires
	/// that setup_residuearrayannealablenergy_for_packing() be called first.
	virtual core::Real calculate_energy(
		utility::vector1< core::conformation::ResidueCOP > const &resvect,
		utility::vector1< AACompositionEnergySetupCOP > const &setup_helpers,
		utility::vector1< core::select::residue_selector::ResidueSubset > const &masks
	) const;

	/// @brief Get a summary of all loaded data.
	///
	void report() const;

	/// @brief Cache data from the pose in this EnergyMethod in anticipation of packing.
	///
	void set_up_residuearrayannealableenergy_for_packing( core::pose::Pose &pose, core::pack::rotamer_set::RotamerSets const &rotamersets, core::scoring::ScoreFunction const &sfxn) override;

	/// @brief Clear the cached data from the pose after packing.
	///
	void clean_up_residuearrayannealableenergy_after_packing( core::pose::Pose &pose ) override;

	/// @brief Disable this energy during minimization.
	void setup_for_minimizing( pose::Pose & pose, ScoreFunction const & sfxn, kinematics::MinimizerMapBase const & minmap ) const override;

	/// @brief Re-enable this energy after minimization.
	void finalize_after_minimizing( pose::Pose & pose ) const override;

private:

	/******************
	Private functions:
	******************/

	/// @brief Get a const-access pointer to a setup helper object.
	///
	inline AACompositionEnergySetupCOP setup_helper_cop( core::Size const index ) const {
		runtime_assert_string_msg( index > 0 && index <= setup_helpers_.size(), "Error in core::scoring::aa_composition_energy::AACompositionEnergy::setup_helper_cop(): Index of setup helper is out of range." );
		return utility::pointer::dynamic_pointer_cast<AACompositionEnergySetup const>(setup_helpers_[index]);
	}

	/// @brief Get the number of setup helper objects:
	///
	inline core::Size setup_helper_count() const { return setup_helpers_.size(); }

	/// @brief Given a pose, pull out the AACompositionEnergySetup objects stored in SequenceConstraints in the pose and
	/// append them to the setup_helpers_ vector, returning a new vector.  This also generates a vector of masks simultaneously.
	/// @param [in] pose The pose from which the AACompositionEnergySetupCOPs will be extracted.
	/// @param [out] setup_helpers The output vector of AACompositionEnergySetupCOPs that is the concatenation of those stored in setup_helpers_ and those from the pose.
	/// @param [out] masks The output vector of ResidueSubsets, which will be equal in size to the helpers vector.
	/// @details The output vectors are first cleared by this operation.
	void get_helpers_from_pose(
		core::pose::Pose const &pose,
		utility::vector1< AACompositionEnergySetupCOP > &setup_helpers,
		utility::vector1< core::select::residue_selector::ResidueSubset > &masks
	) const;

	/******************
	Private variables:
	******************/

	/// @brief Is this energy disabled (e.g. for minimization)?
	mutable bool disabled_;

	/// @brief The vector of helper objects that store all of the data for setting up this scoring function.
	/// @details Initialized on scoreterm initialization.
	utility::vector1<AACompositionEnergySetupOP> setup_helpers_;

	/***********************
	Cached data for packing:
	************************/

	/// @brief A cache of the AACompositionEnergySetup objects to be used for packing.
	/// @details This is the list in setup_helpers_ plus all of the AACompositionEnergySetup objects
	/// stored in AACompositionConstraint objects in the pose.  Initialized by the ResidueArrayAnnealingEvaluator
	/// in its initialize() function.
	utility::vector1<AACompositionEnergySetupCOP> setup_helpers_for_packing_;

	/// @brief A vector of masks corresponding to the AACompositionEnergySetup objects in setup_helpers_for_packing_.
	/// @detalis These are generated from ResidueSelectors.
	utility::vector1< core::select::residue_selector::ResidueSubset > setup_helper_masks_for_packing_;

};

} // aa_composition_energy
} // scoring
} // core


#endif // INCLUDED_core_scoring_EtableEnergy_HH
