// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/task_operations/FindEndpointsOperation.hh
/// @brief  TaskOperation class that restricts a chain to repacking
/// @author Sarel Fleishman sarelf@uw.edu

#ifndef INCLUDED_protocols_splice_FindEndpointsOperation_hh
#define INCLUDED_protocols_splice_FindEndpointsOperation_hh

// Unit Headers
#include <protocols/splice/FindEndpointsOperation.fwd.hh>
#include <protocols/task_operations/RestrictOperationsBase.hh>

// Project Headers
#include <core/pose/Pose.fwd.hh>
#include <utility/tag/Tag.fwd.hh>
#include <utility/tag/XMLSchemaGeneration.fwd.hh>
#include <core/scoring/dssp/Dssp.fwd.hh>

// Utility Headers
#include <core/types.hh>

// C++ Headers
namespace protocols {
namespace splice {

/// @details Find the endpoints on a beta barrel
class FindEndpointsOperation : public protocols::task_operations::RestrictOperationsBase
{
public:
	typedef RestrictOperationsBase parent;

	FindEndpointsOperation();

	~FindEndpointsOperation() override;

	TaskOperationOP clone() const override;


	void
	apply( core::pose::Pose const &, core::pack::task::PackerTask & ) const override;

	void parse_tag( TagCOP, DataMap & ) override;
	static std::string keyname() { return "FindEndpoints"; }
	static void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

	core::Size Cterm_offset() const{ return Cterm_offset_; }
	void Cterm_offset( core::Size const c ){ Cterm_offset_ = c; }
	core::Size Nterm_offset() const{ return Nterm_offset_; }
	void Nterm_offset( core::Size const n ){ Nterm_offset_ = n;}
	bool even() const{ return even_; }
	void even( bool const b ){ even_ = b; }
	bool odd() const{ return odd_; }
	void odd( bool const b ){ odd_ = b; }

	core::Real distance_cutoff() const{ return distance_cutoff_; }
	void distance_cutoff( core::Real const d ) { distance_cutoff_ = d;}

	core::Size neighbors() const{ return neighbors_; }
	void neighbors( core::Size const s ) { neighbors_ = s; }

	bool point_inside() const{ return point_inside_; }
	void point_inside( bool const b ){ point_inside_ = b; }

	core::Size sequence_separation() const { return sequence_separation_; }
	void sequence_separation( core::Size const s ){ sequence_separation_ = s; }

private:
	core::Size Cterm_offset_, Nterm_offset_; //dflt 0,0; whether to count residues N and C-terminally
	bool even_, odd_; //dflt true, true; report on even and odd blades
	core::Real distance_cutoff_; //dflt 18.0A; what distance for counting nterminal neighbours
	core::Size neighbors_; //dflt 6; how many neighbors to require

	bool point_inside_; //dflt true; select residues that point into the barrel
	core::Size sequence_separation_; //dflt 15aa; how many aas between the  strand ntermini to count. useful for eliminating short hairpins
};

core::Size neighbors_in_vector( core::pose::Pose const & pose, core::Size const target_res, utility::vector1< core::Size > const & neighbors, core::Real const dist_threshold, core::scoring::dssp::Dssp & dssp, core::Size const sequence_separation );

} //namespace splice
} //namespace protocols

#endif // INCLUDED_protocols_toolbox_TaskOperations_FindEndpointsOperation_HH
