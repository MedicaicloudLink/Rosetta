// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/task_operations/RestrictToNeighborhoodOperation.cc
/// @brief  TaskOperation class that finds a neighborhood and leaves it mobile in the PackerTask
/// @author Steven Lewis smlewi@gmail.com

// Unit Headers
#include <protocols/task_operations/RestrictToNeighborhoodOperation.hh>
#include <protocols/task_operations/RestrictToNeighborhoodOperationCreator.hh>

// Project Headers
#include <core/pose/Pose.hh>

#include <core/pack/task/PackerTask.hh>

#include <protocols/pose_metric_calculators/NeighborhoodByDistanceCalculator.hh>

#include <core/pose/metrics/CalculatorFactory.hh>

// Utility Headers
#include <core/types.hh>
#include <utility>
#include <utility/vector1_bool.hh>
#include <basic/Tracer.hh>
#include <utility/string_util.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <core/pack/task/operation/task_op_schemas.hh>

#include <utility/excn/Exceptions.hh>

// C++ Headers
#include <set>

using basic::Error;
using basic::Warning;
static basic::Tracer TR( "protocols.TaskOperations.RestrictToNeighborhoodOperation" );

namespace protocols {
namespace task_operations {

using namespace core::pack::task::operation;
using namespace utility::tag;

RestrictToNeighborhoodOperation::RestrictToNeighborhoodOperation() = default;

/// @details this ctor assumes a pregenerated Neighborhood and Neighbors calculators - if you want a particular non-default cutoff distance, assemble those calculators separately then pass them to this operation.
RestrictToNeighborhoodOperation::RestrictToNeighborhoodOperation( std::string const & calculator )
: parent(), calculator_name_(calculator)
{
	//I suppose you could reasonably create this object BEFORE the calculator was generated/registered
	//  if( !core::pose::metrics::CalculatorFactory::Instance().check_calculator_exists( calculator_name_ ) ){
	//   utility_exit_with_message("In RestrictToNeighborhoodOperation, calculator " + calculator + " does not exist.");
	//  }
}

/// @details this ctor generates calculators (easier to use but will rely on defaults, including default distance cutoff)
RestrictToNeighborhoodOperation::RestrictToNeighborhoodOperation( std::set< core::Size > const & central_residues )
: parent(), calculator_name_("")
{
	make_calculator( central_residues );
}

RestrictToNeighborhoodOperation::RestrictToNeighborhoodOperation( std::set< core::Size > const & central_residues, core::Real const dist_cutoff )
: parent(), calculator_name_("")
{
	make_calculator( central_residues, dist_cutoff );
}

RestrictToNeighborhoodOperation::~RestrictToNeighborhoodOperation() = default;

/// @details be warned if you use clone that you'll not get a new interface calculator
core::pack::task::operation::TaskOperationOP RestrictToNeighborhoodOperation::clone() const
{
	return utility::pointer::make_shared< RestrictToNeighborhoodOperation >( *this );
}

RestrictToNeighborhoodOperation::RestrictToNeighborhoodOperation( RestrictToNeighborhoodOperation const & rhs) :
	parent(rhs)
{
	*this = rhs;
}

/// @brief assignment operator
RestrictToNeighborhoodOperation & RestrictToNeighborhoodOperation::operator=(
	RestrictToNeighborhoodOperation const & rhs ){

	//abort self-assignment
	if ( this == &rhs ) return *this;

	calculator_name_ = rhs.get_calculator_name();

	return *this;
}

/// @details private helper function to make calculator - runs in the ctor
void RestrictToNeighborhoodOperation::make_calculator(
	std::set< core::Size > const & central_residues,
	core::Real dist_cutoff
) {
	make_name( central_residues );

	using namespace core::pose::metrics;
	if ( CalculatorFactory::Instance().check_calculator_exists( calculator_name_ ) ) {
		TR.Warning << "In RestrictToNeighborhoodOperation, calculator " << calculator_name_
			<< " already exists, this is hopefully correct for your purposes" << std::endl;
	} else {
		using protocols::pose_metric_calculators::NeighborhoodByDistanceCalculator;
		CalculatorFactory::Instance().register_calculator( calculator_name_, utility::pointer::make_shared< NeighborhoodByDistanceCalculator >( central_residues, dist_cutoff ) );
	}
}

/// @details private helper function to make calculator - runs in the ctor
void RestrictToNeighborhoodOperation::make_calculator( std::set< core::Size > const & central_residues ) {
	make_name( central_residues );

	using namespace core::pose::metrics;
	if ( CalculatorFactory::Instance().check_calculator_exists( calculator_name_ ) ) {
		TR.Warning << "In RestrictToNeighborhoodOperation, calculator " << calculator_name_
			<< " already exists, this is hopefully correct for your purposes" << std::endl;
	} else {
		using protocols::pose_metric_calculators::NeighborhoodByDistanceCalculator;
		CalculatorFactory::Instance().register_calculator( calculator_name_, utility::pointer::make_shared< NeighborhoodByDistanceCalculator >( central_residues ) );
	}
}

/// @details private helper function to name calculator- runs in the ctor
void RestrictToNeighborhoodOperation::make_name( std::set< core::Size > const & central_residues ) {
	calculator_name_ = "RTNhO_calculator";

	for ( unsigned long central_residue : central_residues ) {
		calculator_name_ += '_' + utility::to_string( central_residue );
	}

}

void
RestrictToNeighborhoodOperation::apply( core::pose::Pose const & pose, core::pack::task::PackerTask & task ) const
{

	//vector for filling packertask
	utility::vector1_bool repack(pose.size(), false);

	//this is in the parent class, RestrictOperationsBase
	run_calculator(pose, calculator_name_, "neighbors", repack);

	task.restrict_to_residues(repack);

	return;
}

//utility function for get_central_residues and get_distance_cutoff
protocols::pose_metric_calculators::NeighborhoodByDistanceCalculatorCOP
RestrictToNeighborhoodOperation::get_calculator() const {
	using namespace core::pose::metrics;
	if ( !CalculatorFactory::Instance().check_calculator_exists( calculator_name_ ) ) {
		throw CREATE_EXCEPTION(utility::excn::Exception, "In RestrictToNeighborhoodOperation get_calculator, calculator " + calculator_name_ + " does not exist");
	}

	using protocols::pose_metric_calculators::NeighborhoodByDistanceCalculatorCOP;
	using protocols::pose_metric_calculators::NeighborhoodByDistanceCalculator;

	debug_assert( dynamic_cast< NeighborhoodByDistanceCalculator const * > (CalculatorFactory::Instance().retrieve_calculator(calculator_name_).get()));

	NeighborhoodByDistanceCalculatorCOP calculator(
		utility::pointer::static_pointer_cast< protocols::pose_metric_calculators::NeighborhoodByDistanceCalculator const > ( CalculatorFactory::Instance().retrieve_calculator(calculator_name_) ));

	return calculator;
}

std::set< core::Size > const & RestrictToNeighborhoodOperation::get_central_residues() const {
	return get_calculator()->central_residues();
}

core::Real RestrictToNeighborhoodOperation::get_distance_cutoff() const {
	return get_calculator()->dist_cutoff();
}

/// @brief reskin of normal make_calculator
void  RestrictToNeighborhoodOperation::set_neighborhood_parameters(
	SizeSet const & central_residues,
	core::Real dist_cutoff )
{
	make_calculator(central_residues, dist_cutoff);
}

/// @brief reskin of normal make_calculator
void  RestrictToNeighborhoodOperation::set_neighborhood_parameters( SizeSet const & central_residues )
{
	make_calculator(central_residues);
}

// AMW: no parse_tag.
void RestrictToNeighborhoodOperation::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	task_op_schema_empty( xsd, keyname(), "XRW TO DO" );
}

core::pack::task::operation::TaskOperationOP
RestrictToNeighborhoodOperationCreator::create_task_operation() const
{
	return utility::pointer::make_shared< RestrictToNeighborhoodOperation >();
}

void RestrictToNeighborhoodOperationCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	RestrictToNeighborhoodOperation::provide_xml_schema( xsd );
}

std::string RestrictToNeighborhoodOperationCreator::keyname() const
{
	return RestrictToNeighborhoodOperation::keyname();
}


} //namespace protocols
} //namespace task_operations
