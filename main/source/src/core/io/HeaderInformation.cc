// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/io/HeaderInformation.cc
///
/// @brief  Information stored in the Title Section in the PDB format
/// @author Matthew O'Meara

// Unit headers
#include <core/io/HeaderInformation.hh>
#include <core/io/pdb/Field.hh>
#include <core/io/pdb/RecordCollection.hh>

// Platform headers
#include <core/types.hh>

// Basic headers
#include <basic/Tracer.hh>

// Utility headers
#include <utility/exit.hh>

// ObjexxFCL headers
#include <ObjexxFCL/format.hh>

// Boost headers
#include <boost/lexical_cast.hpp>

// C++ Headers
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>


#ifdef    SERIALIZATION
// Utility serialization headers
#include <utility/vector1.srlz.hh>
#include <utility/serialization/serialization.hh>

// Cereal headers
#include <cereal/types/list.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>
#endif // SERIALIZATION

namespace core {
namespace io {

using std::string;
using std::list;
using std::endl;
using std::pair;
using ObjexxFCL::rstrip_whitespace;   // by reference
using ObjexxFCL::strip_whitespace;    // by reference
using ObjexxFCL::rstripped_whitespace;// copy
using ObjexxFCL::stripped_whitespace; // copy

using core::io::pdb::Record;
using core::io::pdb::Field;
using core::io::pdb::RecordCollection;

static basic::Tracer TR( "core.io.pdb.HeaderInformation" );

HeaderInformation::HeaderInformation() : utility::pointer::ReferenceCount(),
	classification_( "" ),
	dep_year_( 0 ),
	dep_month_( 0 ),
	dep_day_( 0 ),
	idCode_( "" ),
	title_( "" ),
	keywords_(),
	keyword_in_progress_( false ),
	compounds_(),
	compound_in_progress_( "" ),
	experimental_techniques_(),
	experimental_technique_in_progress_( "" ),
	authors_(),
	author_in_progress_( false )
{}

HeaderInformation::HeaderInformation( HeaderInformation const & src ) : utility::pointer::ReferenceCount(),
	classification_( src.classification_ ),
	dep_year_( src.dep_year_ ),
	dep_month_( src.dep_month_ ),
	dep_day_( src.dep_day_ ),
	idCode_( src.idCode_ ),
	title_( "" ),
	keywords_( src.keywords_ ),
	keyword_in_progress_( src.keyword_in_progress_ ),
	compounds_( src.compounds_ ),
	compound_in_progress_( src.compound_in_progress_ ),
	experimental_techniques_( src.experimental_techniques_ ),
	experimental_technique_in_progress_( src.experimental_technique_in_progress_ ),
	authors_( src.authors_ ),
	author_in_progress_( src.author_in_progress_ )
{}

HeaderInformation::~HeaderInformation() = default;


void
HeaderInformation::store_record( Record & R )
{
	string const & type = R[ "type" ].value;
	if ( type == "HEADER" ) {
		store_classification( R[ "classification" ].value );
		store_deposition_date( R[ "depDate" ].value );
		store_idCode( R[ "idCode" ].value );
	} else if ( type == "TITLE " ) {
		store_title( R[ "title" ].value );
	} else if ( type == "COMPND" ) {
		store_compound( R[ "compound" ].value );
	} else if ( type == "KEYWDS" ) {
		store_keywords( R[ "keywords" ].value );
	} else if ( type == "EXPDTA" ) {
		store_experimental_techniques( R[ "technique" ].value );
	} else if ( type == "AUTHOR" ) {
		store_authors( R[ "authorList" ].value );
	} else {
		std::stringstream err_msg;
		err_msg
			<< "Attempting to add unrecognized record type '" << type << "' "
			<< "to header information.";
		utility_exit_with_message( err_msg.str() );
	}
}

void
HeaderInformation::finalize_parse()
{
	finalize_compound_records();
	finalize_keyword_records();
	finalize_experimental_technique_records();
	finalize_author_records();
}

bool
HeaderInformation::parse_in_progress() const
{
	return
		compound_in_progress() ||
		keyword_in_progress() ||
		experimental_technique_in_progress() ||
		author_in_progress();
}

void
HeaderInformation::fill_records( std::vector<Record> & VR ) const
{
	fill_header_record( VR );
	fill_title_records( VR );
	fill_compound_records( VR );
	fill_keyword_records( VR );
	fill_experimental_technique_records( VR );
	fill_author_records( VR );
}


// HEADER /////////////////////////////////////////////////////////////////////

void
HeaderInformation::store_classification(string const & classification){
	classification_ = classification;
	rstrip_whitespace(classification_);

	// TODO: and that the classification is on the list
	// http://www.wwpdb.org/documentation/wwpdb20070104appendices_c.pdf

}

string
HeaderInformation::classification() const {
	return classification_;
}

void
HeaderInformation::store_deposition_date( string const & depDate )
{
	if ( depDate == "xx-MMM-xx" ) {
		dep_day_ = 0;
		dep_month_ = 0;
		dep_year_ = 0;
		return;
	}

	dep_day_ = atoi(depDate.substr(0,2).c_str());
	if ( dep_day_ > 31 || dep_day_ < 1 ) {
		TR.Warning << "Deposition day not in range [1, 31]: " << dep_day_ << endl;
	}

	string const & mon(depDate.substr(3,3));
	if ( mon == "JAN" ) dep_month_ = 1;
	else if ( mon == "FEB" ) dep_month_ = 2;
	else if ( mon == "MAR" ) dep_month_ = 3;
	else if ( mon == "APR" ) dep_month_ = 4;
	else if ( mon == "MAY" ) dep_month_ = 5;
	else if ( mon == "JUN" ) dep_month_ = 6;
	else if ( mon == "JUL" ) dep_month_ = 7;
	else if ( mon == "AUG" ) dep_month_ = 8;
	else if ( mon == "SEP" ) dep_month_ = 9;
	else if ( mon == "OCT" ) dep_month_ = 10;
	else if ( mon == "NOV" ) dep_month_ = 11;
	else if ( mon == "DEC" ) dep_month_ = 12;
	else {
		TR.Warning << "Unrecognized month in HEADER deposition date " + depDate << mon << std::endl;
	}

	//dep_year_ = boost::lexical_cast<Size>(depDate.substr(7,4));
	//fd let's not crash with an invalid header line
	dep_year_ = atoi(depDate.substr(7,4).c_str());
}


/// @note  These parameters are listed in the opposite order that they are given in a .pdb file!
void
HeaderInformation::store_deposition_date(
	Size yy,
	Size mm,
	Size dd
) {

	dep_year_ = yy;
	if ( dep_year_ > 99 || dep_year_ < 1 ) {
		TR.Warning << "Deposition year not in range [01, 99]: " << dep_year_ << endl;
	}

	dep_month_ = mm;
	if ( dep_month_ > 12 || dep_day_ < 1 ) {
		TR.Warning << "Deposition month not in range [1, 12]: " << dep_month_ << endl;
	}

	dep_day_ = dd;
	if ( dep_day_ > 31 || dep_day_ < 1 ) {
		TR.Warning << "Deposition day not in range [1, 31]: " << dep_day_ << endl;
	}
}


string
HeaderInformation::deposition_date() const
{
	if ( ( dep_day_ == 0 ) && ( dep_month_ == 0 ) && ( dep_year_ == 0 ) ) {
		return "xx-MMM-xx";
	}

	std::stringstream dep_date;

	if ( dep_day_ > 31 || dep_day_ < 1 ) {
		utility_exit_with_message("deposition day is outside of range [1,31]: " +
			boost::lexical_cast<std::string>(dep_day_));
	}
	dep_date << (dep_day_ < 10 ? "0" : "") << dep_day_ << "-";
	switch(dep_month_){
	case 1 : dep_date  << "JAN"; break;
	case 2 : dep_date  << "FEB"; break;
	case 3 : dep_date  << "MAR"; break;
	case 4 : dep_date  << "APR"; break;
	case 5 : dep_date  << "MAY"; break;
	case 6 : dep_date  << "JUN"; break;
	case 7 : dep_date  << "JUL"; break;
	case 8 : dep_date  << "AUG"; break;
	case 9 : dep_date  << "SEP"; break;
	case 10 : dep_date << "OCT"; break;
	case 11 : dep_date << "NOV"; break;
	case 12 : dep_date << "DEC"; break;
	default :
		utility_exit_with_message("Unrecognized deposition month index " +
			boost::lexical_cast<std::string>(dep_month_));
	}
	if ( dep_year_ > 99 || dep_year_ < 1 ) {
		utility_exit_with_message("Deposition year is out side of range [01,99]: " +
			boost::lexical_cast<std::string>(dep_year_));
	}
	dep_date << "-" << (dep_year_ < 10 ? "0" : "") << dep_year_;
	return dep_date.str();
}

void
HeaderInformation::deposition_date(
	Size & yy,
	Size & mm,
	Size & dd
) const {
	yy = dep_year_;
	mm = dep_month_;
	dd = dep_day_;
}

std::string
HeaderInformation::idCode() const {
	return idCode_;
}

void
HeaderInformation::store_idCode(string const & idCode) {
	idCode_ = idCode;
}

void
HeaderInformation::fill_header_record( std::vector< Record > & VR ) const
{
	Record R = RecordCollection::record_from_record_type( pdb::HEADER );
	R["type"].value = "HEADER";
	R["classification"].value = classification();
	R["depDate"].value = deposition_date();
	R["idCode"].value = idCode();
	VR.push_back(R);
}


// TITLE //////////////////////////////////////////////////////////////////////

/// @details Append title, strip off white space on the left for the
/// first record and on the right for all records.
void
HeaderInformation::store_title(string const & title){
	if ( title.empty() ) {
		TR.Warning << "Attempting to store empty title record field." << endl;
		return;
	}

	if ( title_.empty() ) {
		title_ = title;
		strip_whitespace(title_);
	} else {

		title_.append(rstripped_whitespace(title));
	}
}

void
HeaderInformation::clear_title(){
	title_.clear();
}

std::string const &
HeaderInformation::title() const {
	return title_;
}

void
HeaderInformation::fill_title_records(
	std::vector< Record > & VR
) const {

	if ( !title_.empty() ) {
		Size line_no(1);
		fill_wrapped_records("TITLE ", "title", title_, line_no, VR);
	}
}


// KEYWDS /////////////////////////////////////////////////////////////////////

void
HeaderInformation::store_keywords(string const & keywords){
	if ( keywords.empty() ) {
		TR.Warning << "Attempting to add empty keywords string." << endl;
		return;
	}

	size_t i(keywords.find_first_not_of(' '));
	while ( i != std::string::npos ) {
		size_t j( keywords.find(',', i) );
		if ( keyword_in_progress_ ) {
			keywords_.back().append(
				" " + rstripped_whitespace(keywords.substr(i, j-i)));
			keyword_in_progress_ = false;
		} else {
			keywords_.push_back(rstripped_whitespace(keywords.substr(i, j-i)));
		}
		if ( j != std::string::npos ) {
			i = keywords.find_first_not_of(' ', j+1);
		} else {
			keyword_in_progress_ = true;
			return;
		}
	}
}

list< string > const &
HeaderInformation::keywords() const {
	return keywords_;
}

void
HeaderInformation::finalize_keyword_records() {
	keyword_in_progress_ = false;
}

bool
HeaderInformation::keyword_in_progress() const {
	return keyword_in_progress_;
}

void
HeaderInformation::clear_keywords() {
	keywords_.clear();
}

void
HeaderInformation::fill_keyword_records(
	std::vector< Record > & VR
) const {
	if ( keywords_.empty() ) return;

	string keywords;
	auto k = keywords_.begin(), ke = keywords_.end();
	for ( ; k!= ke; ++k ) {
		if ( !keywords.empty() ) keywords.append(", ");
		keywords.append(*k);
	}
	Size line_no(1);
	fill_wrapped_records("KEYWDS", "keywords", keywords, line_no, VR);
}

// COMPND /////////////////////////////////////////////////////////////////////

std::string
HeaderInformation::compound_token_to_string(CompoundToken token) {
	string token_str;
	switch ( token ) {
	case UNKNOWN_CMPD :    token_str = "UNKNOWN";         break;
	case MOL_ID :          token_str = "MOL_ID";          break;
	case MOLECULE :        token_str = "MOLECULE";        break;
	case BIOLOGICAL_UNIT : token_str = "BIOLOGICAL_UNIT"; break;
	case CHAIN :           token_str = "CHAIN";           break;
	case FRAGMENT :        token_str = "FRAGMENT";        break;
	case SYNONYM :         token_str = "SYNONYM";         break;
	case EC :              token_str = "EC";              break;
	case ENGINEERED :      token_str = "ENGINEERED";      break;
	case MUTATION :        token_str = "MUTATION";        break;
	case OTHER_DETAILS :   token_str = "OTHER_DETAILS";   break;
	default :
		TR.Error << "Unrecognized compound token '" << token << "'" << endl;
		utility_exit();
	}
	return token_str;
}

HeaderInformation::CompoundToken
HeaderInformation::string_to_compound_token(std::string const & token, bool warn_on_unrecognized) {
	if ( token == "MOL_ID" )               { return MOL_ID; }
	else if ( token == "MOLECULE" )        { return MOLECULE; }
	else if ( token == "BIOLOGICAL_UNIT" ) { return BIOLOGICAL_UNIT; }
	else if ( token == "CHAIN" )           { return CHAIN; }
	else if ( token == "FRAGMENT" )        { return FRAGMENT; }
	else if ( token == "SYNONYM" )         { return SYNONYM; }
	else if ( token == "EC" )              { return EC; }
	else if ( token == "ENGINEERED" )      { return ENGINEERED; }
	else if ( token == "MUTATION" )        { return MUTATION; }
	else if ( token == "OTHER_DETAILS" )   { return OTHER_DETAILS; }
	else {
		if ( warn_on_unrecognized ) {
			TR.Error << "Unrecognized compound token string '" << token << "'" << endl;
		}
		return UNKNOWN_CMPD;
	}
	return UNKNOWN_CMPD;
}

/// @details Assume each new compound token/value pair begins on a new
/// line but the value can be multiple lines. So, if a compound record
/// is encountered when "in progress" then append the results to the
/// value of the previous pair.
void
HeaderInformation::store_compound(std::string const & compound) {

	size_t t_end(compound.find(':'));
	if ( t_end == std::string::npos ) {
		// Continuation of current compound
		compound_in_progress_ += " ";
		compound_in_progress_ += stripped_whitespace( compound );
		return;
	}

	size_t t_begin(compound.find_first_not_of(' '));
	CompoundToken token(
		string_to_compound_token( stripped_whitespace(compound.substr(t_begin,t_end - t_begin)), /*warn_on_unrecognized*/false ));
	if ( token == UNKNOWN_CMPD ) {
		// Probably still part of the same compound record.
		compound_in_progress_ += " ";
		compound_in_progress_ += stripped_whitespace( compound );
		return;
	}

	// We're in a new compound record type - process what we have so far
	finalize_compound_records();

	// Now add the current value to the to-be-processed list.
	// (We can't actually process it untill we know that it's the end of the record.)
	compound_in_progress_ += stripped_whitespace( compound );
}

void
HeaderInformation::store_compound(
	HeaderInformation::CompoundToken token,
	string const & value
) {
	compounds_.push_back(make_pair(token, value));
}

utility::vector1< pair< HeaderInformation::CompoundToken, string > > const &
HeaderInformation::compounds() const{
	return compounds_;
}

void
HeaderInformation::finalize_compound_records() {
	if ( compound_in_progress_.empty() ) { return; }

	size_t t_begin(compound_in_progress_.find_first_not_of(' '));
	size_t t_end(compound_in_progress_.find(':', t_begin));
	if ( t_end == std::string::npos ) {
		TR.Error << "Malformed Compound record found: '" << compound_in_progress_ << "'" << std::endl;
		return; // Just ignore.
	}

	CompoundToken token(
		string_to_compound_token( stripped_whitespace(compound_in_progress_.substr(t_begin,t_end - t_begin)), /*warn_on_unrecognized*/true ));
	if ( token == UNKNOWN_CMPD ) { return; } // Already warned, just ignore.

	// We assume all the records are semicolon separated.
	size_t curpos( t_end + 1 );
	while ( curpos < compound_in_progress_.size() ) {
		size_t nextsemi(compound_in_progress_.find(';', curpos));
		std::string entry;
		if ( nextsemi == std::string::npos ) {
			entry = stripped_whitespace( compound_in_progress_.substr(curpos, std::string::npos) );
			curpos = std::string::npos;
		} else {
			entry = stripped_whitespace( compound_in_progress_.substr(curpos, nextsemi-curpos ) );
			curpos = nextsemi + 1;
		}
		compounds_.push_back( make_pair(token, entry) );
	}
	compound_in_progress_ = "";
}

bool
HeaderInformation::compound_in_progress() const {
	return !compound_in_progress_.empty();
}

void
HeaderInformation::clear_compounds() {
	compounds_.clear();
}

void
HeaderInformation::fill_compound_records(
	std::vector< Record > & VR
) const {

	Size line_no(1);

	for ( Size t=1, te = compounds_.size(); t <= te; ++t ) {

		std::stringstream comp_field;
		comp_field
			// defacto standard in PDB is to add a space after a continuation field
			<< (line_no == 1 ? "" : " ")
			<< compound_token_to_string(
			static_cast<CompoundToken>(compounds_[t].first))
			<< ": "
			<< compounds_[t].second
			// only add ';' to separate compound records
			<< (t < compounds_.size() ? ";" : "");

		fill_wrapped_records("COMPND", "compound", comp_field.str(), line_no, VR);
	}
}


// EXPDTA /////////////////////////////////////////////////////////////////////

void
HeaderInformation::store_experimental_techniques( string const & exp )
{
	if ( exp.empty() ) {
		TR.Error << "Attempting to add empty experimental technique string." << endl;
		utility_exit();
	}

	size_t t_len(0);
	SSize t_end(-1);

	while ( true ) {
		size_t t_begin = exp.find_first_not_of(' ', t_end+1);
		if ( t_begin == std::string::npos ) return;

		t_end = exp.find(';', t_begin);
		if ( t_end == SSize(std::string::npos) ) {
			experimental_technique_in_progress_ =
				rstripped_whitespace(exp.substr(t_begin, std::string::npos));
			return;
		} else if ( exp.length() - t_begin >= 3 && exp.compare(t_begin, 3, "NMR") == 0 ) {
			// The obsolete NMR tag took extra information that is ignored here
			t_len = 3;
		} else {
			t_len = t_end - t_begin;
		}
		if ( experimental_technique_in_progress_.empty() ) {
			experimental_techniques_.push_back(
				rcsb::string_to_experimental_technique( stripped_whitespace( exp.substr( t_begin, t_len ) ) ) );
		} else {
			experimental_technique_in_progress_.append(" ");
			experimental_technique_in_progress_.append( stripped_whitespace( exp.substr( t_begin, t_len ) ) );
			experimental_techniques_.push_back(
				rcsb::string_to_experimental_technique( experimental_technique_in_progress_ ) );
			experimental_technique_in_progress_.clear();
		}

	}
	return;
}

void
HeaderInformation::store_experimental_technique( rcsb::ExperimentalTechnique technique )
{
	experimental_techniques_.push_back(technique);
}

rcsb::ExperimentalTechniques const &
HeaderInformation::experimental_techniques() const
{
	return experimental_techniques_;
}

void
HeaderInformation::finalize_experimental_technique_records()
{
	if ( experimental_technique_in_progress() ) {
		experimental_techniques_.push_back(
			rcsb::string_to_experimental_technique( experimental_technique_in_progress_ ) );
		experimental_technique_in_progress_.clear();
	}
}

bool
HeaderInformation::experimental_technique_in_progress() const
{
	return !experimental_technique_in_progress_.empty();
}

void
HeaderInformation::clear_experimental_techniques()
{
	experimental_techniques_.clear();
}

bool
HeaderInformation::is_experimental_technique ( rcsb::ExperimentalTechnique technique ) const
{
	auto
		t = find(experimental_techniques_.begin(), experimental_techniques_.end(),
		technique);

	return t != experimental_techniques_.end();
}

void
HeaderInformation::fill_experimental_technique_records( std::vector< Record > & VR ) const
{
	if ( parse_in_progress() ) {
		TR.Error
			<< "Attempting to fill experimental technique records the "
			<< "HeaderInformation is in the middle of parsing. If you think the "
			<< "parsing is complete and you have reached this recording in error, "
			<< "please call finalize_parse()";
		utility_exit();
	}

	if ( experimental_techniques_.empty() ) return;
	string techniques;
	auto
		k = experimental_techniques_.begin(),
		ke= experimental_techniques_.end();
	for ( ; k != ke; ++k ) {
		if ( !techniques.empty() ) techniques.append("; ");
		techniques.append( rcsb::experimental_technique_to_string( *k ) );
	}
	Size line_no(1);
	fill_wrapped_records("EXPDTA", "technique", techniques, line_no, VR);
}


// AUTHOR /////////////////////////////////////////////////////////////////////

void
HeaderInformation::store_authors( string const & authors )
{
	if ( authors.empty() ) {
		TR.Trace << "Attempting to add empty authors string." << endl;
		return;
	}

	size_t i( authors.find_first_not_of( ' ' ) );
	while ( i != std::string::npos ) {
		size_t j( authors.find( ',', i ) );
		if ( author_in_progress_ ) {
			authors_.back().append( " " + rstripped_whitespace( authors.substr( i, j - i ) ) );
			author_in_progress_ = false;
		} else {
			authors_.push_back( rstripped_whitespace( authors.substr( i, j - i ) ) );
		}
		if ( j != std::string::npos ) {
			i = authors.find_first_not_of( ' ', j + 1 );
		} else {
			author_in_progress_ = true;
			return;
		}
	}
}

/// @brief Add an author to this Title section.
void
HeaderInformation::add_author( std::string const & author )
{
	authors_.push_back( author );
}

void
HeaderInformation::fill_author_records( std::vector< Record > & VR ) const
{
	if ( authors_.empty() ) return;

	string authors;
	for ( auto k = authors_.begin(), ke = authors_.end(); k != ke; ++k ) {
		if ( ! authors.empty() ) {
			authors.append( "," );
		}
		authors.append( *k );
	}
	core::uint line_no( 1 );
	fill_wrapped_records( "AUTHOR", "authorList", authors, line_no, VR );
}


// Helper Functions ///////////////////////////////////////////////////////////

void
HeaderInformation::fill_wrapped_records(
	string const & record_type,
	string const & field_name,
	string const & contents,
	Size & line_no,
	std::vector< Record > & VR ) const
{
	// Assume contents string is stripped of white space
	size_t l_begin(0), l_len(0), l_end(0);
	size_t field_width(60);
	while ( l_begin != contents.length() ) {
		Record R = RecordCollection::record_from_record_type( record_type );
		R["type"].value = record_type;
		set_line_continuation(R, line_no);

		//Will the remainder of the contents fit on this line?
		if ( contents.length() - l_begin <= field_width ) {
			l_len = contents.length() - l_begin;
		} else {
			// Walk back from end where the field would truncate to locate
			// a reasonable place to word wrapping.

			l_end = l_begin + field_width;
			// Note: Since the rest of the contents don't fit in the field,
			// l_end < contents.length()
			while ( true ) {
				if ( l_end == l_begin ) {
					// We have walked all the way to l_begin. The next word is
					// so big it cannot fit in the field
					TR.Error
						<< "The for record type '" << record_type << "', "
						<< "field '" << field_name << "' "
						<< "contains a word that has more than 59 characters and "
						<< "is too long to fit on one line." << endl;
					TR.Error << field_name << ": " << contents << endl;
					utility_exit();
				}
				if ( contents[l_end] == ' ' || contents[l_end - 1] == '-' ) {
					break;
				} else {
					--l_end;
				}
			}
			l_len = l_end - l_begin;
		}
		R[field_name].value = contents.substr(l_begin, l_len);
		VR.push_back(R);
		++line_no;

		// Note this puts l_begin at a ' ' which is how wrapped records
		// are written in the PDB
		l_begin = l_begin + l_len;
	}
}


void
HeaderInformation::set_line_continuation(
	Record & R,
	Size const line_no
) const {
	std::string & con_field = R["continuation"].value;
	if ( line_no == 0 ) {
		TR.Error << "Attempting to write a line continuation record for line 0, please begin the line continuation count at 1." << endl;
		utility_exit();
	}
	if ( line_no == 1 ) {
		con_field = "  ";
		return;
	} else if ( line_no > 99 ) {
		TR.Error << "Attempting to write record that takes more than 99 lines, which overflows the continuation field in the." << endl;
		utility_exit();
	} else {
		con_field.resize(2);
		sprintf(&con_field[0], "%2d", static_cast<int>(line_no));
	}
}

} // namespace io
} // namespace core


#ifdef    SERIALIZATION

/// @brief Automatically generated serialization method
template< class Archive >
void
core::io::HeaderInformation::save( Archive & arc ) const {
	arc( CEREAL_NVP( classification_ ) ); // std::string
	arc( CEREAL_NVP( dep_year_ ) ); // Size
	arc( CEREAL_NVP( dep_month_ ) ); // Size
	arc( CEREAL_NVP( dep_day_ ) ); // Size
	arc( CEREAL_NVP( idCode_ ) ); // std::string
	arc( CEREAL_NVP( title_ ) ); // std::string
	arc( CEREAL_NVP( keywords_ ) ); // Keywords
	arc( CEREAL_NVP( keyword_in_progress_ ) ); // _Bool
	arc( CEREAL_NVP( compounds_ ) ); // Compounds
	arc( CEREAL_NVP( compound_in_progress_ ) ); // _Bool
	arc( CEREAL_NVP( experimental_techniques_ ) ); // ExperimentalTechniques
	arc( CEREAL_NVP( experimental_technique_in_progress_ ) ); // std::string
	arc( CEREAL_NVP( authors_ ) );  // Authors
	arc( CEREAL_NVP( author_in_progress_ ) );  // _Bool
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::io::HeaderInformation::load( Archive & arc ) {
	arc( classification_ ); // std::string
	arc( dep_year_ ); // Size
	arc( dep_month_ ); // Size
	arc( dep_day_ ); // Size
	arc( idCode_ ); // std::string
	arc( title_ ); // std::string
	arc( keywords_ ); // Keywords
	arc( keyword_in_progress_ ); // _Bool
	arc( compounds_ ); // Compounds
	arc( compound_in_progress_ ); // _Bool
	arc( experimental_techniques_ ); // ExperimentalTechniques
	arc( experimental_technique_in_progress_ ); // std::string
	arc( authors_ );  // Authors
	arc( author_in_progress_ );  // _Bool
}

SAVE_AND_LOAD_SERIALIZABLE( core::io::HeaderInformation );
CEREAL_REGISTER_TYPE( core::io::HeaderInformation )

CEREAL_REGISTER_DYNAMIC_INIT( core_io_pdb_HeaderInformation )
#endif // SERIALIZATION
