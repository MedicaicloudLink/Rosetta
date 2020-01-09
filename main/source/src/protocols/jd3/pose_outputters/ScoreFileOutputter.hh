// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/jd3/pose_outputters/SecondaryPoseOutputter.hh
/// @brief  Definition of the %SecondaryPoseOutputter class
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)


#ifndef INCLUDED_protocols_jd3_pose_outputters_ScoreFileOutputter_HH
#define INCLUDED_protocols_jd3_pose_outputters_ScoreFileOutputter_HH

//unit headers
#include <protocols/jd3/pose_outputters/ScoreFileOutputter.fwd.hh>

//package headers
#include <protocols/jd3/pose_outputters/SecondaryPoseOutputter.hh>
#include <protocols/jd3/LarvalJob.fwd.hh>
#include <protocols/jd3/InnerLarvalJob.fwd.hh>

//project headers
#include <core/pose/Pose.fwd.hh>

// utility headers
#include <utility/file/FileName.hh>
#include <utility/options/OptionCollection.fwd.hh>
#include <utility/options/keys/OptionKey.fwd.hh>

#ifdef    SERIALIZATION
// Cereal headers
#include <cereal/types/polymorphic.fwd.hpp>
#endif // SERIALIZATION

namespace protocols {
namespace jd3 {
namespace pose_outputters {

/// @brief The %ScoreFileOutputter
class ScoreFileOutputter : public SecondaryPoseOutputter
{
public:

	ScoreFileOutputter();
	virtual ~ScoreFileOutputter();

	/// @brief The %ScoreFileOutputter returns the name of the scorefile that it sends its outputs to
	/// for this particular job so that the JobQueen can send multiple outputs to the same outputter
	/// before invoking flush()
	std::string
	outputter_for_job(
		utility::tag::TagCOP output_tag,
		utility::options::OptionCollection const & job_options,
		InnerLarvalJob const & job
	) const override;

	std::string
	outputter_for_job(
		PoseOutputSpecification const & spec
	) const override;

	static
	bool
	outputter_specified_by_command_line();

	/// @brief Using the LarvalJob's job_tag (determined by the (primary) JobOutputter),
	/// write extra data about this output Pose to its destination.
	PoseOutputSpecificationOP
	create_output_specification(
		LarvalJob const & job,
		JobOutputIndex const & output_index,
		utility::options::OptionCollection const & options,
		utility::tag::TagCOP tag // possibly null-pointing tag pointer
	) override;

	/// @brief Write out the scores for a particular Pose (held in the JobResult) to a score file
	void
	write_output(
		output::OutputSpecification const & specification,
		JobResult const & result
	) override;

	void flush() override;

	/// @brief Return the stiring used by the ScoreFileOutputterCreator for this class
	std::string
	class_key() const override;

	static
	std::string
	keyname();

	static void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );
	static void list_options_read( utility::options::OptionKeyList & read_options );

	utility::file::FileName
	filename_for_job(
		utility::tag::TagCOP output_tag,
		utility::options::OptionCollection const & job_options,
		InnerLarvalJob const & /*job*/
	) const;


private:
	utility::file::FileName scorefile_name_;
	utility::vector1< std::string > scorefile_lines_;

#ifdef    SERIALIZATION
public:
	template< class Archive > void save( Archive & arc ) const;
	template< class Archive > void load( Archive & arc );
#endif // SERIALIZATION

};

} // namespace pose_outputters
} // namespace jd3
} // namespace protocols

#ifdef    SERIALIZATION
CEREAL_FORCE_DYNAMIC_INIT( protocols_jd3_pose_outputters_ScoreFileOutputter )
#endif // SERIALIZATION


#endif //INCLUDED_protocols_jd3_pose_outputters_ScoreFileOutputter_HH
