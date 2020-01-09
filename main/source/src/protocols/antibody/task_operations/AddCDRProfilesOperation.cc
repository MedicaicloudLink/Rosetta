// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocol/antibody/task_operations/AddCDRProfilesOperation.cc
/// @brief
/// @author Jared Adolf-Bryfogle (jadolfbr@gmail.com)

#include <protocols/antibody/task_operations/AddCDRProfilesOperation.hh>
#include <protocols/antibody/task_operations/AddCDRProfilesOperationCreator.hh>

#include <protocols/antibody/task_operations/AddCDRProfileSetsOperation.hh>

#include <protocols/antibody/AntibodyInfo.hh>
#include <protocols/antibody/util.hh>
#include <protocols/antibody/design/util.hh>
#include <protocols/antibody/design/AntibodyDesignEnumManager.hh>

#include <protocols/task_operations/ResidueProbDesignOperation.hh>
#include <protocols/task_operations/ConservativeDesignOperation.hh>

#include <core/pack/task/operation/TaskOperations.hh>

#include <basic/Tracer.hh>
#include <utility>
#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <core/pack/task/operation/task_op_schemas.hh>
#include <utility/string_util.hh>

#include <basic/options/option.hh>
#include <basic/options/keys/OptionKeys.hh>
#include <basic/options/keys/antibody.OptionKeys.gen.hh>

static basic::Tracer TR("protocols.antibody.task_operations.AddCDRProfilesOperation");

namespace protocols {
namespace antibody {
namespace task_operations {

using namespace core::pack::task::operation;
using namespace protocols::antibody::design;
using namespace protocols::antibody::clusters;
using namespace protocols::task_operations;
using namespace basic::options;
using namespace basic::options::OptionKeys;
using namespace utility::tag;

AddCDRProfilesOperation::AddCDRProfilesOperation():
	TaskOperation(),
	ab_info_(/* NULL */),
	profile_sets_task_(/* NULL */)
{
	set_defaults();
}

AddCDRProfilesOperation::AddCDRProfilesOperation(AntibodyInfoCOP ab_info):
	TaskOperation(),
	ab_info_(std::move(ab_info)),
	profile_sets_task_(/* NULL */)
{
	set_defaults();
}

AddCDRProfilesOperation::AddCDRProfilesOperation(AntibodyInfoCOP ab_info, const utility::vector1<bool>& cdrs):
	TaskOperation(),
	ab_info_(std::move(ab_info)),
	profile_sets_task_(/* NULL */)
{
	set_defaults();
	set_cdrs(cdrs);
}

void
AddCDRProfilesOperation::set_defaults(){

	seq_design_options_.clear();

	pre_loaded_data_ = false;

	//Profile Options
	picking_rounds_ = 1;
	include_native_restype_ = true;
	force_north_paper_db_ = false;
	use_outliers_ = false;
	stats_cutoff_ = 10;
	zero_prob_sample_ = 0;
	cons_design_data_source_ = "blosum62";
	pre_loaded_data_ = false;
	prob_set_.clear();
	seq_design_options_.clear();

	AntibodyEnumManager manager = AntibodyEnumManager();
	std::string numbering_scheme = option [OptionKeys::antibody::input_ab_scheme]();
	numbering_scheme_ = manager.numbering_scheme_string_to_enum(numbering_scheme);

	for ( core::Size i = 1; i <=CDRNameEnum_proto_total; ++i ) {
		auto cdr = static_cast<CDRNameEnum>(i);
		CDRSeqDesignOptionsOP opt = utility::pointer::make_shared< CDRSeqDesignOptions >(cdr);
		if ( cdr == l4 || cdr == h4 ) {
			opt->design(false);
		}

		seq_design_options_.push_back(opt);
	}

	cons_task_ = utility::pointer::make_shared< ConservativeDesignOperation >();

}



AddCDRProfilesOperation::AddCDRProfilesOperation(AddCDRProfilesOperation const & src):
	TaskOperation(src),
	seq_design_options_(src.seq_design_options_),
	picking_rounds_(src.picking_rounds_),
	include_native_restype_(src.include_native_restype_),
	force_north_paper_db_(src.force_north_paper_db_),
	use_outliers_(src.use_outliers_),
	stats_cutoff_(src.stats_cutoff_),
	zero_prob_sample_(src.zero_prob_sample_),
	cons_design_data_source_(src.cons_design_data_source_),
	pre_loaded_data_(src.pre_loaded_data_),
	prob_set_(src.prob_set_),
	no_profile_data_cdrs_(src.no_profile_data_cdrs_),
	no_profile_sets_data_cdrs_(src.no_profile_sets_data_cdrs_),
	numbering_scheme_(src.numbering_scheme_),
	ignore_light_chain_(src.ignore_light_chain_),
	no_probability_(src.no_probability_)


{
	using namespace task_operations;

	if ( src.ab_info_ ) ab_info_ = utility::pointer::make_shared< AntibodyInfo >( *src.ab_info_);
	if ( src.cons_task_ )  cons_task_ = utility::pointer::make_shared< ConservativeDesignOperation >( *src.cons_task_ );
	if ( src.profile_sets_task_ ) profile_sets_task_ = utility::pointer::make_shared< AddCDRProfileSetsOperation >( *src.profile_sets_task_ );
}

TaskOperationOP
AddCDRProfilesOperation::clone() const{
	return utility::pointer::make_shared< AddCDRProfilesOperation >( *this );
}

AddCDRProfilesOperation::~AddCDRProfilesOperation()= default;

void
AddCDRProfilesOperation::parse_tag(utility::tag::TagCOP tag, basic::datacache::DataMap&){
	if ( tag->hasOption("cdrs") ) {
		TR << "Setting CDRs from settings" << std::endl;
		set_cdrs(get_cdr_bool_from_tag(tag, "cdrs", true /*include CDR4 */));
	}

	AntibodyDesignEnumManager manager = AntibodyDesignEnumManager();
	set_fallback_strategy(manager.seq_design_strategy_string_to_enum(tag->getOption< std::string >("fallback_strategy", "seq_design_conservative")));
	include_native_restype_ = tag->getOption< bool>("include_native_restype", include_native_restype_);
	picking_rounds_ = tag->getOption< core::Size >("picking_rounds", picking_rounds_);
	force_north_paper_db_ = tag->getOption< bool >("force_north_paper_db", force_north_paper_db_);
	use_outliers_ = tag->getOption< bool >("use_outliers", use_outliers_);
	stats_cutoff_ = tag->getOption< core::Size >("stats_cutoff", stats_cutoff_);
	zero_prob_sample_ = tag->getOption< core::Real >("sample_zero_probs_at", zero_prob_sample_);
	set_cons_design_data_source( tag->getOption< std::string >("cons_design_data_source", cons_design_data_source_) );
	set_no_probability(tag->getOption< bool >("no_probabilities", no_probability_));

	if ( tag->hasOption("input_ab_scheme") ) {


		AntibodyEnumManager manager = AntibodyEnumManager();
		numbering_scheme_ = manager.numbering_scheme_string_to_enum(tag->getOption<std::string>("input_ab_scheme"));

	}

	if ( tag->hasOption("cdr_definition") && tag->getOption<std::string>("cdr_definition") != "North" ) {
		TR <<"This operation only works with the North CDR definition." <<std::endl;
	}
}

std::string AddCDRProfilesOperation::keyname() { return "AddCDRProfilesOperation"; }

void AddCDRProfilesOperation::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	AttributeList attributes;

	// Check if this is the real default with Jared: all I know is that the functional default is "six trues"
	// for cdrs_.
	attributes
		+ XMLSchemaAttribute( "cdrs", xs_string , "Which CDRs " )
		+ XMLSchemaAttribute::attribute_w_default( "fallback_strategy", xs_string, "Strategy to use when there is not enough data to use profiles for a particular CDR cluster", "seq_design_conservative" )
		+ XMLSchemaAttribute::attribute_w_default( "include_native_restype", xsct_rosetta_bool, "Should we include the native AA at the position, or not?", "true" )
		+ XMLSchemaAttribute::attribute_w_default( "picking_rounds", xsct_non_negative_integer, "How many times should we choose an AA from the probabilities?", "1" )
		+ XMLSchemaAttribute::attribute_w_default( "force_north_paper_db", xsct_rosetta_bool, "Should we only use the North Paper database, or use newer data if present?", "false" )
		+ XMLSchemaAttribute::attribute_w_default( "use_outliers", xsct_rosetta_bool, "Should we use data for probabilities that includes outliers?", "false" )
		+ XMLSchemaAttribute::attribute_w_default( "stats_cutoff", xsct_non_negative_integer, "If the number of data points for a particular cluster is below this value, we use the fallback strategy instead of cluster sequence profiles", "10" )
		+ XMLSchemaAttribute::attribute_w_default( "sample_zero_probs_at", xsct_real, "If a particular AA has 0 as a probility, whould we keep it at zero or sample it at some probability?  Used to increase variability of designs", "0.0" )
		+ XMLSchemaAttribute::attribute_w_default( "cons_design_data_source", xs_string, "For conservative design, which blosum matrix should we use?", "blosum62" )
		+ XMLSchemaAttribute::attribute_w_default( "no_probabilities", xsct_rosetta_bool, "Should we sample ALL AA that does not have prob of 0 at 1.0 instead?  This basically works to add ALL AA seen at a given position in a particular cluster to the set of design residues.  Used to increase variability of designs or for testing purposes", "false")
		+ XMLSchemaAttribute( "input_ab_scheme", xs_string , "Input Antibody Numbering Scheme.  If not set, uses the cmd-line default." )
		+ XMLSchemaAttribute( "cdr_definition", xs_string , "Input CDR Definition - only works with North currently" );

	std::string description =
		"Author: Jared Adolf-Bryfogle (jadolfbr@gmail.com)\n"
		"@brief Add Cluster-based CDR Profiles as the task operation for the set of CDRs by default.\n"
		" See ResidueProbTaskOperation for more.\n"
		"\n"
		" CDR definitions used are North/Dunbrack as the clusters are defined using it.\n"
		"\n"
		" @details If Cluster-based profiles cannot be used, will use the fallback strategy.\n"
		" This can happen if the the CDR is of an unknown cluster or there is too little data\n"
		" about the cluster to use profiles.\n"
		"\n"
		"\n"
		" FALLBACK STRATEGIES:\n"
		"    seq_design_conservative adds a conservative mutation set to the possible residue types (blosum62 default),\n"
		"    seq_design_basic will do nothing (as the default for design is to allow all residue positions);\n"
		"    seq_design_none will disable design for that CDR (essentially your saying that if it doesn't have profiles, don't design it)\n"
		"\n"
		"\n"
		" This TaskOperation is not currently recommended for H3 as it does not cluster well.\n"
		"\n"
		" Optionally sample whole CDR sequences via the primary strategy of:\n"
		" seq_design_profile_sets (use sets instead of profile probability)\n"
		"     seq_design_profile_sets_combined (use profile sets and profile probability)";

	task_op_schema_w_attributes( xsd, keyname(), attributes, description );
}

void
AddCDRProfilesOperation::set_cdr_only(CDRNameEnum cdr){
	for ( core::Size i = 1; i <= CDRNameEnum_proto_total; ++i ) {
		auto current_cdr = static_cast<CDRNameEnum>( i );

		if ( current_cdr == cdr ) {
			seq_design_options_[ i ]->design(true);
		} else {
			seq_design_options_[ i ]->design(false);
		}
	}
}

void
AddCDRProfilesOperation::set_cdrs(const utility::vector1<bool>& c) {

	utility::vector1< bool > cdrs = c;
	if ( cdrs.size() < CDRNameEnum_proto_total ) {
		for ( core::Size i = cdrs.size() +1; i <= CDRNameEnum_proto_total; ++i ) {
			cdrs.push_back( false );
		}
	}
	debug_assert( cdrs.size() == 8);

	for ( core::Size i = 1; i <= CDRNameEnum_proto_total; ++i ) {
		if ( cdrs[ i ] ) {
			seq_design_options_[ i ]->design(true);
		} else {
			seq_design_options_[ i ]->design(false);
		}
	}
}

void
AddCDRProfilesOperation::set_design_options(design::AntibodyCDRSeqDesignOptions seq_design_options){
	seq_design_options_ = seq_design_options;
}

void
AddCDRProfilesOperation::set_fallback_strategy(SeqDesignStrategyEnum fallback_strategy){
	for ( core::Size i = 1; i <= CDRNameEnum_proto_total; ++i ) {
		seq_design_options_[ i ]->fallback_strategy(fallback_strategy);
	}
}

void
AddCDRProfilesOperation::set_primary_strategy(SeqDesignStrategyEnum primary_strategy){
	for ( core::Size i = 1; i <= CDRNameEnum_proto_total; ++i ) {
		seq_design_options_[ i ]->design_strategy(primary_strategy);
	}
}

void
AddCDRProfilesOperation::set_picking_rounds(core::Size rounds){
	picking_rounds_ = rounds;
}

void
AddCDRProfilesOperation::set_include_native_type(bool use_native) {
	include_native_restype_ = use_native;
}

void
AddCDRProfilesOperation::set_force_north_paper_db(bool force_north_db){
	force_north_paper_db_ = force_north_db;
}

void
AddCDRProfilesOperation::set_stats_cutoff(core::Size stats_cutoff) {
	stats_cutoff_ = stats_cutoff;
}

void
AddCDRProfilesOperation::set_use_outliers(bool use_outliers){
	use_outliers_ = use_outliers;
}

void
AddCDRProfilesOperation::set_sample_zero_probs_at(core::Real zero_prob_sample){
	zero_prob_sample_ = zero_prob_sample;
}

void
AddCDRProfilesOperation::set_cons_design_data_source(std::string data_source){

	//Re-load conservative data
	if ( cons_design_data_source_ != data_source ) {
		cons_task_->set_data_source(data_source);
	}
	cons_design_data_source_ = data_source;
}

///@brief Set to sample all available AAs per position at a weight of 1.0 instead of sampling based on weights
void
AddCDRProfilesOperation::set_no_probability( bool no_probability ){
	no_probability_ = no_probability;
}

utility::vector1<bool>
AddCDRProfilesOperation::get_profile_and_design_cdrs() const {
	utility::vector1<bool> profiles(CDRNameEnum_proto_total, false);
	for ( core::Size i = 1; i <= CDRNameEnum_proto_total; ++i ) {
		if ( ! seq_design_options_[ i ]->design() ) continue;
		SeqDesignStrategyEnum strat = seq_design_options_[ i ]->design_strategy();
		if ( strat == seq_design_profiles || strat == seq_design_profile_sets_combined ) {
			profiles[ i ] = true;
		}
	}
	return profiles;
}

utility::vector1<bool>
AddCDRProfilesOperation::get_profile_set_and_design_cdrs() const {
	utility::vector1<bool> profiles(CDRNameEnum_proto_total, false);
	for ( core::Size i = 1; i <= CDRNameEnum_proto_total; ++i ) {
		if ( ! seq_design_options_[ i ]->design() ) continue;

		SeqDesignStrategyEnum strat = seq_design_options_[ i ]->design_strategy();
		if ( strat == seq_design_profile_sets || strat == seq_design_profile_sets_combined ) {
			profiles[ i ] = true;
		}
	}
	return profiles;
}

utility::vector1<bool>
AddCDRProfilesOperation::get_design_cdrs() const{
	utility::vector1<bool> designing(CDRNameEnum_proto_total, false);
	for ( core::Size i = 1; i <= CDRNameEnum_proto_total; ++i ) {
		if ( seq_design_options_[ i ]->design() ) {
			designing[ i ] = true;
		}
	}
	return designing;
}

void
AddCDRProfilesOperation::pre_load_data(const core::pose::Pose& pose){
	utility::vector1<bool> profile_set_cdrs = this->get_profile_set_and_design_cdrs();
	utility::vector1<bool> profile_cdrs = this->get_profile_and_design_cdrs();

	core::Size n_profile_set_cdrs = count(profile_set_cdrs.begin(), profile_set_cdrs.end(), true);
	core::Size n_profile_cdrs = count(profile_cdrs.begin(), profile_cdrs.end(), true);

	if ( ! ab_info_ ) {
		ab_info_ = utility::pointer::make_shared< AntibodyInfo >(pose, numbering_scheme_, North);
	}

	if ( n_profile_set_cdrs > 0 ) {

		profile_sets_task_ = utility::pointer::make_shared< AddCDRProfileSetsOperation >(ab_info_);
		profile_sets_task_->set_ignore_light_chain( ignore_light_chain_ );
		profile_sets_task_->set_cdrs(profile_set_cdrs);
		profile_sets_task_->set_force_north_paper_db(force_north_paper_db_);
		profile_sets_task_->set_use_outliers(use_outliers_);
		profile_sets_task_->set_cutoff(stats_cutoff_);

		no_profile_sets_data_cdrs_ = profile_sets_task_->pre_load_data(pose);

	}

	if ( n_profile_cdrs > 0 ) {

		prob_set_ = get_cluster_profile_probability_data(
			ab_info_,
			pose,
			profile_cdrs,
			no_profile_data_cdrs_,
			stats_cutoff_,
			use_outliers_,
			force_north_paper_db_,
			ignore_light_chain_);

	}

	pre_loaded_data_ = true;
	TR << "Data pre-loaded." << std::endl;
}

void
AddCDRProfilesOperation::apply(const core::pose::Pose& pose, core::pack::task::PackerTask& task) const{

	//This is due to const apply and no pose in parse_my_tag.
	AntibodyInfoOP local_ab_info;
	if ( ! ab_info_ ) {
		local_ab_info = utility::pointer::make_shared< AntibodyInfo >(pose, numbering_scheme_, North);
	} else {
		local_ab_info = ab_info_->clone();
	}


	utility::vector1<bool> no_profile_set_cdrs;
	utility::vector1<bool> no_profile_cdrs;

	utility::vector1<bool> profile_set_cdrs = this->get_profile_set_and_design_cdrs();
	utility::vector1<bool> profile_cdrs = this->get_profile_and_design_cdrs();

	core::Size n_profile_set_cdrs = count(profile_set_cdrs.begin(), profile_set_cdrs.end(), true);
	core::Size n_profile_cdrs = count(profile_cdrs.begin(), profile_cdrs.end(), true);


	/// Load, setup, and run ProfileSets operation.
	if ( n_profile_set_cdrs > 0 ) {
		AddCDRProfileSetsOperationOP profile_sets_task; //yay const apply
		if ( pre_loaded_data_ ) {
			no_profile_set_cdrs = no_profile_sets_data_cdrs_;
			profile_sets_task = utility::pointer::make_shared< AddCDRProfileSetsOperation >( *profile_sets_task_ );
		} else {
			profile_sets_task = utility::pointer::make_shared< AddCDRProfileSetsOperation >( local_ab_info);

			profile_sets_task->set_cdrs(profile_set_cdrs);
			profile_sets_task->set_force_north_paper_db(force_north_paper_db_);
			profile_sets_task->set_use_outliers(use_outliers_);
			profile_sets_task->set_cutoff(stats_cutoff_);

			no_profile_set_cdrs = profile_sets_task->pre_load_data(pose);
		}
		TR << "applying profile sets op" << std::endl;

		profile_sets_task->set_picking_rounds(picking_rounds_);
		profile_sets_task->set_include_native_type(include_native_restype_);
		profile_sets_task->apply(pose, task);
	}

	if ( n_profile_cdrs > 0 ) {
		std::map< core::Size, std::map< core::chemical::AA, core::Real > > prob_set;

		if ( pre_loaded_data_ ) {
			prob_set = prob_set_;
			no_profile_cdrs = no_profile_data_cdrs_;
		} else {
			prob_set = get_cluster_profile_probability_data(
				local_ab_info,
				pose,
				profile_cdrs,
				no_profile_cdrs,
				stats_cutoff_,
				use_outliers_,
				force_north_paper_db_);
		}

		ResidueProbDesignOperation prob_task = ResidueProbDesignOperation();

		prob_task.set_picking_rounds(picking_rounds_);
		prob_task.set_aa_probability_set( prob_set );
		prob_task.set_no_probability(no_probability_);

		prob_task.set_include_native_restype( include_native_restype_ );
		prob_task.set_sample_zero_probs_at( zero_prob_sample_ );
		TR << "applying prob task op"<<std::endl;
		prob_task.apply(pose, task);
	}

	utility::vector1< bool > no_data_cdrs(8, false);

	if ( n_profile_cdrs > 0 ) {
		no_data_cdrs = no_profile_cdrs;
	} else if ( n_profile_set_cdrs > 0 ) {
		no_data_cdrs = no_profile_set_cdrs;
	}

	/// Add the conservative design op.
	cons_task_->include_native_aa(include_native_restype_);
	core::Size cons_task_residues = 0;

	for ( auto const & cdr : local_ab_info->get_all_cdrs_present(true /* Include DE */) ) {
		if ( (no_data_cdrs[ cdr ]  && seq_design_options_[ cdr ]->fallback_strategy() == seq_design_conservative) || (seq_design_options_[ cdr ]->design() && seq_design_options_[ cdr ]->design_strategy() == seq_design_conservative) ) {

			TR << "Using conservative op for " << local_ab_info->get_CDR_name(cdr) << std::endl;
			for ( core::Size resnum = local_ab_info->get_CDR_start(cdr, pose, North); resnum <= local_ab_info->get_CDR_end(cdr, pose, North); ++resnum ) {
				cons_task_->include_residue( resnum );
				cons_task_residues += 1;
			}
		}
	}

	if ( cons_task_residues > 0 ) {
		if ( has_native_sequence( pose ) ) {
			TR << "Using original bb sequence for conservative design." << std::endl;
			std::string native_seq = get_native_sequence( pose, *ab_info_ );
			cons_task_->set_native_sequence( native_seq );
		} else {
			cons_task_->use_pose_sequence_as_native( pose );
		}

		cons_task_->apply(pose, task);

	}



	/// Disable design if seq design is set as none.
	RestrictResidueToRepacking restrict = RestrictResidueToRepacking();
	core::Size restrict_residues = 0;
	//TR << "No data cdrs: " << utility::to_string(no_data_cdrs) << std::endl;
	for ( auto const & cdr : local_ab_info->get_all_cdrs_present() ) {
		if ( (no_data_cdrs[ cdr ] && seq_design_options_[ cdr ]->fallback_strategy() == seq_design_none) || (seq_design_options_[ cdr ]->design() && seq_design_options_[ cdr ]->design_strategy() == seq_design_none) ) {
			TR << "Disabling design for " << local_ab_info->get_CDR_name(cdr) << std::endl;

			for ( core::Size resnum = local_ab_info->get_CDR_start(cdr, pose, North); resnum <= local_ab_info->get_CDR_end(cdr, pose, North); ++resnum ) {
				restrict.include_residue( resnum );
				restrict_residues += 1;
			}
		}

	}
	if ( restrict_residues > 0 ) {
		restrict.apply(pose, task);
	}
}

core::pack::task::operation::TaskOperationOP
AddCDRProfilesOperationCreator::create_task_operation() const
{
	return utility::pointer::make_shared< AddCDRProfilesOperation >();
}

void AddCDRProfilesOperationCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	AddCDRProfilesOperation::provide_xml_schema( xsd );
}

std::string AddCDRProfilesOperationCreator::keyname() const {
	return AddCDRProfilesOperation::keyname();
}

} //task_operations
} //antibody
} //protocols











