// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file
/// @brief Align a random jump to template
/// @detailed
/// @author Yifan Song

#ifndef apps_pilot_yfsong_FoldTreeHybridize_HH
#define apps_pilot_yfsong_FoldTreeHybridize_HH

#include <core/id/AtomID.hh>
#include <core/id/AtomID_Map.hh>
#include <core/util/kinematics_util.hh>
#include <core/fragment/Frame.hh>
#include <core/fragment/FrameIterator.hh>
#include <apps/pilot/yfsong/FoldTreeHybridize.fwd.hh>

#include <numeric/xyz.functions.hh>

#include <basic/Tracer.hh>

namespace challenge {
	basic::options::StringOptionKey		ss("challenge:ss");
	basic::options::BooleanOptionKey    aligned("challenge:aligned");
	basic::options::IntegerVectorOptionKey    chunk_mapping("challenge:chunk_mapping");
}

namespace protocols {
namespace comparative_modeling {
namespace hybridize {

using namespace core;
using namespace core::kinematics;
using namespace ObjexxFCL;
using namespace protocols::moves;
using namespace protocols::loops;
using namespace protocols::nonlocal;
using namespace numeric::model_quality;
using namespace id;
using namespace basic::options;
using namespace basic::options::OptionKeys;
	
enum AlignOption { all_chunks, random_chunk };
	
class FoldTreeHybridize: public protocols::moves::Mover
{
	
public:
FoldTreeHybridize(numeric::random::RandomGenerator & RG,
						 utility::vector1 < core::pose::PoseOP > const & template_poses,
						 Loops ss_chunks_pose,
						 AlignOption align_option = all_chunks,
						Size max_registry_shift = 0);

void
	get_alignment_from_template(core::pose::PoseCOP const template_pose, std::map <core::Size, core::Size> & seqpos_alignment);

	
void
get_alignment_from_chunk_mapping(std::map <core::Size, core::Size> const & chunk_mapping,
								 Loops const template_ss_chunks,
								 Loops const target_ss_chunks,
								 std::map <core::Size, core::Size> & sequence_alignment);
	
void pick_random_template();

void pick_random_chunk(core::pose::Pose & pose);

Size trial_counter(Size ires);
	
void
	apply(core::pose::Pose & pose);

std::string
	get_name() const;
	
private:
	FoldTreeHybridize align_chunk_;
	numeric::random::RandomGenerator & RG_;
	AlignOption align_option_;
	
	utility::vector1 < core::pose::PoseCOP > template_poses_;
	utility::vector1 < Loops > template_ss_chunks_;
	utility::vector1 < std::map <core::Size, core::Size> > sequence_alignments_;
	Size max_registry_shift_input_;
	utility::vector1 < Size > max_registry_shift_;
	
	Size template_number_; // the jump to be realigned
	Size jump_number_; // the jump to be realigned
}; //class FoldTreeHybridize
	
} // hybridize 
} // comparative_modeling 
} // protocols

#endif
