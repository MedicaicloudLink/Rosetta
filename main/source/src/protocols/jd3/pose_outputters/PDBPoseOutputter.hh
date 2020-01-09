// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/jd3/PDBPoseOutputter.hh
/// @brief  Definition of the %PDBPoseOutputter class
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)


#ifndef INCLUDED_protocols_jd3_pose_outputters_PDBPoseOutputter_HH
#define INCLUDED_protocols_jd3_pose_outputters_PDBPoseOutputter_HH

//unit headers
#include <protocols/jd3/pose_outputters/PDBPoseOutputter.fwd.hh>

//package headers
#include <protocols/jd3/pose_outputters/PoseOutputter.hh>
#include <protocols/jd3/LarvalJob.fwd.hh>

//project headers
#include <core/pose/Pose.fwd.hh>

#ifdef    SERIALIZATION
// Cereal headers
#include <cereal/types/polymorphic.fwd.hpp>
#endif // SERIALIZATION

namespace protocols {
namespace jd3 {
namespace pose_outputters {

/// @brief The %PDBPoseOutputter
class PDBPoseOutputter : public PoseOutputter
{
public:

	PDBPoseOutputter();
	virtual ~PDBPoseOutputter();

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

	bool job_has_already_completed( LarvalJob const & job, utility::options::OptionCollection const & options ) const override;

	/// @brief Create the PoseOutputSpecification for a particular job
	virtual
	PoseOutputSpecificationOP
	create_output_specification(
		LarvalJob const & job,
		JobOutputIndex const & output_index,
		utility::options::OptionCollection const & options,
		utility::tag::TagCOP tag // possibly null-pointing tag pointer
	) override;

	/// @brief Write a pose out to permanent storage (whatever that may be).
	virtual
	void write_output(
		output::OutputSpecification const & specification,
		JobResult const & result
	) override;

	/// @brief Currently a no-op since the "write output pose" method sends all
	/// outputs to disk.
	void flush() override;

	/// @brief Return the stiring used by the PDBPoseOutputterCreator for this class
	virtual
	std::string
	class_key() const override;

	static
	std::string
	keyname();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

	static
	void
	list_options_read( utility::options::OptionKeyList & read_options );

	/// @brief Guess on the name of the output PDB using just the LarvalJob -- i.e. in the absence of
	/// the JobOutputIndex
	/// @details Public for testing
	virtual
	std::string
	output_name(
		LarvalJob const & job,
		utility::options::OptionCollection const & options,
		utility::tag::TagCOP tag // possibly null-pointing tag pointer
	) const;

	///@details Public for testing
	virtual
	std::string
	output_name(
		LarvalJob const & job,
		JobOutputIndex const & output_index,
		utility::options::OptionCollection const & options,
		utility::tag::TagCOP tag // possibly null-pointing tag pointer
	) const;

private:



	// NOTE THERE IS NO PRIVATE DATA AND THERE SHOULD NOT BE:
	// The PDBPoseOutputter should not accumulate state unless the behavior of
	// its outputter_for_job method changes. Currently, this method returns the empty string
	// signifying to the StandardJobQueen that it has no state, and therefore, it
	// is safe to use the same instance of the class in multiple threads. If that pledge
	// should have to change (because this class is given state in the form of private data)
	// then the outputter_for_job_method must be updated to respect the job-distributor
	// assigned suffix.

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
CEREAL_FORCE_DYNAMIC_INIT( protocols_jd3_pose_outputters_PDBPoseOutputter )
#endif // SERIALIZATION


#endif //INCLUDED_protocols_jd3_PDBPoseOutputter_HH
