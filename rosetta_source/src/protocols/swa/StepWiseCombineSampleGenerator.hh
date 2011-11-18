// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file SWA_ResidueSampler.hh
/// @brief
/// @detailed
///
/// @author Rhiju Das
/// @author Parin Sripakdeevong


#ifndef INCLUDED_protocols_swa_SWA_CombineSampleGenerator_HH
#define INCLUDED_protocols_swa_SWA_CombineSampleGenerator_HH

#include <protocols/swa/StepWisePoseSampleGenerator.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/types.hh>
#include <utility/vector1.hh>

namespace protocols {
namespace swa {

	class StepWiseCombineSampleGenerator: public StepWisePoseSampleGenerator {
	public:

		StepWiseCombineSampleGenerator(
																	 StepWisePoseSampleGeneratorOP first_generator,
																	 StepWisePoseSampleGeneratorOP second_generator );


		void reset();

		bool has_another_sample();

		void get_next_sample( core::pose::Pose & pose );

		Size size() const;

	private:

		StepWisePoseSampleGeneratorOP first_generator_, second_generator_;
		bool need_to_initialize_;

  };

} //swa
} // protocols

#endif

