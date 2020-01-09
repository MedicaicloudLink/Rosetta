// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file MinMover.cc
/// @brief
/// @author ashworth

// Unit headers
#include <protocols/symmetry/SetupForSymmetryMoverCreator.hh>

// Package headers
#include <protocols/symmetry/SetupForSymmetryMover.hh>
#include <protocols/symmetry/SymDockingInitialPerturbation.hh>
#include <protocols/cryst/refinable_lattice.hh>
#include <protocols/moves/mover_schemas.hh>

// Core headers
#include <core/conformation/symmetry/SymmData.hh>
#include <core/pose/Pose.hh>
#include <core/pose/PDBInfo.hh>
#include <core/pose/symmetry/util.hh>
#include <core/scoring/ScoreFunction.hh>

//#include <basic/resource_manager/ResourceManager.hh>
//#include <basic/resource_manager/util.hh>

#include <basic/Tracer.hh>
#include <basic/datacache/BasicDataCache.hh>
#include <basic/datacache/DataMap.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/symmetry.OptionKeys.gen.hh>
#include <basic/options/keys/cryst.OptionKeys.gen.hh>

// Utility Headers
#include <utility>
#include <utility/tag/Tag.hh>
#include <utility/excn/Exceptions.hh>
#include <utility/options/keys/OptionKeyList.hh>
#include <utility/vector0.hh>
#include <utility/vector1.hh>
#include <utility/tag/XMLSchemaGeneration.hh>

// C++ Headers
#include <string>

namespace protocols {
namespace symmetry {

static basic::Tracer TR( "protocols.simple_moves.symmetry.SetupForSymmetryMover" );

// creators
// XRW TEMP std::string
// XRW TEMP SetupForSymmetryMoverCreator::keyname() const {
// XRW TEMP  return SetupForSymmetryMover::mover_name();
// XRW TEMP }

// XRW TEMP protocols::moves::MoverOP
// XRW TEMP SetupForSymmetryMoverCreator::create_mover() const {
// XRW TEMP  return utility::pointer::make_shared< SetupForSymmetryMover >();
// XRW TEMP }

// XRW TEMP std::string
// XRW TEMP SetupForSymmetryMover::mover_name() {
// XRW TEMP  return "SetupForSymmetry";
// XRW TEMP }

// XRW TEMP std::string
// XRW TEMP ExtractAsymmetricUnitMoverCreator::keyname() const {
// XRW TEMP  return ExtractAsymmetricUnitMover::mover_name();
// XRW TEMP }

// XRW TEMP protocols::moves::MoverOP
// XRW TEMP ExtractAsymmetricUnitMoverCreator::create_mover() const {
// XRW TEMP  return utility::pointer::make_shared< ExtractAsymmetricUnitMover >();
// XRW TEMP }

// XRW TEMP std::string
// XRW TEMP ExtractAsymmetricUnitMover::mover_name() {
// XRW TEMP  return "ExtractAsymmetricUnit";
// XRW TEMP }

////////////////////

// XRW TEMP std::string
// XRW TEMP ExtractAsymmetricPoseMoverCreator::keyname() const {
// XRW TEMP  return ExtractAsymmetricPoseMover::mover_name();
// XRW TEMP }

// XRW TEMP protocols::moves::MoverOP
// XRW TEMP ExtractAsymmetricPoseMoverCreator::create_mover() const {
// XRW TEMP  return utility::pointer::make_shared< ExtractAsymmetricPoseMover >();
// XRW TEMP }

// XRW TEMP std::string
// XRW TEMP ExtractAsymmetricPoseMover::mover_name() {
// XRW TEMP  return "ExtractAsymmetricPose";
// XRW TEMP }

////////////////////


SetupForSymmetryMover::SetupForSymmetryMover() :
	SetupForSymmetryMover( basic::options::option )
{}

SetupForSymmetryMover::SetupForSymmetryMover( utility::options::OptionCollection const & options ) :
	protocols::moves::Mover("SetupForSymmetryMover"),
	slide_(false),
	cryst1_(false),
	preserve_datacache_(false),
	symmdef_(),
	refinable_lattice_was_set_( false ),
	refinable_lattice_( false ),
	keep_pdb_info_labels_( false ),
	set_global_symmetry_at_parsetime_( true )
{
	using namespace basic::options;
	if ( options[ OptionKeys::symmetry::symmetry_definition ].user() ) {
		symdef_fname_from_options_system_ = options[ OptionKeys::symmetry::symmetry_definition ];
	}
	read_refinable_lattice( options );
}

SetupForSymmetryMover::SetupForSymmetryMover( core::conformation::symmetry::SymmDataOP symmdata ) :
	SetupForSymmetryMover( symmdata, basic::options::option )
{}

SetupForSymmetryMover::SetupForSymmetryMover(
	core::conformation::symmetry::SymmDataOP symmdata,
	utility::options::OptionCollection const & options
) :
	protocols::moves::Mover("SetupForSymmetryMover"),
	slide_(false),
	cryst1_(false),
	preserve_datacache_(false),
	symmdef_(std::move( symmdata )),
	refinable_lattice_was_set_( false ),
	refinable_lattice_( false ),
	keep_pdb_info_labels_( false ),
	set_global_symmetry_at_parsetime_( true )
{
	read_refinable_lattice( options );
}

SetupForSymmetryMover::SetupForSymmetryMover( std::string const & symmdef_file) :
	SetupForSymmetryMover( symmdef_file, basic::options::option )
{}

SetupForSymmetryMover::SetupForSymmetryMover(
	std::string const & symmdef_file,
	utility::options::OptionCollection const & options
) :
	protocols::moves::Mover("SetupForSymmetryMover"),
	slide_(false),
	cryst1_(false),
	preserve_datacache_(false),
	symmdef_(),
	refinable_lattice_was_set_( false ),
	refinable_lattice_( false ),
	keep_pdb_info_labels_( false ),
	set_global_symmetry_at_parsetime_( true )
{
	process_symmdef_file(symmdef_file);
	read_refinable_lattice( options );
}

SetupForSymmetryMover::~SetupForSymmetryMover()= default;

//fpd centralize the logic for processing a symmdef file tag
void
SetupForSymmetryMover::process_symmdef_file(std::string tag) {
	//fd special logic
	if ( tag == "CRYST1" ) {
		cryst1_ = true;
		return;
	}

	symmdef_.reset( new core::conformation::symmetry::SymmData() );
	symmdef_->read_symmetry_data_from_file(tag);
}

/// @brief Sets whether or not the input asymmetric pose's datacache should be copied into
///        the new symmetric pose.
/// @param[in] preserve_cache If true, input pose's datacache is copied into new symmetric pose
///                           If false, input pose's datacache is cleared (default = false)
void
SetupForSymmetryMover::set_preserve_datacache( bool const preserve_cache )
{
	preserve_datacache_ = preserve_cache;
}

void
SetupForSymmetryMover::set_keep_pdb_info_labels( bool const keep_pdb_info_labels )
{
	keep_pdb_info_labels_ = keep_pdb_info_labels;
}

/// @brief   constructs a symmetric pose with a symmetric conformation and energies object.
/// @details Calls core::pose::make_symmetric_pose().  If preserve_datacache is set, this
///          also copies the datacache into the new symmetric pose.
/// @param[in,out] pose Input asymmetric pose, output symmetric pose
void
SetupForSymmetryMover::make_symmetric_pose( core::pose::Pose & pose ) const
{
	using basic::datacache::BasicDataCache;

	BasicDataCache cached;
	if ( preserve_datacache_ ) cached = pose.data();

	core::pose::symmetry::make_symmetric_pose( pose, *symmdef_, keep_pdb_info_labels_ );

	if ( preserve_datacache_ ) pose.data() = cached;
}

void
SetupForSymmetryMover::read_refinable_lattice(
	utility::options::OptionCollection const & options
)
{
	using namespace basic::options;
	// default is not refinable _from this context_
	if ( options[ OptionKeys::cryst::refinable_lattice ].user() ) {
		refinable_lattice_was_set_ = true;
		refinable_lattice_ = options[ OptionKeys::cryst::refinable_lattice ];
	}
}

void
SetupForSymmetryMover::apply( core::pose::Pose & pose )
{
	using namespace basic::options;

	// If we are alredy symmetric do nothing
	if ( core::pose::symmetry::is_symmetric( pose ) ) return;

	if ( !symmdef_ && !cryst1_ ) {
		if ( ! symdef_fname_from_options_system_.empty() ) {
			process_symmdef_file( symdef_fname_from_options_system_ );
		} else {
			throw CREATE_EXCEPTION(utility::excn::BadInput,
				"The -symmetry:symmetry_definition command line option "
				"was not specified.");
		}
	}

	if ( cryst1_ ) {
		protocols::cryst::MakeLatticeMover make_lattice;
		if ( refinable_lattice_was_set_ ) {
			make_lattice.set_refinable_lattice( refinable_lattice_ );
		} else {
			// default is not refinable _from this context_
			make_lattice.set_refinable_lattice( false );
		}
		make_lattice.apply(pose);
	} else {
		make_symmetric_pose( pose );
	}

	debug_assert( core::pose::symmetry::is_symmetric( pose ) );

	//fpd  explicitly update disulfide lr energy container
	if ( pose.is_fullatom() ) {
		core::scoring::ScoreFunction disulf_score;
		disulf_score.set_weight( core::scoring::dslf_ss_dst, 1.0 );
		disulf_score.setup_for_scoring( pose );
	}

	// (Optionally) set rigid-body dofs from file
	//    SymDockingInitialPerturbation's behavior is controlled by flags and does nothing by default
	protocols::moves::MoverOP symdock( new protocols::symmetry::SymDockingInitialPerturbation(slide_) );
	symdock->apply( pose );
}

void SetupForSymmetryMover::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & datamap,
	filters::Filters_map const & /*filters*/,
	moves::Movers_map const & /*movers*/,
	core::pose::Pose const & /*pose*/
) {

	using namespace basic::options;

	preserve_datacache_ = tag->getOption< bool >( "preserve_datacache", preserve_datacache_ );
	keep_pdb_info_labels_ = tag->getOption< bool >( "keep_pdb_info_labels", false );
	if ( tag->hasOption("definition") && tag->hasOption("symmetry_resource") ) {
		throw CREATE_EXCEPTION(utility::excn::BadInput,
			"SetupForSymmetry takes either a 'definition' OR "
			"a 'symmetry_resource' tag but not both.");
	}

	//Tag option to set the symdef option globally during parse_my_tag.  Necessary for compatibility with many protocols.
	if ( tag->hasOption("set_global_symmetry_at_parsetime") ) {
		set_global_symmetry_at_parsetime_ = tag->getOption<bool>( "set_global_symmetry_at_parsetime" );
	}

	if ( tag->hasOption("definition") ) {
		process_symmdef_file(tag->getOption<std::string>("definition"));

		// TL: Setting global option flags like this is dangerous!
		//     Setting global option flags to invalid values is even more dangerous!
		// I don't like it, but for compatibility I'm going to set it to the correct filename
		//option[OptionKeys::symmetry::symmetry_definition].value( "dummy" );
		//BJY: Only set the symmetry_definition option if set_global_symmetry_at_parsetime is true (the default)
		//This is necessary for compatibility, but can cause problems if symmetry changes during the protocol.
		if ( set_global_symmetry_at_parsetime_ ) {
			option[OptionKeys::symmetry::symmetry_definition].value( tag->getOption< std::string >( "definition" ) );
			TR.Warning << "Symmetry is being set globally, which can cause conflicts with certain protocols, but is necessary for backwards compatibility." << std::endl;
			TR.Warning << "If you are experiencing unexpected behaviour related to symmetry, try setting set_global_symmetry_at_parsetime=0 in <SetupForSymmetry>." << std::endl;
		} else {
			TR.Warning << "Symmetry is NOT being set globally.  While this is safer behaviour, it may cause problems with some protocols." << std::endl;
			TR.Warning << "If you are receiving symmetry errors, try setting set_global_symmetry_at_parsetime=1 in <SetupForSymmetry>." << std::endl;
		}
	} else if ( tag->hasOption("symmetry_resource") ) {

		std::string symdef_resource = tag->getOption<std::string>("symmetry_resource");
		if ( datamap.has_resource( symdef_resource ) ) {
			symmdef_ = datamap.get_resource< core::conformation::symmetry::SymmData >( symdef_resource )->clone();
		} else {
			std::ostringstream oss;
			oss << "Error trying to retrieve SymmData resource from the datamap with name '" <<
				symdef_resource << "' while parsing the SetupForSymmetryMover named '" <<
				tag->getOption< std::string >( "name", "(unnamed") << "'. Has this Resource" <<
				" been declared in the ResourceDefinition file and also been imported " <<
				" in the RESOURCES block at the top of this XML file?\n";
			throw CREATE_EXCEPTION( utility::excn::Exception, oss.str() );
		}

		// TL: Setting global option flags like this is dangerous!
		//     Setting global option flags to invalid values is even more dangerous!
		// To illustrate, there is at least one mover that checks this flag and uses
		// this 'dummy' value.  I think the following line should be removed altogether,
		// but for now I will leave it and refrain from using the 'symmetry_resource'
		// XML option
		// APL: Just to clarify -- Symmetry is shamefully a "mode" in Rosetta in that
		// in order for the proper type of ScoreFunction to be created and used, the
		// option[symmetry_definition] flag has to be "on". This is so so sad. I fret
		// over this regularly.
		//BJY: Only set the symmetry_definition option if set_global_symmetry_at_parsetime is true (the default)
		//This is necessary for compatibility, but can cause problems if symmetry changes during the protocol.
		if ( set_global_symmetry_at_parsetime_ ) {
			option[OptionKeys::symmetry::symmetry_definition].value( "dummy" );
			TR.Warning << "Symmetry is being set globally, which can cause conflicts with certain protocols, but is necessary for backwards compatibility." << std::endl;
			TR.Warning << "If you are experiencing unexpected behaviour related to symmetry, try setting set_global_symmetry_at_parsetime=0 in <SetupForSymmetry>." << std::endl;
		} else {
			TR.Warning << "Symmetry is NOT being set globally.  While this is safer behaviour, it may cause problems with some protocols." << std::endl;
			TR.Warning << "If you are receiving symmetry errors, try setting set_global_symmetry_at_parsetime=1 in <SetupForSymmetry>." << std::endl;
		}
	} else if ( ! symdef_fname_from_options_system_.empty() ) {
		process_symmdef_file( symdef_fname_from_options_system_ );
	} else {
		throw CREATE_EXCEPTION(utility::excn::BadInput,
			"To use SetupForSymmetryMover with rosetta scripts please supply either a 'definition' tag, a 'symmetry_resource' tag or specify -symmetry:symmetry_definition the command line.");
	}
}

std::string SetupForSymmetryMover::get_name() const {
	return mover_name();
}

std::string SetupForSymmetryMover::mover_name() {
	return "SetupForSymmetry";
}

void SetupForSymmetryMover::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;
	AttributeList attlist;
	// XRW TO DO: check these attributes
	attlist + XMLSchemaAttribute( "definition", xs_string , "The path and filename for a symmetry definition file. This is optional because you can also specify -symmetry:symmetry_definition {pathto/filename_symmetry_definition_file} on the command line." )
		+ XMLSchemaAttribute::attribute_w_default( "preserve_datacache", xsct_rosetta_bool , "If true, the datacache from the input asymmetric pose will be copied into the new symmetric pose. If false, the pose datacache will be cleared. Default is false for historical reasons." , "0" )
		+ XMLSchemaAttribute::attribute_w_default( "keep_pdb_info_labels", xsct_rosetta_bool , "If true, keep PDB Info labels from input pose." , "0" )
		+ XMLSchemaAttribute( "symmetry_resource", xs_string, "The name for symmetry definition object created by the ResourceManager; this Resource must be declared in the RESOURCES block at the top of the protocol XML file" )
		+ XMLSchemaAttribute::attribute_w_default( "set_global_symmetry_at_parsetime", xsct_rosetta_bool, "Should the -symmetry_definition flag be set globally by this mover at parse time.  The default (true) maintains compatibility with all protocols, but can cause problems if symmetry is set or changed partway through a protocol.", "true" );
	// At XSD XRW, we choose to purposefully not document "resource_description." -UN

	protocols::moves::xsd_type_definition_w_attributes( xsd, mover_name(), "Given a symmetry definition file that describes configuration and scoring of a symmetric system, this mover 'symmetrizes' an asymmetric pose.", attlist );
}

void
SetupForSymmetryMover::options_read_in_ctor( utility::options::OptionKeyList & opts )
{
	// *sigh* Symmetry does not lend itself to multithreaded (JD3) applications because it
	// is so dependent on global data.
	using namespace basic::options;
	opts
		+ OptionKeys::symmetry::symmetry_definition
		+ OptionKeys::cryst::refinable_lattice;
}

void
SetupForSymmetryMover::set_refinable_lattice( bool setting )
{
	refinable_lattice_was_set_ = true;
	refinable_lattice_ = setting;
}

std::string SetupForSymmetryMoverCreator::keyname() const {
	return SetupForSymmetryMover::mover_name();
}

protocols::moves::MoverOP
SetupForSymmetryMoverCreator::create_mover() const {
	return utility::pointer::make_shared< SetupForSymmetryMover >();
}

void SetupForSymmetryMoverCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	SetupForSymmetryMover::provide_xml_schema( xsd );
}


////////////////////

ExtractAsymmetricUnitMover::ExtractAsymmetricUnitMover()
: protocols::moves::Mover("ExtractAsymmetricUnitMover"),
	keep_virtual_residues_( true ),
	keep_unknown_aas_( false ) { }

ExtractAsymmetricUnitMover::~ExtractAsymmetricUnitMover()= default;

void
ExtractAsymmetricUnitMover::apply( core::pose::Pose & pose )
{
	// If we are not symmetric do nothing
	if ( !core::pose::symmetry::is_symmetric( pose ) ) return;

	core::pose::Pose pose_asu;
	core::pose::symmetry::extract_asymmetric_unit(pose, pose_asu, keep_virtual_residues_, keep_unknown_aas_);
	pose = pose_asu;
}

void ExtractAsymmetricUnitMover::parse_my_tag(
	utility::tag::TagCOP const tag,
	basic::datacache::DataMap & /*data*/,
	filters::Filters_map const & /*filters*/,
	moves::Movers_map const & /*movers*/,
	core::pose::Pose const & /*pose*/ )
{
	set_keep_virtual_residues( tag->getOption< bool >( "keep_virtual", keep_virtual_residues_ ) );
	set_keep_unknown_aas( tag->getOption< bool >( "keep_unknown_aas", keep_unknown_aas_ ) );
}

void
ExtractAsymmetricUnitMover::set_keep_virtual_residues( bool const keep_virt )
{
	keep_virtual_residues_ = keep_virt;
}

/// @brief If true, residues with aa() == aa_unk will be kept in the asymmetric unit. If false,
///        residues of aa type aa_unk will be ignored in the conversion and left out of the
///        asymmetric unit.
/// @param[in] keep_unk Desired value for keep_unknown_aas (default=false)
/// @details If there are NCAAs in the pose, this must be set to false, or the NCAAs will be
///          ignored.  The keep_unknown_aas defaults to false for historical reasons.
void
ExtractAsymmetricUnitMover::set_keep_unknown_aas( bool const keep_unk )
{
	keep_unknown_aas_ = keep_unk;
}

// XRW TEMP std::string
// XRW TEMP ExtractAsymmetricUnitMover::get_name() const {
// XRW TEMP  return ExtractAsymmetricUnitMover::mover_name();
// XRW TEMP }

std::string ExtractAsymmetricUnitMover::get_name() const {
	return mover_name();
}

std::string ExtractAsymmetricUnitMover::mover_name() {
	return "ExtractAsymmetricUnit";
}

void ExtractAsymmetricUnitMover::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;
	AttributeList attlist;
	// XRW TO DO: check these attributes
	attlist + XMLSchemaAttribute( "keep_virtual", xsct_rosetta_bool, "If true, virtual atoms will be left in the pose. If false, the extracted asymmetric unit will not contain virtual atoms." )
		+ XMLSchemaAttribute( "keep_unknown_aas", xsct_rosetta_bool, "If true, amino acids in the input symmetric pose with aa type aa_unk will be included in the asymmetric unit. If false, amino acids with type aa_unk will be ignored and will not be included in the resulting asymmetric unit." );

	protocols::moves::xsd_type_definition_w_attributes( xsd, mover_name(), "The inverse of SetupForSymmetry: given a symmetric pose, make a nonsymmetric pose that contains only the asymmetric unit.", attlist );
}

std::string ExtractAsymmetricUnitMoverCreator::keyname() const {
	return ExtractAsymmetricUnitMover::mover_name();
}

protocols::moves::MoverOP
ExtractAsymmetricUnitMoverCreator::create_mover() const {
	return utility::pointer::make_shared< ExtractAsymmetricUnitMover >();
}

void ExtractAsymmetricUnitMoverCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	ExtractAsymmetricUnitMover::provide_xml_schema( xsd );
}


/////////////////

ExtractAsymmetricPoseMover::ExtractAsymmetricPoseMover()
: protocols::moves::Mover("ExtractAsymmetricPoseMover"),
	clear_sym_def_( false ) { }

ExtractAsymmetricPoseMover::~ExtractAsymmetricPoseMover()= default;


void
ExtractAsymmetricPoseMover::apply( core::pose::Pose & pose )
{
	// If we are not symmetric do nothing
	if ( !core::pose::symmetry::is_symmetric( pose ) ) return;

	//if symmetric
	TR << "Current pose is symmetric. Making the pose asymmetric." << std::endl;
	core::pose::symmetry::make_asymmetric_pose( pose );

	//clear the symmetry_definition option so it doesn't interfere with repack and minimization of the asymmetric pose.
	if ( clear_sym_def_ == true ) {
		TR << "Clearing symmetry_definition." << std::endl;
		using namespace basic::options;
		option[OptionKeys::symmetry::symmetry_definition].clear();
	}
}

void ExtractAsymmetricPoseMover::parse_my_tag(
	utility::tag::TagCOP const tag,
	basic::datacache::DataMap & /*data*/,
	filters::Filters_map const & /*filters*/,
	moves::Movers_map const & /*movers*/,
	core::pose::Pose const & /*pose*/ )
{
	clear_sym_def( tag->getOption< bool >( "clear_sym_def", clear_sym_def_ ) );
}

// XRW TEMP std::string
// XRW TEMP ExtractAsymmetricPoseMover::get_name() const {
// XRW TEMP  return ExtractAsymmetricPoseMover::mover_name();
// XRW TEMP }

void
ExtractAsymmetricPoseMover::clear_sym_def( bool const clr_sym_def )
{
	clear_sym_def_ = clr_sym_def;
}

std::string ExtractAsymmetricPoseMover::get_name() const {
	return mover_name();
}

std::string ExtractAsymmetricPoseMover::mover_name() {
	return "ExtractAsymmetricPose";
}

void ExtractAsymmetricPoseMover::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;
	AttributeList attlist;
	// XRW TO DO: check
	attlist + XMLSchemaAttribute( "clear_sym_def", xsct_rosetta_bool, "If true, the symmetry_definition option key will be cleared." );

	protocols::moves::xsd_type_definition_w_attributes( xsd, mover_name(), "Similar to ExtractAsymmetricUnit: given a symmetric pose, make a nonsymmetric pose that contains the entire system (all monomers). Can be used to run symmetric and asymmetric moves in the same trajectory.", attlist );
}

std::string ExtractAsymmetricPoseMoverCreator::keyname() const {
	return ExtractAsymmetricPoseMover::mover_name();
}

protocols::moves::MoverOP
ExtractAsymmetricPoseMoverCreator::create_mover() const {
	return utility::pointer::make_shared< ExtractAsymmetricPoseMover >();
}

void ExtractAsymmetricPoseMoverCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	ExtractAsymmetricPoseMover::provide_xml_schema( xsd );
}



} //symmetry
} // protocols
