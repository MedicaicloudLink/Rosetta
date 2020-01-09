// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/rotamer_recovery/RotamerRecoveryCreator.cc
/// @brief  Creator classes for components of the RotamerRecovery framework
/// @author Matthew O'Meara

// Unit Headers
#include <protocols/rotamer_recovery/RotamerRecoveryCreator.hh>

// Package Headers
#include <protocols/rotamer_recovery/RRProtocolReferenceStructure.hh>
#include <protocols/rotamer_recovery/RRProtocolRTMin.hh>
#include <protocols/rotamer_recovery/RRProtocolRotamerTrials.hh>
#include <protocols/rotamer_recovery/RRProtocolMinPack.hh>
#include <protocols/rotamer_recovery/RRProtocolPackRotamers.hh>
#include <protocols/rotamer_recovery/RRProtocolRelax.hh>
#include <protocols/rotamer_recovery/RRProtocolMover.hh>
#include <protocols/rotamer_recovery/RRComparer.hh>
#include <protocols/rotamer_recovery/RRComparerAutomorphicRMSD.hh>
#include <protocols/rotamer_recovery/RRComparerElecDensDiff.hh>
#include <protocols/rotamer_recovery/RRReporter.hh>
#include <protocols/rotamer_recovery/RRReporterSQLite.hh>
#include <protocols/rotamer_recovery/RRReporterHuman.hh>

#include <utility/vector1.hh>

// C++ Headers
#include <string>

namespace protocols {
namespace rotamer_recovery {

using std::string;

/// Protocols /////

//////////////// ReferenceStructure ///////////////////
RRProtocolOP
RRProtocolReferenceStructureCreator::create_protocol(
) const {
	return utility::pointer::make_shared< RRProtocolReferenceStructure >();
}

string
RRProtocolReferenceStructureCreator::type_name() const {
	return "RRProtocolReferenceStructure";
}

void RRProtocolReferenceStructureCreator::append_attributes( utility::tag::AttributeList & attlist ) const {
	RRProtocolReferenceStructure::append_attributes( attlist );
}

//////////////// RTMin ///////////////////
RRProtocolOP
RRProtocolRTMinCreator::create_protocol(
) const {
	return utility::pointer::make_shared< RRProtocolRTMin >();
}

string
RRProtocolRTMinCreator::type_name() const {
	return "RRProtocolRTMin";
}

void RRProtocolRTMinCreator::append_attributes( utility::tag::AttributeList & ) const {}

//////////////// RotamerTrials ///////////////////
RRProtocolOP
RRProtocolRotamerTrialsCreator::create_protocol(
) const {
	return utility::pointer::make_shared< RRProtocolRotamerTrials >();
}

string
RRProtocolRotamerTrialsCreator::type_name() const {
	return "RRProtocolRotamerTrials";
}

void RRProtocolRotamerTrialsCreator::append_attributes( utility::tag::AttributeList & ) const {}

//////////////// MinPack ///////////////////
RRProtocolOP
RRProtocolMinPackCreator::create_protocol(
) const {
	return utility::pointer::make_shared< RRProtocolMinPack >();
}

string
RRProtocolMinPackCreator::type_name() const {
	return "RRProtocolMinPack";
}

void RRProtocolMinPackCreator::append_attributes( utility::tag::AttributeList & ) const {}

//////////////// PackRotamers ///////////////////
RRProtocolOP
RRProtocolPackRotamersCreator::create_protocol(
) const {
	return utility::pointer::make_shared< RRProtocolPackRotamers >();
}

string
RRProtocolPackRotamersCreator::type_name() const {
	return "RRProtocolPackRotamers";
}

void RRProtocolPackRotamersCreator::append_attributes( utility::tag::AttributeList & ) const {}

//////////////// Relax ///////////////////
RRProtocolOP
RRProtocolRelaxCreator::create_protocol(
) const {
	return utility::pointer::make_shared< RRProtocolRelax >();
}

string
RRProtocolRelaxCreator::type_name() const {
	return "RRProtocolRelax";
}

void RRProtocolRelaxCreator::append_attributes( utility::tag::AttributeList & ) const {}

//////////////// Mover ///////////////////
string
RRProtocolMoverCreator::type_name() const {
	return "RRProtocolMover";
}

RRProtocolOP
RRProtocolMoverCreator::create_protocol(
) const {
	return utility::pointer::make_shared< RRProtocolMover >();
}

void RRProtocolMoverCreator::append_attributes( utility::tag::AttributeList & ) const {}

/// Protocols /////
//////////////// AutomorphicRMSD ///////////////////
RRComparerOP
RRComparerAutomorphicRMSDCreator::create_comparer(
) const {
	return utility::pointer::make_shared< RRComparerAutomorphicRMSD >();
}

string
RRComparerAutomorphicRMSDCreator::type_name() const {
	return "RRComparerAutomorphicRMSD";
}

void RRComparerAutomorphicRMSDCreator::append_attributes( utility::tag::AttributeList & ) const {}

//////////////// RotBins ///////////////////
RRComparerOP
RRComparerRotBinsCreator::create_comparer(
) const {
	return utility::pointer::make_shared< RRComparerRotBins >();
}

string
RRComparerRotBinsCreator::type_name() const {
	return "RRComparerRotBins";
}

void RRComparerRotBinsCreator::append_attributes( utility::tag::AttributeList & ) const {}

//////////////// ChiDiff ///////////////////
RRComparerOP
RRComparerChiDiffCreator::create_comparer(
) const {
	return utility::pointer::make_shared< RRComparerChiDiff >();
}

string
RRComparerChiDiffCreator::type_name() const {
	return "RRComparerChiDiff";
}

void RRComparerChiDiffCreator::append_attributes( utility::tag::AttributeList & attlist ) const
{
	RRComparerChiDiff::append_attributes( attlist );
}

//////////////// ElecDensDiff ///////////////////
RRComparerOP
RRComparerElecDensDiffCreator::create_comparer(
) const {
	return utility::pointer::make_shared< RRComparerElecDensDiff >();
}

string
RRComparerElecDensDiffCreator::type_name() const {
	return "RRComparerElecDensDiff";
}

void RRComparerElecDensDiffCreator::append_attributes( utility::tag::AttributeList & ) const {}

//////////////// Simple ///////////////////
RRReporterOP
RRReporterSimpleCreator::create_reporter(
) const {
	return utility::pointer::make_shared< RRReporterSimple >();
}

string
RRReporterSimpleCreator::type_name() const {
	return "RRReporterSimple";
}

void RRReporterSimpleCreator::append_attributes( utility::tag::AttributeList & ) const {}

//// Reporters ////
//////////////// Human ///////////////////
RRReporterOP
RRReporterHumanCreator::create_reporter(
) const {
	return utility::pointer::make_shared< RRReporterHuman >();
}

string
RRReporterHumanCreator::type_name() const {
	return "RRReporterHuman";
}

void RRReporterHumanCreator::append_attributes( utility::tag::AttributeList & ) const {}

//////////////// SQLite ///////////////////
RRReporterOP
RRReporterSQLiteCreator::create_reporter(
) const {
	return utility::pointer::make_shared< RRReporterSQLite >();
}

string
RRReporterSQLiteCreator::type_name() const {
	return "RRReporterSQLite";
}

void RRReporterSQLiteCreator::append_attributes( utility::tag::AttributeList & ) const {}

} //namespace features
} //namespace protocols
