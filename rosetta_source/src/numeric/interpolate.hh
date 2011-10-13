// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file numeric/interpolate.hh
/// @author Christopher Miles (cmiles@uw.edu)

#ifndef NUMERIC_INTERPOLATE_hh_
#define NUMERIC_INTERPOLATE_hh_

namespace numeric {

/// @brief Linearly interpolates a quantity from start to stop over (num_stages + 1) stages
template <class Value>
double linear_interpolate(Value start, Value stop, unsigned curr_stage, unsigned num_stages) {
  return start + curr_stage * (stop - start) / num_stages;
}

}  // namespace numeric

#endif  // NUMERIC_PROB_UTIL_hh_
