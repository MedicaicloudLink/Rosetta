// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file DockingFilters
/// @brief filters for docking
/// @author Jeff Gray

// Unit Headers
#include <protocols/docking/DockFilters.hh>

// Package headers
#include <protocols/docking/metrics.hh>

// Project headers
#include <basic/Tracer.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>

#include <basic/options/option.hh>
#include <basic/options/keys/docking.OptionKeys.gen.hh>

// ObjexxFCL Headers

// C++ Headers
#include <string>

//Utility Headers
#include <utility/tools/make_vector1.hh>

#include <utility/vector1.hh>


using basic::Error;
using basic::Warning;

static basic::Tracer TR( "protocols.docking.DockFilters" );

namespace protocols {
namespace docking {

///////////////// Low-Res Filter /////////////////

DockingLowResFilter::DockingLowResFilter() :
	Filter(),
	use_constraints_(false)
{
	using namespace protocols::score_filters;
	using namespace core::scoring;

	ScoreCutoffFilterOP hascontacts( new ScoreCutoffFilter() );
	hascontacts->set_score_type( interchain_contact );
	hascontacts->set_unweighted( true );
	hascontacts->set_cutoff(9.99); // interchain_contact of 10 indicates no contacts

	ScoreCutoffFilterOP fewclashes( new ScoreCutoffFilter() );
	fewclashes->set_score_type( interchain_vdw );
	fewclashes->set_unweighted( true );
	fewclashes->set_cutoff(1.0); // no more than 1.0 clash (low-res bump score)
	if ( basic::options::option[ basic::options::OptionKeys::docking::docking_low_res_score ]()=="motif_dock_score" ) {
		fewclashes->set_cutoff(5.0);
		TR << "setting clash filter to 5.0" << std::endl;
	}

	filters_ = utility::pointer::make_shared< protocols::filters::FilterCollection >();
	filters_->add_filter( hascontacts );
	filters_->add_filter( fewclashes );
}

DockingLowResFilter::DockingLowResFilter( const DockingLowResFilter & /*init*/ ) = default;

DockingLowResFilter::~DockingLowResFilter() = default;

void
DockingLowResFilter::set_use_constraints( bool setting, core::Real cutoff )
{
	using namespace protocols::score_filters;
	using namespace core::scoring;

	use_constraints_ = setting;
	if ( use_constraints_ ) {
		ScoreCutoffFilterOP constraint_filter( new ScoreCutoffFilter() ) ;
		constraint_filter->set_score_type( atom_pair_constraint );
		constraint_filter->set_cutoff( cutoff );
		constraint_filter->set_unweighted( true );
		filters_->add_filter( constraint_filter );
	} else {
		if ( filters_->size() > 2 ) filters_->remove_last_filter();
	}
}

bool
DockingLowResFilter::apply( core::pose::Pose const & pose ) const
{
	//assert ( !pose.is_fullatom() );
	return filters_->apply(pose);
}

void
DockingLowResFilter::report( std::ostream & out, core::pose::Pose const & pose ) const
{
	filters_->report( out, pose );
}

///////////////// High-Res Filter /////////////////

DockingHighResFilter::DockingHighResFilter( ) : Filter()
{
	movable_jumps_ = utility::tools::make_vector1<core::Size>(1);
	scorefunction_ = core::scoring::get_score_function();
	score_margin_ = 0.0;
	scorefilter_ = utility::pointer::make_shared< protocols::score_filters::ScoreCutoffFilter >();
	scorefilter_->set_score_type( core::scoring::total_score );
	scorefilter_->set_cutoff( 1000000.0 );
}

DockingHighResFilter::DockingHighResFilter( const DockingHighResFilter & /*init*/ ) = default;

DockingHighResFilter::~DockingHighResFilter() = default;

void DockingHighResFilter::set_score_margin( core::Real new_score_margin )
{
	scorefilter_->set_cutoff( scorefilter_->cutoff() + score_margin_ - new_score_margin );
	score_margin_ = new_score_margin;
}

void DockingHighResFilter::set_scorefunction( core::scoring::ScoreFunctionOP const scorefunction ) { scorefunction_ = scorefunction; }
protocols::filters::FilterOP DockingHighResFilter::clone() const { return utility::pointer::make_shared< DockingHighResFilter >( *this ); }

bool
DockingHighResFilter::apply( core::pose::Pose const & pose ) const
{
	//assert ( pose.is_fullatom() );
	core::Real interface_score = protocols::docking::calc_interaction_energy( pose, scorefunction_, movable_jumps_ );
	return scorefilter_->apply(pose) && (interface_score < 0.0);
}

} // namespace docking
} // namespace protocols
