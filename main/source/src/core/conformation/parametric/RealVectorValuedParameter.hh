// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/conformation/parameters/RealVectorValuedParameter.hh
/// @brief  Prototypes and method declarations for the RealVectorValuedParameter class, a class for holding a single vector-valued parameter for parametric backbone generation.
/// @author Vikram K. Mulligan (vmullig@uw.edu)


#ifndef INCLUDED_core_conformation_parametric_RealVectorValuedParameter_hh
#define INCLUDED_core_conformation_parametric_RealVectorValuedParameter_hh


// Unit headers
#include <core/conformation/parametric/Parameter.hh>
#include <core/conformation/parametric/RealVectorValuedParameter.fwd.hh>

// Project headers
#include <core/types.hh>

// Utility headers
#include <utility/pointer/access_ptr.hh>
#include <utility/pointer/owning_ptr.hh>
#include <utility/pointer/ReferenceCount.hh>
#include <utility/vector1.hh>
#include <utility/tag/Tag.fwd.hh>

// Numeric headers

// C++ headers


#ifdef    SERIALIZATION
// Cereal headers
#include <cereal/types/polymorphic.fwd.hpp>
#endif // SERIALIZATION


namespace core {
namespace conformation {
namespace parametric {

/// @brief  RealVectorValuedParameter class, used to store a single vector-valued parameter for parametric backbone generation.
///
class RealVectorValuedParameter : public Parameter
{
public:

	/// @brief constructors
	///
	RealVectorValuedParameter();

	RealVectorValuedParameter( RealVectorValuedParameter const & src );

	~RealVectorValuedParameter() override;

	/// @brief Make a copy of this object ( allocate actual memory for it )
	///
	ParameterOP clone() const override;

public: //Getters

	/// @brief Get the value of this parameter.
	inline utility::vector1< core::Real > const & value() const { return values_; }

	/// @brief Get the defaults for this parameter.
	inline utility::vector1< core::Real > const & default_value() const { return default_value_; }

public: //Setters

	/// @brief Set the value of this parameter.
	void set_value( utility::vector1< core::Real > const &value_in );

	/// @brief Sets the parameter type.
	/// @details Override is limited to real types.
	void set_parameter_type( ParameterType const type_in ) override;

	/// @brief Set the default value for this parameter.
	void set_default_value( utility::vector1< core::Real > const & value_in );

	/// @brief Given another parameter of the same type, copy its value.  This does *not* set value_set_ to true.
	/// @details Performs type checking in debug mode.
	bool copy_value_from_parameter( ParameterCOP other_parameter, ParametersCOP other_parameter_collection, ParametersCOP this_parameter_collection ) override;

private: //Functions

	/// @brief If this is a parameter storing an angle, correct the value to be (-Pi:Pi].  If it is supposed to be nonnegative, or nonzero/positive throw
	/// an error if it is not.
	void correct_values();

public: //Parse functions

	/// @brief Given a tag, parse out the setting for this parameter.
	/// @details If parse_setting is true, this tries to parse the value for this parameter.  If parse_grid_sampling is true, this tries to parse
	/// a range of values to sample, and a number of samples.  If parse_perturbation is true, this parses options for perturbing the value of the
	/// parameter.  Note that, if multiple options are true, grid sampling or perturbation options are given priority over a flat setting (and an error
	/// is thrown if more than one of these is provided).
	/// @note A vector of floats must be provided as a whitespace-separated string.
	void parse_setting( utility::tag::TagCOP tag, bool const parse_setting, bool const parse_grid_sampling, bool const parse_perturbation, bool const parse_copying ) override;

	/// @brief Return the XSD information for this parameter.
	/// @note Currently, RealVector-valued parameters can't be sampled or perturbed, so this only provides setting information.
	void provide_xsd_information( utility::tag::AttributeList & xsd, bool const provide_setting, bool const /*provide_copying*/, bool const /*provide_grid_sampling*/, bool const /*provide_perturbation*/ ) const override;

	/// @brief Reset the sampling and perturbation options before storing this Parameter object in a parametric Conformation.
	/// @details Pure virtual.  Must be implemented by derived classes.
	void reset_sampling_and_perturbation_settings() override;

	/// @brief Reset the value settings for this Parameter object.
	void reset_value_settings() override;

private:

	/********************************************************************************
	PRIVATE DATA
	*********************************************************************************/

	/// @brief The vector of values for this parameter.
	utility::vector1< core::Real > values_;

	/// @brief The default values for this parameter.
	utility::vector1< core::Real > default_value_;

#ifdef    SERIALIZATION
public:
	template< class Archive > void save( Archive & arc ) const;
	template< class Archive > void load( Archive & arc );
#endif // SERIALIZATION

}; //class RealVectorValuedParameter

} // namespace parametric
} // namespace conformation
} // namespace core

#ifdef    SERIALIZATION
CEREAL_FORCE_DYNAMIC_INIT( core_conformation_parametric_RealVectorValuedParameter )
#endif // SERIALIZATION


#endif
