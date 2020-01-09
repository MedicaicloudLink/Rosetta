// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available
// (c) under license. The Rosetta software is developed by the contributing
// (c) members of the Rosetta Commons. For more information, see
// (c) http://www.rosettacommons.org. Questions about this can be addressed to
// (c) University of Washington UW TechTransfer,email:license@u.washington.edu.

/// @file core.pose.carbohydrates.GlycanTreeSet.cc
/// @brief Class to store info on all glycan trees of a pose
/// @author Jared Adolf-Bryfogle (jadolfbr@gmail.com)
/// @author Sebastian Rämisch (raemisch@scripps.edu)

// Unit headers
#include <core/conformation/carbohydrates/GlycanTreeSet.hh>

// Package headers
#include <core/conformation/carbohydrates/util.hh>
#include <core/conformation/carbohydrates/GlycanTree.hh>
#include <core/conformation/carbohydrates/GlycanNode.hh>

#include <core/conformation/signals/LengthEvent.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/Conformation.hh>
#include <core/id/SequenceMapping.hh>

// Utility headers
#include <utility/vector1.hh>
#include <utility/vector1.functions.hh>

// Basic headers
#include <basic/Tracer.hh>
#include <map>

#ifdef    SERIALIZATION
// Utility serialization headers
#include <utility/serialization/serialization.hh>
#include <utility/vector1.srlz.hh>
#include <utility/string_util.hh>

// Numeric serialization headers
#include <numeric/xyz.serialization.hh>

// Cereal headers
#include <cereal/access.hpp>
#include <cereal/types/base_class.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/map.hpp>

#endif // SERIALIZATION

static basic::Tracer TR( "core.conformation.carbohydrates.GlycanTreeSet" );

namespace core {
namespace conformation {
namespace carbohydrates {


GlycanTreeSet::GlycanTreeSet():
	utility::pointer::ReferenceCount()
{

}

/// @brief Standard constructor
GlycanTreeSet::GlycanTreeSet( conformation::Conformation const & conf ): utility::pointer::ReferenceCount()
{
	setup_glycan_trees( conf );
}


GlycanTreeSet::GlycanTreeSet( GlycanTreeSet const & src ):
	utility::pointer::ReferenceCount()
{
	glycan_tree_set_.clear();
	for ( auto const & kv: src.glycan_tree_set_ ) {
		GlycanTreeOP GT = utility::pointer::make_shared< GlycanTree >( *kv.second);
		glycan_tree_set_[ kv.first] = GT;
		//Populate res to tree map.
		for ( core::Size res : GT->get_residues() ) {
			glycan_res_to_tree_[res] = GT;
		}
	}
}

GlycanTreeSetOP
GlycanTreeSet::clone() const {
	return utility::pointer::make_shared< GlycanTreeSet >( *this);
}


Size
GlycanTreeSet::n_trees() const {
	return glycan_tree_set_.size();
}

Size
GlycanTreeSet::get_size() const {
	return n_trees();
}

Size
GlycanTreeSet::size() const {
	return n_trees();
}

Size
GlycanTreeSet::get_largest_glycan_tree_length() const {
	utility::vector1< core::Size > tree_sizes;
	for ( auto const & start_tree : glycan_tree_set_ ) {
		tree_sizes.push_back( start_tree.second->get_size() );

	}
	return utility::max( tree_sizes );
}

Size
GlycanTreeSet::get_largest_glycan_tree_layer() const {
	utility::vector1< core::Size > layer_sizes;
	for ( auto const & start_tree : glycan_tree_set_ ) {
		for ( core::Size const & node_residue : start_tree.second->get_residues() ) {
			layer_sizes.push_back( start_tree.second->get_node( node_residue )->get_distance_to_start() );
		}
	}
	return utility::max( layer_sizes );
}

///@brief Get the largest glycan tree layer (from 0 to N). of the glycan tree starts passed in.
core::Size
GlycanTreeSet::get_largest_glycan_tree_layer( utility::vector1< bool > const & subset) const {
	utility::vector1< core::Size > layer_sizes;
	for ( core::Size node_residue = 1; node_residue <= subset.size(); ++node_residue  ) {
		if ( subset[ node_residue ] ) {
			layer_sizes.push_back( get_node( node_residue )->get_distance_to_start() );
		}
	}
	return utility::max( layer_sizes );
}

Size
GlycanTreeSet::get_smallest_glycan_tree_layer() const {
	utility::vector1< core::Size > layer_sizes;
	for ( auto const & start_tree : glycan_tree_set_ ) {
		for ( core::Size const & node_residue : start_tree.second->get_residues() ) {
			layer_sizes.push_back( start_tree.second->get_node( node_residue )->get_distance_to_start() );
		}
	}
	return utility::min( layer_sizes );
}

///@brief Get the largest glycan tree layer (from 0 to N). of the glycan tree starts passed in.
core::Size
GlycanTreeSet::get_smallest_glycan_tree_layer( utility::vector1< bool > const & subset) const {
	utility::vector1< core::Size > layer_sizes;
	for ( core::Size node_residue = 1; node_residue <= subset.size(); ++node_residue  ) {
		if ( subset[ node_residue ] ) {
			layer_sizes.push_back( get_node( node_residue )->get_distance_to_start() );
		}
	}
	return utility::min( layer_sizes );
}

bool
GlycanTreeSet::has_tree(const core::Size glycan_start_position) const {
	bool result = glycan_tree_set_.count(glycan_start_position) != 0;
	return result;
}

/// @author Labonte <JWLabonte@jhu.edu>
bool
GlycanTreeSet::is_residue_in_tree( const core::uint glycan_residue ) const
{
	return glycan_res_to_tree_.count( glycan_residue ) != 0;
}

GlycanTreeCOP
GlycanTreeSet::get_tree(const core::Size glycan_start_position) const {
	if ( has_tree( glycan_start_position) ) {
		return glycan_tree_set_.at( glycan_start_position );
	} else {
		TR << "Total Trees: " << glycan_tree_set_.size() << std::endl;
		utility_exit_with_status("Could not find GlycanTree at position "+utility::to_string(glycan_start_position));
	}
}

std::map<Size, GlycanTreeOP> const &
GlycanTreeSet::get_tree_map() const {
	return glycan_tree_set_;
}

utility::vector1< GlycanTreeCOP > const
GlycanTreeSet::get_all_trees() const {
	utility::vector1< GlycanTreeCOP > trees;
	for ( auto const & key_value : glycan_tree_set_ ) {
		trees.push_back(key_value.second);
	}
	return trees;
}

utility::vector1< Size >
GlycanTreeSet::get_start_points() const {
	utility::vector1 <Size > start_points;
	for ( auto const & kv : glycan_tree_set_ ) {
		start_points.push_back(kv.first);
	}
	return start_points;
}

GlycanTreeCOP
GlycanTreeSet::get_tree_containing_residue(const core::Size glycan_residue) const {
	return glycan_res_to_tree_.at(glycan_residue);
}

bool
GlycanTreeSet::has_node( core::Size glycan_residue ) const {
	bool result = glycan_res_to_tree_.count(glycan_residue) != 0;
	return result;
}

GlycanNodeCOP
GlycanTreeSet::get_node(const core::Size glycan_residue) const {
	if ( has_node( glycan_residue ) ) {
		return glycan_res_to_tree_.at(glycan_residue)->get_node(glycan_residue);
	} else {
		TR << "Total Trees: " << glycan_tree_set_.size() << std::endl << "Total ResToTrees: "<< glycan_res_to_tree_.size() << std::endl;
		TR << "Writing debugging output" << std::endl;
		for ( auto pair : glycan_tree_set_ ) {
			TR << "TreeStart: " << pair.first << std::endl;
			utility::vector1< core::Size > nodes = pair.second->get_residues();
			TR << " Nodes: " << utility::to_string( nodes ) << std::endl;
			for ( core::Size node_res : nodes ) {
				TR << "Parent " << get_parent( node_res ) << std::endl;

			}
		}
		utility_exit_with_message("Could not find Node at " + utility::to_string( glycan_residue ));
	}
}

core::Size
GlycanTreeSet::get_parent(const core::Size glycan_residue) const {
	return get_node(glycan_residue)->get_parent();
}

core::uint
GlycanTreeSet::get_mainchain_child( const core::Size glycan_residue ) const
{
	return get_node( glycan_residue )->get_mainchain_child();
}

core::Size
GlycanTreeSet::get_distance_to_start(const core::Size glycan_residue ) const {
	return get_node(glycan_residue)->get_distance_to_start();
}

core::uint
GlycanTreeSet::get_linkage_position(  const core::Size resnum ) const {
	return get_node( resnum )->get_linkage_position();
}

core::Size
GlycanTreeSet::get_tree_start_of_glycan_residue(core::Size resnum) const {
	return glycan_res_to_tree_.at(resnum)->get_start();
}

core::Size
GlycanTreeSet::get_tree_root_of_glycan_residue(core::Size resnum) const {
	return glycan_res_to_tree_.at(resnum)->get_root();
}

bool
GlycanTreeSet::has_exocyclic_glycosidic_linkage(core::Size resnum) const {
	return get_node( resnum )->has_exocyclic_linkage();
}

GlycanTreeSet::~GlycanTreeSet()= default;

//GlycanTreeSet::GlycanTreeSet( GlycanTreeSet const & ) {

//}



void
GlycanTreeSet::setup_glycan_trees( conformation::Conformation const & conf )
{
	glycan_tree_set_.clear();
	// find the first residue of all glycans and use them to populate the set.

	TR << "Setting up Glycan Trees" << std::endl;
	utility::vector1< bool > const start_points = conformation::carbohydrates::get_glycan_start_points( conf );

	for ( core::Size i = 1; i <= conf.size(); ++i ) {
		if ( start_points[ i ] ) {
			GlycanTreeOP GT = utility::pointer::make_shared< GlycanTree >( conf, i );
			glycan_tree_set_[ i ] = GT;

			//Populate res to tree map.
			for ( core::Size res : GT->get_residues() ) {
				glycan_res_to_tree_[ res ] = GT;
			}
		}
	}
	TR << "Found " << glycan_tree_set_.size() << " glycan trees." << std::endl;
}


void
GlycanTreeSet::on_length_change( core::conformation::signals::LengthEvent const & event ){

	using namespace core::conformation::signals;

	//Easy way - we invalidate this, and then call setup when we get it from the pose.
	//Significant pose-editing will make this extremely slow, which is why we do this.  So we can do Enzymatic Movers and not be screwed.

	//ALL this does is update the Sets list of trees and the indexing.
	// The data within those trees will update when we need it, and the tree is responsible for this.

	//If that residue has been deleted, we must update accordingly!
	//  Parents have to be recalculated
	//  That residue must not be in the tree

	std::map< Size, GlycanTreeOP > new_trees; // Need this due to residue number updates as these have correct start positions!
	//TR << "On Length Change starting points " << get_start_points() << std::endl;

	if ( event.tag == LengthEvent::RESIDUE_DELETE ) {


		/// If the residue is the first residue of the glycan, we need to update here:
		if ( glycan_tree_set_.count( event.position ) != 0 ) {

			GlycanTreeOP tree = glycan_tree_set_[ event.position ];
			GlycanNodeCOP node = tree->get_node( event.position );

			utility::vector1< core::Size > children = node->get_children();

			glycan_tree_set_.erase( event.position );


			///This glycan is not a single residue glycan.  Thats good.  We need to update it accordingly now.

			//Need to find out what the new glycan start is.

			// 1) Old residue was a branch point, and so (at least) two new glycan trees need to be created.
			// HOWEVER - we have NO POSE!
			if ( children.size()  > 1 ) {

				for ( core::Size child : children ) {
					GlycanTreeOP new_tree = utility::pointer::make_shared< GlycanTree >( *event.conformation, child );
					new_trees[ child] = new_tree ;

				}
			} else if ( children.size() == 1 ) {
				// 2) Old residue was not a branch point, and so we can assume the next one up is N+1.
				glycan_tree_set_.erase( tree->get_start());
				tree->update_start_position( node->get_mainchain_child() );
				new_trees[ node->get_mainchain_child() ]  = tree ;
			}
			// No Children.  The residue is the only member of the tree.  We don't do anything here.  We have already erased it from the tree set.

		} else if ( glycan_res_to_tree_.count( event.position) != 0 ) {
			///Here the deletion is a glycan residue.  This may split our tree or not.
			GlycanTreeOP tree = glycan_res_to_tree_[ event.position ];
			GlycanNodeCOP node = tree->get_node( event.position );

			utility::vector1< core::Size > children = node->get_children();
			//Size n_glycan_residues = tree->get_tree_size();


			if ( children.size() > 0 ) {
				//Add the tree before
				//The current tree (before the deletion), will be updated accordingly later.
				//Nodes will be deleted, etc.  So, we only need to make new trees here.

				//Make new subtrees from the new children.
				for ( Size child : children ) {
					GlycanTreeOP new_child_tree = utility::pointer::make_shared< GlycanTree >( *event.conformation, child );
					new_trees[ child ] = new_child_tree;
				}

			} else {
				new_trees[ tree->get_start()] = tree;
			}

		}
	} else if ( event.tag == LengthEvent::RESIDUE_PREPEND || event.tag == LengthEvent::RESIDUE_APPEND ) {
		//We can access the Residue from the event.

		//If it is a glycan residue, we need to update our trees.
		core::Size new_position = event.residue->seqpos();

		if ( event.residue->is_carbohydrate() ) {
			Size child = find_seqpos_of_saccharides_mainchain_child( *event.residue );
			Size parent = find_seqpos_of_saccharides_parent_residue( *event.residue );
			if ( child == 0 &&  parent == 0 ) {
				//Single-residue tree.
				GlycanTreeOP new_tree = utility::pointer::make_shared< GlycanTree >( *event.conformation, new_position );
				new_trees[ new_position ] = new_tree;
			} else if ( child == 0 && ! event.conformation->residue(parent).is_carbohydrate() ) {
				GlycanTreeOP new_tree = utility::pointer::make_shared< GlycanTree >( *event.conformation, new_position );
				new_trees[ new_position ] = new_tree;
			} else if ( parent == 0 ) {
				///Beginning of tree, not connected to protein.
				GlycanTreeOP old_tree = glycan_res_to_tree_[ child ];
				glycan_tree_set_.erase( old_tree->get_start() );

				old_tree->update_start_position( new_position );
				new_trees[ event.position] = old_tree ;
			}
		}

	} else if ( event.tag == LengthEvent::INVALIDATE ) {
		//don't know what the best behaviour is in this case
		//probably nothing, because pose destruction is imminent
		return;
	} else {
		TR << "Event not understood!" << std::endl;
		return;
	}

	//Update Residue Numbers for starting trees.  We will update the full residue numbers later
	core::id::SequenceMapping smap( event );
	for ( Size old_resnum : this->get_start_points() ) {
		Size new_start_pos = smap.get_corresponding_residue_in_current( old_resnum );

		GlycanTreeOP old_tree = glycan_tree_set_[ old_resnum ];
		old_tree->update_start_position( new_start_pos );

		new_trees[ new_start_pos ] = old_tree;
	}

	//TR << "Final start points " << utility::to_string( this->get_start_points() ) << std::endl;


	//We don't update the nodes here.  The tree updates those while updating the connectivity.
	// This is partly because there may be deleted nodes
	glycan_tree_set_ = new_trees;

	glycan_res_to_tree_.clear();
	for ( auto & root_tree : glycan_tree_set_ ) {

		GlycanTreeOP GT = root_tree.second;

		//We have a new residue at a new tree.  Skip any updating as it is already up-to-data.
		if ( ( event.tag != LengthEvent::RESIDUE_DELETE ) && event.residue->seqpos() == root_tree.first ) {
			continue;
		} else {
			GT->update_on_length_change( event );
		}
	}

	//Now that the trees are updated, update our res to tree mapping.
	for ( auto & root_tree : glycan_tree_set_ ) {
		for ( core::Size res : root_tree.second->get_residues() ) {
			glycan_res_to_tree_[res] = root_tree.second;
		}
	}
}


} //core
} //chemical
} //carbohydrates


#ifdef    SERIALIZATION

/// @brief Automatically generated serialization method
template< class Archive >
void
core::conformation::carbohydrates::GlycanTreeSet::save( Archive & arc ) const {
	arc( CEREAL_NVP( glycan_tree_set_ ) ); //std::map< core::Size, GlycanTreeOP >
	arc( CEREAL_NVP( glycan_res_to_tree_ ) ); //std::map< core::Size, GlycanTreeOP >
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::conformation::carbohydrates::GlycanTreeSet::load( Archive & arc ) {
	arc( glycan_tree_set_ ); //std::map< core::Size, GlycanTreeOP >
	arc( glycan_res_to_tree_ ); //std::map< core::Size, GlycanTreeOP >
}

SAVE_AND_LOAD_SERIALIZABLE( core::conformation::carbohydrates::GlycanTreeSet );
CEREAL_REGISTER_TYPE( core::conformation::carbohydrates::GlycanTreeSet )

CEREAL_REGISTER_DYNAMIC_INIT( core_conformation_carbohydrates_GlycanTreeSet )
#endif // SERIALIZATION



