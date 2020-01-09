// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @brief This mover is based on the Steven Lewis' protocols/floppy_tail/TailSegmentMover.cc. some addaptations were made to
/// fit it to the Splice app
/// @author Gideon Lapidoth

// Unit Headers
#include <protocols/splice/TailSegmentMover.hh>
#include <protocols/splice/TailSegmentMoverCreator.hh>
// Project Headers
#include <core/pose/Pose.hh>
#include <core/pose/PDBPoseMap.hh>
#include <core/pose/PDBInfo.hh>

#include <core/conformation/Conformation.hh>

#include <core/chemical/ChemicalManager.fwd.hh>

#include <core/kinematics/MoveMap.hh>
#include <core/kinematics/FoldTree.hh>
#include <core/kinematics/util.hh>

#include <core/fragment/ConstantLengthFragSet.hh>

#include <core/pack/task/TaskFactory.hh>
#include <core/pack/task/operation/TaskOperations.hh>

#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/ScoreFunction.hh>

#include <core/scoring/constraints/util.hh>

#include <protocols/moves/MonteCarlo.hh>

//movers
#include <protocols/simple_moves/BackboneMover.hh> //SmallMover
#include <protocols/simple_moves/FragmentMover.hh>
#include <protocols/minimization_packing/MinMover.hh>
#include <protocols/moves/MoverContainer.hh> //Sequence Mover
#include <protocols/minimization_packing/PackRotamersMover.hh>
#include <protocols/minimization_packing/RotamerTrialsMover.hh>
#include <protocols/simple_moves/SwitchResidueTypeSetMover.hh> //typeset swapping
#include <protocols/simple_moves/ReturnSidechainMover.hh>
#include <protocols/minimization_packing/TaskAwareMinMover.hh>
#include <protocols/moves/OutputMovers.hh> //pdbdumpmover
#include <protocols/task_operations/DesignAroundOperation.hh>
#include <basic/options/keys/TailSegment.OptionKeys.gen.hh>


//calculators and neighbor detection machinery
#include <protocols/pose_metric_calculators/InterGroupNeighborsCalculator.hh>
#include <core/pose/metrics/CalculatorFactory.hh>
#include <protocols/task_operations/RestrictByCalculatorsOperation.hh>

// Utility Headers
#include <basic/options/option.hh>
#include <basic/Tracer.hh>
#include <utility/exit.hh>

#include <utility/vector0.hh>
#include <utility/vector1.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <protocols/moves/mover_schemas.hh>
#include <protocols/rosetta_scripts/util.hh>


using basic::Error;
using basic::Warning;

static basic::Tracer TR( "protocols.splice.TailSegmentMover" );

namespace protocols {
namespace splice {

std::string TailSegmentMoverCreator::keyname() const {
	return TailSegmentMoverCreator::mover_name();
}

protocols::moves::MoverOP TailSegmentMoverCreator::create_mover() const {
	return utility::pointer::make_shared< TailSegmentMover >();
}

std::string TailSegmentMoverCreator::mover_name() {
	return "TailSegmentMover";
}



TailSegmentMover::TailSegmentMover(): protocols::moves::Mover("TailSegment"),
	start_(0),
	stop_(0),
	fullatom_scorefunction_(/* NULL */),
	task_factory_(/* NULL */),
	movemap_(/* NULL */),
	movemap_lesstail_(/* NULL */),
	foldtree_(/* NULL */),
	temp_initial_(1.5),
	temp_final_(0.5)
{
	protocols::moves::Mover::type( "TailSegment" );

}

TailSegmentMover::~TailSegmentMover() = default;

/// @brief copy ctor
TailSegmentMover::TailSegmentMover( TailSegmentMover const & rhs ) :
	Mover(rhs)
{
	*this = rhs;
}

TailSegmentMover & TailSegmentMover::operator=( TailSegmentMover const & rhs ){

	//abort self-assignment
	if ( this == &rhs ) return *this;

	start_         = rhs.start_;
	stop_          = rhs.stop_;
	init_for_input_yet_   = rhs.init_for_input_yet_;
	centroid_scorefunction_ = rhs.centroid_scorefunction_->clone();
	fullatom_scorefunction_ = rhs.fullatom_scorefunction_->clone();
	task_factory_      = rhs.task_factory_->clone();
	movemap_        = rhs.movemap_->clone();
	movemap_lesstail_    = rhs.movemap_lesstail_->clone();
	foldtree_ = utility::pointer::make_shared< core::kinematics::FoldTree >(*rhs.foldtree_); //no clone operation, and no proper copy ctor
	return *this;
}

void TailSegmentMover::set_movemap(core::kinematics::MoveMapOP const movemap){
	movemap_ = movemap->clone();
}

void TailSegmentMover::set_fa_scorefxn(core::scoring::ScoreFunctionOP const fa_scorefxn){
	fullatom_scorefunction_=fa_scorefxn->clone();
}

void TailSegmentMover::set_task_factory(
	core::pack::task::TaskFactoryCOP task_factory_in
)
{
	// make local, non-const copy from const input
	runtime_assert( task_factory_in != nullptr );
	task_factory_ = utility::pointer::make_shared< core::pack::task::TaskFactory >( *task_factory_in );
}

void TailSegmentMover::apply( core::pose::Pose & pose ){
	//First setup movemap, this should be already set, if not then exit with error msg
	fullatom_scorefunction_->show(pose); //test scorefunction to make sure all constraints are in place. remove after
	if ( !movemap_ ) {
		utility_exit_with_message( "Movemap not defined. Can't apply TailSegmentMover\n");

	}
	TR<<"TSM fold tree::"<<pose.fold_tree()<<std::endl;


	/////////////////////////////generate full repack&minimize mover//////////////////////////////
	protocols::minimization_packing::PackRotamersMoverOP pack_mover( new protocols::minimization_packing::PackRotamersMover );
	pack_mover->task_factory( task_factory_ );
	pack_mover->score_function( fullatom_scorefunction_ );
	utility::vector1<core::Size> designable_residues = protocols::rosetta_scripts::residue_packer_states(pose, task_factory_, true, false);
	TR << "Residues Allowed to Design: "<< std::endl;
	for ( utility::vector1<core::Size>::const_iterator i(designable_residues.begin());
			i != designable_residues.end(); ++i ) {
		TR << *i << "+";
	}
	TR << std::endl;
	protocols::minimization_packing::MinMoverOP min_mover_fa( new protocols::minimization_packing::MinMover(
		movemap_,
		fullatom_scorefunction_,
		"lbfgs_armijo_nonmonotone",
		0.01,
		true /*use_nblist*/ ) );

	using protocols::minimization_packing::TaskAwareMinMoverOP;
	using protocols::minimization_packing::TaskAwareMinMover;
	//getting the start and end tail residues from the movemap
	core::Size const nres(pose.size());
	for ( core::Size i(1); i<=nres; ++i ) {
		if ( movemap_->get_bb(i) ) {
			start_ = i;
			break;
		}
	}
	for ( core::Size i(nres); i>=1; --i ) {
		if ( movemap_->get_bb(i) ) {
			stop_ = i;
			break;
		}
	}
	using namespace protocols::task_operations;
	TR<<"The start and and residue of the tail segment are:"<<start_<<"-"<<stop_<<std::endl;
	core::pack::task::TaskFactoryOP task_factory_min( new core::pack::task::TaskFactory ); //set specific task factory for minimization
	DesignAroundOperationOP dao( new DesignAroundOperation );
	dao->design_shell(0.001); // threaded sequence operation needs to design, and will restrict design to the loop, unless design_task_factory is defined, in which case a larger shell can be defined
	dao->repack_shell(6);
	for ( core::Size i = start_; i <=stop_; ++i ) {
		dao->include_residue(i);

	} //for
	task_factory_min->push_back(dao);
	protocols::minimization_packing::TaskAwareMinMoverOP TAmin_mover_fa( new protocols::minimization_packing::TaskAwareMinMover(min_mover_fa, task_factory_min) );

	/////////////////////////repack/minimize once to fix sidechains//////////////////////////////////
	// TR << "packing" << std::endl;
	pack_mover->apply(pose);
	// TR << "minimizing" << std::endl;
	TAmin_mover_fa->apply(pose);

	//////////////////////////////////////// backbone mover/////////////////////////////////////////
	protocols::moves::RandomMoverOP backbone_mover_fa( new protocols::moves::RandomMover() );

	protocols::simple_moves::BackboneMoverOP small_mover_fa( new protocols::simple_moves::SmallMover(movemap_, 0.8, 0) );
	small_mover_fa->angle_max( 'H', 4.0 );
	small_mover_fa->angle_max( 'E', 4.0 );
	small_mover_fa->angle_max( 'L', 4.0 );

	protocols::simple_moves::BackboneMoverOP shear_mover_fa( new protocols::simple_moves::ShearMover(movemap_, 0.8, 0) );
	shear_mover_fa->angle_max( 'H', 4.0 );
	shear_mover_fa->angle_max( 'E', 4.0 );
	shear_mover_fa->angle_max( 'L', 4.0 );

	backbone_mover_fa->add_mover(small_mover_fa, 1.0);
	backbone_mover_fa->add_mover(shear_mover_fa, 1.0);

	/////////////////fullatom Monte Carlo//////////////////////////////////////////////////////////
	//make the monte carlo object
	protocols::moves::MonteCarloOP mc_fa( new protocols::moves::MonteCarlo( pose, *fullatom_scorefunction_, temp_initial_ ) );

	/////////////////////////////////rotamer trials mover///////////////////////////////////////////
	using protocols::minimization_packing::RotamerTrialsMoverOP;
	using protocols::minimization_packing::EnergyCutRotamerTrialsMover;
	protocols::minimization_packing::RotamerTrialsMoverOP rt_mover( new protocols::minimization_packing::EnergyCutRotamerTrialsMover(
		fullatom_scorefunction_,
		task_factory_,
		mc_fa,
		0.01 /*energycut*/ ) );

	/////////////////////////////////////////refine loop///////////////////////////////////////////
	using namespace basic::options;
	core::Size const refine_applies = option[ basic::options::OptionKeys::TailSegment::refine_cycles ].value();//dflt set to 100
	core::Size const repack_cycles = option[ basic::options::OptionKeys::TailSegment::refine_repack_cycles ].value();;//dflt set to 10
	core::Size const min_cycles = repack_cycles/2;
	TR << "   Current     Low    total cycles =" << refine_applies << std::endl;
	small_mover_fa->movemap(movemap_);
	shear_mover_fa->movemap(movemap_);
	for ( core::Size i(1); i <= refine_applies; ++i ) {
		//TR<<"TSM fold tree::"<<pose.fold_tree()<<std::endl;
		if ( (i % repack_cycles == 0) || (i == refine_applies) ) { //full repack
			TR<<"Doing Repacking"<<std::endl;
			pack_mover->apply(pose);
			TAmin_mover_fa->apply(pose);
		} else if ( i % min_cycles == 0 ) { //minimize
			TAmin_mover_fa->apply(pose);
		} else {
			backbone_mover_fa->apply(pose);
			rt_mover->apply(pose);
		}

		mc_fa->boltzmann(pose);
		TR << i << "  " << mc_fa->last_accepted_score() << "  " << mc_fa->lowest_score() << std::endl;
	}//end the exciting for loop
	mc_fa->recover_low( pose );

	//let's store some energies/etc of interest
	//this code is specific to the E2/RING/E3 system for which this code was written; it is refactored elsewhere
	// BARAK: this line should be commented out if applied to other systems
	//SML 2/1/11: it's now under commandline control
	(*fullatom_scorefunction_)(pose);
	set_last_move_status(protocols::moves::MS_SUCCESS); //this call is unnecessary but let's be safe
	return;
}
void
TailSegmentMover::parse_my_tag( TagCOP const tag, basic::datacache::DataMap &data, protocols::filters::Filters_map const &, protocols::moves::Movers_map const &, core::pose::Pose const & )
{
	fullatom_scorefunction(protocols::rosetta_scripts::parse_score_function(tag, data));
	set_task_factory(protocols::rosetta_scripts::parse_task_operations(tag, data));
}

std::string
TailSegmentMover::mover_name()  {
	return "TailSegmentMover";
}
void TailSegmentMover::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{

	using namespace utility::tag;

	AttributeList attlist;
	protocols::rosetta_scripts::attributes_for_parse_score_function( attlist );
	protocols::rosetta_scripts::attributes_for_parse_task_operations( attlist );
	protocols::moves::xsd_type_definition_w_attributes( xsd, mover_name(), "change conformation of ", attlist );

}

void TailSegmentMoverCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	TailSegmentMover::provide_xml_schema( xsd );
}


} //splice
} //protocols
