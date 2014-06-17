// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   devel/vardist_solaccess/RotamerDots.fwd.hh
/// @brief  VarSolDRotamerDots classes, forward declaration
/// @author Ron Jacak

#ifndef INCLUDED_devel_vardist_solaccess_VarSolDRotamerDots_FWD_HH
#define INCLUDED_devel_vardist_solaccess_VarSolDRotamerDots_FWD_HH

#include <utility/pointer/owning_ptr.hh>

namespace devel {
namespace vardist_solaccess {

class VarSolDRotamerDots;
typedef utility::pointer::owning_ptr< VarSolDRotamerDots > VarSolDRotamerDotsOP;
typedef utility::pointer::owning_ptr< VarSolDRotamerDots const > VarSolDRotamerDotsCOP;

class VarSolDistSasaCalculator;
typedef utility::pointer::owning_ptr< VarSolDistSasaCalculator > VarSolDistSasaCalculatorOP;
typedef utility::pointer::owning_ptr< VarSolDistSasaCalculator const > VarSolDistSasaCalculatorCOP;


} // vardist_solaccess
} // devel


#endif // INCLUDED_devel_vardist_solaccess_VarSolDRotamerDots_FWD_HH
