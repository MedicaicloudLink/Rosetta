// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/splice/SpliceSegment.hh
/// @author Sarel Fleishman

#ifndef INCLUDED_protocols_splice_SpliceSegment_hh
#define INCLUDED_protocols_splice_SpliceSegment_hh

#include <protocols/splice/SpliceSegment.fwd.hh>
#include <protocols/splice/Splice.fwd.hh>
#include <core/sequence/SequenceProfile.fwd.hh>
#include <utility/pointer/ReferenceCount.hh>
#include <utility/vector1.hh>
#include <map>
#include <core/types.hh>


namespace protocols {
namespace splice {

/* A SpliceSegment is one stretch of residues, which is associated with one or more sequence profiles. SpliceSegment is a class that helps manage these profiles

Two types of input files per splice segment are expected:
pdb-profile match: This file should be formatted as pdb, profile_name:
1y32 L1.1
1y33 L1.1
1ya3 L1.2
.
.
.

Each splice_segment is additionally associated with the sequence profiles mentioned in the pdb-profile match file. These should be
formatted as Rosetta-readable PSSM files*/

/// @brief utility class and functions for dealing with sequence profiles for varying segments in Splice
class SpliceSegment : public utility::pointer::ReferenceCount
{
public:
	SpliceSegment();
	~SpliceSegment() override;
	void read_profile( std::string const & file_name, std::string const & segment_name ); /// read pssm
	void read_many( std::string const & protein_family , std::string const & segmetn); /// read pssm
	void read_pdb_profile_file(std::string const & s, std::string const );
	void read_pdb_profile( std::string const & file_name ); /// read the pdb-profile match from a disk file
	core::sequence::SequenceProfileOP get_profile( std::string const & segment_name ); // return the requested sequence profile
	void add_pdb_profile_pair( std::string const & pdb, std::string const & profile_name ); /// add a sequence profile
	core::sequence::SequenceProfileOP pdb_profile( std::string const & pdb_name ); // return a sequence profile according to a pdb file name
	core::sequence::SequenceProfileOP sequence_profile( std::string const & profile_name ); // return a sequence profile according to a profile name
	std::string get_cluster_name_by_PDB(std::string pdbNAme){return pdb_to_profile_map_[pdbNAme];}
	void add_profile( std::string const & pdb_name,  core::sequence::SequenceProfileOP const prof);
	std::map< std::string, std::string> pdb_to_profile_map(){return pdb_to_profile_map_;}
private:
	std::map< std::string/*L3.10.1*/, core::sequence::SequenceProfileOP > sequence_profile_;
	std::map< std::string/*1AHW*/, std::string/*L1.1*/ > pdb_to_profile_map_;

};

/* pose comments
segment_L1 1y32
segment_FR2 1x9q
segment_L2 1jxw
*/

core::sequence::SequenceProfileOP concatenate_profiles( utility::vector1< core::sequence::SequenceProfileOP > const & profiles, utility::vector1< std::string > segment_names_ordered,std::string  H3_seq ); // utility function to generate a single concatenated profile from a vector of profiles
std::map< std::string, std::string> read_H3_seq( std::string const & Protein_family_path);
std::map< std::string, std::string> read_pdb_seq( std::string const & Protein_family_path);

} //splice
} //protocols

#endif //INCLUDED_protocols_splice_SpliceSegment_hh
