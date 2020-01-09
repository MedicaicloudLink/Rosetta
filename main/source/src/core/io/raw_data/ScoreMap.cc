// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   ScoreMap.cc
/// @brief  A place to put some common functions for score-file output
/// @author Monica Berrondo

// Unit headers
#include <core/io/raw_data/ScoreMap.hh>

// Project headers
#include <core/pose/Pose.hh>
#include <core/pose/datacache/CacheableDataType.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/simple_metrics/SimpleMetricData.hh>

// utility
#include <utility/string_util.hh>

// ObjexxFCL headers
#include <ObjexxFCL/format.hh>

///Basic headers
#include <basic/datacache/BasicDataCache.hh>
#include <basic/datacache/CacheableString.hh>
#include <basic/datacache/CacheableStringFloatMap.hh>
#include <basic/datacache/CacheableStringIntegerMap.hh>
#include <basic/datacache/CacheableStringMap.hh>



using namespace core;

namespace core {
namespace io {
namespace raw_data {

/// @details Auto-generated virtual destructor
ScoreMap::~ScoreMap() = default;

/// Score output helper functions
/// @details
///  creates a list of non zero weighted energies and adds them to the
///  score_file information that is to be written out at the end of the
///  protocol.
void
ScoreMap::score_and_add_energies_to_map(
	std::map < std::string, core::Real > & score_map,
	scoring::ScoreFunctionCOP score_fxn,
	pose::Pose & pose
)
{
	using namespace core::scoring;

	(*score_fxn)(pose);

	add_energies_data_from_scored_pose(pose, score_map);
}


std::map< std::string, core::Real >
ScoreMap::get_energies_map_from_scored_pose( core::pose::Pose const & pose ){
	using namespace core::scoring;
	std::map< std::string, core::Real > score_map;
	add_energies_data_from_scored_pose(pose, score_map );
	return score_map;
}

void
ScoreMap::add_energies_data_from_scored_pose( core::pose::Pose const & pose, std::map< std::string, core::Real > & score_map ){
	// Which score terms to use
	core::scoring::EnergyMap weights = pose.energies().weights();
	using ScoreTypeVec = utility::vector1<core::scoring::ScoreType>;
	ScoreTypeVec score_types;
	for ( int i = 1; i <= core::scoring::n_score_types; ++i ) {
		auto ii = core::scoring::ScoreType(i);
		if ( weights[ii] != 0 ) score_types.push_back(ii);
	}

	core::Real total(0);

	for ( auto & score_type : score_types ) {
		core::Real const some_score( weights[score_type] * pose.energies().total_energies()[ score_type ] );
		score_map[ name_from_score_type(score_type) ] = some_score;
		total += some_score;
	}
	score_map[ name_from_score_type(core::scoring::total_score) ] = total;
}

/// @brief print out the contents of the ScoreMap
void
ScoreMap::print(
	std::map < std::string, core::Real > const & score_map,
	std::ostream & out
)
{
	using namespace ObjexxFCL::format;

	std::map< std::string, Real >::const_iterator pair;
	Size width (8), precision (3);

	//print the header
	for ( pair=score_map.begin(); pair!=score_map.end(); ++pair ) {
		if ( pair->first.length() > 8 ) width = pair->first.length();
		out << ' ' << A( width, pair->first );
	}
	out << std::endl;

	//print the information
	for ( pair=score_map.begin(); pair!=score_map.end(); ++pair ) {
		if ( pair->first.length() > 8 ) width = pair->first.length();
		out << ' ' << F( width, precision, pair->second );
	}
	out << std::endl;

}

void
ScoreMap::add_arbitrary_string_data_from_pose( pose::Pose const & pose, std::map< std::string, std::string > & string_map){
	if ( pose.data().has( core::pose::datacache::CacheableDataType::ARBITRARY_STRING_DATA ) ) {
		basic::datacache::CacheableStringMapCOP data
			= utility::pointer::dynamic_pointer_cast< basic::datacache::CacheableStringMap const >
			( pose.data().get_const_ptr( core::pose::datacache::CacheableDataType::ARBITRARY_STRING_DATA ) );
		debug_assert( data.get() != nullptr );

		for ( auto const & it : data->map() ) {
			string_map[it.first] = it.second;
		}
	}

	//Simple Metric Data
	if ( pose.data().has( core::pose::datacache::CacheableDataType::SIMPLE_METRIC_DATA) ) {
		simple_metrics::SimpleMetricDataCOP sm_cache
			= utility::pointer::dynamic_pointer_cast< simple_metrics::SimpleMetricData const >
			( pose.data().get_const_ptr( core::pose::datacache::CacheableDataType::SIMPLE_METRIC_DATA ) );
		debug_assert( sm_cache.get() != nullptr );

		//String Metric
		for ( auto const & pair : sm_cache->get_string_metric_data() ) {
			string_map[pair.first] = pair.second;
		}

		//Composite String Metric
		for ( auto const & name_map_pair : sm_cache->get_composite_string_metric_data() ) {
			std::string const & name( name_map_pair.first );

			for ( auto const & composite_value_pair : name_map_pair.second ) {
				std::string final_name = name+"_"+composite_value_pair.first;
				string_map[final_name] = composite_value_pair.second;
			}
		}

		//Per Residue String Metric
		for ( auto const & name_map_pair : sm_cache->get_per_residue_string_metric_output() ) {
			std::string name = name_map_pair.first;

			for ( auto const & res_value : name_map_pair.second ) {
				std::string res_output = res_value.first;
				string_map[name+"_"+res_output] = res_value.second;
			}
		}

	} // End SimpleMetric extraction
}

std::map< std::string, std::string >
ScoreMap::get_arbitrary_string_data_from_pose( pose::Pose const & pose){
	std::map< std::string, std::string > string_map;
	add_arbitrary_string_data_from_pose(pose, string_map);
	return string_map;
}

void
ScoreMap::add_arbitrary_score_data_from_pose(core::pose::Pose const & pose, std::map<std::string, core::Real> & score_map){

	if ( pose.data().has( core::pose::datacache::CacheableDataType::ARBITRARY_FLOAT_DATA ) ) {
		basic::datacache::CacheableStringFloatMapCOP data
			= utility::pointer::dynamic_pointer_cast< basic::datacache::CacheableStringFloatMap const >
			( pose.data().get_const_ptr( core::pose::datacache::CacheableDataType::ARBITRARY_FLOAT_DATA ) );
		debug_assert( data.get() != nullptr );

		for ( auto const & it : data->map() ) {
			score_map[it.first] = it.second;
		}
	}
	if ( pose.data().has( core::pose::datacache::CacheableDataType::SIMPLE_METRIC_DATA) ) {}

	//Simple Metric Data
	if ( pose.data().has( core::pose::datacache::CacheableDataType::SIMPLE_METRIC_DATA) ) {
		simple_metrics::SimpleMetricDataCOP sm_cache
			= utility::pointer::dynamic_pointer_cast< simple_metrics::SimpleMetricData const >
			( pose.data().get_const_ptr( core::pose::datacache::CacheableDataType::SIMPLE_METRIC_DATA ) );
		debug_assert( sm_cache.get() != nullptr );

		//String Metric
		for ( auto const & pair : sm_cache->get_real_metric_data() ) {
			score_map[pair.first] = pair.second;
		}

		//Composite String Metric
		for ( auto const & name_map_pair : sm_cache->get_composite_real_metric_data() ) {
			std::string const & name( name_map_pair.first );

			for ( auto const & composite_value_pair : name_map_pair.second ) {
				std::string final_name = name+"_"+composite_value_pair.first;
				score_map[final_name] = composite_value_pair.second;
			}
		}

		//Per Residue String Metric
		for ( auto const & name_map_pair : sm_cache->get_per_residue_real_metric_output() ) {
			std::string const & name( name_map_pair.first );

			for ( auto const & res_value : name_map_pair.second ) {
				std::string const & res_output( res_value.first);
				score_map[name+"_"+res_output] = res_value.second;
			}
		}

	} // End SimpleMetric extraction
}


std::map< std::string, core::Real >
ScoreMap::get_arbitrary_score_data_from_pose( pose::Pose const & pose){
	std::map< std::string, core::Real > score_map;
	add_arbitrary_score_data_from_pose(pose, score_map);
	return score_map;
}


} // raw_data
} // io
} // core
