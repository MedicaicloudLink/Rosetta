// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/moves/StructureRestrictor.cc
///
/// @brief  lookup relevant chains for a structure in a table.
/// @author Matthew O'Meara

/// This should probably be a pilot app, but the way Rosetta Scripts
/// is set up, it can't be in the pilot apps

#include <boost/algorithm/string.hpp>
#include <core/conformation/Conformation.hh>
#include <core/pose/datacache/CacheableDataType.hh>
#include <core/pose/Pose.hh>
#include <core/pose/PDBInfo.hh>
#include <basic/datacache/BasicDataCache.hh>
#include <basic/datacache/CacheableString.hh>
#include <basic/Tracer.hh>
#include <protocols/moves/StructureRestrictor.hh>
#include <protocols/jd2/util.hh>


#include <utility/io/izstream.hh>
#include <utility/tag/Tag.hh>


#include <string>

#include <utility/vector0.hh>
#include <utility/vector1.hh>


using namespace std;
using namespace core;
using namespace boost;
using namespace pose;

static basic::Tracer TR_SR( "protocols.moves.StructureRestrictor" );

namespace protocols {
namespace moves {

StructureRestrictor::StructureRestrictor():
	Mover("StructureRestrictor"),
	//  relevant_chains_fname_( basic::options::option[ basic::options::OptionKeys::StructureRestrictor::relevant_chains].value() ),
	initialized_( false )
{}

StructureRestrictor::StructureRestrictor( string const & name):
	Mover(name),
	//  relevant_chains_fname_( basic::options::option[ basic::options::OptionKeys::StructureRestrictor::relevant_chains].value() ),
	initialized_( false )
{}

StructureRestrictor::StructureRestrictor( StructureRestrictor const & src):
	//utility::pointer::ReferenceCount(),
	Mover(src)
{
	chain_map_ = std::map< std::string, std::string>( src.chain_map_ );
	relevant_chains_fname_ = src.relevant_chains_fname_;
	initialized_ = src.initialized_;
}

StructureRestrictor::~StructureRestrictor()= default;

MoverOP StructureRestrictor::fresh_instance() const { return utility::pointer::make_shared< StructureRestrictor >(); }

MoverOP StructureRestrictor::clone() const
{
	return utility::pointer::make_shared< StructureRestrictor >( *this );
}


// So this this can be called from RosettaScripts
void
StructureRestrictor::parse_my_tag(
	TagCOP const tag,
	basic::datacache::DataMap & /*data*/,
	Filters_map const & /*filters*/,
	protocols::moves::Movers_map const & /*movers*/,
	Pose const & /*pose*/ )
{
	if ( tag->hasOption("relevant_chains") ) {
		relevant_chains_fname_ = tag->getOption<string>("relevant_chains");
	}
}

void
StructureRestrictor::setup_relevant_chains(
	string const & relevant_chains_fname,
	map< string, string > & chain_map
){
	if ( relevant_chains_fname.length() == 0 ) {
		TR_SR.Error << " Cannot open relevant_chains_file '"<< relevant_chains_fname << "'" << endl;
		return;
	}
	utility::io::izstream relevant_chains_file( relevant_chains_fname );
	if ( !relevant_chains_file ) {
		TR_SR.Error << " Cannot open relevant_chains_file '"<< relevant_chains_fname << "'" << endl;
		return;
	} else {
		TR_SR << "Reading in relevant chains from file '"<< relevant_chains_fname << "'" << endl;
	}
	string line;
	//string chains = "*";
	vector<string> tokens;
	getline(relevant_chains_file,line); // header
	while ( getline( relevant_chains_file, line ) ) {
		string tab("\t");
		split(tokens, line, is_any_of(tab) );
		chain_map.insert(std::pair<string, string>(tokens[0], tokens[1]));
	}
	initialized_ = true;
}

// this is a hack because poses do not have canonical names!
string
StructureRestrictor::pose_name(Pose const & pose){
	//silent files and pdbs set the name of the pose differently
	string name = "No_Name_Found";
	if ( pose.pdb_info() ) {
		name = pose.pdb_info()->name();
	} else if ( pose.data().has( datacache::CacheableDataType::JOBDIST_OUTPUT_TAG ) ) {
		name = static_cast< basic::datacache::CacheableString const & >
			( pose.data().get( datacache::CacheableDataType::JOBDIST_OUTPUT_TAG ) ).str();
	} else {
		name = protocols::jd2::current_input_tag();
	}
	return name;
}


void
StructureRestrictor::apply( Pose& pose ){
	if ( !initialized_ ) {
		setup_relevant_chains(relevant_chains_fname_, chain_map_);
	}


	string const & name = pose_name(pose);
	auto i(chain_map_.find(name));
	if ( i == chain_map_.end() ) {
		TR_SR << "No chain information found for structure " << name << "." << endl;
		return;
	}
	string chains = i->second;
	TR_SR << "Restricting structure " << name << " to chains " << chains << "." << endl;
	Size res_begin_delete = 1;
	for ( Size j=1; j <= pose.size(); ++j ) {
		//INVARIANT: if we're in a stretch to delete then res_begin_delete
		//indicates the first residue in this stretch to delete
		if ( chains.find( pose.pdb_info()->chain(j), 0) != string::npos ) {
			//keep this position
			if ( res_begin_delete != j ) {
				pose.conformation().delete_residue_range_slow(res_begin_delete, j-1);
			}
			res_begin_delete = j+1;
		}
	}
	// don't for get the last section to delete
	if ( res_begin_delete <= pose.size() ) {
		pose.conformation().delete_residue_range_slow(res_begin_delete, pose.size());
	}
}


} // moves namespace
} // protocols namespace
