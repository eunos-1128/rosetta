// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file
/// @brief Add constraints to the current pose conformation.
/// @author Yifan Song

#ifndef INCLUDED_protocols_comparative_modeling_hybridize_HybridizeProtocol_hh
#define INCLUDED_protocols_comparative_modeling_hybridize_HybridizeProtocol_hh

#include <protocols/comparative_modeling/hybridize/HybridizeProtocol.fwd.hh>
#include <protocols/comparative_modeling/hybridize/FoldTreeHybridize.hh>

#include <protocols/moves/Mover.hh>

#include <protocols/loops/Loops.hh>

#include <core/pose/Pose.fwd.hh>
#include <core/scoring/constraints/ConstraintSet.fwd.hh>
#include <core/fragment/FragSet.fwd.hh>
#include <core/sequence/SequenceAlignment.fwd.hh>
#include <core/sequence/SequenceAlignment.hh>
#include <core/pack/task/TaskFactory.fwd.hh>

#include <utility/file/FileName.hh>

namespace protocols {
namespace comparative_modeling {
namespace hybridize {

class HybridizeProtocol : public protocols::moves::Mover {

public:
	HybridizeProtocol();
	virtual ~HybridizeProtocol();
		
	void read_template_structures(utility::file::FileName template_list);
	void read_template_structures(utility::vector1 < utility::file::FileName > const & template_filenames);

    void add_template( core::pose::PoseOP template_in, std::string cst_fn, core::Real weight = 1., core::Size cluster_id = 1 );
	void add_template( std::string template_fn, std::string cst_fn, core::Real weight = 1., core::Size cluster_id = 1);

    core::Real get_gdtmm( core::pose::Pose & pose );

	void check_options();

	void pick_starting_template(core::Size & initial_template_index,
								core::Size & initial_template_index_icluster,
                                utility::vector1 < core::Size > template_index_icluster,
                                utility::vector1 < core::pose::PoseOP > & templates_icluster,
								utility::vector1 < core::Real > & weights_icluster,
								utility::vector1 < protocols::loops::Loops > & template_chunks_icluster,
								utility::vector1 < protocols::loops::Loops > & template_contigs_icluster);
	
	virtual void apply( Pose & );
	virtual std::string get_name() const;

	virtual protocols::moves::MoverOP clone() const;
	virtual protocols::moves::MoverOP fresh_instance() const;
	
	//virtual void
	//parse_my_tag( TagPtr const, DataMap &, Filters_map const &, Movers_map const &, Pose const & );

private:
	// created by template initialization
	utility::vector1 < core::pose::PoseOP > templates_;
	//utility::vector1 < core::scoring::constraints::ConstraintSetOP > template_csts_;
	utility::vector1 < std::string > template_cst_fn_;
	utility::vector1 < core::Real > template_weights_;
	utility::vector1 < core::Size > template_clusterID_;
	utility::vector1 < protocols::loops::Loops > template_chunks_;
	utility::vector1 < protocols::loops::Loops > template_contigs_;
	core::Real template_weights_sum_;

	std::map< Size, utility::vector1 < Size > > clusterID_map_;
	
	core::fragment::FragSetOP fragments9_, fragments3_; // abinitio frag9,frag3 flags

	// native pose, aln
	core::pose::PoseOP native_;
	core::sequence::SequenceAlignmentOP aln_;
};

} // hybridize 
} // comparative_modeling 
} // protocols

#endif
