// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/simple_filters/InterfaceSasaFilter.cc
/// @brief
/// @author Sarel Fleishman (sarelf@u.washington.edu), Jacob Corn (jecorn@u.washington.edu)

#include <protocols/simple_filters/InterfaceSasaFilter.hh>
#include <protocols/simple_filters/InterfaceSasaFilterCreator.hh>
#include <core/pose/Pose.hh>
#include <core/id/AtomID_Map.hh>
#include <core/conformation/Residue.hh>
#include <core/chemical/AtomType.hh>
#include <protocols/rigid/RigidBodyMover.hh>
#include <basic/MetricValue.hh>
#include <utility/tag/Tag.hh>
#include <protocols/moves/DataMap.hh>
// Jacob
#include <core/pose/symmetry/util.hh>
#include <core/conformation/symmetry/SymmetryInfo.hh>
#include <core/pose/symmetry/util.hh>

// Project Headers
#include <core/types.hh>

namespace protocols {
namespace simple_filters {

static basic::Tracer interface_sasa_filter_tracer( "protocols.simple_filters.InterfaceSasaFilter" );

protocols::filters::FilterOP
InterfaceSasaFilterCreator::create_filter() const { return new InterfaceSasaFilter; }

std::string
InterfaceSasaFilterCreator::keyname() const { return "Sasa"; }


InterfaceSasaFilter::InterfaceSasaFilter() :
	Filter( "Sasa" ),
	lower_threshold_( 0.0 ),
	hydrophobic_( false ),
	polar_( false ),
	jump_( 1 ),
	upper_threshold_(100000000000.0)
{}

InterfaceSasaFilter::InterfaceSasaFilter( core::Real const lower_threshold, bool const hydrophobic/*=false*/, bool const polar/*=false*/, core::Real upper_threshold ) :
	Filter( "Sasa" ),
	lower_threshold_( lower_threshold ),
	hydrophobic_( hydrophobic ),
	polar_( polar ),
	upper_threshold_(upper_threshold)
{}

InterfaceSasaFilter::~InterfaceSasaFilter(){}

filters::FilterOP
InterfaceSasaFilter::clone() const{
	return new InterfaceSasaFilter( *this );
}

filters::FilterOP
InterfaceSasaFilter::fresh_instance() const{
	return new InterfaceSasaFilter;
}

void
InterfaceSasaFilter::parse_my_tag( utility::tag::TagPtr const tag, moves::DataMap &, filters::Filters_map const &,moves::Movers_map const &, core::pose::Pose const & )
{
	lower_threshold_ = tag->getOption<core::Real>( "threshold", 800 );
	upper_threshold_ = tag->getOption<core::Real>( "upper_threshold", 1000000);
	jump( tag->getOption< core::Size >( "jump", 1 ));
	hydrophobic_ = tag->getOption<bool>( "hydrophobic", false );
	polar_ = tag->getOption<bool>( "polar", false );
	runtime_assert( !hydrophobic_ || !polar_ );
	if( jump() != 1 && ( polar_ || hydrophobic_ ) )
		utility_exit_with_message( "ERROR: presently, only total sasa is supported across a jump other than 1. Remove polar and hydrophobic flags and try again." );

	interface_sasa_filter_tracer<<"SasaFilter with lower threshold of "<<lower_threshold_<<" Ang^2 and jump "<<jump()<<'\n';
	if( hydrophobic_ )
		interface_sasa_filter_tracer<<"Only reporting hydrophobic sasa\n";
	if( polar_ )
		interface_sasa_filter_tracer<<"Only reporting polar sasa\n";
	interface_sasa_filter_tracer.flush();
}
void InterfaceSasaFilter::parse_def( utility::lua::LuaObject const & def,
				utility::lua::LuaObject const & score_fxns,
				utility::lua::LuaObject const & tasks ) {
	lower_threshold_ = def["threshold"] ? def["threshold"].to<core::Real>() : 800;
	upper_threshold_ = def["upper_threshold"] ? def["upper_threshold"].to<core::Real>() : 1000000;
	jump( def["jump"] ? def["jump"].to<core::Size>() : 1 );
	hydrophobic_ = def["hydrophobic"] ? def["hydrophobic"].to<bool>() : false;
	polar_ = def["polar"] ? def["polar"].to<bool>() : false;
	runtime_assert( !hydrophobic_ || !polar_ );
	if( jump() != 1 && ( polar_ || hydrophobic_ ) )
		utility_exit_with_message( "ERROR: presently, only total sasa is supported across a jump other than 1. Remove polar and hydrophobic flags and try again." );

	interface_sasa_filter_tracer<<"SasaFilter with lower threshold of "<<lower_threshold_<<" Ang^2 and jump "<<jump()<<'\n';
	if( hydrophobic_ )
		interface_sasa_filter_tracer<<"Only reporting hydrophobic sasa\n";
	if( polar_ )
		interface_sasa_filter_tracer<<"Only reporting polar sasa\n";
	interface_sasa_filter_tracer.flush();
}

bool
InterfaceSasaFilter::apply( core::pose::Pose const & pose ) const {
	core::Real const sasa( compute( pose ) );

	interface_sasa_filter_tracer<<"sasa is "<<sasa<<". ";
	if( sasa >= lower_threshold_ && sasa <= upper_threshold_ ){
		interface_sasa_filter_tracer<<"passing." <<std::endl;
		return true;
	}
	else {
		interface_sasa_filter_tracer<<"failing."<<std::endl;
		return false;
	}
}

void
InterfaceSasaFilter::report( std::ostream & out, core::pose::Pose const & pose ) const {
	core::Real const sasa( compute( pose ));
	out<<"Sasa= "<< sasa<<'\n';
}

core::Real
InterfaceSasaFilter::report_sm( core::pose::Pose const & pose ) const {
	core::Real const sasa( compute( pose ));
	return( sasa );
}

void
InterfaceSasaFilter::jump( core::Size const jump )
{
	jump_ = jump;
}

core::Size
InterfaceSasaFilter::jump() const
{
	return jump_;
}

core::Real
InterfaceSasaFilter::compute( core::pose::Pose const & pose ) const {
	using namespace core::pose::metrics;
	using basic::MetricValue;
	using namespace core;
	using namespace protocols::moves;

	core::pose::Pose split_pose( pose );
	int sym_aware_jump_id = core::pose::symmetry::get_sym_aware_jump_num(split_pose, jump() ); // JB 120516
	rigid::RigidBodyTransMoverOP translate( new rigid::RigidBodyTransMover( split_pose, sym_aware_jump_id ) ); // JB 120516
	translate->step_size( 1000.0 );
	translate->apply( split_pose );

	runtime_assert( !hydrophobic_ || !polar_ );
	if( !hydrophobic_ && !polar_ ){
		MetricValue< core::Real > mv_sasa;

		pose.metric( "sasa", "total_sasa", mv_sasa);
		core::Real const bound_sasa( mv_sasa.value() );
		split_pose.metric( "sasa", "total_sasa", mv_sasa );
		core::Real const unbound_sasa( mv_sasa.value() );
		if( core::pose::symmetry::is_symmetric( pose )) {
			core::conformation::symmetry::SymmetryInfoCOP sym_info = core::pose::symmetry::symmetry_info(pose);
			core::Real const buried_sasa( (unbound_sasa - bound_sasa) /(sym_info->subunits()));
			return( buried_sasa );
		} else {
			core::Real const buried_sasa(unbound_sasa - bound_sasa);
			return( buried_sasa );
		}
	}
	else{
		MetricValue< id::AtomID_Map< core::Real > > atom_sasa;
		pose.metric( "sasa_interface", "delta_atom_sasa", atom_sasa );
		core::Real polar_sasa( 0.0 ), hydrophobic_sasa( 0.0 );
		for( core::Size pos(1); pos<=pose.total_residue(); ++pos ){
			for( core::Size atomi( 1 ); atomi <= atom_sasa.value().n_atom( pos ); ++atomi ){
				core::Real const atomi_delta_sasa( atom_sasa.value()( pos, atomi ) );
				core::conformation::Residue const pos_rsd( pose.residue( pos ) );
				core::chemical::AtomType const atom_type( pos_rsd.atom_type( atomi ) );
				bool const is_polar( atom_type.is_donor() || atom_type.is_acceptor() || atom_type.is_polar_hydrogen() );
				if( is_polar ) polar_sasa += atomi_delta_sasa;
				else hydrophobic_sasa += atomi_delta_sasa;
			}
		}
		if( hydrophobic_ ) return hydrophobic_sasa;
		if( polar_ ) return polar_sasa;
	}
	utility_exit_with_message( "Execution should never have reached this point." );
	return( 0 );
}

}
}
