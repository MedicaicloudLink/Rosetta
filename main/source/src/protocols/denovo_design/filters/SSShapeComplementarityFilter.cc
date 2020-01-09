// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file src/protocols/denovo_design/filters/SSShapeComplementarityFilter.cc
/// @brief Tom's Denovo design protocol
/// @details
/// @author Tom Linsky (tlinsky@gmail.com)
/// @author Vikram K. Mulligan (vmullig@uw.edu -- added threshold option)

// Unit headers
#include <protocols/denovo_design/filters/SSShapeComplementarityFilter.hh>
#include <protocols/denovo_design/filters/SSShapeComplementarityFilterCreator.hh>

// Protocol Headers
#include <protocols/fldsgn/topology/HelixPairing.hh>
#include <protocols/fldsgn/topology/HSSTriplet.hh>
#include <protocols/fldsgn/topology/SS_Info2.hh>
#include <protocols/parser/BluePrint.hh>

// Core Headers
#include <core/conformation/Residue.hh>
#include <core/pose/Pose.hh>
#include <core/scoring/dssp/Dssp.hh>
#include <core/scoring/sc/ShapeComplementarityCalculator.hh>
#include <core/select/residue_selector/ResidueSelector.hh>
#include <core/select/residue_selector/ResidueVector.hh>
#include <core/select/residue_selector/util.hh>

// Basic Headers
#include <basic/Tracer.hh>

// Utility Headers
#include <utility/tag/Tag.hh>
// XSD XRW Includes
#include <utility/tag/XMLSchemaGeneration.hh>
#include <protocols/filters/filter_schemas.hh>

// ObjexxFCL Headers

//C++ Headers


static basic::Tracer TR( "protocols.denovo_design.SSShapeComplementarityFilter" );

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace protocols {
namespace denovo_design {
namespace filters {

// XRW TEMP std::string
// XRW TEMP SSShapeComplementarityFilterCreator::keyname() const
// XRW TEMP {
// XRW TEMP  return SSShapeComplementarityFilter::class_name();
// XRW TEMP }

// XRW TEMP protocols::filters::FilterOP
// XRW TEMP SSShapeComplementarityFilterCreator::create_filter() const {
// XRW TEMP  return utility::pointer::make_shared< SSShapeComplementarityFilter >();
// XRW TEMP }

// XRW TEMP std::string
// XRW TEMP SSShapeComplementarityFilter::class_name()
// XRW TEMP {
// XRW TEMP  return "SSShapeComplementarity";
// XRW TEMP }

///  ---------------------------------------------------------------------------------
///  SSShapeComplementarityFilter main code:
///  ---------------------------------------------------------------------------------
SSShapeComplementarityFilter::SSShapeComplementarityFilter() :
	Filter( "SSShapeComplementarityFilter" ),
	verbose_( false ),
	calc_loops_( true ),
	calc_helices_( true ),
	rejection_thresh_(0),
	secstruct_( "" ),
	selector_(),
	scc_( utility::pointer::make_shared< core::scoring::sc::ShapeComplementarityCalculator >() )
{
}

/// @brief destructor - this class has no dynamic allocation, so
//// nothing needs to be cleaned. C++ will take care of that for us.
SSShapeComplementarityFilter::~SSShapeComplementarityFilter() = default;

/// Return a copy of ourselves
protocols::filters::FilterOP
SSShapeComplementarityFilter::clone() const
{
	return utility::pointer::make_shared< SSShapeComplementarityFilter >(*this);
}

protocols::filters::FilterOP
SSShapeComplementarityFilter::fresh_instance() const
{
	return utility::pointer::make_shared< SSShapeComplementarityFilter >();
}

void
SSShapeComplementarityFilter::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & data,
	protocols::filters::Filters_map const &,
	protocols::moves::Movers_map const &,
	core::pose::Pose const & )
{
	if ( tag->hasOption( "blueprint" ) && tag->hasOption( "secstruct" ) ) {
		std::stringstream msg;
		msg << "SSShapeComplementarityFilter: You cannot specify both blueprint and secstruct options" << std::endl;
		utility_exit_with_message( msg.str() );
	}

	std::string const bp_filename( tag->getOption< std::string >( "blueprint", "" ) );
	if ( bp_filename != "" ) {
		protocols::parser::BluePrint bp( bp_filename );
		secstruct_ = bp.secstruct();
	}

	secstruct_ = tag->getOption< std::string >( "secstruct", secstruct_ );

	core::select::residue_selector::ResidueSelectorCOP selector =
		core::select::residue_selector::parse_residue_selector( tag, data );
	if ( selector ) set_residue_selector( *selector );

	verbose_ = tag->getOption< bool >( "verbose", verbose_ );
	calc_loops_ = tag->getOption< bool >( "loops", calc_loops_ );
	calc_helices_ = tag->getOption< bool >( "helices", calc_helices_ );
	set_rejection_thresh( tag->getOption< core::Real >("min_sc", 0.0) );
}

std::string
SSShapeComplementarityFilter::get_name() const
{
	return "SSShapeComplementarity";
}

void
SSShapeComplementarityFilter::report( std::ostream & out, core::pose::Pose const & pose ) const
{
	out << "SS Shape complementarity is:" << compute( pose ) << std::endl;
}

core::Real
SSShapeComplementarityFilter::report_sm( core::pose::Pose const & pose ) const
{
	return compute( pose );
}

core::Real
SSShapeComplementarityFilter::compute( core::pose::Pose const & pose ) const
{
	// if scc_ is not set, something horrible has happened
	runtime_assert( scc_ != nullptr );
	if ( !scc_->Init() ) {
		TR.Error << "Failed to initialize ShapeComplementarityCalculator!" << std::endl;
		return -1;
	}

	if ( !selector_ ) {
		return compute_from_ss_info( pose );
	} else {
		return compute_from_selector( pose );
	}
}

void
SSShapeComplementarityFilter::set_residue_selector( core::select::residue_selector::ResidueSelector const & selector )
{
	selector_ = selector.clone();
}

/// @brief computes sc score for selectoed residues vs the rest of the pose
///        Residue selector MUST be set to call this
core::Real
SSShapeComplementarityFilter::compute_from_selector( core::pose::Pose const & pose ) const
{
	debug_assert( selector_ );
	using core::select::residue_selector::ResidueSubset;
	using core::select::residue_selector::ResidueVector;
	ResidueSubset const subset = selector_->apply( pose );

	// setup calculator
	scc_->Reset();
	core::Size resid = 1;
	ResidueVector residues;
	for ( auto s=subset.begin(); s!=subset.end(); ++s, ++resid ) {
		if ( *s ) {
			scc_->AddResidue( 0, pose.residue( resid ) );
			residues.push_back( resid );
		} else {
			scc_->AddResidue( 1, pose.residue( resid ) );
		}
	}

	TR << "Running SC calculator for residues " << residues << std::endl;

	if ( residues.size() == 0 ) {
		TR << "No residues selected, returning 0.0" << std::endl;
		return 0.0;
	}

	core::scoring::sc::RESULTS const & r( get_sc_and_area() );
	if ( r.valid == 1 ) {
		core::Real const sum = ( r.sc * r.area );
		core::Real const total_area = r.area;
		core::Real score;
		if ( total_area == 0.0 ) score = 0.0;
		else score = sum / total_area;
		TR << "SUM=" << sum << "; area=" << r.area << "; sc=" << r.sc << "; num_res="
			<< residues.size() << "; score=" << score << std::endl;
		return score;
	} else {
		std::stringstream msg;
		msg << "SSShapeComplementarityFilter::compute_from_selector(): Error running SC Calculator! Residues :";
		for ( ResidueVector::const_iterator r=residues.begin(); r!=residues.end(); ++r ) {
			if ( r != residues.begin() ) msg << ", ";
			msg << pose.residue( *r ).name() << *r;
		}
		msg << std::endl;
		utility_exit_with_message( msg.str() );
		return 0.0;
	}
}

core::Real
SSShapeComplementarityFilter::compute_from_ss_info( core::pose::Pose const & pose ) const
{
	protocols::fldsgn::topology::SS_Info2_OP ss_info;
	if ( secstruct_.empty() ) {
		core::scoring::dssp::Dssp dssp( pose );
		ss_info = utility::pointer::make_shared< protocols::fldsgn::topology::SS_Info2 >( pose, dssp.get_dssp_secstruct() );
	} else {
		ss_info = utility::pointer::make_shared< protocols::fldsgn::topology::SS_Info2 >( pose, secstruct_ );
	}

	// we will average out the shape complementarity from HSS triplets and Helix-Helix pairings
	core::Real sum( 0.0 );
	core::Real total_area( 0.0 );
	core::Size count( 0 );
	if ( calc_helices_ ) {
		for ( core::Size i=1; i<=ss_info->helices().size(); ++i ) {
			// only do this if the helix size >= 5 residues
			if ( ( ss_info->helix( i )->end() - ss_info->helix( i )->begin() ) >= 4 ) {
				setup_sc( pose, ss_info->helix( i ) );
				core::scoring::sc::RESULTS const & r( get_sc_and_area() );
				core::Size num_res( ss_info->helix( i )->end() - ss_info->helix( i )->begin() + 1 );
				if ( r.valid == 1 ) {
					sum += (r.sc * r.area);
					total_area += r.area;
					++count;
					TR << "SUM=" << sum << "; area=" << r.area << "; sc=" << r.sc << "; num_res=" << num_res << std::endl;
				}
				// reset SCC for reuse
				scc_->Reset();
			}
		}
	}
	if ( calc_loops_ ) {
		for ( core::Size i=1; i<=ss_info->loops().size(); ++i ) {
			setup_sc( pose, ss_info->loop( i ) );
			core::scoring::sc::RESULTS const & r( get_sc_and_area() );
			core::Size num_res( ss_info->loop( i )->end() - ss_info->loop( i )->begin() + 1);
			if ( r.valid == 1 ) {
				sum += (r.sc * r.area);
				total_area += r.area;
				++count;
				TR << "SUM=" << sum << "; area=" << r.area << "; sc=" << r.sc << "; num_res=" << num_res << std::endl;
			}
			scc_->Reset();
		}
	}
	/*std::string helix_string;
	if ( blueprint_ ) {
	helix_string = blueprint_->helix_pairings();
	} else {
	helix_string = detect_helix_pairings( pose, ss_info );
	TR << "Helix_string=" << helix_string << std::endl;
	}
	protocols::fldsgn::topology::HelixPairings helix_pairs( protocols::fldsgn::topology::HelixPairingSet( helix_string ).helix_pairings() );
	for ( core::Size i=1; i<=helix_pairs.size(); ++i ) {
	setup_sc_hh( pose, *ss_info, helix_pairs[i] );

	core::scoring::sc::RESULTS const & r( get_sc_and_area() );
	if ( r.valid == 1 ) {
	sum += r.sc * r.area;
	total_area += r.area;
	++count;
	TR << "SUM=" << sum << "; area=" << r.area << "; sc=" << r.sc << std::endl;
	}
	// reset SCC for reuse
	scc_->Reset();
	}

	std::string hss_string;
	if ( blueprint_ ) {
	hss_string = blueprint_->hss_triplets();
	} else {
	hss_string = detect_hss_triplets( pose, ss_info );
	TR << "HSS string = " << hss_string << std::endl;
	}
	protocols::fldsgn::topology::HSSTriplets hss_triplets( protocols::fldsgn::topology::HSSTripletSet( hss_string ).hss_triplets() );
	for ( core::Size i=1; i<=hss_triplets.size(); ++i ) {
	setup_sc_hss( pose, *ss_info, hss_triplets[i] );

	core::scoring::sc::RESULTS const & r( get_sc_and_area() );

	// this check makes sure there wasn't an error in the filter calculation.
	// if there was an error, the result is simply ignored.
	if ( r.valid == 1 ) {
	sum += r.sc * r.area;
	total_area += r.area;
	++count;
	TR << "SUM=" << sum << "; area=" << r.area << "; sc=" << r.sc << std::endl;
	}
	// reset SCC for reuse
	scc_->Reset();
	}*/

	if ( count == 0 || total_area <= 0.0 ) {
		return 0;
	}
	//TR << "Returning " << sum / total_area << std::endl;
	//return sum / total_area;
	TR << "Returning " << sum / total_area << std::endl;
	return sum / total_area;
}

/// @brief Does the SSShapeComplementarity Filtering
bool
SSShapeComplementarityFilter::apply( core::pose::Pose const & pose ) const
{
	core::Real const returnval( report_sm( pose ) );

	if ( returnval < rejection_thresh() ) {
		if ( TR.visible() ) TR << "SSShapecomplementarity result is less than rejection threshold (" << returnval << " < " << rejection_thresh() << ").  Filter failed." << std::endl;
		return false;
	}

	if ( TR.visible() ) TR << "SSShapecomplementarityFilter passed." << std::endl;
	return true;
}


/// @brief sets up the underlying filter to work based on a helix
void
SSShapeComplementarityFilter::setup_sc( core::pose::Pose const & pose,
	protocols::fldsgn::topology::SS_BaseCOP const ss ) const
{
	utility::vector1< core::Size > set1, set2;
	runtime_assert( ss->begin() <= ss->end() );

	// set1 is the helix
	for ( core::Size i=1; i<=pose.size(); ++i ) {
		// exclude ligands and other weird things
		if ( ! pose.residue( i ).is_protein() ) {
			continue;
		}

		if ( i >= ss->begin() && i <= ss->end() ) {
			set1.push_back( i );
		} else {
			if ( i < ss->begin() - 1 || i > ss->end() + 1 ) {
				set2.push_back( i );
			}
		}
	}

	TR.Debug << "Set residues1 to ";
	for ( core::Size i=1; i<=set1.size(); ++i ) {
		TR.Debug << set1[i] << " ";
		scc_->AddResidue( 0, pose.residue( set1[i] ) );
	}
	TR.Debug << std::endl << "Set residues2 to ";
	for ( core::Size i=1; i<=set2.size(); ++i ) {
		TR.Debug << set2[i] << " ";
		scc_->AddResidue( 1, pose.residue( set2[i] ) );
	}
	TR.Debug << std::endl;
}

/// @brief sets up the underlying shapecomplementarity filter to work based on secondary structure elements
void
SSShapeComplementarityFilter::setup_sc_hss( core::pose::Pose const & pose,
	protocols::fldsgn::topology::SS_Info2 const & ss_info,
	protocols::fldsgn::topology::HSSTripletCOP hss_triplet ) const
{
	utility::vector1< core::Size > set1, set2;
	// set 1 is the helix
	protocols::fldsgn::topology::HelixCOP const helix( ss_info.helix( hss_triplet->helix() ) );
	for ( core::Size i=helix->begin(); i<=helix->end(); ++i ) {
		set1.push_back( i );
	}

	protocols::fldsgn::topology::StrandCOP const strand( ss_info.strand( hss_triplet->strand1() ) );
	for ( core::Size i=strand->begin(); i<=strand->end(); ++i ) {
		set2.push_back( i );
	}
	protocols::fldsgn::topology::StrandCOP const strand2( ss_info.strand( hss_triplet->strand2() ) );
	for ( core::Size i=strand2->begin(); i<=strand2->end(); ++i ) {
		set2.push_back( i );
	}

	TR.Debug << "Set residues1 to ";
	for ( core::Size i=1; i<=set1.size(); ++i ) {
		TR.Debug << set1[i] << " ";
		scc_->AddResidue( 0, pose.residue( set1[i] ) );
	}
	TR.Debug << std::endl << "Set residues2 to ";
	for ( core::Size i=1; i<=set2.size(); ++i ) {
		TR.Debug << set2[i] << " ";
		scc_->AddResidue( 1, pose.residue( set2[i] ) );
	}
	TR.Debug << std::endl;
}

/// @brief sets up the underlying shapecomplementarity filter to work based on secondary structure elements
void
SSShapeComplementarityFilter::setup_sc_hh( core::pose::Pose const & pose,
	protocols::fldsgn::topology::SS_Info2 const & ss_info,
	protocols::fldsgn::topology::HelixPairingCOP helix_pair ) const
{
	utility::vector1< core::Size > set1, set2;
	// set 1 is helix1, set2 is helix2
	protocols::fldsgn::topology::HelixCOP const helix( ss_info.helix( helix_pair->h1() ) );
	for ( core::Size i=helix->begin(); i<=helix->end(); ++i ) {
		set1.push_back( i );
	}

	protocols::fldsgn::topology::HelixCOP const helix2( ss_info.helix( helix_pair->h2() ) );
	for ( core::Size i=helix2->begin(); i<=helix2->end(); ++i ) {
		set2.push_back( i );
	}

	TR.Debug << "Set residues1 to ";
	for ( core::Size i=1; i<=set1.size(); ++i ) {
		TR.Debug << set1[i] << " ";
		scc_->AddResidue( 0, pose.residue( set1[i] ) );
	}
	TR.Debug << std::endl << "Set residues2 to ";
	for ( core::Size i=1; i<=set2.size(); ++i ) {
		TR.Debug << set2[i] << " ";
		scc_->AddResidue( 1, pose.residue( set2[i] ) );
	}
	TR.Debug << std::endl;
}

/// @brief Runs the SC calculator to obtain an SC score and an interaction area. Returns a result in the format core::scoring::sc::RESULTS.  Assumes the SC calculator has been initialized and has the correct residues added.
core::scoring::sc::RESULTS const &
SSShapeComplementarityFilter::get_sc_and_area() const
{
	if ( ! scc_->Calc() ) {
		TR.Error << "Error in SC calculation!" << std::endl;
		return scc_->GetResults();
	}

	core::scoring::sc::RESULTS const & r = scc_->GetResults();

	if ( verbose_ ) {
		TR << "==================================================" << std::endl;
		TR << std::endl;
		for ( int i = 0; i <= 2; i++ ) {
			if ( i < 2 ) {
				TR << "Molecule " << (i+1) << ":" << std::endl;
			} else {
				TR << "Total/Average for both molecules:" << std::endl;
			}

			TR << "          Total Atoms: " << r.surface[i].nAtoms << std::endl;
			TR << "         Buried Atoms: " << r.surface[i].nBuriedAtoms << std::endl;
			TR << "        Blocked Atoms: " << r.surface[i].nBlockedAtoms << std::endl;
			TR << "           Total Dots: " << r.surface[i].nAllDots << std::endl;
			TR << " Trimmed Surface Dots: " << r.surface[i].nTrimmedDots << std::endl;
			TR << "         Trimmed Area: " << r.surface[i].trimmedArea << " (avg) " << std::endl;
			TR << std::endl;
		}
		TR << std::endl;

		for ( int i = 0; i <= 2; i++ ) {
			if ( i < 2 ) {
				TR << "Molecule " << (i+1) << "->" << ((i+1)%2+1) << ": " << std::endl;
			} else {
				TR << "Average for both molecules:" << std::endl;
			}
			TR << "      Mean Separation: " << r.surface[i].d_mean << std::endl;
			TR << "    Median Separation: " << r.surface[i].d_median << std::endl;
			TR << "    Mean Shape Compl.: " << r.surface[i].s_mean << std::endl;
			TR << "  Median Shape Compl.: " << r.surface[i].s_median << std::endl;
			TR << std::endl;
		}
	}  //verbose output
	return r;
}

std::string SSShapeComplementarityFilter::name() const {
	return class_name();
}

std::string SSShapeComplementarityFilter::class_name() {
	return "SSShapeComplementarity";
}

void SSShapeComplementarityFilter::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;
	AttributeList attlist;
	attlist + XMLSchemaAttribute(
		"blueprint", xs_string,
		"If specified, the given blueprint file will be used to assign secondary structure. If not specified, DSSP will be used."
		"blueprint and secstruct tags are mutually exclusive");

	attlist + XMLSchemaAttribute(
		"secstruct", xs_string,
		"blueprint and secstruct tags are mutually exclusive");

	core::select::residue_selector::attributes_for_parse_residue_selector(attlist);

	attlist + XMLSchemaAttribute::attribute_w_default(
		"verbose", xsct_rosetta_bool,
		"If set, verbose output will be generated by the ShapeComplementarity calculator",
		"false");

	attlist + XMLSchemaAttribute::attribute_w_default(
		"loops", xsct_rosetta_bool,
		"If set, loops will be included in the calculations. Otherwise, they will be skipped.",
		"true");

	attlist + XMLSchemaAttribute::attribute_w_default(
		"helices", xsct_rosetta_bool,
		"If set, helices will be included in the calculations. Otherwise, they will be skipped.",
		"true");

	attlist + XMLSchemaAttribute::attribute_w_default(
		"min_sc", xsct_real,
		"The filter fails if the calculated overall shape complementarity is below this value. Set to 0.0 (always passing) by default.",
		"0.0");

	protocols::filters::xsd_type_definition_w_attributes(
		xsd, class_name(),
		"Uses the same code underlying the ShapeComplementarity filter to "
		"calculates the Lawrence and Coleman shape complementarity of secondary "
		"structure elements of a protein with one another. "
		"Essentially, secondary structure elements (e.g. loops, helices) are "
		"cut from the protein and their shape complementarity is calculated "
		"against the rest of the protein. The protein is then returned to its "
		"state when the filter was called and the next secondary structure "
		"element is cut and computed. The value calculated is equal to "
		"sum( sc_i*a_i )/a_tot, where sc_i is the shape complementarity value "
		"of secondary structure element i with the rest of the protein, a_i "
		"is the area of interaction of secondary structure element i with "
		"the rest of the protein, and a_tot is the total intraprotein area of "
		"all secondary structure elements. Sheets are currently not supported. "
		"The secondary structure elements can optionally be specified using "
		"a blueprint file. For antibody-antigen interfaces, a value of 0.65-0.67 "
		"is typical, while complementarity among intra-protein secondary "
		"structure elements is typically higher, on the order of 0.7-0.8. "
		"This filter returns false if the overall shape complementarity "
		"value is below min_sc (set by the user), true otherwise.",
		attlist );
}

std::string SSShapeComplementarityFilterCreator::keyname() const {
	return SSShapeComplementarityFilter::class_name();
}

protocols::filters::FilterOP
SSShapeComplementarityFilterCreator::create_filter() const {
	return utility::pointer::make_shared< SSShapeComplementarityFilter >();
}

void SSShapeComplementarityFilterCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	SSShapeComplementarityFilter::provide_xml_schema( xsd );
}


} // namespace filters
} // namespace denovo_design
} // namespace protocols


