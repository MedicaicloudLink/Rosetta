// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/jd3/DeNovoSilentFilePoseOutputter.hh
/// @brief  Definition of the %DeNovoSilentFilePoseOutputter class
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com), Andy Watkins (amw579@stanford.edu)


#ifndef INCLUDED_protocols_jd3_pose_outputters_DeNovoSilentFilePoseOutputter_HH
#define INCLUDED_protocols_jd3_pose_outputters_DeNovoSilentFilePoseOutputter_HH

//unit headers
#include <protocols/jd3/pose_outputters/DeNovoSilentFilePoseOutputter.fwd.hh>

//package headers
#include <protocols/jd3/pose_outputters/PoseOutputter.hh>
#include <protocols/jd3/LarvalJob.fwd.hh>

// Project headers
#include <core/types.hh>
#include <core/io/silent/SilentStruct.fwd.hh>
#include <core/io/silent/SilentFileOptions.fwd.hh>
#include <core/pose/Pose.fwd.hh>

#include <utility/file/FileName.fwd.hh>

//project headers

#ifdef    SERIALIZATION
// Cereal headers
#include <cereal/types/polymorphic.fwd.hpp>
#endif // SERIALIZATION

namespace protocols {
namespace jd3 {
namespace pose_outputters {

/// @brief The %DeNovoSilentFilePoseOutputter
class DeNovoSilentFilePoseOutputter : public PoseOutputter
{
public:

	DeNovoSilentFilePoseOutputter();
	virtual ~DeNovoSilentFilePoseOutputter();

	static
	bool
	outputter_specified_by_command_line();

	void
	determine_job_tag(
		utility::tag::TagCOP output_tag,
		utility::options::OptionCollection const & job_options,
		InnerLarvalJob & job
	) const override;

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

	PoseOutputSpecificationOP
	create_output_specification(
		LarvalJob const & job,
		JobOutputIndex const & output_index,
		utility::options::OptionCollection const & job_options,
		utility::tag::TagCOP tag // possibly null-pointing tag pointer
	) override;

	bool job_has_already_completed( LarvalJob const & job, utility::options::OptionCollection const & options ) const override;

	void write_output( output::OutputSpecification const & spec, JobResult const & result ) override;

	/// @brief Write out all cached silent files -- may happen after an exception is caught by
	/// the JobDistributor and the process is about to be spun down.
	void flush() override;

	/// @brief Return the string used by the DeNovoSilentFilePoseOutputterCreator to identify the
	/// class in an XML file (which is the string returned by the static keyname method)
	std::string
	class_key() const override;

	//std::string
	//output_silent_name( LarvalJob const & job ) const;

	static
	std::string
	keyname();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

	static
	void
	list_options_read( utility::options::OptionKeyList & read_options );

private:
	void
	initialize_sf_options(
		utility::options::OptionCollection const & job_options,
		utility::tag::TagCOP tag // possibly null-pointing tag pointer
	);

private:
	std::string fname_out_;
	//std::string lores_fname_out_;
	core::Size buffer_limit_;
	core::io::silent::SilentFileOptionsOP opts_;
	utility::vector1< core::io::silent::SilentStructOP > buffered_structs_;
	utility::vector1< core::io::silent::SilentStructOP > buffered_lores_structs_;
	bool dump_ = false;

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
CEREAL_FORCE_DYNAMIC_INIT( protocols_jd3_pose_outputters_DeNovoSilentFilePoseOutputter )
#endif // SERIALIZATION


#endif //INCLUDED_protocols_jd3_DeNovoSilentFilePoseOutputter_HH
