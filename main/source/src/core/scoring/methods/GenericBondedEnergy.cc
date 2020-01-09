// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/scoring/methods/GenericBondedEnergy.cc
/// @brief  A "generic" (atom-type-only-based) torsional potential
/// @author Hahnbeom Park and Frank DiMaio


// Unit headers
#include <core/scoring/GenericBondedPotential.hh>
#include <core/scoring/methods/GenericBondedEnergy.hh>
#include <core/scoring/methods/GenericBondedEnergyCreator.hh>

// Package headers
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoringManager.hh>
#include <core/scoring/methods/ContextIndependentTwoBodyEnergy.hh>
#include <core/scoring/EnergyMap.hh>
#include <core/scoring/PolymerBondedEnergyContainer.hh>
#include <core/scoring/Energies.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/AtomType.hh>

#include <core/pose/symmetry/util.hh>
#include <core/conformation/symmetry/SymmetryInfo.hh>

// Project headers
#include <core/pose/Pose.hh>
#include <core/conformation/Residue.hh>

// Numeric headers
#include <basic/basic.hh>
#include <basic/Tracer.hh>
#include <math.h>
#include <utility/vector1.hh>

#include <basic/options/option.hh>
#include <basic/options/keys/score.OptionKeys.gen.hh>

namespace core {
namespace scoring {
namespace methods {

static basic::Tracer TR( "core.scoring.GenericBondedEnergy" );

/// @details This must return a fresh instance of the P_AA_pp_Energy class,
/// never an instance already in use
methods::EnergyMethodOP
GenericBondedEnergyCreator::create_energy_method(
	methods::EnergyMethodOptions const & options
) const {
	return utility::pointer::make_shared< GenericBondedEnergy >( options );
}

ScoreTypes
GenericBondedEnergyCreator::score_types_for_method() const {
	ScoreTypes sts;
	sts.push_back( gen_bonded );
	sts.push_back( gen_bonded_bond );
	sts.push_back( gen_bonded_angle );
	sts.push_back( gen_bonded_torsion );
	sts.push_back( gen_bonded_improper );
	return sts;
}

/// ctor
GenericBondedEnergy::GenericBondedEnergy( GenericBondedEnergy const & src ) :
	parent( src ),
	potential_( src.potential_ ),
	score_canonical_aas_( src.score_canonical_aas() )
{
}

GenericBondedEnergy::GenericBondedEnergy( EnergyMethodOptions const & eopt ):
	parent( utility::pointer::make_shared< GenericBondedEnergyCreator >() ),
	potential_( ScoringManager::get_instance()->get_GenericBondedPotential() )
{
	score_canonical_aas_ = eopt.genbonded_score_canonical_aas();
}

/// clone
EnergyMethodOP
GenericBondedEnergy::clone() const
{
	return utility::pointer::make_shared< GenericBondedEnergy >( *this );
}

void
GenericBondedEnergy::setup_for_scoring(
	pose::Pose & pose,
	ScoreFunction const &sfxn
) const {
	using namespace methods;

	// create LR energy container
	LongRangeEnergyType const & lr_type( long_range_type() );
	Energies & energies( pose.energies() );
	bool create_new_lre_container( false );

	if ( energies.long_range_container( lr_type ) == 0 ) {
		create_new_lre_container = true;
	} else {
		LREnergyContainerOP lrc = energies.nonconst_long_range_container( lr_type );
		PolymerBondedEnergyContainerOP dec( utility::pointer::static_pointer_cast< core::scoring::PolymerBondedEnergyContainer > ( lrc ) );
		if ( !dec || !dec->is_valid( pose ) ) {
			create_new_lre_container = true;
		}
	}

	if ( create_new_lre_container ) {
		Size nres = pose.size();
		if ( core::pose::symmetry::is_symmetric(pose) ) {
			nres = core::pose::symmetry::symmetry_info(pose)->last_independent_residue();
		}

		TR << "Creating new peptide-bonded energy container (" << nres << ")" << std::endl;
		utility::vector1< ScoreType > s_types;
		s_types.push_back( gen_bonded );
		s_types.push_back( gen_bonded_bond );
		s_types.push_back( gen_bonded_angle );
		s_types.push_back( gen_bonded_torsion );
		s_types.push_back( gen_bonded_improper );
		LREnergyContainerOP new_dec( new PolymerBondedEnergyContainer( pose, s_types ) );
		energies.set_long_range_container( lr_type, new_dec );
	}

	potential_.setup_for_scoring( pose, sfxn );
}

methods::LongRangeEnergyType
GenericBondedEnergy::long_range_type() const { return methods::gen_bonded_lr; }

void
GenericBondedEnergy::setup_for_derivatives(
	pose::Pose & pose,
	ScoreFunction const &sfxn
) const {
	potential_.setup_for_scoring( pose, sfxn );
}

void
GenericBondedEnergy::residue_pair_energy(
	conformation::Residue const & rsd1,
	conformation::Residue const & rsd2,
	pose::Pose const & pose,
	ScoreFunction const & ,
	EnergyMap & emap
) const {

	if ( !defines_score_for_residue_pair( rsd1, rsd2 ) ) return;
	potential_.residue_pair_energy( rsd1, rsd2, pose, emap );
}

void
GenericBondedEnergy::eval_residue_pair_derivatives(
	conformation::Residue const & rsd1,
	conformation::Residue const & rsd2,
	ResSingleMinimizationData const &,
	ResSingleMinimizationData const &,
	ResPairMinimizationData const & /*min_data*/,
	pose::Pose const & pose, // provides context
	EnergyMap const & weights,
	utility::vector1< DerivVectorPair > & r1_atom_derivs,
	utility::vector1< DerivVectorPair > & r2_atom_derivs
) const {

	if ( !defines_score_for_residue_pair( rsd1, rsd2 ) ) return;

	potential_.residue_pair_derivatives( rsd1, rsd2, pose, weights, r1_atom_derivs, r2_atom_derivs );
}

bool
GenericBondedEnergy::defines_score_for_residue_pair(
	conformation::Residue const & rsd1,
	conformation::Residue const & rsd2,
	bool /*res_moving_wrt_eachother*/ ) const
{
	if ( !score_canonical_aas() ) {
		bool rsd1_is_canonical_aa = chemical::is_canonical_L_aa_or_gly( rsd1.aa() ) ||
			chemical::is_canonical_D_aa( rsd1.aa() );
		bool rsd2_is_canonical_aa = chemical::is_canonical_L_aa_or_gly( rsd2.aa() ) ||
			chemical::is_canonical_D_aa( rsd2.aa() );

		// still allow scoring if either is non-canonical_aa
		if ( rsd1_is_canonical_aa && rsd2_is_canonical_aa ) return false;
	}

	return true;
}

void
GenericBondedEnergy::eval_intrares_energy(
	conformation::Residue const & rsd,
	pose::Pose const & pose,
	ScoreFunction const & /*sfxn*/,
	EnergyMap & emap
) const {

	if ( !score_canonical_aas() ) {
		bool rsd_is_canonical_aa = chemical::is_canonical_L_aa_or_gly( rsd.aa() ) ||
			chemical::is_canonical_D_aa( rsd.aa() );
		if ( rsd_is_canonical_aa ) return;
	}

	potential_.residue_energy( rsd, pose, emap );
}

// using atom derivs instead of dof derivates;
void
GenericBondedEnergy::eval_intrares_derivatives(
	conformation::Residue const & rsd,
	ResSingleMinimizationData const & /*res_data_cache*/,
	pose::Pose const & pose,
	EnergyMap const & weights,
	utility::vector1< DerivVectorPair > & atom_derivs
) const {

	if ( !score_canonical_aas() ) {
		bool rsd_is_canonical_aa = chemical::is_canonical_L_aa_or_gly( rsd.aa() ) ||
			chemical::is_canonical_D_aa( rsd.aa() );
		if ( rsd_is_canonical_aa ) return;
	}

	potential_.residue_derivatives( rsd, pose, weights, atom_derivs );
}

void
GenericBondedEnergy::indicate_required_context_graphs( utility::vector1< bool > & ) const {}

core::Size
GenericBondedEnergy::version() const {
	return 1; // Initial versioning
}

} // methods
} // scoring
} // core

