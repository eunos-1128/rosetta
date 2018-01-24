// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/jd3/VanillaJobDistributor.cc
/// @brief  VanillaJobDistributor class declaration
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)


#ifndef INCLUDED_protocols_jd3_VanillaJobDistributor_FWD_HH
#define INCLUDED_protocols_jd3_VanillaJobDistributor_FWD_HH

// Utility headers
#include <utility/pointer/owning_ptr.hh>

namespace protocols {
namespace jd3 {

class VanillaJobDistributor;

typedef utility::pointer::shared_ptr< VanillaJobDistributor > VanillaJobDistributorOP;
typedef utility::pointer::shared_ptr< VanillaJobDistributor const > VanillaJobDistributorCOP;

}//jd2
}//protocols

#endif //INCLUDED_protocols_jd3_VanillaJobDistributor_HH
