// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

///@file protocols/drug_design/BCLFragmentMutateMoverBCLFragmentMutateMoverCreator.hh
///@brief This class will create instances of Mover BCLFragmentMutateMover for the MoverFactory
///@author Benjamin P. Brown (benjamin.p.brown17@gmail.com)

#ifndef INCLUDED_protocols_drug_design_bcl_BCLFragmentMutateMoverCreator_hh
#define INCLUDED_protocols_drug_design_bcl_BCLFragmentMutateMoverCreator_hh

#include <protocols/moves/MoverCreator.hh>

namespace protocols
{
namespace drug_design
{
namespace bcl
{
class BCLFragmentMutateMoverCreator : public protocols::moves::MoverCreator
{
public:
	moves::MoverOP create_mover() const override;
	std::string keyname() const override;
	void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const override;
};
} // bcl
} // drug_design
} // protocols
#endif // INCLUDED_protocols_drug_design_bcl_BCLFragmentMutateMoverCreator_hh
