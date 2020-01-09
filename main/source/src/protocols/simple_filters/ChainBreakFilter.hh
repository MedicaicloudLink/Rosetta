// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/simple_filters/ChainBreakFilter.hh

#ifndef INCLUDED_protocols_simple_filters_ChainBreakFilter_hh
#define INCLUDED_protocols_simple_filters_ChainBreakFilter_hh

//unit headers
#include <protocols/simple_filters/ChainBreakFilter.fwd.hh>

// Project Headers
#include <protocols/filters/Filter.hh>
#include <core/pose/Pose.fwd.hh>
#include <basic/datacache/DataMap.fwd.hh>
#include <protocols/moves/Mover.fwd.hh>

namespace protocols {
namespace simple_filters {

/// @brief test whether a pose contains a comment that evaluates to a predefined value. This is useful in controlling execution flow in RosettaScripts.
class ChainBreak : public filters::Filter
{
public:
	ChainBreak();
	~ChainBreak() override;
	filters::FilterOP clone() const override {
		return utility::pointer::make_shared< ChainBreak >( *this );
	}
	filters::FilterOP fresh_instance() const override{
		return utility::pointer::make_shared< ChainBreak >();
	}

	bool apply( core::pose::Pose const & pose ) const override;
	void report( std::ostream & out, core::pose::Pose const & pose ) const override;
	core::Real report_sm( core::pose::Pose const & pose ) const override;
	void parse_my_tag( utility::tag::TagCOP tag, basic::datacache::DataMap &, filters::Filters_map const &filters, moves::Movers_map const &, core::pose::Pose const & ) override;
	core::Size compute( core::pose::Pose const & pose ) const;

	core::Size threshold() const { return threshold_; }
	void threshold( core::Size const & t ) { threshold_ = t; }

	core::Size chain_num() const { return chain_num_; }
	void chain_num( core::Size const & t ) { chain_num_ = t; }

	core::Real tolerance() const { return tolerance_; }
	void tolerance( core::Real const & t ) { tolerance_ = t; }


	std::string
	name() const override;

	static
	std::string
	class_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );


private:
	core::Size threshold_;
	core::Size chain_num_;
	core::Real tolerance_;
};
}
}

#endif
