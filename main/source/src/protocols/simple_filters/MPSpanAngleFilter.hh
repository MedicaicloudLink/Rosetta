// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/simple_filters/MPSpanAngleFilter.hh
/// @brief definition of filter class MPSpanAngleFilter.
/// @author Jonathan Weinstein (jonathan.weinstein@weizmann.ac.il)

#ifndef INCLUDED_protocols_simple_filters_MPSpanAngleFilter_hh
#define INCLUDED_protocols_simple_filters_MPSpanAngleFilter_hh

#include <protocols/simple_filters/MPSpanAngleFilter.fwd.hh>


// Project Headers
#include <core/scoring/ScoreFunction.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/types.hh>
#include <protocols/filters/Filter.hh>
#include <utility/tag/Tag.fwd.hh>
#include <basic/datacache/DataMap.fwd.hh>
#include <protocols/moves/Mover.fwd.hh>
#include <core/scoring/ScoreType.hh>
#include <utility/exit.hh>

#include <string>
#include <utility/vector1.hh>

namespace protocols {
namespace simple_filters {

class MPSpanAngleFilter : public filters::Filter
{
public:
	MPSpanAngleFilter() : filters::Filter( "MPSpanAngle" ) {}
	//MPSpanAngleFilter( core::Real const threshold );
	bool apply( core::pose::Pose const & pose ) const override;
	void report( std::ostream & out, core::pose::Pose const & pose ) const override;
	core::Real report_sm( core::pose::Pose const & pose ) const override;
	utility::vector1 < core::Real > compute( core::pose::Pose const & pose ) const;
	filters::FilterOP clone() const  override {
		return utility::pointer::make_shared< MPSpanAngleFilter >( *this );
	}
	filters::FilterOP fresh_instance() const override {
		return utility::pointer::make_shared< MPSpanAngleFilter >();
	}

	virtual ~MPSpanAngleFilter();
	void parse_my_tag( utility::tag::TagCOP tag, basic::datacache::DataMap &, filters::Filters_map const &, protocols::moves::Movers_map const &, core::pose::Pose const & ) override;

	std::string
	name() const override;

	static
	std::string
	class_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

private:
	core::Real threshold_ = -10.0;
	std::string output_ = "TR";
	core::Size tm_num_ = 0;
	std::string angle_or_score_ = "angle";
	core::Real ang_max_ = 90;
	core::Real ang_min_ = 0;

};

}
}
#endif
