// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/ligand_docking/HBondDonorFilter.cc
/// @brief Find packing defects at an interface using packstat score terms
/// @author Jacob Corn (jecorn@u.washington.edu)

// Unit headers
#include <protocols/ligand_docking/HBondDonorFilter.hh>
#include <protocols/ligand_docking/HBondDonorFilterCreator.hh>


#include <protocols/filters/Filter.hh>
// Project Headers
#include <core/types.hh>
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/conformation/Conformation.hh>
#include <utility/tag/Tag.hh>
#include <protocols/moves/Mover.fwd.hh> //Movers_map
// AUTO-REMOVED #include <core/pose/PDB_Info.hh>
#include <utility/exit.hh>

#include <basic/Tracer.hh>
#include <utility/vector0.hh>
#include <utility/excn/Exceptions.hh>
#include <utility/vector1.hh>


namespace protocols {
namespace ligand_docking {

static basic::Tracer hbond_donor_tracer( "protocols.ligand_docking.HBondDonorFilter" );

bool
HBondDonorFilter::apply( core::pose::Pose const & pose ) const {
	assert(chain_.size()==1 );
	assert(hbond_donor_limit_ >0 );
	core::Size const chain_id= core::pose::get_chain_id_from_chain(chain_, pose);
	core::Size const begin = pose.conformation().chain_begin(chain_id);
	core::Size const end = pose.conformation().chain_end(chain_id);

	if(	core::pose::num_hbond_donors(begin,end,pose) > hbond_donor_limit_ ){
		hbond_donor_tracer<< "Reached hbond donor limit"<< std::endl;
		return false;
	}
	return true;
}

void
HBondDonorFilter::parse_my_tag( utility::tag::TagCOP tag, basic::datacache::DataMap &, protocols::filters::Filters_map const &, protocols::moves::Movers_map const &, core::pose::Pose const & )
{

	if ( tag->getName() != "HBondDonor" ) {
		hbond_donor_tracer << " received incompatible Tag " << tag << std::endl;
		assert(false);
		return;
	}
	if ( ! (tag->hasOption("chain") && tag->hasOption("hbond_donor_limit") ) ){
		throw utility::excn::EXCN_RosettaScriptsOption("HBondDonor filter needs a 'chain' and an 'hbond_donor_limit' option");
	}
	chain_ = tag->getOption<std::string>("chain");
	hbond_donor_limit_ = tag->getOption<core::Size>("hbond_donor_limit");

}

protocols::filters::FilterOP
HBondDonorFilterCreator::create_filter() const { return new HBondDonorFilter; }

std::string
HBondDonorFilterCreator::keyname() const { return "HBondDonor"; }



} // ligand_docking
} // protocols
