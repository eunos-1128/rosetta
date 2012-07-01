// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
// :noTabs=false:tabSize=4:indentSize=4:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file
/// @brief  analyse sets of structures
/// @detailed This tool allows to superimpose structures using the wRMSD method [ Damm&Carlson, Biophys J (2006) 90:4558-4573 ]
/// @detailed Superimposed structures can be written as output pdbs and the converged residues can be determined
/// @author Oliver Lange

#include <protocols/moves/Mover.hh>
#include <core/pose/Pose.hh>
// AUTO-REMOVED #include <core/pose/util.hh>
#include <devel/init.hh>
#include <core/io/pdb/pose_io.hh>
#include <core/types.hh>
#include <core/chemical/ChemicalManager.hh>

#include <protocols/jd2/JobDistributor.hh>
#include <protocols/jd2/NoOutputJobOutputter.hh>
#include <protocols/jd2/JobOutputter.hh>
#include <protocols/jd2/SilentFileJobInputter.hh>

#include <protocols/toolbox/DecoySetEvaluation.hh>

#include <protocols/toolbox/superimpose.hh>
#include <protocols/loops/Loop.hh>
#include <protocols/loops/Loops.hh>
#include <protocols/loops/LoopsFileIO.hh>

// Utility headers
#include <basic/options/option_macros.hh>
#include <utility/io/ozstream.hh>

#include <utility/vector1.hh>
#include <basic/Tracer.hh>
#include <utility/exit.hh>

#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>

// ObjexxFCL includes
#include <ObjexxFCL/FArray1D.hh>
#include <ObjexxFCL/format.hh>
#include <ObjexxFCL/string.functions.hh>

#include <ostream>
#include <algorithm>
#include <string>

//Auto Headers
#include <core/import_pose/import_pose.hh>
#include <utility/excn/EXCN_Base.hh>


static basic::Tracer tr("main");

using namespace core;
using namespace protocols;
using namespace protocols::jd2;
//using namespace pose;
using namespace basic::options;
using namespace basic::options::OptionKeys;
using namespace toolbox;
using namespace ObjexxFCL;
//using namespace ObjexxFCL::fmt;

OPT_KEY( Real, wRMSD )
OPT_KEY( Real, tolerance )
OPT_1GRP_KEY( File, rmsf, out )
OPT_1GRP_KEY( File, rigid, out )
OPT_1GRP_KEY( Real, rigid, cutoff )
OPT_1GRP_KEY( Integer, rigid, min_gap )
OPT_1GRP_KEY( File, rigid, in )

void register_options() {
	using namespace basic::options;
	using namespace basic::options::OptionKeys;
  OPT( in::file::s );
	OPT( in::file::silent );
	OPT( in::file::native );
	OPT( out::pdb );
	NEW_OPT( wRMSD, "compute wRMSD with this sigma", 2 );
	NEW_OPT( tolerance, "stop wRMSD iteration if <tolearance change in wSum", 0.00001);
	//	NEW_OPT( dump_fit, "output pdbs of fitted structures ", false );
	NEW_OPT( rigid::in, "residues that are considered for wRMSD or clustering", "rigid.loop");
	NEW_OPT( rmsf::out, "write rmsf into this file", "rmsf.dat" );
	NEW_OPT( rigid::out, "write a RIGID definition", "rigid.loops" );
	NEW_OPT( rigid::cutoff, "residues with less then loop_cutoff will go into RIGID definition", 2 );
	NEW_OPT( rigid::min_gap, "don't have less then min_gap residues between rigid regions", 5 );
}

// Forward
class RmsfMover;

// Types
typedef  utility::pointer::owning_ptr< RmsfMover >  RmsfMoverOP;
typedef  utility::pointer::owning_ptr< RmsfMover const >  RmsfMoverCOP;

class RmsfMover : public moves::Mover {
public:
	virtual void apply( core::pose::Pose& );
	std::string get_name() const { return "RmsfMover"; }

	DecoySetEvaluation eval_;
};

void RmsfMover::apply( core::pose::Pose &pose ) {
	if ( eval_.n_decoys_max() < eval_.n_decoys() + 1 ) {
		eval_.reserve( eval_.n_decoys() + 100 );
	}
	eval_.push_back( pose );
}

class FitMover : public moves::Mover {
public:
	virtual void apply( core::pose::Pose& );
	std::string get_name() const { return "FitMover"; }
	FitMover() : first( true ), iref( 1 ) {};
	ObjexxFCL::FArray1D_double weights_;
	core::pose::Pose ref_pose_;
	bool first;
	Size iref;
};

typedef utility::pointer::owning_ptr< FitMover > FitMoverOP;

void FitMover::apply( core::pose::Pose &pose ) {
	if ( iref && first ) {
		--iref;
		return;
	}
	if ( first ) {
		ref_pose_ = pose;
		first = false;
		return;
	}
	runtime_assert( !first );
	CA_superimpose( weights_, ref_pose_, pose );
}

void read_structures( RmsfMoverOP rmsf_tool ) {
	//get silent-file job-inputter if available
	SilentFileJobInputterOP sfd_inputter (
							dynamic_cast< SilentFileJobInputter* > ( JobDistributor::get_instance()->job_inputter().get() ) );
	if ( sfd_inputter ) {
		//this allows very fast reading of CA coords
		io::silent::SilentFileData const& sfd( sfd_inputter->silent_file_data() );
		rmsf_tool->eval_.push_back_CA_xyz_from_silent_file( sfd, false /*store energies*/ );
	} else {
		JobDistributor::get_instance()->go( rmsf_tool, new jd2::NoOutputJobOutputter );
	}
}

void read_input_weights( FArray1D_double& weights, Size natoms) {
	if ( !option[ rigid::in ].user() ) return;
	loops::LoopsFileIO loop_file_reader;
	std::ifstream is( option[ rigid::in ]().name().c_str() );
	if (!is) utility_exit_with_message( "[ERROR] Error opening RBSeg file '" + option[ rigid::in ]().name() + "'" );
	loops::SerializedLoopList loops = loop_file_reader.use_custom_legacy_file_format(is, option[ rigid::in ](), false, "RIGID");
	loops::Loops rigid = loops::Loops( loops );
	for ( Size i=1;i<=natoms; ++i ) {
		if (rigid.is_loop_residue( i ) ) weights( i )=1.0;
		else weights( i )=0.0;
	}
}

Size superimpose( DecoySetEvaluation& eval, utility::vector1< Real >& rmsf_result, FArray1D_double& weights ) {
	Size icenter = 1;
	eval.superimpose();
	icenter = eval.wRMSD( option[ wRMSD ], option[ tolerance ](), weights );
	eval.rmsf( rmsf_result );
	return icenter;
}

void run() {
	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	using namespace protocols::jd2;

	//store structures in rmsf_tool ...
	RmsfMoverOP rmsf_tool =  new RmsfMover;
	read_structures( rmsf_tool );

	//initialize wRMSD weights with 1.0 unless we have -rigid:in file active
	FArray1D_double weights( rmsf_tool->eval_.n_atoms(), 1.0 );
	FArray1D_double input_weights( rmsf_tool->eval_.n_atoms(), 1.0 );
	read_input_weights(weights,rmsf_tool->eval_.n_atoms());
	input_weights=weights;
	rmsf_tool->eval_.set_weights( weights );

	//superposition...
	utility::vector1< Real > rmsf_result;
	Size icenter=0;
	if ( option[ rmsf::out ].user() || option[ rigid::out ].user() || option[ out::pdb ] )	{//output rmsf file
		icenter=superimpose( rmsf_tool->eval_, rmsf_result, weights );
	}

	//output rmsf file...
	if ( option[ rmsf::out ].user() ) {
		utility::io::ozstream os_rmsf( option[ rmsf::out ]());
		Size ct = 1;
		for ( utility::vector1<Real>::const_iterator it = rmsf_result.begin(); it != rmsf_result.end(); ++it, ++ct ) {
			os_rmsf << ct << " " << *it << " " << weights( ct ) << std::endl;
		}
	}

	///write rigid-out loops file
	if ( option[ rigid::out ].user() ) {//create RIGID output
		Real const cutoff ( option[ rigid::cutoff ] );
		tr.Info << "make rigid with cutoff " << cutoff << " and write to file... " << option[ rigid::out ]() << std::endl;
		loops::Loops rigid;
		for ( Size i=1; i<=rmsf_result.size(); ++i ) {
			if ( rmsf_result[ i ]<cutoff && input_weights( i ) > 0 ) rigid.add_loop( loops::Loop(  i, i ), option[ rigid::min_gap ]() );
		}
		tr.Info << rigid << std::endl;
		rigid.write_loops_to_file( option[ rigid::out ](), "RIGID" );
		std::string fname =  option[ rigid::out ]();
		loops::Loops loops = rigid.invert( rmsf_result.size() );
		loops.write_loops_to_file( fname + ".loopfile" , "LOOP" );
	}

	///write superimposed structures
	if ( option[ out::pdb ]() ) {
		FitMoverOP fit_tool = new FitMover;
		fit_tool->weights_ = weights;
		if ( icenter > 1 ) {
			fit_tool->iref = icenter - 1; //ignores the first iref structures before it takes the pose as reference pose.
			JobDistributor::get_instance()->restart();
			JobDistributor::get_instance()->go( fit_tool, new jd2::NoOutputJobOutputter );
		}
		JobDistributor::get_instance()->restart();
		JobDistributor::get_instance()->go( fit_tool );
		if ( option[ in::file::native ].user() ) {
				pose::Pose native_pose;
				core::import_pose::pose_from_pdb( native_pose,
					*core::chemical::ChemicalManager::get_instance()->residue_type_set( chemical::CENTROID ), option[ in::file::native ]() );
				fit_tool->apply( native_pose );
				native_pose.dump_pdb( "fit_native.pdb");
		}
	}

	return;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/// =============================== MAIN ============================================================
/////////////////////////////////////////////////////////////////////////////////////////////////////////
int
main( int argc, char * argv [] )
{
	register_options();
	devel::init( argc, argv );

	try{
		run();
	} catch ( utility::excn::EXCN_Base& excn ) {
		excn.show( std::cerr );
	}

	return 0;
}


