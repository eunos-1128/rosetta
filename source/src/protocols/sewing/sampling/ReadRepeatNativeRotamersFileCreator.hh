// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet;
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file AssemblyConstraintsMover.hh
///
/// @brief A simple mover that reads a NativeRotamersFile generated by a an AssemblyMover run and generates the necessary
/// ResidueType constraints to add to the pose
/// @author Tim Jacobs

#ifndef INCLUDED_protocols_sewing_sampling_ReadRepeatNativeRotamersFileCreator_HH
#define INCLUDED_protocols_sewing_sampling_ReadRepeatNativeRotamersFileCreator_HH

#include <core/pack/task/operation/TaskOperationCreator.hh>

namespace protocols {
namespace sewing  {

class ReadRepeatNativeRotamersFileCreator : public core::pack::task::operation::TaskOperationCreator {
public:
	virtual
	core::pack::task::operation::TaskOperationOP
	create_task_operation() const;

	virtual
	std::string keyname() const;

	virtual void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const;

};

} //sewing
} //protocols

#endif

