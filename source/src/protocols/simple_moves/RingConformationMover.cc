// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file    protocols/simple_moves/RingConformationMover.cc
/// @brief   Method definitions for RingConformationMover.
/// @author  Labonte  <JWLabonte@jhu.edu>

// Unit headers
#include <protocols/simple_moves/RingConformationMover.hh>
#include <protocols/simple_moves/RingConformationMoverCreator.hh>
#include <protocols/moves/Mover.hh>

// Project headers
#include <core/types.hh>
#include <core/chemical/RingConformer.hh>
#include <core/chemical/RingConformerSet.hh>
#include <core/pose/Pose.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/conformation/Residue.hh>

#include <protocols/rosetta_scripts/util.hh>

// Utility headers
#include <utility/tag/Tag.hh>

// Numeric headers
#include <numeric/random/random.hh>

// Basic header
#include <basic/options/option.hh>
#include <basic/options/keys/rings.OptionKeys.gen.hh>
#include <basic/Tracer.hh>

// C++ headers
#include <string>
#include <iostream>


// Construct tracers.
static thread_local basic::Tracer TR( "protocols.simple_moves.RingConformationMover" );


namespace protocols {
namespace simple_moves {

using namespace core;


// Public methods /////////////////////////////////////////////////////////////
// Standard methods ///////////////////////////////////////////////////////////
// Default constructor
/// @details  By default, all rings within a given pose will be allowed to move.
RingConformationMover::RingConformationMover(): Mover()
{
	using namespace kinematics;

	// Set default MoveMap.
	MoveMapOP default_movemap( new MoveMap() );
	default_movemap->set_nu( true );

	init( default_movemap );
}

// Copy constructor
RingConformationMover::RingConformationMover( RingConformationMover const & object_to_copy ) : Mover( object_to_copy )
{
	copy_data( *this, object_to_copy );
}

// Constructor with MoveMap input option
/// @param    <input_movemap>: a MoveMap with desired nu torsions set to true
/// @remarks  Movable cyclic residues will generally be a subset of residues in the MoveMap whose nu
/// torsions are set to true.
RingConformationMover::RingConformationMover( core::kinematics::MoveMapOP input_movemap )
{
	init( input_movemap );
}

// Assignment operator
RingConformationMover &
RingConformationMover::operator=( RingConformationMover const & object_to_copy )
{
	// Abort self-assignment.
	if ( this == &object_to_copy ) {
		return *this;
	}

	Mover::operator=( object_to_copy );
	copy_data( *this, object_to_copy );
	return *this;
}

// Destructor
RingConformationMover::~RingConformationMover() {}


// Standard Rosetta methods ///////////////////////////////////////////////////
// General methods
void
RingConformationMover::register_options()
{
	using namespace basic::options;

	option.add_relevant( OptionKeys::rings::sample_high_energy_conformers );

	// Call register_options() on all other Movers used by this class.
	Mover::register_options();  // Mover's register_options() doesn't do anything; it's just here in principle.
}

void
RingConformationMover::show(std::ostream & output) const
{
	using namespace std;

	Mover::show( output );  // name, type, tag

	if ( locked_ ) {
		output << "This Mover was locked from the command line with the -lock_rings flag " <<
				"and will not do anything!" << endl;
	} else {
		output << "Current MoveMap:" << endl << *movemap_ << endl;
		if ( sample_all_conformers_ ) {
			output << "Sampling from all ideal ring conformations." << endl;
		} else {
			output << "Sampling from only low-energy ring conformations, if known." << endl;
		}
	}
}


// Mover methods
std::string
RingConformationMover::get_name() const
{
	return type();
}

protocols::moves::MoverOP
RingConformationMover::clone() const
{
	return protocols::moves::MoverOP( new RingConformationMover( *this ) );
}

protocols::moves::MoverOP
RingConformationMover::fresh_instance() const
{
	return protocols::moves::MoverOP( new RingConformationMover() );
}

void
RingConformationMover::parse_my_tag(
		TagCOP tag,
		basic::datacache::DataMap & data,
		Filters_map const & /*filters*/,
		moves::Movers_map const & /*movers*/,
		Pose const & pose )
{
	using namespace core::kinematics;

	// Parse the MoveMap tag.
	MoveMapOP mm( new MoveMap );
	protocols::rosetta_scripts::parse_movemap( tag, pose, mm, data );
	movemap_ = mm;

	// Parse option specific to rings.
	if ( tag->hasOption( "sample_high_energy_conformers" ) ) {
		sample_all_conformers_ = tag->getOption< bool >( "sample_high_energy_conformers" );
	}
}


/// @details  The mover will create a list of movable residues based on the given MoveMap and select a residue from
/// the list at random.  The torsion angles of a randomly selected ring conformer will be applied to the selected
/// residue.
/// @param    <input_pose>: the structure to be moved
void
RingConformationMover::apply( Pose & input_pose )
{
	using namespace std;
	using namespace utility;
	using namespace conformation;
	using namespace chemical;

	if ( locked_ ) {
		TR.Warning << "Rings were locked from the command line with the -lock_rings flag.  " <<
				"Did you intend to call this Mover?" << endl;
		return;
	}

	show( TR );

	TR << "Getting movable residues...." << endl;

	setup_residue_list( input_pose );

	if ( residue_list_.empty() ) {
		TR.Warning << "There are no movable cyclic residues available in the given pose." << endl;
		return;
	}

	TR << "Applying " << get_name() << " to pose...." << endl;

	core::uint const i( core::uint( numeric::random::rg().uniform() * residue_list_.size() + 1 ) );
	core::uint const res_num( residue_list_[ i ] );
	Residue const & res( input_pose.residue( res_num ) );

	TR << "Selected residue " << res_num << ": " << res.name() << endl;

	RingConformer conformer;
	if ( sample_all_conformers_ || ( ! res.type().ring_conformer_set()->low_energy_conformers_are_known() ) ) {
		conformer = res.type().ring_conformer_set()->get_random_conformer();
	} else /* default behavior */ {
		conformer = res.type().ring_conformer_set()->get_random_local_min_conformer();
	}

	TR << "Selected the " << conformer.specific_name << " conformation to apply." << endl;

	TR << "Making move...." << endl;

	input_pose.set_ring_conformation( res_num, conformer );

	TR << "Move complete." << endl;
}


// Accessors/Mutators
kinematics::MoveMapCOP
RingConformationMover::movemap() const
{
	return movemap_;
}

void
RingConformationMover::movemap( kinematics::MoveMapOP new_movemap )
{
	movemap_ = new_movemap;
}


// Private methods ////////////////////////////////////////////////////////////
// Set command-line options.  (Called by init())
void
RingConformationMover::set_commandline_options()
{
	using namespace basic::options;

	locked_ = option[ OptionKeys::rings::lock_rings ].user();

	sample_all_conformers_ = option[ OptionKeys::rings::sample_high_energy_conformers ].user();
}

// Initialize data members from arguments.
void
RingConformationMover::init( core::kinematics::MoveMapOP movemap )
{
	type( "RingConformationMover" );

	movemap_ = movemap;
	locked_ = false;
	sample_all_conformers_ = false;

	set_commandline_options();
}

// Copy all data members from <object_to_copy_from> to <object_to_copy_to>.
void
RingConformationMover::copy_data(
		RingConformationMover & object_to_copy_to,
		RingConformationMover const & object_to_copy_from )
{
	object_to_copy_to.movemap_ = object_to_copy_from.movemap_;
	object_to_copy_to.residue_list_ = object_to_copy_from.residue_list_;
	object_to_copy_to.locked_ = object_to_copy_from.locked_;
	object_to_copy_to.sample_all_conformers_ = object_to_copy_from.sample_all_conformers_;
}

// Setup list of movable cyclic residues from MoveMap.
void
RingConformationMover::setup_residue_list( core::pose::Pose & pose )
{
	using namespace conformation;

	residue_list_.clear();

	Size const last_res_num( pose.total_residue() );
	for ( core::uint res_num( 1 ); res_num <= last_res_num; ++res_num ) {
		Residue const & residue( pose.residue( res_num ) );
		if ( residue.type().is_cyclic() ) {
			if ( movemap_->get_nu( res_num ) == true ) {
				residue_list_.push_back( res_num );
			}
		}
	}
}


// Creator methods ////////////////////////////////////////////////////////////
// Return an up-casted owning pointer (MoverOP) to the mover.
protocols::moves::MoverOP
RingConformationMoverCreator::create_mover() const
{
	return moves::MoverOP( new RingConformationMover );
}

// Return the string identifier for the associated Mover (RingConformationMover).
std::string
RingConformationMoverCreator::keyname() const
{
	return RingConformationMoverCreator::mover_name();
}

// Static method that returns the keyname for performance reasons.
std::string
RingConformationMoverCreator::mover_name()
{
	return "RingConformationMover";
}


// Helper methods /////////////////////////////////////////////////////////////
// Insertion operator (overloaded so that RingConformationMover can be "printed" in PyRosetta).
std::ostream &
operator<<( std::ostream & output, RingConformationMover const & object_to_output )
{
	object_to_output.show( output );
	return output;
}

}  // namespace simple_moves
}  // namespace protocols
