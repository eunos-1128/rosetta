// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file devel/strand_assembly/StrandBundleFeaturesCreator.cc
/// @brief
/// @author Tim Jacobs

// Unit Headers
#include <protocols/features/strand_assembly/StrandBundleFeaturesCreator.hh>

// Package Headers

#include <protocols/features/FeaturesReporterCreator.hh>
// AUTO-REMOVED #include <protocols/features/FeaturesReporterFactory.hh>

#include <protocols/features/strand_assembly/StrandBundleFeatures.hh>
#include <utility/vector1.hh>


namespace protocols {
namespace features {
namespace strand_assembly {

StrandBundleFeaturesCreator::StrandBundleFeaturesCreator() {}
StrandBundleFeaturesCreator::~StrandBundleFeaturesCreator() {}
FeaturesReporterOP StrandBundleFeaturesCreator::create_features_reporter() const {
	return new StrandBundleFeatures;
}

std::string StrandBundleFeaturesCreator::type_name() const {
	return "StrandBundleFeatures";
}

} //namespace strand_assembly
} //namespace features
} //namespace protocols
