// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file --path--/--class--.hh
/// @brief --brief--
/// @author --name-- (--email--)

#ifndef INCLUDED_--path_underscore--_--class--_hh
#define INCLUDED_--path_underscore--_--class--_hh

// Unit headers
#include <--path--/--class--.fwd.hh>
#include <core/scoring/methods/OneBodyEnergy.hh>

// Package Headers
#include <core/scoring/methods/EnergyMethod.hh> 
#include <core/scoring/EnergyMap.fwd.hh> 
#include <core/scoring/ScoreFunction.fwd.hh>

// Project headers
#include <core/conformation/Residue.fwd.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/id/TorsionID.fwd.hh>
#include <core/id/DOF_ID.fwd.hh>
#include <core/kinematics/MinimizerMapBase.fwd.hh>

// Utility headers
#include <utility/vector1.fwd.hh>

#include <core/scoring/DerivVectorPair.fwd.hh>
#include <utility/vector1.hh>

--namespace--

///@brief --brief--
class --class-- : public core::scoring::methods::OneBodyEnergy {

public:

	typedef core::scoring::methods::OneBodyEnergy parent; 
    typedef core::pose::Pose Pose;
    typedef core::conformation::Residue Residue;
    typedef core::scoring::ScoreFunction ScoreFunction;
    typedef core::scoring::EnergyMap EnergyMap;
    typedef core::scoring::ResPairMinimizationData ResPairMinimizationData;

public:

	--class--();

	// copy constructor (not needed unless you need deep copies)
	//--class--( --class-- const & src );

	// destructor (important for properly forward-declaring smart-pointer members)
	virtual ~--class--();

	/// @brief Evaluate the one-body energies for a particular residue, in the context of a
	/// given Pose, and increment those energies in the input Emap (do not overwrite them).
	virtual
	void
	residue_energy(
		Residue const & rsd,
		Pose const & pose,
		EnergyMap & emap
	) const;

	/// @brief During minimization, energy methods are allowed to decide that they say nothing
	/// about a particular residue (e.g. no non-zero energy) and as a result they will not be queried for
	/// a derivative or an energy.  The default behavior is to return "true" for all residues.
	virtual
	bool
	defines_score_for_residue(
		Residue const &
	) const;

	/// @brief Rely on the extended version of the residue_energy function during score-function
	/// evaluation in minimization? The extended version (below) takes a ResSingleMinimizationData.
	/// Return 'true' for the extended version.  The default method implemented in this class returns 'false'
	virtual
	bool
	use_extended_residue_energy_interface() const;

	/// @brief Evaluate the one-body energies for a particular residue, in the context of a
	/// given Pose, and with the help of a piece of cached data for minimization, increment those
	/// one body energies into the input EnergyMap.  The calling function must guarantee that this
	/// EnergyMethod has had the opportunity to update the input ResSingleMinimizationData object
	/// for the given residue in a call to setup_for_minimizing_for_residue before this function is
	/// invoked. This function should not be called unless the use_extended_residue_energy_interface()
	/// method returns "true".  Default implementation provided by this base class calls
	/// utility::exit(). The Pose merely serves as context, and the input residue is not required
	/// to be a member of the Pose.
	virtual
	void
	residue_energy_ext(
		Residue const & rsd,
		ResSingleMinimizationData const & min_data,
		Pose const & pose,
		EnergyMap & emap
	) const;


	/// @brief Called at the beginning of minimization, allowing this energy method to cache data
	/// pertinent for a single residue in the the ResSingleMinimizationData that is used for a
	/// particular residue in the context of a particular Pose.  This base class provides a noop
	/// implementation for this function if there is nothing that the derived class needs to perform
	/// in this setup phase.   The Pose merely serves as context, and the input residue is not
	/// required to be a member of the Pose.
	virtual
	void
	setup_for_minimizing_for_residue(
		Residue const & rsd,
		Pose const & ,
		ScoreFunction const & ,
		MinimizerMapBase const & ,
		ResSingleMinimizationData &
	) const;

	/// @brief Does this EnergyMethod require the opportunity to examine the residue before scoring begins?  Not
	/// all energy methods would.  The ScoreFunction will not ask energy methods to examine residues that are uninterested
	/// in doing so.
	virtual
	bool
	requires_a_setup_for_scoring_for_residue_opportunity( Pose const & pose ) const;

	/// @brief Do any setup work should the coordinates of this residue, who is still guaranteed to be
	/// of the same residue type as when setup_for_minimizing_for_residue was called, have changed so dramatically
	/// as to possibly require some amount of setup work before scoring should proceed
	virtual
	void
	setup_for_scoring_for_residue(
		Residue const & rsd,
		Pose const & pose,
		ScoreFunction const & sfxn,
		ResSingleMinimizationData & min_data
	) const;

	/// @brief Do any setup work necessary before evaluating the derivatives for this residue
	virtual
	void
	setup_for_derivatives_for_residue(
		Residue const & rsd,
		Pose const & pose,
		ScoreFunction const & sfxn,
		ResSingleMinimizationData & min_data
	) const;

	/// @brief Evaluate the derivatives for all atoms on this residue and increment them
	/// into the input atom_derivs vector1.  The calling function must guarantee that
	/// setup for derivatives is called before this function is, and that the atom_derivs
	/// vector contains at least as many entries as there are atoms in the input Residue.
	/// This base class provides a default noop implementation of this function.
	virtual
	void
	eval_residue_derivatives(
		Residue const & rsd,
		ResSingleMinimizationData const & min_data,
		Pose const & pose,
		EnergyMap const & weights,
		utility::vector1< DerivVectorPair > & atom_derivs
	) const;

private:

};

--end_namespace--

#endif //--path--_--class--_hh
