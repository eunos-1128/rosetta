// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/pack/task/residue_selector/JumpUpstreamSelector.hh
/// @brief  The JumpUpstreamSelector selects residues downstream of a given jump in a FoldTree 
/// @author Robert Lindner (rlindner@mpimf-heidelberg.mpg.de)

#ifndef INCLUDED_core_pack_task_residue_selector_JumpUpstreamSelector_HH
#define INCLUDED_core_pack_task_residue_selector_JumpUpstreamSelector_HH

// Unit headers
#include <core/pack/task/residue_selector/JumpUpstreamSelector.fwd.hh>

// Package headers
#include <core/types.hh>
#include <core/pack/task/residue_selector/ResidueSelector.hh>
#include <core/pose/Pose.hh>

// Utility Headers
#include <utility/tag/Tag.fwd.hh>
#include <utility/vector1.hh>

// C++ headers
#include <set>

namespace core {
namespace pack {
namespace task {
namespace residue_selector {

/// @brief The JumpUpstreamSelector returns a ResidueSubset, i.e. a utility::vector1< bool > containing
/// 'true' for residue positions which lie upstream of a given jump in the FoldTree. The jump is
/// specified by its integer index.
class JumpUpstreamSelector : public ResidueSelector {
public:
	// derived from base class
	JumpUpstreamSelector();
	JumpUpstreamSelector( int jump );
	virtual ~JumpUpstreamSelector();

	virtual void apply( core::pose::Pose const & pose, ResidueSubset & subset ) const;
	virtual void parse_my_tag(
		utility::tag::TagCOP tag,
		basic::datacache::DataMap &
	);
	
	virtual
	std::string
	get_name() const;

	static std::string class_name();


	//unit-specific
	/**
	* @brief sets the string by which residues are selected
	*/
	void set_jump( int jump );

private: // data members
	int jump_;
};

} //namespace residue_selector
} //namespace task
} //namespace pack
} //namespace core


#endif
