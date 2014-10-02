// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/pack/task/operation/ResLvlTaskOperationFactory.cc
/// @brief
/// @author ashworth

#include <core/pack/task/operation/ResLvlTaskOperationFactory.hh>

#ifdef WIN32
#include <core/pack/task/operation/ResLvlTaskOperations.hh>
#include <core/pack/task/operation/ResFilter.hh>

#endif

#include <utility/exit.hh> // runtime_assert, utility_exit_with_message
// AUTO-REMOVED #include <utility/tag/Tag.hh> // REQUIRED FOR WINDOWS

#include <core/pack/task/operation/ResLvlTaskOperation.hh>
#include <core/pack/task/operation/ResLvlTaskOperationCreator.hh>

// Utility headers
#include <utility/vector0.hh>
#include <utility/vector1.hh>
#include <utility/thread/threadsafe_creation.hh>

// C++ headers
#include <iostream>

// Boost headers
#include <boost/bind.hpp>
#include <boost/function.hpp>

// Singleton instance and mutex static data members
namespace utility {

using core::pack::task::operation::ResLvlTaskOperationFactory;

#if defined MULTI_THREADED && defined CXX11
template <> std::mutex utility::SingletonBase< ResLvlTaskOperationFactory > ::singleton_mutex_;
template <> std::atomic< ResLvlTaskOperationFactory * > utility::SingletonBase< ResLvlTaskOperationFactory >::instance_( 0 );
#else
template <> ResLvlTaskOperationFactory * utility::SingletonBase< ResLvlTaskOperationFactory >::instance_( 0 );
#endif

}

namespace core {
namespace pack {
namespace task {
namespace operation {

ResLvlTaskOperationFactory *
ResLvlTaskOperationFactory::create_singleton_instance()
{
	return new ResLvlTaskOperationFactory;
}

void
ResLvlTaskOperationFactory::factory_register( ResLvlTaskOperationCreatorOP creator )
{
	add_creator( creator );
}

ResLvlTaskOperationFactory::ResLvlTaskOperationFactory() {}

ResLvlTaskOperationFactory::~ResLvlTaskOperationFactory(){}

///@brief add a ResLvlTaskOperation prototype, using its default type name as the map key
void
ResLvlTaskOperationFactory::add_creator( ResLvlTaskOperationCreatorOP rltoc )
{
	runtime_assert( rltoc != 0 );
	rltoc_map_[ rltoc->keyname() ] = rltoc;
}

bool ResLvlTaskOperationFactory::has_type( std::string const & type ) const
{
	return ( rltoc_map_.find( type ) != rltoc_map_.end() );
}

///@brief return new ResLvlTaskOperation by key lookup in rltoc_map_ (new ResLvlTaskOperation parses Tag if provided)
ResLvlTaskOperationOP
ResLvlTaskOperationFactory::newRLTO( std::string const & type ) const
{
	RLTOC_Map::const_iterator iter( rltoc_map_.find( type ) );
	if ( iter != rltoc_map_.end() ) {
		ResLvlTaskOperationOP rlto( iter->second->create_res_level_task_operation() );
		return rlto;
	} else {
		utility_exit_with_message( type + " is not known to the ResLvlTaskOperationFactory. Was its ResLvlTaskOperationCreator class registered at initialization?" );
		return NULL;
	}
}

} //namespace operation
} //namespace task
} //namespace pack
} //namespace core
