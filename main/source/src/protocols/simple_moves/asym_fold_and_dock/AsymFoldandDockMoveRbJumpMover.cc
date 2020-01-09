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
/// @author Ingemar Andre

// Unit headers
#include <protocols/simple_moves/asym_fold_and_dock/AsymFoldandDockMoveRbJumpMover.hh>
#include <core/pose/Pose.hh>
#include <core/kinematics/FoldTree.hh>
#include <core/conformation/Conformation.hh>
#include <core/conformation/Residue.hh>
#include <ObjexxFCL/FArray2D.hh>

// Package headers

// options

// ObjexxFCL Headers

// C++ Headers

// Utility Headers
#include <basic/Tracer.hh>
#include <utility/exit.hh>
#include <numeric/random/random.hh>

//Auto Headers
#include <utility/vector1.hh>
//Auto Headers
#include <basic/options/keys/fold_and_dock.OptionKeys.gen.hh>
#include <basic/options/option.hh>
// XSD XRW Includes
#include <utility/tag/XMLSchemaGeneration.hh>
#include <protocols/moves/mover_schemas.hh>
#include <utility/tag/Tag.hh>
#include <protocols/simple_moves/asym_fold_and_dock/AsymFoldandDockMoveRbJumpMoverCreator.hh>

namespace protocols {
namespace simple_moves {
namespace asym_fold_and_dock {

static basic::Tracer TR( "protocols.simple_moves.asym_fold_and_dock.AsymFoldandDockMoveRbJumpMover" );

AsymFoldandDockMoveRbJumpMover::AsymFoldandDockMoveRbJumpMover() : moves::Mover( "AsymFoldandDockMoveRbJumpMover" ),
	chain_start_( 0 )
{
}

AsymFoldandDockMoveRbJumpMover::AsymFoldandDockMoveRbJumpMover( core::Size chain_start )
: protocols::moves::Mover("AsymFoldandDockMoveRbJumpMover"), chain_start_( chain_start )
{}

void
AsymFoldandDockMoveRbJumpMover::apply( core::pose::Pose & pose )
{
	find_new_jump_residue( pose );
}

void
AsymFoldandDockMoveRbJumpMover::find_new_jump_residue( core::pose::Pose & pose )
{
	using namespace core::kinematics;

	//core::Size nres ( pose.size() );
	//if ( pose.conformation().is_membrane() ) nres--;
	core::Size nres ( 0 );
	for ( core::Size i=1; i <= pose.size(); i++ ) {
		if ( pose.residue( i ).is_protein() ) ++nres;
	}
	core::Size nres_flexible_segment( nres - chain_start_ );
	// Does the chain start at a reasonable place in the sequence?
	runtime_assert( chain_start_ > 1 && chain_start_ <= nres );

	// find the anchor
	FoldTree f( pose.fold_tree() );
	Size anchor_start(0); //
	int jump_number(1);
	Size residue_that_builds_anchor(0);
	for ( Size i=1; i<= f.num_jump(); ++i ) {
		Size res( f.downstream_jump_residue( i ) );
		Size res_start( f.upstream_jump_residue( i ) );
		if ( res >= chain_start_ ) {
			anchor_start = res;
			residue_that_builds_anchor = res_start;
			jump_number = i;
		}
	}
	runtime_assert(anchor_start > 0 && residue_that_builds_anchor > 0 );

	TR << "The anchor residues are at residue " << residue_that_builds_anchor  << " : " << anchor_start << std::endl;

	//Looking in central half of each segment -- pick a random point.
	Size anchor_chain1 = static_cast<Size>( numeric::random::rg().uniform() * (chain_start_) ) + 1;
	Size anchor_chain2 = static_cast<Size>( numeric::random::rg().uniform() * (nres_flexible_segment) ) +
		chain_start_ + 1;

	if ( basic::options::option[ basic::options::OptionKeys::fold_and_dock::set_anchor_at_closest_point ] ) {
		//Find Closest point of contact to the anchor of in the non-moving subunit. Really silly, should be the real minimal distance!!!
		core::Real mindist = pose.residue(residue_that_builds_anchor).xyz("CEN").distance( pose.residue(anchor_chain2).xyz("CEN") );
		for ( Size i = chain_start_; i <= nres; i++ ) {
			core::Real dist = pose.residue(residue_that_builds_anchor).xyz("CEN").distance( pose.residue(i).xyz("CEN") );
			if ( dist < mindist ) {
				mindist = dist;
				anchor_chain2 = i;
			}
		}
	}


	TR << "The anchor residues are moved to residue " << anchor_chain1  << " : " << anchor_chain2 << std::endl;
	// Setyp the lists of jumps and cuts
	Size num_jumps( f.num_jump() );
	Size num_cuts( f.num_cutpoint() );

	utility::vector1< Size > cuts_vector( f.cutpoints() );
	ObjexxFCL::FArray1D< Size > cuts( num_cuts );
	ObjexxFCL::FArray2D< Size > jumps( 2, num_jumps );

	// Initialize jumps
	for ( Size i = 1; i<= num_jumps; ++i ) {
		int down ( f.downstream_jump_residue(i) );
		int up ( f.upstream_jump_residue(i) );
		if ( down < up ) {
			jumps(1,i) = down;
			jumps(2,i) = up;
		} else {
			jumps(1,i) = up;
			jumps(2,i) = down;
		}
	}

	for ( Size i = 1; i<= num_cuts; ++i ) {
		cuts(i) = cuts_vector[i];
	}

	int root ( f.root() );

	jumps(1, jump_number ) = anchor_chain1;
	jumps(2, jump_number ) = anchor_chain2;

	/* debug
	std::cout<<"cuts ";
	for ( Size i = 1; i<= num_cuts; ++i ) {
	std::cout<< cuts(i) << ' ';
	}
	std::cout<<std::endl;
	std::cout<<"jumps ";
	for ( Size i = 1; i<= num_jumps; ++i ) {
	std::cout<< " ( "<<jumps(1,i) << " , " << jumps(2,i) << " ) ";
	}
	std::cout<<std::endl; */
	f.tree_from_jumps_and_cuts( pose.conformation().size(), num_jumps, jumps, cuts );
	f.reorder( root );
	pose.conformation().fold_tree( f );

}

moves::MoverOP
AsymFoldandDockMoveRbJumpMover::clone() const
{
	return utility::pointer::make_shared< AsymFoldandDockMoveRbJumpMover >( *this );
}

moves::MoverOP
AsymFoldandDockMoveRbJumpMover::fresh_instance() const
{
	return utility::pointer::make_shared< AsymFoldandDockMoveRbJumpMover >();
}

void
AsymFoldandDockMoveRbJumpMover::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap &,
	protocols::filters::Filters_map const &,
	protocols::moves::Movers_map const &,
	core::pose::Pose const &
)
{
	chain_start_ = tag->getOption< core::Size >( "chain_start", 0 ) ;
	runtime_assert_string_msg( chain_start_ > 0, "chain start must be provided" );
}


std::string
AsymFoldandDockMoveRbJumpMover::get_name() const {
	return mover_name();
}

std::string
AsymFoldandDockMoveRbJumpMover::mover_name()
{
	return "AsymFoldandDockMoveRbJumpMover";
}

void AsymFoldandDockMoveRbJumpMover::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;
	AttributeList attlist;

	attlist
		+ XMLSchemaAttribute::attribute_w_default( "chain_start", xsct_non_negative_integer, "at what residue doe the chain to which the fragment belongs starts?", "0" );

	protocols::moves::xsd_type_definition_w_attributes( xsd, mover_name(), "a symmetric fold and dock", attlist  );
}

std::string AsymFoldandDockMoveRbJumpMoverCreator::keyname() const {
	return AsymFoldandDockMoveRbJumpMover::mover_name();
}

protocols::moves::MoverOP
AsymFoldandDockMoveRbJumpMoverCreator::create_mover() const {
	return utility::pointer::make_shared< AsymFoldandDockMoveRbJumpMover >();
}

void AsymFoldandDockMoveRbJumpMoverCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	AsymFoldandDockMoveRbJumpMover::provide_xml_schema( xsd );
}

}
} // moves
} // protocols
