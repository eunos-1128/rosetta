// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/outputter/ResFileOutputter.cc
/// @brief Saves a resfile that is generated by applying the input task operations to the pose
/// @author Ken Jung

// Unit Headers
#include <protocols/outputter/ResFileOutputter.hh>

#include <protocols/elscripts/util.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/pack/task/TaskFactory.hh>
#include <iostream>
#include <fstream>

// tracer
#include <basic/Tracer.hh>

namespace protocols {
namespace outputter {

static basic::Tracer TR("protocols.outputter.ResFileOutputter");

#ifdef USELUA
void lregister_ResFileOutputter( lua_State * lstate ) {
	lregister_FormatStringOutputter( lstate );

	luabind::module(lstate, "protocols")
	[
		luabind::namespace_( "outputter")
		[
			luabind::class_<ResFileOutputter, FormatStringOutputter>("ResFileOutputter")
		]
	];
}
#endif

ResFileOutputter::ResFileOutputter(){}
ResFileOutputter::~ResFileOutputter(){}
OutputterSP ResFileOutputter::create() {
	return OutputterSP( new ResFileOutputter () );
}

void ResFileOutputter::write( Pose & p ) {
	utility::vector1< core::Size > selected_residues;
	// Prepare the PackerTask
	runtime_assert( task_factory_ );
  core::pack::task::PackerTaskCOP task( task_factory_->create_task_and_apply_taskoperations( p ) );
	// Find out which residues are packable or designable
	for( core::Size resi = 1; resi <= p.total_residue(); ++resi ) {
		if ( designable_only_ ) {
    	if( task->residue_task( resi ).being_designed() && p.residue(resi).is_protein() )
      	selected_residues.push_back( resi );
		} else {
      if( task->residue_task( resi ).being_packed() && p.residue(resi).is_protein() )
        selected_residues.push_back( resi );
		}
  }
  if( selected_residues.empty() )
    TR.Warning << "WARNING: No residues were selected by your TaskOperations." << std::endl;

	// write to disk
	std::string outfilename;
	parse_format_string( filenameparts_, format_string_, outfilename );
  runtime_assert( outfilename != "" );
	std::ofstream resfile;
  resfile.open( outfilename.c_str(), std::ios::out );
  resfile << resfile_general_property_ << "\nstart\n";
	for ( core::Size i=1; i<=selected_residues.size(); i++ ) {
		resfile << selected_residues[i] << '\t' << p.pdb_info()->chain(selected_residues[i]) << " PIKAA " << p.residue(selected_residues[i]).name1() << '\n';
  }
  resfile.close();
}

#ifdef USELUA
void ResFileOutputter::parse_def( utility::lua::LuaObject const & def,
		utility::lua::LuaObject const & tasks ) {
	format_string_ = def["format_string"] ? def["format_string"].to<std::string>() : "%filebasename_%filemultiplier.resfile";
	if( def["tasks"] ) {
		task_factory_ = protocols::elscripts::parse_taskdef( def["tasks"], tasks );
	}
	designable_only_ = def["designable_only"] ? def["designable_only"].to<bool>() : false;
	resfile_general_property_ = def["resfile_general_property"] ? def["resfile_general_property"].to<std::string>() : "NATAA";
}

void ResFileOutputter::lregister( lua_State * lstate ) {
	lregister_ResFileOutputter(lstate);
}
#endif

} // outputter
} // protocols
