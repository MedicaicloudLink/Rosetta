// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/io/silent/SilentStructCreator.hh
/// @brief  Base class for SilentStructCreators for the SilentStruct load-time factory registration scheme
/// @author James Thompson

// Unit Headers
#include <core/io/silent/BasicSilentStructCreators.hh>

// Package Headers

#include <core/io/silent/SilentStruct.fwd.hh>
#include <core/io/silent/ProteinSilentStruct.hh>
#include <core/io/silent/BinarySilentStruct.hh>
#include <core/io/silent/RigidBodySilentStruct.hh>
#include <core/io/silent/RNA_SilentStruct.hh>
#include <core/io/silent/ScoreFileSilentStruct.hh>
#include <core/io/silent/ScoreJumpFileSilentStruct.hh>

//Auto Headers
#include <core/io/silent/ProteinSilentStruct.tmpl.hh>
#include <utility/vector1.hh>


namespace core {
namespace io {
namespace silent {


ProteinSilentStruct_SinglePrecCreator::ProteinSilentStruct_SinglePrecCreator() = default;
ProteinSilentStruct_SinglePrecCreator::~ProteinSilentStruct_SinglePrecCreator() = default;
SilentStructOP ProteinSilentStruct_SinglePrecCreator::create_silent_struct( SilentFileOptions const & opts ) const {
	return utility::pointer::make_shared< ProteinSilentStruct_SinglePrec >( opts );
}

std::string ProteinSilentStruct_SinglePrecCreator::keyname() const {
	return "protein_float";
}

// class def for ProteinSilentStruct
ProteinSilentStructCreator::ProteinSilentStructCreator() = default;
ProteinSilentStructCreator::~ProteinSilentStructCreator() = default;
SilentStructOP ProteinSilentStructCreator::create_silent_struct( SilentFileOptions const & opts ) const {
	return utility::pointer::make_shared< ProteinSilentStruct >( opts );
}

std::string ProteinSilentStructCreator::keyname() const {
	return "protein";
}

// class def for RNA_SilentStruct
RNA_SilentStructCreator::RNA_SilentStructCreator() = default;
RNA_SilentStructCreator::~RNA_SilentStructCreator() = default;
SilentStructOP RNA_SilentStructCreator::create_silent_struct( SilentFileOptions const & opts ) const {
	return utility::pointer::make_shared< RNA_SilentStruct >( opts );
}

std::string RNA_SilentStructCreator::keyname() const {
	return "rna";
}

// class def for BinarySilentStruct
BinarySilentStructCreator::BinarySilentStructCreator() = default;
BinarySilentStructCreator::~BinarySilentStructCreator() = default;
SilentStructOP BinarySilentStructCreator::create_silent_struct( SilentFileOptions const & opts ) const {
	return utility::pointer::make_shared< BinarySilentStruct >( opts );
}

std::string BinarySilentStructCreator::keyname() const {
	return "binary";
}

// class def for ScoreFileSilentStruct
ScoreFileSilentStructCreator::ScoreFileSilentStructCreator() = default;
ScoreFileSilentStructCreator::~ScoreFileSilentStructCreator() = default;
SilentStructOP ScoreFileSilentStructCreator::create_silent_struct( SilentFileOptions const & opts ) const {
	return utility::pointer::make_shared< ScoreFileSilentStruct >( opts );
}

std::string ScoreFileSilentStructCreator::keyname() const {
	return "score";
}

// class def for ScoreJumpFileSilentStruct
ScoreJumpFileSilentStructCreator::ScoreJumpFileSilentStructCreator() = default;
ScoreJumpFileSilentStructCreator::~ScoreJumpFileSilentStructCreator() = default;
SilentStructOP ScoreJumpFileSilentStructCreator::create_silent_struct( SilentFileOptions const & opts ) const {
	return utility::pointer::make_shared< ScoreJumpFileSilentStruct >( opts );
}

std::string ScoreJumpFileSilentStructCreator::keyname() const {
	return "score_jump";
}


// class def for ProteinSilentStruct
RigidBodySilentStructCreator::RigidBodySilentStructCreator() = default;
RigidBodySilentStructCreator::~RigidBodySilentStructCreator() = default;
SilentStructOP RigidBodySilentStructCreator::create_silent_struct( SilentFileOptions const & opts ) const {
	return utility::pointer::make_shared< RigidBodySilentStruct >( opts );
}

std::string RigidBodySilentStructCreator::keyname() const {
	return "rigid_body";
}


} //namespace silent
} //namespace io
} //namespace core
