// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file LegacyRepeatAssemblyMover.cc
///
/// @brief
/// @author Tim Jacobs

// Unit Headers
#include <protocols/legacy_sewing/sampling/LegacyRepeatAssemblyMover.hh>
#include <protocols/legacy_sewing/sampling/LegacyRepeatAssemblyMoverCreator.hh>

//Package headers
#include <protocols/legacy_sewing/util/io.hh>
#include <protocols/legacy_sewing/util/util.hh>
#include <protocols/legacy_sewing/conformation/Model.hh>
#include <protocols/legacy_sewing/conformation/AssemblyFactory.hh>
#include <protocols/legacy_sewing/scoring/LegacyClashScorer.hh>
#include <protocols/legacy_sewing/scoring/LegacyInterModelMotifScorer.hh>

#include <core/import_pose/import_pose.hh>
#include <core/scoring/Energies.hh>

#include <core/conformation/Residue.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/chemical/ResidueTypeSet.hh>

#include <core/pose/symmetry/util.hh>

#include <core/pack/task/operation/TaskOperations.hh>
#include <core/pack/task/operation/TaskOperationCreators.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/rotamer_set/RotamerLinks.hh>
#include <protocols/task_operations/LinkResidues.hh>

#include <protocols/jd2/util.hh>

#include <protocols/relax/AtomCoordinateCstMover.hh>
#include <protocols/relax/FastRelax.hh>
#include <protocols/relax/RelaxProtocolBase.hh>
#include <protocols/relax/util.hh>
#include <protocols/relax/cst_util.hh>

#include <protocols/minimization_packing/PackRotamersMover.hh>
#include <protocols/symmetry/SetupNCSMover.hh>

//Utility headers
#include <basic/Tracer.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/legacy_sewing.OptionKeys.gen.hh>
#include <basic/options/keys/score.OptionKeys.gen.hh>
#include <basic/options/keys/relax.OptionKeys.gen.hh>

#include <numeric/random/random.hh>
#include <numeric/random/random_permutation.hh>
#include <numeric/xyz.functions.hh>
#include <numeric/model_quality/rms.hh>
#include <numeric/xyzTransform.hh>

#include <utility/string_util.hh>
#include <utility/tag/Tag.hh>

#include <boost/unordered_set.hpp>
// XSD XRW Includes
#include <utility/tag/XMLSchemaGeneration.hh>
#include <protocols/moves/mover_schemas.hh>

namespace protocols {
namespace legacy_sewing  {

static basic::Tracer TR( "protocols.legacy_sewing.LegacyRepeatAssemblyMover" );

////////////////////////////////////////////////////////////////////////////////////
///////////////////////////  Boiler Plate Code   ///////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////
// XRW TEMP protocols::moves::MoverOP
// XRW TEMP LegacyRepeatAssemblyMoverCreator::create_mover() const
// XRW TEMP {
// XRW TEMP  return utility::pointer::make_shared< LegacyRepeatAssemblyMover >();
// XRW TEMP }

// XRW TEMP std::string
// XRW TEMP LegacyRepeatAssemblyMoverCreator::keyname() const
// XRW TEMP {
// XRW TEMP  return LegacyRepeatAssemblyMover::mover_name();
// XRW TEMP }

// XRW TEMP std::string
// XRW TEMP LegacyRepeatAssemblyMover::mover_name()
// XRW TEMP {
// XRW TEMP  return "LegacyRepeatAssemblyMover";
// XRW TEMP }

protocols::moves::MoverOP
LegacyRepeatAssemblyMover::clone() const {
	return( utility::pointer::make_shared< LegacyRepeatAssemblyMover >( *this ) );
}
protocols::moves::MoverOP
LegacyRepeatAssemblyMover::fresh_instance() const {
	return utility::pointer::make_shared< LegacyRepeatAssemblyMover >();
}

// XRW TEMP std::string
// XRW TEMP LegacyRepeatAssemblyMover::get_name() const {
// XRW TEMP  return "LegacyRepeatAssemblyMover";
// XRW TEMP }

/////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////  LegacyRepeatAssemblyMover function   //////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////


LegacyRepeatAssemblyMover::LegacyRepeatAssemblyMover():
	parent()
{
	num_repeats_ =
		basic::options::option[ basic::options::OptionKeys::legacy_sewing::num_repeats ].value();
}

///@brief recursive function. Start with a given reference node and randomly traverse
///edges in a depth-first-search of a cycle of a given size (number of nodes).
std::pair< bool, AssemblyOP >
LegacyRepeatAssemblyMover::dfs_cycle_finder(
	ModelNode const * reference_node,
	utility::vector1<ModelNode const *> visited_nodes,
	AssemblyOP assembly
) const {

	using namespace basic::options;
	using namespace basic::options::OptionKeys;

	//Add this node to our path list, if this list has gotten bigger than our desired cycle size then
	//return false
	visited_nodes.push_back(reference_node);

	//Go through all other nodes in the model
	std::set<core::Size> model_nodes = graph_->get_node_indices_from_model_id(reference_node->model().model_id_);
	debug_assert( model_nodes.find( reference_node->get_node_index() ) != model_nodes.end() );
	model_nodes.erase(model_nodes.find(reference_node->get_node_index()));

	auto it = model_nodes.begin();
	auto it_end = model_nodes.end();
	for ( ; it != it_end; ++it ) {
		reference_node = graph_->get_model_node(*it);
		visited_nodes.push_back(reference_node);

		//Explanation of wierd stuff below: All we are trying ot do is iterate the edges from
		//this node in a random order. However, each edge has check has the ability to recurse,
		//and with that recursion, more calls to the "add_edge_from_binary" function, which *drops*
		//all existing edges in the graph. These result of this is that the edge iterator is invalidated
		//on recursion. Thus, a new iterator must be generated. I'm assuming here (and this is
		//probably a bad assumption, hence the asserts) that the edge iteration would be the
		//same every time
		graph_->add_edges_from_binary(edge_file_, reference_node->get_node_index());
		core::Size num_edges =reference_node->num_edges();
		if ( num_edges == 0 ) {
			visited_nodes.pop_back();
			continue;
		}

		utility::vector1<core::Size> traversal_order(num_edges);
		for ( core::Size i=1; i<=num_edges; ++i ) {
			traversal_order[i]=i-1;
		}
		numeric::random::random_permutation(traversal_order, numeric::random::rg());


		core::Size starttime = time(nullptr);
		//Temporary workaround for a bug where excess recursion takes a ton of time.
		core::Size n_recursions = 0;
		core::Size max_recursions = 10;
		for ( core::Size i=1; i<=num_edges; ++i ) {
			//re-add the edges that may have been lost during a recursive call
			graph_->add_edges_from_binary(edge_file_, reference_node->get_node_index());

			//Get an interator the first edge and advance it the number of times indicated
			//in the current index of the traversal_order vector (used for randomness in iteration)
			utility::graph::EdgeListConstIterator edge_it = reference_node->const_edge_list_begin();
			for ( core::Size j=0; j<traversal_order[i]; ++j ) {
				++edge_it;
			}
			auto const * cur_edge = static_cast< HashEdge const * >(*edge_it);
			ModelNode const * other_node = graph_->get_model_node(cur_edge->get_other_ind(reference_node->get_node_index()));

			//check to see if we have a cycle of sufficient size back to the first node
			if ( visited_nodes.size() == (num_repeating_segments_*2) && std::find(visited_nodes.begin(), visited_nodes.end(), other_node) == visited_nodes.begin() ) {
				visited_nodes.push_back(other_node);
				if ( TR.Debug.visible() ) {
					TR.Debug << "Found cycle!: " << std::endl;
					TR.Debug << "visited_nodes.size(): " << visited_nodes.size() << std::endl;
					for ( core::Size i=1; i<=visited_nodes.size(); ++i ) {
						TR.Debug << "model_id: " << visited_nodes[i]->model().model_id_ << " (node " << visited_nodes[i]->get_node_index() << ")" << std::endl;
					}
				}
				assembly->follow_edge(graph_, cur_edge, reference_node->get_node_index());
				core::Real score = clash_scorefxn_->score(assembly);
				TR << "Score of complete cycle " << score << std::endl;
				//if ( score < -0.5 && !requirement_set_->violates(assembly) ) {
				if ( score < threshold_score_of_complete_cycle_ && !requirement_set_->violates(assembly) ) {
					TR << "passed Requirements" << std::endl;
					return std::make_pair(true, assembly);
				} else {
					return std::make_pair(false, assembly);
				}
			} else if ( visited_nodes.size() < (num_repeating_segments_*2) ) {
				//if we don't have a good cycle, but are still less than the max nodes, keep recursing
				//TR.Debug << "other node " << other_node->get_node_index() << std::endl;

				//////TEST/////////
				AssemblyOP pre_assembly = assembly->clone();
				assembly->follow_edge(graph_, cur_edge, reference_node->get_node_index());
				core::Real score = clash_scorefxn_->score(assembly);
				TR.Debug << "Score: " << score << std::endl;
				//core::Real score = assembly_scorefxn_->score(assembly);
				//////TEST/////////

				//If there is a clash, don't keep going
				//if ( score < -0.5 && !requirement_set_->violates(assembly) && n_recursions < max_recursions ) {
				if ( score < threshold_score_of_complete_cycle_ && !requirement_set_->violates(assembly) && n_recursions < max_recursions ) {
					++n_recursions;
					std::pair< bool, AssemblyOP> result = dfs_cycle_finder(other_node, visited_nodes, assembly);
					if ( result.first ) {
						return result;
					}
				}
				assembly = pre_assembly;
			}

		}
		//if this other model node didn't find a cycle, pop it back before trying the next one
		core::Size endtime = time(nullptr);
		TR << visited_nodes.size() << "-" << n_recursions << ": Tested " << num_edges << " edges in " << endtime - starttime << " seconds" << std::endl;
		visited_nodes.pop_back();
	}
	return std::make_pair(false, assembly);
}



AssemblyOP
LegacyRepeatAssemblyMover::generate_assembly(){

	num_trials_ = 100;

	clash_scorefxn_.reset(new scoring::LegacyAssemblyScoreFunction());
	scoring::LegacyAssemblyScorerOP clash_scorer(new scoring::LegacyClashScorer());
	scoring::LegacyAssemblyScorerOP imm_scorer(new scoring::LegacyInterModelMotifScorer());
	clash_scorefxn_->add_scorer("ClashScore", 100.0, clash_scorer);
	clash_scorefxn_->add_scorer("InterModelMotifScore", 100.0, imm_scorer);

	TR << "Generating repeat assembly" << std::endl;
	core::Size num_nodes = graph_->num_nodes();
	for ( core::Size i = 1; i <= num_trials_; ++i ) {
		ModelNode const * const node = graph_->get_model_node(numeric::random::random_range(1, (int)num_nodes));

		//Don't both looking for cycles back to nodes without edges
		graph_->add_edges_from_binary(edge_file_, node->get_node_index());
		if ( !node->num_edges() ) {
			continue;
		}
		graph_->drop_all_edges();

		core::Size starttime = time(nullptr);
		TR << "Trying to find cycle from start node " << node->get_node_index()
			<< " (model " << node->model().model_id_ << ")" << std::endl;

		AssemblyOP assembly = AssemblyFactory::create_assembly("continuous");
		assembly->add_model(graph_, node->model());

		utility::vector1<ModelNode const *> cycle;
		std::pair< bool, AssemblyOP > result = dfs_cycle_finder(node, cycle, assembly);
		if ( result.first ) {
			core::Size endtime = time(nullptr);
			TR << "Successfully found cycle in " << endtime - starttime << " seconds" << std::endl;
			AssemblyOP repeat_assembly = result.second;
			repeat_assembly->to_pose(core::chemical::FA_STANDARD, false).dump_pdb("pre_repeat.pdb");
			output_base_stats(repeat_assembly); ///SingleRepeatScore
			add_repeats(repeat_assembly);
			repeat_assembly->to_pose(core::chemical::FA_STANDARD, false).dump_pdb("post_repeat.pdb");
			core::Real score = assembly_scorefxn_->score(repeat_assembly);
			//   if ( score < -0.5 ) {
			if ( score < threshold_score_of_complete_cycle_ ) {
				return repeat_assembly;
			}
		} else {
			core::Size endtime = time(nullptr);
			TR << "No cycle found with starting node " << node->get_node_index()
				<< "(model " << node->model().model_id_ << "): " << endtime - starttime
				<< " seconds" << std::endl;
		}
	}
	TR << "No cycles found" << std::endl;
	return nullptr;
}



void
LegacyRepeatAssemblyMover::add_repeats(
	AssemblyOP assembly
) const{

	//Get a copy of the existing segments, which currently has an identical first and last segment
	utility::vector1<SewSegment> segments = assembly->segments();

	utility::vector1< numeric::xyzVector<core::Real> > reference_coords = get_segment_coords(segments.back());

	//DIRTY HACK ASSUMES ALL MODELS ARE 3 SEGMENTS LONG!!!!
	utility::vector1< numeric::xyzVector<core::Real> > mobile_coords;
	for ( core::Size i=1; i<=assembly->all_segments()[3].size(); ++i ) {
		TR << "i : " << i << std::endl;
		if ( segments.back() == assembly->all_segments()[3][i] ) {
			mobile_coords = get_segment_coords(assembly->all_segments()[3][i]);
		}
	}

	//runtime_assert(reference_coords.size() == mobile_coords.size());
	if ( reference_coords.size() != mobile_coords.size() ) {
		TR << "Reference segment: " << segments.back().model_id_ << " " << segments.back().segment_id_ << std::endl;
		TR << "Assembly: " << *assembly << std::endl;
		utility_exit_with_message("Reference coords don't match mobile coords!");
	}

	numeric::xyzVector<core::Real> ref_com = numeric::center_of_mass(reference_coords);
	numeric::xyzVector<float> mobile_com = numeric::center_of_mass(mobile_coords);

	utility::vector1<core::Real> weights(reference_coords.size(), 1.0);
	numeric::xyzMatrix<core::Real> uu;
	core::Real sigma3;
	numeric::model_quality::findUU(reference_coords, mobile_coords, weights, (int)reference_coords.size(), uu, sigma3);

	numeric::xyzTransform<core::Real> transformer(uu,ref_com);
	TR.Debug << "Adding " << num_repeats_ << " repeats" << std::endl;
	for ( core::Size i=1; i<=num_repeats_; ++i ) {
		utility::vector1<SewSegment> repeat_segments = segments;
		for ( core::Size j=1; j<=repeat_segments.size(); ++j ) {
			transform_segment_coords(repeat_segments[j], transformer, mobile_com, i);
		}
		//Get rid of the first segment, it is a duplicate
		assembly->delete_segments(assembly->segments().size());
		repeat_segments.erase(repeat_segments.begin());
		repeat_segments.erase(repeat_segments.begin());
		assembly->append_segments(repeat_segments);
	}
	//Remove the first 2 and last two segments of the entire repeat, they are not
	//actually part of repeating unit
	assembly->delete_segments(assembly->segments().size());
	assembly->delete_segments(assembly->segments().size());
	assembly->delete_segments(1);
	assembly->delete_segments(1);
}



utility::vector1< numeric::xyzVector<core::Real> >
LegacyRepeatAssemblyMover::get_segment_coords(
	SewSegment const & segment
) const {
	utility::vector1< numeric::xyzVector<core::Real> > coords;
	for ( core::Size i=1; i <= segment.residues_.size(); ++i ) {
		for ( core::Size j=1; j <= segment.residues_[i].basis_atoms_.size(); ++j ) {
			coords.push_back(segment.residues_[i].basis_atoms_[j].coords_);
		}
	}
	return coords;
}



void
LegacyRepeatAssemblyMover::transform_segment_coords(
	SewSegment & segment,
	numeric::xyzTransform<core::Real> transformer,
	numeric::xyzVector<float> com,
	core::Size n_transformations
) const {
	for ( core::Size i=1; i <= segment.residues_.size(); ++i ) {
		for ( core::Size j=1; j <= segment.residues_[i].basis_atoms_.size(); ++j ) {
			for ( core::Size k=1; k <= n_transformations; ++k ) {
				segment.residues_[i].basis_atoms_[j].coords_ = segment.residues_[i].basis_atoms_[j].coords_ - com;//move to origin
				segment.residues_[i].basis_atoms_[j].coords_ = transformer(segment.residues_[i].basis_atoms_[j].coords_);
			}
		}
	}
}


core::pose::Pose
LegacyRepeatAssemblyMover::refine_assembly(
	AssemblyOP & assembly
) {

	using namespace basic::options;
	using namespace basic::options::OptionKeys;

	//Make sure we're adding the constraints used for NCS symmetry
	fa_scorefxn_->set_weight(core::scoring::dihedral_constraint, 1.0);
	fa_scorefxn_->set_weight(core::scoring::atom_pair_constraint, 1.0);

	//Create a task factory to work with
	core::pack::task::TaskFactoryOP task_factory(new core::pack::task::TaskFactory());

	//Get a pose to work with
	core::pose::Pose pose = get_fullatom_pose(assembly);

	////Prepeare the TaskFactory for packing from both the command line and from
	////Assemblie's native retention
	//core::pack::task::operation::InitializeFromCommandlineOP command_line =
	// new core::pack::task::operation::InitializeFromCommandline();

	//task_factory->push_back(command_line);

	////FastDesign
	//core::Size repeats = option[relax::default_repeats].value();
	//protocols::relax::FastRelaxOP fast_design = new protocols::relax::FastRelax(repeats);
	//fast_design->set_scorefxn(fa_scorefxn_);
	//fast_design->cartesian(true);
	//fast_design->min_type("lbfgs_armijo_nonmonotone");
	//fast_design->set_task_factory(task_factory);
	//fast_design->constrain_relax_to_start_coords(true);
	//fast_design->apply(pose);

	return pose;
}

void
LegacyRepeatAssemblyMover::parse_my_tag(
	TagCOP const tag,
	basic::datacache::DataMap & data,
	protocols::filters::Filters_map const & filters,
	protocols::moves::Movers_map const & movers,
	core::pose::Pose const & pose
){
	parent::parse_my_tag(tag, data, filters, movers, pose);

	if ( tag->hasOption("num_repeating_segments") ) {
		num_repeating_segments_ = tag->getOption<core::Size>("num_repeating_segments");
	}
	if ( tag->hasOption("threshold_score_of_complete_cycle") ) {
		threshold_score_of_complete_cycle_ = tag->getOption<core::Real>("threshold_score_of_complete_cycle", -0.5);
		// before this option addition, it was hard-coded as minus 0.5
	}

}

void
LegacyRepeatAssemblyMover::output_base_stats(
	AssemblyOP const & assembly
) {

	core::Real base_score = assembly_scorefxn_->score(assembly);
	utility::vector1< std::pair< std::string, core::Real > > assembly_scores = assembly_scorefxn_->get_all_scores(assembly);
	for ( core::Size i=1; i<=assembly_scores.size(); ++i ) {
		protocols::jd2::add_string_real_pair_to_current_job("Base"+assembly_scores[i].first, assembly_scores[i].second);
	}
	protocols::jd2::add_string_real_pair_to_current_job( "BaseScore", base_score );

}

std::string LegacyRepeatAssemblyMover::get_name() const {
	return mover_name();
}

std::string LegacyRepeatAssemblyMover::mover_name() {
	return "LegacyRepeatAssemblyMover";
}

void LegacyRepeatAssemblyMover::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;
	//Extra attributes
	AttributeList extra_ats;
	extra_ats
		+ XMLSchemaAttribute( "num_repeating_segments", xsct_non_negative_integer, "Number of segments per repeating unit" )
		+ XMLSchemaAttribute( "threshold_score_of_complete_cycle", xsct_real, "Assemblies with scores below this value after each cycle will proceed to the requirement check." );

	XMLSchemaComplexTypeGeneratorOP ct_gen = LegacyAssemblyMover::define_assembly_mover_ct_gen( xsd );
	ct_gen->element_name( mover_name() );
	ct_gen->description( "Builds repeat proteins using cycles in the SewGraph" );
	ct_gen->add_attributes( extra_ats );
	ct_gen->write_complex_type_to_schema( xsd );

}

std::string LegacyRepeatAssemblyMoverCreator::keyname() const {
	return LegacyRepeatAssemblyMover::mover_name();
}

protocols::moves::MoverOP
LegacyRepeatAssemblyMoverCreator::create_mover() const {
	return utility::pointer::make_shared< LegacyRepeatAssemblyMover >();
}

void LegacyRepeatAssemblyMoverCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	LegacyRepeatAssemblyMover::provide_xml_schema( xsd );
}


//void
//LegacyRepeatAssemblyMover::output_stats(
// AssemblyOP const & assembly,
// core::pose::Pose & pose
//) {
//
// ///Keep all the usual information
// AssemblyMover::output_stats(assembly, pose);
//
// ///Add new information
// protocols::jd2::add_string_real_pair_to_current_job( "1RepeatScore", single_score );

//}

} //legacy_sewing
} //protocols

// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
