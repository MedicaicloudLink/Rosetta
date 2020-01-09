// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/jd3/pose_outputters/ScoreFileOutputter.cc
/// @brief  Definition of the %ScoreFileOutputter class's methods
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)

//unit headers
#include <protocols/jd3/pose_outputters/ScoreFileOutputter.hh>
#include <protocols/jd3/pose_outputters/ScoreFileOutputterCreator.hh>

// package headers
#include <protocols/jd3/pose_outputters/ScoreFileOutputSpecification.hh>
#include <protocols/jd3/pose_outputters/PoseOutputterFactory.hh>
#include <protocols/jd3/InnerLarvalJob.hh>
#include <protocols/jd3/LarvalJob.hh>
#include <protocols/jd3/job_results/PoseJobResult.hh>

#include <core/types.hh>
#include <core/io/raw_data/ScoreFileData.hh>
#include <core/io/raw_data/ScoreMap.hh>

#include <basic/datacache/ConstDataMap.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/run.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>
//#include <basic/options/keys/jd3.OptionKeys.gen.hh>

#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <utility/options/keys/OptionKeyList.hh>

#ifdef    SERIALIZATION
// Utility serialization headers
#include <utility/vector1.srlz.hh>
#include <utility/serialization/serialization.hh>

// Cereal headers
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/string.hpp>
#endif // SERIALIZATION

namespace protocols {
namespace jd3 {
namespace pose_outputters {

ScoreFileOutputter::ScoreFileOutputter() = default;

ScoreFileOutputter::~ScoreFileOutputter() { flush(); }

std::string
ScoreFileOutputter::outputter_for_job(
	utility::tag::TagCOP output_tag,
	utility::options::OptionCollection const & job_options,
	InnerLarvalJob const & job
) const
{
	utility::file::FileName scorefile_name_for_job = filename_for_job( output_tag, job_options, job );
	return scorefile_name_for_job.name();
}

std::string
ScoreFileOutputter::outputter_for_job(
	PoseOutputSpecification const & spec
) const
{
	using SFOP = ScoreFileOutputSpecification;
	debug_assert( dynamic_cast< SFOP const * > ( &spec ) );
	auto const & sf_spec( static_cast< SFOP const & > (spec) );
	return sf_spec.out_fname();
}

bool
ScoreFileOutputter::outputter_specified_by_command_line()
{
	return ! basic::options::option[ basic::options::OptionKeys::run::no_scorefile ];
}

/// @brief Using the LarvalJob's job_tag (determined by the (primary) JobOutputter),
/// write extra data about this output Pose to its destination.
PoseOutputSpecificationOP
ScoreFileOutputter::create_output_specification(
	LarvalJob const & job,
	JobOutputIndex const & output_index,
	utility::options::OptionCollection const & options,
	utility::tag::TagCOP my_outputter_tag
)
{
	ScoreFileOutputSpecificationOP spec( new ScoreFileOutputSpecification );

	spec->out_fname( filename_for_job( my_outputter_tag, options, *job.inner_job() ) );
	spec->pose_tag( job.status_prefix() + job.job_tag_with_index_suffix( output_index ) + job.status_suffix() );
	return spec;
}

/// @brief Write a pose out to permanent storage (whatever that may be).
///
/// @details currently the score file outputter writes scoress immediately for every
/// output; when it starts to buffer the scoress inside of itself instead,
// then the flush() method must be changed.
void
ScoreFileOutputter::write_output(
	output::OutputSpecification const & spec,
	JobResult const & result
)
{
	using job_results::PoseJobResult;
	debug_assert( dynamic_cast< PoseJobResult const * > ( &result ));
	auto const & pose_result( static_cast< PoseJobResult const &  > ( result ));
	core::pose::Pose const & pose( *pose_result.pose() );

	using SFOS = ScoreFileOutputSpecification;
	debug_assert( dynamic_cast< SFOS const * > ( &spec ) );
	auto const & sfspec( static_cast< SFOS const & > ( spec ) );
	core::io::raw_data::ScoreFileData sfd( sfspec.out_fname() );

	std::map < std::string, core::Real > score_map;
	std::map < std::string, std::string > string_map;
	core::io::raw_data::ScoreMap::add_energies_data_from_scored_pose( pose, score_map );
	core::io::raw_data::ScoreMap::add_arbitrary_score_data_from_pose( pose, score_map );
	core::io::raw_data::ScoreMap::add_arbitrary_string_data_from_pose( pose, string_map );

	// TO DO:
	// buffer the score file output and then write out the contents
	// in "flush," which is called only sporadically

	sfd.write_pose( pose, score_map, sfspec.pose_tag(), string_map );
}

void
ScoreFileOutputter::flush()
{
	// currently noop; when/if the score file outputter starts to buffer the
	// scoress inside of itself instead of writing immediately for every output,
	// then this method must be changed.
}

std::string
ScoreFileOutputter::class_key() const
{
	return keyname();
}

std::string
ScoreFileOutputter::keyname()
{
	return "ScoreFile";
}

void ScoreFileOutputter::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;

	//XMLSchemaRestriction string_w_one_dollarsign;
	//string_w_one_dollarsign.name( "string_w_one_dollarsign" );
	//string_w_one_dollarsign.base_type( xs_string );
	//// this reads "anything but a dollar sign or nothing followed by a dollar sign followed by anything but a dollar sign or nothing
	//string_w_one_dollarsign.add_restriction( xsr_pattern, "[^$]*$[^$]*" );
	//xsd.add_top_level_element( string_w_one_dollarsign );

	AttributeList output_scorefile_attributes;
	output_scorefile_attributes
		+ XMLSchemaAttribute::required_attribute( "filename", xs_string , "XRW TO DO" ) // this stops being required if we instead accept a filename-pattern
		+ XMLSchemaAttribute( "path", xs_string , "XRW TO DO" );
	XMLSchemaComplexTypeGenerator output_scorefile;
	output_scorefile.element_name( keyname() )
		.description( "XRW TO DO" )
		.complex_type_naming_func( & PoseOutputterFactory::complex_type_name_for_secondary_pose_outputter )
		.add_attributes( output_scorefile_attributes )
		.write_complex_type_to_schema( xsd );
}

void ScoreFileOutputter::list_options_read( utility::options::OptionKeyList & read_options )
{
	using namespace basic::options::OptionKeys;
	read_options
		+ run::no_scorefile
		+ out::file::scorefile
		+ out::path::score
		+ out::path::all
		+ out::path::path
		+ out::prefix
		+ out::suffix;
}


utility::file::FileName
ScoreFileOutputter::filename_for_job(
	utility::tag::TagCOP my_output_tag,
	utility::options::OptionCollection const & job_options,
	InnerLarvalJob const & /*job*/
) const
{
	using namespace basic::options::OptionKeys;

	bool scorefile_path_in_tag = false;
	utility::file::FileName scorefile_name;

	if ( my_output_tag ) {
		if ( my_output_tag->hasOption( "filename" ) ) {
			std::string option_val = my_output_tag->getOption< std::string >( "filename" );
			scorefile_name = option_val;
			if ( option_val.find( "/" ) != std::string::npos ) {
				scorefile_path_in_tag = true;
			}
		}
		if ( my_output_tag->hasOption( "path" ) ) {
			scorefile_name.path( my_output_tag->getOption< std::string >( "path" ) );
			scorefile_path_in_tag = true;
		}
	}

	if ( job_options[ run::no_scorefile ] && ! my_output_tag ) {
		scorefile_name = "(none)";
		return scorefile_name;
	}

	if ( scorefile_name.bare_name() == "" && job_options[ out::file::scorefile ].user() ) {
		std::string existing_path = scorefile_name.path();
		scorefile_name = job_options[ out::file::scorefile ];
		if ( existing_path != "" ) {
			// Concatentate the path from the  the Tag wth the path from the options system
			if ( scorefile_name.path() != "" ) {
				scorefile_name.path( existing_path + "/" + scorefile_name.path() );
			} else {
				scorefile_name.path( existing_path );
			}
		}
	} else if ( scorefile_name.bare_name() == "" ) {

		// Deviation from JD2's score file naming scheme:
		// Do not put .fasc at the end of a score file if the out::file::fullatom flag
		// is on the command line; instead, if the user wants to
		// say that their score file should should be named with the .fasc extension
		// then they can give a name for the file explicitly.


		std::ostringstream oss;

		//prefix, suffix
		// default base name "score.sc"
		oss << job_options[ out::prefix ]() << "score"
			<< job_options[ out::suffix ]();
		scorefile_name.base( oss.str() );
		scorefile_name.ext(".sc");
	}

	if ( scorefile_path_in_tag ) {
		// no op
	} else if ( ! scorefile_name.absolute() && job_options[ out::path::score ].user() ) {
		scorefile_name.path( job_options[ out::path::score ]().path() +
			( scorefile_name.path().empty() ? "" : ( "/" + scorefile_name.path()  ) ));
		scorefile_name.vol( job_options[ out::path::score ]().vol() );
	} else if ( ! scorefile_name.absolute() && job_options[ out::path::all ].user() ) {
		scorefile_name.path( job_options[ out::path::all ]().path() +
			( scorefile_name.path().empty() ? "" : ( "/" + scorefile_name.path()  ) ));
		scorefile_name.vol( job_options[ out::path::all ]().vol() );
	} else if ( ! scorefile_name.absolute() && job_options[ out::path::path ].user() ) {
		scorefile_name.path( job_options[ out::path::path ]().path() +
			( scorefile_name.path().empty() ? "" : ( "/" + scorefile_name.path()  ) ));
		scorefile_name.vol( job_options[ out::path::path ]().vol() );
	}

	return scorefile_name;
}


SecondaryPoseOutputterOP ScoreFileOutputterCreator::create_outputter() const {
	return utility::pointer::make_shared< ScoreFileOutputter >();
}

std::string ScoreFileOutputterCreator::keyname() const
{
	return ScoreFileOutputter::keyname();
}

void ScoreFileOutputterCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	ScoreFileOutputter::provide_xml_schema( xsd );
}

void ScoreFileOutputterCreator::list_options_read( utility::options::OptionKeyList & read_options ) const
{
	ScoreFileOutputter::list_options_read( read_options );
}

bool ScoreFileOutputterCreator::outputter_specified_by_command_line() const {
	return ScoreFileOutputter::outputter_specified_by_command_line();
}

} // namespace pose_outputters
} // namespace jd3
} // namespace protocols

#ifdef    SERIALIZATION

/// @brief Automatically generated serialization method
template< class Archive >
void
protocols::jd3::pose_outputters::ScoreFileOutputter::save( Archive & arc ) const {
	arc( cereal::base_class< protocols::jd3::pose_outputters::SecondaryPoseOutputter >( this ) );
	arc( CEREAL_NVP( scorefile_name_ ) ); // utility::file::FileName
	arc( CEREAL_NVP( scorefile_lines_ ) ); // utility::vector1<std::string>
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
protocols::jd3::pose_outputters::ScoreFileOutputter::load( Archive & arc ) {
	arc( cereal::base_class< protocols::jd3::pose_outputters::SecondaryPoseOutputter >( this ) );
	arc( scorefile_name_ ); // utility::file::FileName
	arc( scorefile_lines_ ); // utility::vector1<std::string>
}

SAVE_AND_LOAD_SERIALIZABLE( protocols::jd3::pose_outputters::ScoreFileOutputter );
CEREAL_REGISTER_TYPE( protocols::jd3::pose_outputters::ScoreFileOutputter )

CEREAL_REGISTER_DYNAMIC_INIT( protocols_jd3_pose_outputters_ScoreFileOutputter )
#endif // SERIALIZATION
