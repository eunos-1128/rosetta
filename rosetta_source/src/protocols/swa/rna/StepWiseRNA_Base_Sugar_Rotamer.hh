// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file StepWiseRNA_Base_Sugar_Rotamer
/// @brief
/// @detailed
/// @author Parin Sripakdeevong


#ifndef INCLUDED_protocols_swa_SWA_RNA_Base_Sugar_Rotamer_HH
#define INCLUDED_protocols_swa_SWA_RNA_Base_Sugar_Rotamer_HH

#include <protocols/swa/rna/StepWiseRNA_Classes.hh>
#include <protocols/swa/rna/StepWiseRNA_RotamerGenerator.fwd.hh>
#include <protocols/swa/rna/StepWiseRNA_Base_Sugar_Rotamer.fwd.hh>
//#include <protocols/swa/rna/StepWiseRNA_RotamerGenerator.hh>

#include <core/types.hh>
#include <core/id/TorsionID.fwd.hh>
#include <core/pose/Pose.fwd.hh>
#include <utility/vector1.hh>
#include <utility/pointer/ReferenceCount.hh>
#include <core/scoring/rna/RNA_FittedTorsionInfo.hh>

#include <string>
#include <map>

namespace protocols {
namespace swa {
namespace rna {

	class StepWiseRNA_Base_Sugar_Rotamer: public utility::pointer::ReferenceCount {
	public:
		//constructor!
		StepWiseRNA_Base_Sugar_Rotamer(
											BaseState const & base_state,
											PuckerState const & pucker_state,
											core::scoring::rna::RNA_FittedTorsionInfo const & rna_fitted_torsion_info,
											core::Size const bin_size,
											core::Size const bins4);

    ~StepWiseRNA_Base_Sugar_Rotamer();

		void reset();

		bool get_next_rotamer();

		PuckerState const & current_pucker_state() const;

		core::Real const & chi()   const {return chi_;}
		core::Real const & delta() const {return delta_;}
		core::Real const & nu2() 	 const {return nu2_;}
		core::Real const & nu1() 	 const {return nu1_;}

	private:

	private:


		BaseState const base_state_;
		PuckerState const pucker_state_;
		core::scoring::rna::RNA_FittedTorsionInfo const rna_fitted_torsion_info_;
		core::Size const bin_size_; // must be 20, 10, or 5
		core::Size const num_base_std_ID_;

		core::Size num_base_ID_;  //Should make this a const
		utility::vector1 < PuckerState > pucker_state_list_; //Should make this a const

		core::Size pucker_ID_;
		core::Size pucker_ID_old_;
		core::Size base_ID_;
		core::Size base_std_ID_;

		core::Real chi_;
		core::Real delta_;
		core::Real nu2_;
		core::Real nu1_;
	};
}
} //swa
} // protocols

#endif
