// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file LegacyAssemblyMover.cc
///
/// @brief
/// @author Tim Jacobs

// Unit Headers
#include <protocols/legacy_sewing/sampling/LegacyAssemblyMover.hh>
#include <protocols/legacy_sewing/sampling/LegacyAssemblyMoverCreator.hh>

// Package Headers
#include <protocols/legacy_sewing/conformation/Assembly.hh>
#include <protocols/legacy_sewing/sampling/SewGraph.hh>
#include <protocols/legacy_sewing/sampling/requirements/LegacyRequirementSet.hh>
#include <protocols/legacy_sewing/sampling/requirements/LegacyRequirementFactory.hh>
#include <protocols/legacy_sewing/scoring/LegacyAssemblyScorer.hh>
#include <protocols/legacy_sewing/util/io.hh>

// Basic Headers
#include <basic/options/option.hh>
#include <basic/Tracer.hh>

#include <numeric/random/random.hh>
#include <numeric/random/random_permutation.hh>

#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>

#include <core/scoring/Energies.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/rms_util.hh>

#include <core/pack/task/operation/TaskOperations.hh>
#include <core/pack/task/operation/TaskOperationCreators.hh>
#include <core/pack/task/TaskFactory.hh>

#include <basic/options/option.hh>
#include <basic/options/keys/legacy_sewing.OptionKeys.gen.hh>
#include <basic/options/keys/relax.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>

#include <protocols/minimization_packing/PackRotamersMover.hh>
#include <protocols/minimization_packing/PackRotamersMoverCreator.hh>
#include <protocols/moves/mover_schemas.hh>
#include <protocols/filters/Filter.hh>

#include <protocols/jd2/util.hh>

#include <protocols/minimization_packing/MinMover.hh>

#include <protocols/relax/AtomCoordinateCstMover.hh>
#include <protocols/relax/FastRelax.hh>
#include <protocols/relax/RelaxProtocolBase.hh>
#include <protocols/relax/util.hh>
#include <protocols/relax/cst_util.hh>

#include <utility/io/ozstream.hh>
#include <ObjexxFCL/format.hh>

///Temp will be removed when requirement files are in place
#include <protocols/legacy_sewing/scoring/LegacyClashScorer.hh>
#include <protocols/legacy_sewing/scoring/LegacyInterModelMotifScorer.hh>
#include <protocols/legacy_sewing/scoring/LegacyMotifScorer.hh>
#include <protocols/legacy_sewing/scoring/LegacyPartnerMotifScorer.hh>

namespace protocols {
namespace legacy_sewing  {

static basic::Tracer TR( "protocols.legacy_sewing.LegacyAssemblyMover" );

////////////////////////////////////////////////////////////////////////////////////
///////////////////////////  Mover  Functions   ////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////

bool
LegacyAssemblyMover::reinitialize_for_new_input() const {
	return true;
}

std::string
LegacyAssemblyMover::get_name() const {
	return LegacyAssemblyMover::mover_name();
}

LegacyAssemblyMover::LegacyAssemblyMover():
	graph_(nullptr),
	requirement_factory_(sampling::requirements::LegacyRequirementFactory::get_instance()),
	cen_scorefxn_(nullptr),
	fa_scorefxn_(nullptr),
	base_native_bonus_(1.5),
	neighbor_cutoff_(15)
{
	requirement_set_.reset( new sampling::requirements::LegacyRequirementSet() );
}

void
LegacyAssemblyMover::apply( core::pose::Pose & pose ) {

	TR << "Attempting to generate a assembly with the following requirments: " << std::endl;
	requirement_set_->show(TR);

	///////// Score Function setup ////////////
	fa_scorefxn_ = core::scoring::ScoreFunctionFactory::create_score_function( core::scoring::TALARIS_2013_CART );
	fa_scorefxn_->set_weight(core::scoring::res_type_constraint, 1.0);
	fa_scorefxn_->set_weight(core::scoring::coordinate_constraint, 0.5);

	cen_scorefxn_ = core::scoring::ScoreFunctionFactory::create_score_function( core::scoring::SCORE4_SMOOTH_CART );

	assembly_scorefxn_.reset(new scoring::LegacyAssemblyScoreFunction());

	scoring::LegacyAssemblyScorerOP clash_scorer(new scoring::LegacyClashScorer());
	assembly_scorefxn_->add_scorer("ClashScore", 100.0, clash_scorer);

	scoring::LegacyAssemblyScorerOP motif_scorer(new scoring::LegacyMotifScorer());
	assembly_scorefxn_->add_scorer("MotifScore", 1.0, motif_scorer);

	scoring::LegacyAssemblyScorerOP inter_model_motif_scorer(new scoring::LegacyInterModelMotifScorer());
	assembly_scorefxn_->add_scorer("InterModelMotifScore", 10.0, inter_model_motif_scorer);

	scoring::LegacyAssemblyScorerOP partner_motif_scorer(new scoring::LegacyPartnerMotifScorer());
	assembly_scorefxn_->add_scorer("InterfaceMotif", 1.0, partner_motif_scorer);

	//First, try to generate an Assembly
	core::Size starttime = time(nullptr);
	AssemblyOP assembly = generate_assembly();
	if ( assembly == nullptr ) {
		TR << "Failed to generate an Assembly (if this comes from EnumerateAssembly, this is actually a success, hopefully later I can better code this part)" << std::endl;
		set_last_move_status(protocols::moves::FAIL_RETRY);
		return;
	}
	// else if(requirement_set_->satisfies(assembly)) {
	//  TR << "Failed to find an Assembly that satisfies all requirements!" << std::endl;
	//  set_last_move_status(protocols::moves::FAIL_RETRY);
	//  return;
	// }
	core::Size endtime = time(nullptr);
	TR << "Successfully generated Assembly in " << endtime - starttime << " seconds" << std::endl;

	//We have a complete, valid Assembly. Now change
	//it to a pose and refine it.
	starttime = time(nullptr);
	if ( ! basic::options::option[ basic::options::OptionKeys::legacy_sewing::skip_refinement ].value() ) {
		pose = refine_assembly(assembly);
	} else {
		pose = get_fullatom_pose(assembly);
	}
	TR << "Got Pose!" << std::endl;

	output_stats(assembly, pose, "from_LegacyMonteCarloAssemblyMover");
	endtime = time(nullptr);
	TR << "Refined Assembly in " << endtime - starttime << " seconds" << std::endl;

	//SUCCESS!
	set_last_move_status(protocols::moves::MS_SUCCESS);
}//LegacyAssemblyMover::apply( core::pose::Pose & pose ) {


void
LegacyAssemblyMover::add_starting_model(
	AssemblyOP assembly
) const {
	core::Size num_nodes = graph_->num_nodes();
	utility::vector1<core::Size> node_order(num_nodes);
	for ( core::Size i = 1; i <= num_nodes; ++i ) {
		node_order[i]=i;
	}
	numeric::random::random_permutation(node_order, numeric::random::rg());

	for ( core::Size i=1; i<=num_nodes; ++i ) {
		ModelNode const * const node = graph_->get_model_node(node_order[i]);

		AssemblyOP pre_op_assembly = assembly->clone();
		assembly->add_model(graph_, node->model());
		if ( !requirement_set_->violates(assembly) ) {
			return;
		}
		assembly = pre_op_assembly;
	}
	utility_exit_with_message("Failed to find an initial model that satisfies requirements!");
}//LegacyAssemblyMover::add_starting_model(

core::pose::Pose
LegacyAssemblyMover::get_fullatom_pose(
	AssemblyOP assembly
) const {
	return assembly->to_pose(core::chemical::FA_STANDARD, false);
}

// not needed for LegacyMonteCarloAssemblyMover, but keep this
// not needed for LegacyEnumerateAssemblyMover, but keep this
bool
LegacyAssemblyMover::follow_random_edge_from_node(
	AssemblyOP assembly,
	ModelNode const * reference_node
) const {

	if ( TR.Debug.visible() ) { TR << "LegacyAssemblyMover::follow_random_edge_from_node " << std::endl;}

	//Randomly permute the edges of the current node until we find one that
	//satisfies all requirements
	core::Size num_edges = reference_node->num_edges();
	utility::vector1<core::Size> edge_order(num_edges);
	for ( core::Size i = 0; i < num_edges; ++i ) {
		//if(TR.Debug.visible()) { TR << "i: " << i << std::endl;}
		edge_order[i+1]=i;
	}
	numeric::random::random_permutation(edge_order, numeric::random::rg());

	for ( core::Size cur_edge_ind=1; cur_edge_ind<=num_edges; ++cur_edge_ind ) {
		//if(TR.Debug.visible()) { TR << "cur_edge_ind: " << cur_edge_ind << std::endl;}
		utility::graph::EdgeListConstIterator edge_it = reference_node->const_edge_list_begin();
		for ( core::Size j=0; j<edge_order[cur_edge_ind]; ++j ) {
			++edge_it;
		}

		//Cast the edge to a proper HashEdge, follow it, and make sure we haven't violated
		//any requirements. If we have, revert and try the next edge
		auto const * const cur_edge = static_cast< HashEdge const * >(*edge_it);

		AssemblyOP pre_op_assembly = assembly->clone();
		assembly->follow_edge(graph_, cur_edge, reference_node->get_node_index());
		if ( !requirement_set_->violates(assembly) ) {
			return true;
		}
		assembly = pre_op_assembly;
	}
	TR << "Failed to find any edges that satisfy all requirements, consider less strigent requirements!" << std::endl;
	return false;
}//follow_random_edge_from_node


core::pose::Pose
LegacyAssemblyMover::get_centroid_pose(
	AssemblyOP assembly
) const {
	return assembly->to_pose(core::chemical::CENTROID, false);
}

core::pose::Pose
LegacyAssemblyMover::refine_assembly(
	AssemblyOP & assembly
) {

	using namespace basic::options;
	using namespace basic::options::OptionKeys;

	/************** Full-atom Stage ****************/
	//Before relaxing and packing
	core::pose::Pose pose = get_fullatom_pose(assembly);

	//Prepeare the TaskFactory for packing from both the command line and from
	//Assemblie's native retention
	core::pack::task::operation::InitializeFromCommandlineOP command_line(
		new core::pack::task::operation::InitializeFromCommandline() );

	core::pack::task::TaskFactoryOP task_factory(new core::pack::task::TaskFactory());
	task_factory->push_back(command_line);
	assembly->prepare_for_packing(pose, task_factory, base_native_bonus_, neighbor_cutoff_);

	//Pack
	// protocols::minimization_packing::PackRotamersMoverOP pack = new protocols::minimization_packing::PackRotamersMover();
	// pack->score_function(fa_scorefxn_);
	// pack->task_factory(task_factory);

	//Relax
	// protocols::relax::RelaxProtocolBaseOP relax = new protocols::relax::FastRelax(fa_scorefxn_);
	// relax->set_scorefxn(fa_scorefxn_);
	// relax->cartesian(true);
	// relax->min_type("lbfgs_armijo_nonmonotone");
	// relax->constrain_relax_to_start_coords(true);

	//FastDesign
	core::Size repeats = option[OptionKeys::relax::default_repeats].value();
	protocols::relax::FastRelaxOP fast_design(new protocols::relax::FastRelax(repeats));
	fast_design->set_scorefxn(fa_scorefxn_);
	fast_design->cartesian(true);
	fast_design->min_type("lbfgs_armijo_nonmonotone");
	fast_design->set_task_factory(task_factory);
	fast_design->constrain_relax_to_start_coords(true);
	fast_design->apply(pose);

	protocols::relax::delete_virtual_residues( pose );
	//pose.energies().clear();

	return pose;
}//refine_assembly

void
LegacyAssemblyMover::output_stats(
	AssemblyOP const & assembly,
	core::pose::Pose & pose,
	std::string pdb_name // and indicator_whether_from_LegacyMonteCarloAssemblyMover_or_LegacyEnumerateAssemblyMover
) {

	using namespace basic::options;
	using namespace basic::options::OptionKeys;

	///Somehow, the energies object is getting totally F*#*%ed
	pose.energies().structure_has_moved(true);

	if ( pdb_name == "from_LegacyMonteCarloAssemblyMover" ) {

		TR << "Base class output stats!" << std::endl;

		std::string const job_name ( protocols::jd2::current_output_name() );

		/**************** Dump multi chain pose and NativeRotamersMap ******************/
		//Dump the multichain pose
		core::pose::Pose multi_chain_pose = assembly->to_multichain_pose(core::chemical::FA_STANDARD);
		multi_chain_pose.dump_pdb(std::string(option[ OptionKeys::out::path::all ]())  + "/" + job_name+"_multichain.pdb");

		NativeRotamersMap nat_ro_map = assembly->generate_native_rotamers_map();
		write_native_residue_file(nat_ro_map, std::string(option[ OptionKeys::out::path::all ]())  + "/" + job_name+".rot");



		/************** Assembly-only Statistics ****************/
		std::string path = assembly->string_path();
		protocols::jd2::add_string_string_pair_to_current_job("path", path);

		utility::vector1< std::pair< std::string, core::Real > > assembly_scores = assembly_scorefxn_->get_all_scores(assembly);
		for ( core::Size i=1; i<=assembly_scores.size(); ++i ) {
			protocols::jd2::add_string_real_pair_to_current_job(assembly_scores[i].first, assembly_scores[i].second);
		}

		/************** Centroid Statistics ****************/

		//Report centroid score terms
		core::pose::Pose cen_pose = get_centroid_pose(assembly);
		cen_scorefxn_->score(cen_pose);
		core::Real env_score = cen_pose.energies().total_energies()[core::scoring::cen_env_smooth];
		core::Real rg_score = cen_pose.energies().total_energies()[core::scoring::rg];
		core::Real pair_score = cen_pose.energies().total_energies()[core::scoring::cen_pair_smooth];
		protocols::jd2::add_string_real_pair_to_current_job( "cen_env_smooth",  env_score );
		protocols::jd2::add_string_real_pair_to_current_job( "cen_rg",  rg_score );
		protocols::jd2::add_string_real_pair_to_current_job( "cen_pair_smooth",  pair_score );

		/************** Full-Atom Statistics ****************/

		//Write a residue-normalized score to the score file
		protocols::jd2::add_string_real_pair_to_current_job( "nres", pose.size());
		protocols::jd2::add_string_real_pair_to_current_job( "norm_tot_score", fa_scorefxn_->score(pose)/pose.size());
		protocols::jd2::add_string_real_pair_to_current_job( "percent_native", assembly->percent_native(pose));

		//Write RMS data to the score file by generating pose from the Assembly, and comparing it to the refined pose
		core::pose::Pose unrefined_pose = get_fullatom_pose(assembly);
		if ( unrefined_pose.size() == pose.size() ) {
			core::Real rms = core::scoring::bb_rmsd_including_O(unrefined_pose, pose);
			protocols::jd2::add_string_real_pair_to_current_job( "bb_rmsd",  rms );
		} else {
			TR << "NOT THE SAME NUMBER OF RESIDUES" << std::endl;
			TR << "pose: " << pose.size() << std::endl;
			TR << "unrefined pose: " << unrefined_pose.size() << std::endl;
			core::Real rms = pose.size() - unrefined_pose.size();
			protocols::jd2::add_string_real_pair_to_current_job( "bb_rmsd",  rms );
		}

		//Print a PyMOL selection for 'native' positions
		TR << "Pymol select " << assembly->natives_select(pose, job_name) << std::endl;
	} else { // when it is from LegacyEnumerateAssemblyMover

		TR << "it is from LegacyEnumerateAssemblyMover!" << std::endl;

		/**************** Dump multi chain pose and NativeRotamersMap ******************/
		//Dump the multichain pose
		core::pose::Pose multi_chain_pose = assembly->to_multichain_pose(core::chemical::FA_STANDARD);
		multi_chain_pose.dump_pdb(pdb_name + "_multichain.pdb");

		NativeRotamersMap nat_ro_map = assembly->generate_native_rotamers_map();
		write_native_residue_file(nat_ro_map, pdb_name + ".rot");

		//Report centroid score terms
		//  core::pose::Pose cen_pose = get_centroid_pose(assembly);
		//  cen_scorefxn_->score(cen_pose);
		//  core::Real env_score = cen_pose.energies().total_energies()[core::scoring::cen_env_smooth];
		//  core::Real rg_score = cen_pose.energies().total_energies()[core::scoring::rg];
		//  core::Real pair_score = cen_pose.energies().total_energies()[core::scoring::cen_pair_smooth];
		//
		//  /************** Full-Atom Statistics ****************/

		//Write a residue-normalized score to the score file
		core::Real nres = pose.size();
		//  core::Real norm_tot_score = fa_scorefxn_->score(pose)/pose.size();
		//  core::Real percent_native = assembly->percent_native(pose);
		//
		//  //Write RMS data to the score file by generating pose from the Assembly, and comparing it to the refined pose
		////  core::pose::Pose unrefined_pose = get_fullatom_pose(assembly);
		////  if(unrefined_pose.size() == pose.size()) {
		////   core::Real rms = core::scoring::bb_rmsd_including_O(unrefined_pose, pose);
		////   core::Real bb_rmsd = rms;
		//  }
		//  else {
		//   TR << "NOT THE SAME NUMBER OF RESIDUES" << std::endl;
		//   TR << "pose: " << pose.size() << std::endl;
		//   TR << "unrefined pose: " << unrefined_pose.size() << std::endl;
		//   core::Real rms = pose.size() - unrefined_pose.size();
		//   core::Real bb_rmsd = rms;
		//  }

		//Print a PyMOL selection for 'native' positions
		TR << "Pymol select " << assembly->natives_select(pose, pdb_name) << std::endl;


		///////// write individual score file

		utility::io::ozstream individual_score_file;
		std::string score_filename = pdb_name + ".score";
		individual_score_file.open(score_filename);

		utility::vector1< std::pair< std::string, core::Real > > assembly_scores = assembly_scorefxn_->get_all_scores(assembly);
		for ( core::Size i=1; i<=assembly_scores.size(); ++i ) {
			individual_score_file << assembly_scores[i].first << " ";
			//job_me->add_string_real_pair(assembly_scores[i].first, assembly_scores[i].second);
		}

		individual_score_file << " nres assembly_name path" << std::endl;

		for ( core::Size i=1; i<=assembly_scores.size(); ++i ) {
			individual_score_file << assembly_scores[i].second << " ";
			//job_me->add_string_real_pair(assembly_scores[i].first, assembly_scores[i].second);
		}
		std::string path = assembly->string_path();
		individual_score_file << nres << " " << pdb_name << " " << path << std::endl;

	}//when it is from EnumerateLegacyAssemblyMover
}//output_stats


void
LegacyAssemblyMover::append_movie_frame(
	AssemblyOP assembly,
	core::Size cycle
) const {

	if ( !basic::options::option[basic::options::OptionKeys::legacy_sewing::dump_pdbs] ) {
		return;
	}

	using namespace ObjexxFCL::format;
	utility::io::ozstream out("test_movie.pdb", std::ios_base::app);
	out << "MODEL     " << cycle << "\n";
	utility::vector1<SewSegment> segments = assembly->segments();
	core::Size atomno=0;
	core::Size resnum=0;

	//core::Real x_offset = segments[1].residues_[1].basis_atoms_[1].coords_(1);
	//core::Real y_offset = segments[1].residues_[1].basis_atoms_[1].coords_(2);
	//core::Real z_offset = segments[1].residues_[1].basis_atoms_[1].coords_(3);
	core::Real x_offset = 0;
	core::Real y_offset = 0;
	core::Real z_offset = 0;
	for ( core::Size i=1; i<=segments.size(); ++i ) {
		for ( core::Size j=1; j<=segments[i].residues_.size(); ++j ) {
			++resnum;

			out << "ATOM  " << I(5,++atomno) << "  N   " <<
				"GLY" << ' ' << 'A' << I(4,resnum ) << "    " <<
				F(8,3,segments[i].residues_[j].basis_atoms_[1].coords_(1) - x_offset) <<
				F(8,3,segments[i].residues_[j].basis_atoms_[1].coords_(2) - y_offset) <<
				F(8,3,segments[i].residues_[j].basis_atoms_[1].coords_(3) - z_offset) <<
				F(6,2,1.0) << F(6,2,1.0) << '\n';

			out << "ATOM  " << I(5,++atomno) << "  CA  " <<
				"GLY" << ' ' << 'A' << I(4,resnum ) << "    " <<
				F(8,3,segments[i].residues_[j].basis_atoms_[2].coords_(1) - x_offset) <<
				F(8,3,segments[i].residues_[j].basis_atoms_[2].coords_(2) - y_offset) <<
				F(8,3,segments[i].residues_[j].basis_atoms_[2].coords_(3) - z_offset) <<
				F(6,2,1.0) << F(6,2,1.0) << '\n';

			out << "ATOM  " << I(5,++atomno) << "  C   " <<
				"GLY" << ' ' << 'A' << I(4,resnum ) << "    " <<
				F(8,3,segments[i].residues_[j].basis_atoms_[3].coords_(1) - x_offset) <<
				F(8,3,segments[i].residues_[j].basis_atoms_[3].coords_(2) - y_offset) <<
				F(8,3,segments[i].residues_[j].basis_atoms_[3].coords_(3) - z_offset) <<
				F(6,2,1.0) << F(6,2,1.0) << '\n';
			out << "ATOM  " << I(5,++atomno) << "  O   " <<
				"GLY" << ' ' << 'A' << I(4,resnum ) << "    " <<
				F(8,3,segments[i].residues_[j].basis_atoms_[4].coords_(1) - x_offset) <<
				F(8,3,segments[i].residues_[j].basis_atoms_[4].coords_(2) - y_offset) <<
				F(8,3,segments[i].residues_[j].basis_atoms_[4].coords_(3) - z_offset) <<
				F(6,2,1.0) << F(6,2,1.0) << '\n';
		}
	}
	out << "ENDMDL\n";
}

void
LegacyAssemblyMover::parse_requirements(
	TagCOP const tag,
	basic::datacache::DataMap & data,
	protocols::filters::Filters_map const & filters,
	protocols::moves::Movers_map const & movers,
	core::pose::Pose const & pose
){
	auto begin=tag->getTags().begin();
	auto end=tag->getTags().end();
	for ( ; begin != end; ++begin ) {
		TagCOP requirement_tag= *begin;

		if ( requirement_tag->getName() == "GlobalRequirements" ) {
			parse_global_requirements(requirement_tag, data, filters, movers, pose);
		} else if ( requirement_tag->getName() == "IntraSegmentRequirements" ) {
			parse_intra_segment_requirements(requirement_tag, data, filters, movers, pose);
		} else {
			TR.Error << "Only allowed sub-tags of LegacyAssemblyMover are GlobalRequirements" << std::endl;
			TR.Error << "and IntraSegmentRequirements. Please see the LEGACY_SEWING protocol documentation" << std::endl;
			TR.Error << "Tag with name '" << requirement_tag->getName() << "' is invalid" << std::endl;
			throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError, "");
		}
	}
}

void
LegacyAssemblyMover::parse_global_requirements(
	TagCOP const tag,
	basic::datacache::DataMap & data,
	protocols::filters::Filters_map const & filters,
	protocols::moves::Movers_map const & movers,
	core::pose::Pose const & pose
){
	auto begin=tag->getTags().begin();
	auto end=tag->getTags().end();
	for ( ; begin != end; ++begin ) {
		TagCOP requirement_tag= *begin;
		sampling::requirements::LegacyGlobalRequirementOP requirement =
			requirement_factory_->get_global_requirement(requirement_tag->getName());
		requirement->parse_my_tag(requirement_tag, data, filters, movers, pose);
		requirement_set_->add_requirement(requirement);
	}
}

void
LegacyAssemblyMover::parse_intra_segment_requirements(
	TagCOP const tag,
	basic::datacache::DataMap & data,
	protocols::filters::Filters_map const & filters,
	protocols::moves::Movers_map const & movers,
	core::pose::Pose const & pose
){
	if ( !tag->hasOption("index") ) {
		utility_exit_with_message("You must give an 'index' attribute to the IntraSegmentRequirements tag!");
	}
	auto index = tag->getOption<core::Size>("index");

	auto begin=tag->getTags().begin();
	auto end=tag->getTags().end();
	for ( ; begin != end; ++begin ) {
		TagCOP requirement_tag= *begin;
		sampling::requirements::LegacyIntraSegmentRequirementOP requirement =
			requirement_factory_->get_intra_segment_requirement(requirement_tag->getName());
		requirement->parse_my_tag(requirement_tag, data, filters, movers, pose);
		requirement_set_->add_requirement(index, requirement);
	}
}

void
LegacyAssemblyMover::parse_my_tag(
	TagCOP const tag,
	basic::datacache::DataMap & data,
	protocols::filters::Filters_map const & filters,
	protocols::moves::Movers_map const & movers,
	core::pose::Pose const & pose
){
	using namespace basic::options;

	parse_requirements(tag, data, filters, movers, pose);

	utility::vector1<BasisPair> alignment_pairs;
	std::map< int, Model > models;

	if ( tag->hasOption("model_file") && tag->hasOption("score_file") ) {
		edge_file_ = tag->getOption<std::string>("score_file");
		std::string model_file = tag->getOption<std::string>("model_file");
		//model_file = tag->getOption<std::string>("model_file");
		//TR << "model_file: " << model_file << std::endl;
		models = read_model_file(model_file);
	} else if ( option[basic::options::OptionKeys::legacy_sewing::score_file_name].user() &&
			option[basic::options::OptionKeys::legacy_sewing::model_file_name].user() ) {
		edge_file_ = option[basic::options::OptionKeys::legacy_sewing::score_file_name].value();
		std::string model_file = option[basic::options::OptionKeys::legacy_sewing::model_file_name].value();
		// TR << "model_file by option: " << model_file << std::endl;
		models = read_model_file(model_file);
	} else {
		utility_exit_with_message("You must give a model file and score file to an LegacyAssemblyMover either through options or tags");
	}
	if ( option[ basic::options::OptionKeys::legacy_sewing::assembly_type ].value() == "discontinuous" ) {
		graph_.reset( new SewGraph(models, 2) );
	} else if ( option[ basic::options::OptionKeys::legacy_sewing::assembly_type ].value() == "continuous" ) {
		graph_.reset( new SewGraph(models, 1) );
	}

	/////////Segments////////////
	if ( tag->hasOption("min_segments") ) {
		auto min_segments = tag->getOption<core::Size>("min_segments");
		requirement_set_->min_segments(min_segments);
	} else {
		TR.Warning << "You have not specified a min segment size, using 0" << std::endl;
	}
	if ( tag->hasOption("max_segments") ) {
		auto max_segments = tag->getOption<core::Size>("max_segments");
		requirement_set_->max_segments(max_segments);
	} else {
		TR.Warning << "You have not specified a min segment size, using arbitrary maximum of 50 segments" << std::endl;
		requirement_set_->max_segments(50);
	}

	/////////Base native bonus////////////
	if ( tag->hasOption("base_native_bonus") ) {
		base_native_bonus_ = tag->getOption<core::Real>("base_native_bonus");
	} else if ( option[basic::options::OptionKeys::legacy_sewing::base_native_bonus].user() ) {
		base_native_bonus_ = option[basic::options::OptionKeys::legacy_sewing::base_native_bonus];
	}


	/////////Number of neighbors cutoff////////////
	if ( tag->hasOption("neighbor_cutoff") ) {
		neighbor_cutoff_ = tag->getOption<core::Size>("neighbor_cutoff");
	} else if ( option[basic::options::OptionKeys::legacy_sewing::neighbor_cutoff].user() ) {
		neighbor_cutoff_ = option[basic::options::OptionKeys::legacy_sewing::neighbor_cutoff];
	}

}

std::string
LegacyAssemblyMover::mover_name(){
	return "LegacyAssemblyMover";
}

utility::tag::XMLSchemaComplexTypeGeneratorOP
LegacyAssemblyMover::define_assembly_mover_ct_gen( utility::tag::XMLSchemaDefinition & xsd ){
	using namespace utility::tag;

	//Set up parsing of requirements
	sampling::requirements::LegacyRequirementFactory::get_instance()->define_global_requirements_subelement( xsd );
	sampling::requirements::LegacyRequirementFactory::get_instance()->define_intra_segment_requirements_subelement( xsd );
	XMLSchemaSimpleSubelementList subs;
	subs
		.add_already_defined_subelement( "GlobalRequirements", & legacy_assembly_mover_ct_namer )
		.add_already_defined_subelement( "IntraSegmentRequirements", & legacy_assembly_mover_ct_namer);

	//Attributes
	AttributeList attlist;
	attlist
		+ XMLSchemaAttribute( "model_file", xs_string, "Name of input model file (can also be provided on command line)")
		+ XMLSchemaAttribute( "score_file", xs_string, "Name of input score (edge) file (can also be provided on command line)")
		+ XMLSchemaAttribute::attribute_w_default( "min_segments", xsct_non_negative_integer, "Minimum number of segments (SSEs including loops) in the assembly", "0" )
		+ XMLSchemaAttribute::attribute_w_default( "max_segments", xsct_non_negative_integer, "Maximum number of segments in the assembly", "50" )
		+ XMLSchemaAttribute( "base_native_bonus", xsct_real, "If performing design, what score bonus should residues be given for maintaining one of their native identities? Defaults to 1.5 in constructor." )
		+ XMLSchemaAttribute( "neighbor_cutoff", xsct_non_negative_integer, "Minimum number of neighbors to turn on favoring native residue. NOTE: This option currently doesn't do anything. Defaults to 15 in constructor." )
		+ optional_name_attribute();

	XMLSchemaComplexTypeGeneratorOP ct_gen( new XMLSchemaComplexTypeGenerator );
	ct_gen->complex_type_naming_func( & protocols::moves::complex_type_name_for_mover );
	ct_gen->add_attributes( attlist );
	ct_gen->set_subelements_repeatable( subs, 0, 1 );
	return ct_gen;
}





void
LegacyAssemblyMover::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ){
	using namespace utility::tag;

	XMLSchemaComplexTypeGeneratorOP ct_gen = LegacyAssemblyMover::define_assembly_mover_ct_gen( xsd );
	ct_gen->element_name( mover_name() );
	ct_gen->description( "Base class for using the old SEWING framework to generate protein backbones" );
	ct_gen->write_complex_type_to_schema( xsd );
}

std::string
LegacyAssemblyMover::legacy_assembly_mover_ct_namer( std::string const & input){
	return "LegacyAssemblyMover_" + input + "_type";
}

} //legacy_sewing
} //protocols
