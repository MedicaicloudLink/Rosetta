// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file core/import_pose/options/RNA_DeNovoProtocolOptions.hh
/// @brief
/// @details
/// @author Rhiju Das, rhiju@stanford.edu


#ifndef INCLUDED_core_import_pose_options_RNA_DeNovoProtocolOptions_HH
#define INCLUDED_core_import_pose_options_RNA_DeNovoProtocolOptions_HH

#include <core/import_pose/options/RNA_FragmentMonteCarloOptions.hh>
#include <core/import_pose/options/RNA_DeNovoProtocolOptions.fwd.hh>
#include <utility/options/OptionCollection.fwd.hh>
#include <utility/options/keys/OptionKeyList.fwd.hh>

namespace core {
namespace import_pose {
namespace options {

class RNA_DeNovoProtocolOptions: public RNA_FragmentMonteCarloOptions {

public:

	//constructor
	RNA_DeNovoProtocolOptions();

public:

	RNA_DeNovoProtocolOptionsOP clone() const;

	/// @brief Initialize from the recursive "tag" structure.
	virtual
	void
	parse_my_tag( utility::tag::TagCOP ){}

	/// @brief The class name (its type) for a particular instance.
	/// This function allows for better error message delivery.
	virtual
	std::string
	type() const{ return "RNA_DeNovoProtocolOptions";}

	void set_nstruct( core::Size const setting ){ nstruct_ = setting; }
	core::Size nstruct() const { return nstruct_; }

	void set_lores_scorefxn( std::string const & setting ){ lores_scorefxn_ = setting; }
	std::string lores_scorefxn() const { return lores_scorefxn_; }

	void set_align_output( bool const setting ) { align_output_ = setting; }
	bool align_output() const { return align_output_; }

	void set_binary_rna_output( bool const setting ){ binary_rna_output_ = setting; }
	bool binary_rna_output() const { return binary_rna_output_; }

	void set_use_legacy_setup( bool const setting ){ use_legacy_setup_ = setting; }
	bool use_legacy_setup() const { return use_legacy_setup_; }

	void set_cst_gap( bool const setting ){ cst_gap_ = setting; }
	bool cst_gap() const { return cst_gap_; }

	void set_overwrite( bool const setting ){ overwrite_ = setting; }
	bool overwrite() const { return overwrite_; }

	void set_silent_file( std::string const & setting ){ silent_file_ = setting; }
	std::string silent_file() const { return silent_file_; }

	void set_dump_stems( bool const setting ){ dump_stems_ = setting; }
	bool dump_stems() const { return dump_stems_; }

	void
	initialize_from_command_line();
	void
	initialize_from_options( utility::options::OptionCollection const & opts );
	static void
	list_options_read( utility::options::OptionKeyList & opts );

private:

	Size nstruct_;
	std::string lores_scorefxn_;
	std::string silent_file_;

	bool align_output_;

	bool overwrite_;
	bool binary_rna_output_;
	bool use_legacy_setup_;
	bool cst_gap_;
	bool dump_stems_;
};

} //options
} //rna
} //protocols

#endif
