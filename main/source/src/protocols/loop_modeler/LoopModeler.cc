// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

// Headers
#include <protocols/loop_modeler/LoopModeler.hh>
#include <protocols/loop_modeler/LoopModelerCreator.hh>
#include <protocols/loop_modeling/LoopMover.hh>
#include <protocols/loop_modeling/LoopBuilder.hh>
#include <protocols/loop_modeling/LoopProtocol.hh>
#include <protocols/loop_modeling/utilities/rosetta_scripts.hh>
#include <protocols/loop_modeling/utilities/LoopMoverGroup.hh>
#include <protocols/loop_modeling/utilities/AcceptanceCheck.hh>
#include <protocols/loop_modeling/utilities/TrajectoryLogger.hh>

// Protocols headers
#include <protocols/filters/Filter.hh>
#include <protocols/kinematic_closure/KicMover.hh>
#include <protocols/kinematic_closure/pivot_pickers/FixedOffsetsPivots.hh>
#include <protocols/kinematic_closure/perturbers/OmegaPerturber.hh>
#include <protocols/kinematic_closure/perturbers/Rama2bPerturber.hh>
#include <protocols/kinematic_closure/perturbers/FragmentPerturber.hh>
#include <protocols/loop_modeler/perturbers/LoopHashPerturber.hh>
#include <protocols/loop_modeling/refiners/MinimizationRefiner.hh>
#include <protocols/loop_modeling/refiners/RepackingRefiner.hh>
#include <protocols/loop_modeling/refiners/RotamerTrialsRefiner.hh>
#include <protocols/loop_modeling/utilities/PrepareForCentroid.hh>
#include <protocols/loop_modeling/utilities/PrepareForFullatom.hh>
#include <protocols/loops/Loop.hh>
#include <protocols/loops/loop_mover/LoopMover.hh>
#include <protocols/loops/Loops.hh>
#include <protocols/loops/loops_main.hh>
#include <protocols/loops/util.hh>
#include <protocols/loophash/LoopHashLibrary.hh>
#include <protocols/moves/mover_schemas.hh>
#include <protocols/moves/MonteCarlo.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/moves/MoverFactory.hh>
#include <protocols/rosetta_scripts/util.hh>
#include <protocols/simple_filters/BuriedUnsatHbondFilter.hh>
#include <protocols/minimization_packing/PackRotamersMover.hh>

// Core headers
#include <core/chemical/ChemicalManager.fwd.hh>
#include <core/chemical/VariantType.hh>
#include <core/conformation/Conformation.hh>
#include <core/import_pose/import_pose.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/optimization/AtomTreeMinimizer.hh>
#include <core/optimization/MinimizerOptions.hh>
#include <core/optimization/symmetry/SymAtomTreeMinimizer.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/task/operation/TaskOperations.hh>
#include <core/pack/task/operation/NoRepackDisulfides.hh>
#include <core/pose/util.hh>
#include <core/pose/extra_pose_info_util.hh>
#include <core/pose/Pose.hh>
#include <core/pose/symmetry/util.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/rms_util.tmpl.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/util/SwitchResidueTypeSet.hh>

// Utility headers
#include <utility/exit.hh>
#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <basic/Tracer.hh>
#include <basic/datacache/DataMap.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/lh.OptionKeys.gen.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/loops.OptionKeys.gen.hh>
#include <boost/algorithm/string.hpp>

// C++ headers
#include <iostream>
#include <cmath>
#include <ctime>

// Namespaces
using namespace std;
using namespace basic::options;

using core::Real;
using core::Size;
using core::chemical::CENTROID;
using core::chemical::FA_STANDARD;
using core::pose::Pose;
using core::import_pose::pose_from_file;
using core::scoring::ScoreFunctionCOP;
using core::scoring::ScoreFunctionOP;
using core::pack::task::TaskFactoryOP;
using core::pack::task::TaskFactoryCOP;

using protocols::filters::FilterOP;
using protocols::loop_modeling::utilities::PrepareForCentroid;
using protocols::loop_modeling::utilities::PrepareForCentroidOP;
using protocols::loop_modeling::utilities::PrepareForFullatom;
using protocols::loop_modeling::utilities::PrepareForFullatomOP;
using protocols::loops::Loop;
using protocols::loops::Loops;
using protocols::moves::MonteCarloOP;
using protocols::moves::MoverOP;

using utility::tag::TagCOP;
using basic::datacache::DataMap;
using protocols::filters::Filters_map;
using protocols::moves::Movers_map;

namespace protocols {
namespace loop_modeler {

static basic::Tracer TR("protocols.loop_modeler.LoopModeler");

LoopModeler::LoopModeler() {
	build_stage_ = add_child( utility::pointer::make_shared< loop_modeling::LoopBuilder >() );
	centroid_stage_ = add_child( utility::pointer::make_shared< loop_modeling::LoopProtocol >() );
	fullatom_stage_ = add_child( utility::pointer::make_shared< loop_modeling::LoopProtocol >() );
	prepare_for_centroid_ = add_child( utility::pointer::make_shared< PrepareForCentroid >() );
	prepare_for_fullatom_ = add_child( utility::pointer::make_shared< PrepareForFullatom >() );

	is_build_stage_enabled_ = true;
	is_centroid_stage_enabled_ = true;
	is_fullatom_stage_enabled_ = true;
	use_default_task_factory_ = true;

	set_fa_scorefxn(loops::get_fa_scorefxn());
	set_cen_scorefxn(loops::get_cen_scorefxn());

	build_stage_->get_logger()->set_prefix("build_");
	centroid_stage_->get_logger()->set_prefix("centroid_");

	setup_kic_config();
}

LoopModeler::~LoopModeler() = default;

MoverOP LoopModeler::clone() const {
	// This is dangerous.  It only works if something else is holding a shared
	// pointer to this object.  The proper thing to do would be to construct a
	// new loop modeler on the fly, but I think this is impossible because
	// there's no way to make a deep copy of a data map.
	return utility::pointer::const_pointer_cast<Mover>(shared_from_this());
}

void LoopModeler::parse_my_tag(
	TagCOP tag,
	DataMap & data,
	Filters_map const & filters,
	Movers_map const & movers,
	Pose const & pose
) {

	loop_modeling::LoopMover::parse_my_tag(tag, data, filters, movers, pose);
	loop_modeling::utilities::set_task_factory_from_tag(*this, tag, data);

	// Parse the 'config' option.

	string config = tag->getOption<string>("config", "kic");

	if ( config == "empty" ) {
		setup_empty_config();
	} else if ( config == "kic" ) {
		setup_kic_config();
	} else if ( config == "kic_with_frags" ) {
		setup_kic_with_fragments_config();
	} else if ( config == "loophash_kic" ) {
		bool loophash_perturb_sequence = tag->getOption<bool>("loophash_perturb_sequence", false);
		std::string seqposes_no_mutate_str = tag->getOption<std::string>("loophash_seqposes_no_mutate", "");

		setup_loophash_kic_config(loophash_perturb_sequence, seqposes_no_mutate_str);
	} else {
		stringstream message;
		message << "Unknown <LoopModeler> config option: '" << config << "'";
		throw CREATE_EXCEPTION(utility::excn::Exception, message.str());
	}

	// Parse the 'auto_refine' option.

	if ( ! tag->getOption<bool>("auto_refine", true) ) {
		centroid_stage_->clear_refiners();
		fullatom_stage_->clear_refiners();
	}

	// Pares the 'scorefxn_fa' and 'scorefxn_cen' options.

	using protocols::rosetta_scripts::parse_score_function;

	if ( tag->hasOption("scorefxn_fa") ) {
		set_fa_scorefxn(parse_score_function(tag, "scorefxn_fa", data, ""));
	}
	if ( tag->hasOption("scorefxn_cen") ) {
		set_cen_scorefxn(parse_score_function(tag, "scorefxn_cen", data, ""));
	}

	// Parse the 'fast' option.

	if ( tag->getOption<bool>("fast", false) ) {
		centroid_stage_->mark_as_test_run();
		fullatom_stage_->mark_as_test_run();
	}

	// Parse subtags.

	for ( TagCOP subtag : tag->getTags() ) {

		// Ignore <Loop> subtags (parsed by parent class).

		if ( subtag->getName() == "Loop" ) { continue; }

		// Parse <Build> subtags.

		else if ( subtag->getName() == "Build" ) {
			build_stage_->parse_my_tag(subtag, data, filters, movers, pose);
			is_build_stage_enabled_ = ! subtag->getOption<bool>("skip", ! is_build_stage_enabled_);
		} else if ( subtag->getName() == "Centroid" ) {
			// Parse <Centroid> subtags.
			centroid_stage_->parse_my_tag(subtag, data, filters, movers, pose);
			is_centroid_stage_enabled_ = ! subtag->getOption<bool>("skip", ! is_centroid_stage_enabled_);
		} else if ( subtag->getName() == "Fullatom" ) {
			// Parse <Fullatom> subtags.
			fullatom_stage_->parse_my_tag(subtag, data, filters, movers, pose);
			is_fullatom_stage_enabled_ = ! subtag->getOption<bool>("skip", ! is_fullatom_stage_enabled_);
		} else {
			// Parse LoopMover subtags.
			loop_modeling::LoopMoverOP centroid_mover = loop_modeling::utilities::loop_mover_from_tag(subtag, data, filters, movers, pose);
			loop_modeling::LoopMoverOP fullatom_mover = loop_modeling::utilities::loop_mover_from_tag(subtag, data, filters, movers, pose);

			centroid_stage_->add_mover(centroid_mover);
			fullatom_stage_->add_mover(fullatom_mover);
		}
	}
}

bool LoopModeler::do_apply(Pose & pose) {
	// Save a copy of the pose with it's original sidechains before they're
	// (possibly) removed below, so that we can restore them without having to
	// repack the whole structure when we transition back into fullatom mode.

	prepare_for_fullatom_->set_original_pose(pose);

	// Set default task factory if the user doesn't give one

	if ( use_default_task_factory_ ) {
		TR << "Set default task factory." << std::endl;
		set_task_factory(get_default_task_factory(pose));
		use_default_task_factory_ = true; // This line is important because the set_task_factory() function changes use_default_task_factory_
	}

	// Converting the pose to centroid mode can sometimes cause headaches with
	// non-canonical residue type sets, so this conditional makes sure that this
	// conversion only happens if necessary.

	if ( is_build_stage_enabled_ || is_centroid_stage_enabled_ ) {
		prepare_for_centroid_->apply(pose);
	}

	if ( is_build_stage_enabled_ ) {
		TR << "*******************************************" << endl;
		TR << "                                           " << endl;
		TR << "   Build Stage                             " << endl;
		TR << "                                           " << endl;
		TR << "*******************************************" << endl;
		build_stage_->apply(pose);
	}

	if ( is_centroid_stage_enabled_ ) {
		TR << "*******************************************" << endl;
		TR << "                                           " << endl;
		TR << "   Centroid Stage                          " << endl;
		TR << "                                           " << endl;
		TR << "*******************************************" << endl;
		centroid_stage_->apply(pose);
	}

	if ( is_fullatom_stage_enabled_ ) {
		TR << "*******************************************" << endl;
		TR << "                                           " << endl;
		TR << "   Fullatom Stage                          " << endl;
		TR << "                                           " << endl;
		TR << "*******************************************" << endl;
		prepare_for_fullatom_->apply(pose);
		fullatom_stage_->apply(pose);
	}

	return true;
}

void LoopModeler::setup_empty_config() {
	centroid_stage_->set_sfxn_cycles(5);
	centroid_stage_->set_temp_cycles(20, true);
	centroid_stage_->set_mover_cycles(1);
	centroid_stage_->set_temperature_schedule(2.0, 1.0);
	centroid_stage_->clear_movers_and_refiners();

	fullatom_stage_->set_sfxn_cycles(5);
	fullatom_stage_->set_temp_cycles(10, true);
	fullatom_stage_->set_mover_cycles(2);
	fullatom_stage_->clear_movers_and_refiners();
}

void LoopModeler::setup_kic_config() {
	using namespace protocols::kinematic_closure;
	using namespace protocols::loop_modeling::refiners;
	using namespace protocols::loop_modeling::utilities;

	setup_empty_config();

	centroid_stage_->add_mover(utility::pointer::make_shared< KicMover >());
	centroid_stage_->add_refiner(utility::pointer::make_shared< MinimizationRefiner >());
	centroid_stage_->mark_as_default();

	fullatom_stage_->set_temperature_ramping(true);
	fullatom_stage_->set_rama_term_ramping(true);
	fullatom_stage_->set_repulsive_term_ramping(true);
	fullatom_stage_->add_mover(utility::pointer::make_shared< KicMover >());
	fullatom_stage_->add_refiner(utility::pointer::make_shared< RepackingRefiner >(40));
	fullatom_stage_->add_refiner(utility::pointer::make_shared< RotamerTrialsRefiner >());
	fullatom_stage_->add_refiner(utility::pointer::make_shared< MinimizationRefiner >());
	fullatom_stage_->mark_as_default();
}

void LoopModeler::setup_kic_with_fragments_config() {
	using namespace basic::options;
	using namespace protocols::kinematic_closure;
	using namespace protocols::kinematic_closure::perturbers;

	setup_kic_config();

	// Read fragment data from the command line.

	utility::vector1<core::fragment::FragSetOP> frag_libs;

	if ( option[ OptionKeys::loops::frag_files ].user() ) {
		loops::read_loop_fragments(frag_libs);
	} else {
		stringstream message;
		message << "Must specify the -loops:frag_sizes and -loops:frag_files ";
		message << "options in order to use the FragmentPerturber." << endl;
		throw CREATE_EXCEPTION(utility::excn::Exception, message.str());
	}

	// Enable fragments during the build stage.

	build_stage_->use_fragments(frag_libs);

	// Create a centroid "KIC with fragments" mover (see note).

	KicMoverOP centroid_kic_mover( new KicMover );
	centroid_kic_mover->add_perturber(utility::pointer::make_shared< FragmentPerturber >(frag_libs));
	centroid_stage_->add_mover(centroid_kic_mover);
	centroid_stage_->mark_as_default();

	// Create a fullatom "KIC with fragments" mover (see note).

	KicMoverOP fullatom_kic_mover( new KicMover );
	fullatom_kic_mover->add_perturber(utility::pointer::make_shared< FragmentPerturber >(frag_libs));
	fullatom_stage_->add_mover(fullatom_kic_mover);
	fullatom_stage_->mark_as_default();

	// Note: Because loop movers can query their parents for attributes like
	// score functions and task operations, no loop mover can have more than one
	// parent.  This is why two separate centroid and fullatom KicMover objects
	// must be created.
}

void LoopModeler::setup_loophash_kic_config(bool perturb_sequence, std::string seqposes_no_mutate_str) {
	using namespace basic::options;
	using namespace protocols::kinematic_closure;
	using namespace protocols::kinematic_closure::pivot_pickers;
	using namespace protocols::loophash;
	using namespace protocols::loop_modeler::perturbers;

	setup_kic_config();

	// Initialize the loop hash library from command line
	// I don't like command line options, but this is the quick way to do things

	if ( !option[ OptionKeys::lh::loopsizes ].user() ) {
		std::stringstream message;
		message << "Must specify the -lh:loopsizes and -lh:db_path ";
		message << "options in order to use the LoopHashPerturber." << std::endl;
		throw CREATE_EXCEPTION(utility::excn::Exception, message.str());
	}

	utility::vector1<core::Size> loop_sizes = option[ OptionKeys::lh::loopsizes ].value();
	LoopHashLibraryOP lh_library(new LoopHashLibrary( loop_sizes ));
	lh_library->load_mergeddb();

	// Set the offsets for pivot picking
	utility::vector1 <core::Size> pp_offsets;
	for ( auto s : loop_sizes ) {
		// We need right_pivot - left_pivot > 2. Otherwise nothing is really changed
		if ( s > 5 ) {
			pp_offsets.push_back(s - 3);
		}
	}

	runtime_assert(pp_offsets.size() > 0);

	// Create a centroid "loophash KIC" mover (see note).

	KicMoverOP centroid_kic_mover( new KicMover );
	centroid_kic_mover->set_pivot_picker(utility::pointer::make_shared< FixedOffsetsPivots >(pp_offsets));
	perturbers::LoopHashPerturberOP centroid_loophash_perturber_(new LoopHashPerturber(lh_library) );
	centroid_loophash_perturber_->perturb_sequence(perturb_sequence);
	centroid_loophash_perturber_->seqposes_no_mutate_str(seqposes_no_mutate_str);
	centroid_kic_mover->add_perturber(centroid_loophash_perturber_);
	centroid_stage()->add_mover(centroid_kic_mover);
	centroid_stage()->mark_as_default();

	// Create a fullatom "loophash KIC" mover (see note).

	KicMoverOP fullatom_kic_mover( new KicMover );
	fullatom_kic_mover->set_pivot_picker(utility::pointer::make_shared< FixedOffsetsPivots >(pp_offsets));
	perturbers::LoopHashPerturberOP fullatom_loophash_perturber_(new LoopHashPerturber(lh_library) );
	fullatom_loophash_perturber_->perturb_sequence(perturb_sequence);
	fullatom_loophash_perturber_->seqposes_no_mutate_str(seqposes_no_mutate_str);
	fullatom_kic_mover->add_perturber(fullatom_loophash_perturber_);
	fullatom_stage()->add_mover(fullatom_kic_mover);
	fullatom_stage()->mark_as_default();

	// Note: Because loop movers can query their parents for attributes like
	// score functions and task operations, no loop mover can have more than one
	// parent.  This is why two separate centroid and fullatom KicMover objects
	// must be created.

}

loop_modeling::LoopBuilderOP LoopModeler::build_stage() {
	return build_stage_;
}

loop_modeling::LoopProtocolOP LoopModeler::centroid_stage() {
	return centroid_stage_;
}

loop_modeling::LoopProtocolOP LoopModeler::fullatom_stage() {
	return fullatom_stage_;
}

void LoopModeler::enable_build_stage() {
	is_build_stage_enabled_ = true;
}

void LoopModeler::disable_build_stage() {
	is_build_stage_enabled_ = false;
}

void LoopModeler::enable_centroid_stage() {
	is_centroid_stage_enabled_ = true;
}

void LoopModeler::disable_centroid_stage() {
	is_centroid_stage_enabled_ = false;
}

void LoopModeler::enable_fullatom_stage() {
	is_fullatom_stage_enabled_ = true;
}

void LoopModeler::disable_fullatom_stage() {
	is_fullatom_stage_enabled_ = false;
}

TaskFactoryOP LoopModeler::get_task_factory() {
	return get_tool<TaskFactoryOP>(loop_modeling::ToolboxKeys::TASK_FACTORY);
}

TaskFactoryOP LoopModeler::get_task_factory(TaskFactoryOP fallback) {
	return get_tool<TaskFactoryOP>(loop_modeling::ToolboxKeys::TASK_FACTORY, fallback);
}

void LoopModeler::set_task_factory(TaskFactoryOP task_factory) {
	set_tool(loop_modeling::ToolboxKeys::TASK_FACTORY, task_factory);
	use_default_task_factory_ = false;
}

core::pack::task::TaskFactoryOP
LoopModeler::get_default_task_factory(core::pose::Pose &pose){
	using namespace core::pack::task;
	using namespace core::pack::task::operation;

	// Get residues that are packable

	loop_modeling::LoopsCOP loops = get_loops();

	utility::vector1<bool> is_packable( pose.size(), false );

	TR << loops->size() << std::endl; ///DEBUG

	pose.update_residue_neighbors();
	select_loop_residues( pose, *loops,
		/*include_neighbors*/ true, is_packable, /*cutoff_distance*/ 10.0 );

	PreventRepackingOP turn_off_packing( new PreventRepacking() );

	for ( core::Size i=1; i<=pose.size(); ++i ) {
		if ( !is_packable[i] ) {
			turn_off_packing->include_residue(i);
		}
	}

	// Use extra rotamers

	ExtraRotamersGenericOP extra_rotamers( new ExtraRotamersGeneric );
	extra_rotamers->ex1(true);
	extra_rotamers->ex2(true);
	extra_rotamers->extrachi_cutoff(0);

	// Set the task factory

	TaskFactoryOP task_factory = utility::pointer::make_shared< TaskFactory >();
	task_factory->push_back(turn_off_packing);
	task_factory->push_back(utility::pointer::make_shared< RestrictToRepacking >());
	task_factory->push_back(extra_rotamers);

	return task_factory;
}

void LoopModeler::set_fa_scorefxn(ScoreFunctionOP function) {
	prepare_for_fullatom_->set_score_function(function);
	fullatom_stage_->set_score_function(function);
}

void LoopModeler::set_cen_scorefxn(ScoreFunctionOP function) {
	build_stage_->set_score_function(function);
	centroid_stage_->set_score_function(function);
}

std::string LoopModeler::get_name() const {
	return mover_name();
}

std::string LoopModeler::mover_name() {
	return "LoopModeler";
}

void LoopModeler::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;

	// config restriction (kic or kic_with_frags or empty)
	XMLSchemaRestriction restriction_type;
	restriction_type.name( "config_options" );
	restriction_type.base_type( xs_string );
	restriction_type.add_restriction( xsr_pattern, "kic|kic_with_frags|loophash_kic|empty" );
	xsd.add_top_level_element( restriction_type );

	AttributeList attlist;
	attlist
		+ XMLSchemaAttribute::attribute_w_default("config", "config_options",
		"Configure the local backbone move used in the refinement steps."
		"If kic_with_frags is specified, -loops:frag_sizes and -loops:frag_files options are required.", "kic")
		+ XMLSchemaAttribute::attribute_w_default("auto_refine", xsct_rosetta_bool,
		"Disable \"auto_refine\" if you want"
		"to specify your own refinement moves", "true")
		+ XMLSchemaAttribute("scorefxn_fa", xs_string, "Score function for full atom modeling.")
		+ XMLSchemaAttribute("scorefxn_cen", xs_string, "Score function for modeling in centroid representation.")
		+ XMLSchemaAttribute::attribute_w_default("fast", xsct_rosetta_bool, "Only test run (fewer cycles)", "false")
		+ XMLSchemaAttribute::attribute_w_default("loophash_perturb_sequence", xsct_rosetta_bool, "Let LoopHashKIC also perturb the amino acid sequence", "false")
		+ XMLSchemaAttribute::attribute_w_default("loophash_seqposes_no_mutate", xs_string, "Sequence positions that should not be mutated by LoopHashKIC.", "");

	// Use helper function in ./utilities/rosetta_scripts.cc to parse task operations
	loop_modeling::utilities::attributes_for_set_task_factory_from_tag( attlist );

	// subelements
	XMLSchemaSimpleSubelementList subelement_list;
	// Create a complex type and get the LoopMover attributes, as parse_my_tag calls LoopMover::parse_my_tag
	XMLSchemaComplexTypeGenerator ct_gen;

	// Get LoopMover tag
	LoopMover::define_composition_schema( xsd, ct_gen, subelement_list );

	// Subelements for LoopProtocols


	AttributeList subelement_attributes = loop_modeling::LoopProtocol::loop_protocol_attlist();
	subelement_attributes + XMLSchemaAttribute("skip", xsct_rosetta_bool, "If \"skip\" is enabled, the corresponding step will be skipped");
	subelement_list.add_simple_subelement("Build", subelement_attributes,
		"Configure the build step. If \"skip\" is enabled, none of the loops will be rebuilt."
		"You may also provide this tag with any option or subtag that would be understood by LoopBuilder.");
	subelement_list.add_simple_subelement("Centroid", subelement_attributes,
		"Configure the centroid refinement step. If \"skip\" is enabled, none of the loops will be rebuilt. "
		"You may also provide this tag with any option or subtag that would be understood by LoopProtocol.");
	subelement_list.add_simple_subelement("Fullatom", subelement_attributes,
		"Configure the centroid refinement step. If \"skip\" is enabled, none of the loops will be rebuilt. "
		"You may also provide this tag with any option or subtag that would be understood by LoopProtocol.");

	// Create a complex type and  get the LoopMover attributes, as parse_my_tag calls LoopMover::parse_my_tag
	ct_gen.element_name( mover_name() )
		.description( "Perform a complete loop modeling simulation, including the build, centroid, and fullatom steps. "
		"It is a wrapper around other modeling movers." )
		.add_attributes( attlist  )
		.set_subelements_repeatable( subelement_list )
		.write_complex_type_to_schema( xsd );

}


std::string LoopModelerCreator::keyname() const {
	return LoopModeler::mover_name();
}

protocols::moves::MoverOP
LoopModelerCreator::create_mover() const {
	return utility::pointer::make_shared< LoopModeler >();
}

void LoopModelerCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	LoopModeler::provide_xml_schema( xsd );
}


}
}
