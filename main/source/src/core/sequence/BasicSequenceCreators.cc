// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/sequence/SequenceCreator.hh
/// @brief  Base class for SequenceCreators for the Sequence load-time factory registration scheme
/// @author James Thompson

#include <core/sequence/BasicSequenceCreators.hh>


#include <core/sequence/Sequence.hh>
#include <core/sequence/Sequence.fwd.hh>
#include <core/sequence/SequenceProfile.hh>
#include <core/sequence/SequenceCoupling.hh>
#include <core/sequence/CompositeSequence.hh>
#include <core/sequence/ChemicalShiftSequence.hh>

#include <utility/vector1.hh>


namespace core {
namespace sequence {

// class def for SimpleSequence
SimpleSequenceCreator::SimpleSequenceCreator() = default;
SimpleSequenceCreator::~SimpleSequenceCreator() = default;
SequenceOP SimpleSequenceCreator::create_sequence() const {
	return utility::pointer::make_shared< Sequence >();
}

std::string SimpleSequenceCreator::keyname() const {
	return "sequence";
}

SequenceCouplingCreator::SequenceCouplingCreator() = default;
SequenceCouplingCreator::~SequenceCouplingCreator() = default;
SequenceOP SequenceCouplingCreator::create_sequence() const {
	return utility::pointer::make_shared< SequenceCoupling >();
}

std::string SequenceCouplingCreator::keyname() const {
	return "sequence_coupling";
}
SequenceProfileCreator::SequenceProfileCreator() = default;
SequenceProfileCreator::~SequenceProfileCreator() = default;
SequenceOP SequenceProfileCreator::create_sequence() const {
	return utility::pointer::make_shared< SequenceProfile >();
}

std::string SequenceProfileCreator::keyname() const {
	return "sequence_profile";
}

CompositeSequenceCreator::CompositeSequenceCreator() = default;
CompositeSequenceCreator::~CompositeSequenceCreator() = default;
SequenceOP CompositeSequenceCreator::create_sequence() const {
	return utility::pointer::make_shared< CompositeSequence >();
}

std::string CompositeSequenceCreator::keyname() const {
	return "composite_sequence";
}

ChemicalShiftSequenceCreator::ChemicalShiftSequenceCreator() = default;
ChemicalShiftSequenceCreator::~ChemicalShiftSequenceCreator() = default;
SequenceOP ChemicalShiftSequenceCreator::create_sequence() const {
	return utility::pointer::make_shared< ChemicalShiftSequence >();
}

std::string ChemicalShiftSequenceCreator::keyname() const {
	return "composite_sequence";
}

} //namespace sequence
} //namespace core
