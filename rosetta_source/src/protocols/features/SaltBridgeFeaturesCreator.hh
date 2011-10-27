// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/features/SaltBridgeFeaturesCreator.hh
/// @brief  Header for SaltBridgeFeaturesCreator for the SaltBridgeFeatures load-time factory registration scheme
/// @author Matthew O'Meara

#ifndef INCLUDED_protocols_features_SaltBridgeFeaturesCreator_hh
#define INCLUDED_protocols_features_SaltBridgeFeaturesCreator_hh

// Unit Headers
#include <protocols/features/SaltBridgeFeatures.hh>
#include <protocols/features/FeaturesReporterCreator.hh>

// c++ headers
#include <string>

namespace protocols {
namespace features {

/// @brief creator for the SaltBridgeFeatures class
class SaltBridgeFeaturesCreator : public FeaturesReporterCreator
{
public:
	SaltBridgeFeaturesCreator();
	virtual ~SaltBridgeFeaturesCreator();

	virtual FeaturesReporterOP create_features_reporter() const;
	virtual std::string type_name() const;
};

} //namespace features
} //namespace protocols

#endif
