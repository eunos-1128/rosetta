// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.
//////////////////////////////////////////////////////////////////////
/// @begin ResidueTypeSet
///
/// @brief
/// Residue Type Set class
///
/// @detailed
/// This class is responsible for iterating through the set of canonical amino acids and non-canonical
/// amino acids. It first reads through a file that contains the location of residues in the database.
/// At the begining of that file are the atom types, mm atom types, element sets, and orbital types that will
/// be used. The sets are all for fa_standard. If a new type of atoms are added for residues, this is where they
/// would be added. Once it assigns the types, it then reads in extra residue params that are passed through the
/// command line. Finally, patches are applied to all residues added.
///
///
/// @authors
/// Phil Bradley
/// Steven Combs - these comments
///
///
///
/////////////////////////////////////////////////////////////////////////


// Rosetta headers
#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/Patch.hh>
#include <core/chemical/ChemicalManager.hh>

//XRW_B_T1
/*
// Commented by inclean daemon #include <core/coarse/Translator.hh>
#include <core/coarse/TranslatorSet.hh>
#include <core/coarse/Rules.hh>
*/
//XRW_E_T1

#include <core/chemical/residue_io.hh>
#include <core/chemical/adduct_util.hh>

// Basic headers
#include <basic/database/open.hh>
#include <basic/options/option.hh>
#include <utility/io/izstream.hh>
#include <basic/Tracer.hh>

// Utility headers
#include  <utility/file/FileName.hh>


// C++ headers
#include <fstream>
#include <string>
#include <sstream>
#include <set>

// option key includes
#include <basic/options/keys/packing.OptionKeys.gen.hh>
#include <basic/options/keys/pH.OptionKeys.gen.hh>
#include <basic/options/keys/chemical.OptionKeys.gen.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <core/chemical/orbitals/AssignOrbitals.hh>

// Boost Headers
#include <boost/foreach.hpp>

#include <utility/vector1.hh>

#define foreach BOOST_FOREACH

namespace core {
namespace chemical {

static basic::Tracer tr("core.chemical.ResidueTypeSet");

///////////////////////////////////////////////////////////////////////////////
/// @brief c-tor from directory
ResidueTypeSet::ResidueTypeSet(
	std::string const & name,
	std::string const & directory,
	std::vector< std::string > const & extra_res_param_files // defaults to empty
) :
	name_( name ),
	database_directory_(directory)
{
	//XRW_B_T1
	//coarse::RuleSetOP coarsify_rule_set;
	//XRW_E_T1
	ResidueTypeSetCAP fine_res_set;
	// read ResidueTypes
	{
		AtomTypeSetCAP atom_types;
		ElementSetCAP elements;
		MMAtomTypeSetCAP mm_atom_types;
		orbitals::OrbitalTypeSetCAP orbital_types;
//		CSDAtomTypeSetCAP csd_atom_types; kwk commenting out until there are fully implemented

		std::string const list_filename( directory + "residue_types.txt" );
		utility::io::izstream data( list_filename.c_str() );
		if ( !data.good() ) {
			utility_exit_with_message( "Unable to open file: " + list_filename + '\n' );
		}
		std::string line,tag;
		while ( getline( data,line) ) {
			bool no_proton_states = false;
			if ( line.size() < 1 || line[0] == '#' ) continue;
			// kp don't consider files for protonation versions of the residues if flag pH_mode is not used
			// to make sure even applications that use ResidueTypeSet directly never run into problems
			if ( line.size() > 20 ){
			  if ( ( !basic::options::option[ basic::options::OptionKeys::pH::pH_mode ].user() ) &&
			       ( line.substr (14,6) == "proton" ) )
				  no_proton_states = true;
			}
			if ( no_proton_states ) continue;
			std::istringstream l( line );
			l >> tag;
			if ( tag == "ATOM_TYPE_SET" ) {
				l >> tag;
				atom_types = ChemicalManager::get_instance()->atom_type_set( tag );
			} else if ( tag == "ELEMENT_SET" ) {
				l >> tag;
				elements = ChemicalManager::get_instance()->element_set( tag );
			} else if ( tag == "MM_ATOM_TYPE_SET" ) {
				l >> tag;
				mm_atom_types = ChemicalManager::get_instance()->mm_atom_type_set( tag );
			} else if(tag == "ORBITAL_TYPE_SET"){
				l >> tag;
				orbital_types = ChemicalManager::get_instance()->orbital_type_set(tag);
				// kwk commenting out until the CSD_ATOM_TYPE_SET has been fully implemented
	//			} else if ( tag == "CSD_ATOM_TYPE_SET" ) {
//				l >> tag;
//				csd_atom_types = ChemicalManager::get_instance()->csd_atom_type_set( tag );
//XRW_B_T1
/*
			} else if ( tag == "COARSE_RULE" ) {
				l >> tag; // the ruleset
				coarsify_rule_set = new coarse::RuleSet(tag);
				l >> tag; // the fine residue set
				fine_res_set = ChemicalManager::get_instance()->residue_type_set( tag );
*/
//XRW_E_T1
			} else {
				std::string const filename( directory + line );
				ResidueTypeOP rsd_type( read_topology_file( filename, atom_types, elements, mm_atom_types, orbital_types, this ) ); //, csd_atom_types ) );
				if(basic::options::option[ basic::options::OptionKeys::in::add_orbitals]){
					orbitals::AssignOrbitals add_orbitals_to_residue(rsd_type);
					add_orbitals_to_residue.assign_orbitals();
				}
				//kwk commenting out csd_atom_types until they have been fully implemented
				residue_types_.push_back( rsd_type );
			}
		}

		foreach(std::string filename, extra_res_param_files){
			ResidueTypeOP rsd_type( read_topology_file( filename, atom_types, elements, mm_atom_types, orbital_types, this ) ); //, csd_atom_types ) );
			// kwk commenting out csd atom types until they have been fully implemented
			if(basic::options::option[ basic::options::OptionKeys::in::add_orbitals]){
				orbitals::AssignOrbitals add_orbitals_to_residue(rsd_type);
				add_orbitals_to_residue.assign_orbitals();
			}
			residue_types_.push_back( rsd_type );
		}

		update_residue_maps();
	}

	// now apply patches
	{
		std::string const list_filename( directory+"/patches.txt" );
		utility::io::izstream data( list_filename.c_str() );

		if ( !data.good() ) {
			utility_exit_with_message( "Unable to open file: "+list_filename );
		}

		// Read the command line and avoid applying patches that the user has requested be
		// skipped.  The flag allows the user to specify a patch by its name or by its file.
		// E.g. "SpecialRotamer" or "SpecialRotamer.txt".  Directory names will be ignored if given.
		std::set< std::string > patches_to_avoid;
		if ( basic::options::option[ basic::options::OptionKeys::chemical::exclude_patches ].user() ) {
			utility::vector1< std::string > avoidlist = basic::options::option[ basic::options::OptionKeys::chemical::exclude_patches ];
			for ( Size ii = 1; ii <= avoidlist.size(); ++ii ) {
				utility::file::FileName fname( avoidlist[ ii ] );
				patches_to_avoid.insert( fname.base() );
			}
		}

		utility::vector1< std::string > patch_filenames;
		std::string line;
		while ( getline( data,line) ) {
			if ( line.size() < 1 || line[0] == '#' ) continue;

			// Skip this patch if the "patches_to_avoid" set contains the named patch.
			utility::file::FileName fname( line );
			if ( patches_to_avoid.find( fname.base() ) != patches_to_avoid.end() ) {
				tr << "While generating ResidueTypeSet " << name << ": Skipping patch " << fname.base() << " as requested" << std::endl;
				continue;
			}
			patch_filenames.push_back( directory + line );
		}
		apply_patches( patch_filenames );
	}

	// Generate combinations of adducts as specified by the user
	place_adducts();

//	for( Size ii = 1 ; ii <= residue_types_.size() ; ++ii ) {
//		residue_types_[ii]->debug_dump_icoor();
//	}

//XRW_B_T1
/*
	if ( coarsify_rule_set ) {
		coarsifier_ = new coarse::TranslatorSet(*coarsify_rule_set, fine_res_set, this);
	}
*/
//XRW_E_T1

	tr << "Finished initializing " << name << " residue type set.  Created " << residue_types_.size() << " residue types" << std::endl;
}


ResidueTypeSet::ResidueTypeSet() {}


ResidueTypeSet::~ResidueTypeSet() {}

///////////////////////////////////////////////////////////////////////////////
/// @details the file contains a list of names of residue type parameter files
/// stored in the database path
///
void
ResidueTypeSet::read_list_of_residues(
	std::string const & list_filename,
	AtomTypeSetCAP atom_types,
	ElementSetCAP elements,
	MMAtomTypeSetCAP mm_atom_types,
	orbitals::OrbitalTypeSetCAP orbital_types//,
//	CSDAtomTypeSetCAP csd_atom_types //kwk commented out until they hae been fully implemented
)
{

	// read the files
	utility::vector1< std::string > filenames;
	{
		utility::io::izstream data( list_filename.c_str() );
		std::string line;
		while ( getline( data, line ) ) {
			// add full database path to the AA.params filename
			filenames.push_back( basic::database::full_name( line ) );
		}
		data.close();
	}

	read_files( filenames, atom_types, elements, mm_atom_types, orbital_types ); // , csd_atom_types ); //kwk commented out csd atom types until they have been fully implemented
}

///////////////////////////////////////////////////////////////////////////////
void
ResidueTypeSet::read_files(
	utility::vector1< std::string > const & filenames,
	AtomTypeSetCAP atom_types,
	ElementSetCAP elements,
	MMAtomTypeSetCAP mm_atom_types,
	orbitals::OrbitalTypeSetCAP orbital_types//,
//	CSDAtomTypeSetCAP csd_atom_types //kwk commented out csd atomtypes until they have been fully implemented
)
{
	for ( Size ii=1; ii<= filenames.size(); ++ii ) {
		ResidueTypeOP rsd_type( read_topology_file( filenames[ii], atom_types, elements, mm_atom_types,orbital_types, this ) ); //, csd_atom_types ) );
		//Commented out csd_atom_types until they have been fully implemented
		residue_types_.push_back( rsd_type );
	}

	update_residue_maps();
}

///////////////////////////////////////////////////////////////////////////////
/// @brief apply patches to base ResidueType to generate variant ResidueTyes
///
/// @details loop through all the existing ResidueTypes and apply patch files
/// given certain selector rules to each of the ResidueTypes to create new variant
/// types. Patches can be applied combinatorially to create more variant types, e.g.,
/// a residue with both N-terminus and C-terminus patches.
///
void
ResidueTypeSet::apply_patches(
	std::string const & list_filename
)
{

	// read the files
	utility::vector1< std::string > filenames;
	{
		utility::io::izstream data( list_filename.c_str() );
		std::string line;
		while ( getline( data, line ) ) {
			// add full database path to the patch filename
			filenames.push_back( basic::database::full_name( line ) );
		}
		data.close();
	}

	apply_patches( filenames );
}

	//XRW_B_T1
	/*
///////////////////////////////////////////////////////////////////////////////
bool
ResidueTypeSet::coarsify(
	pose::Pose &coarse_pose, pose::Pose const &fine_pose
) const
{
	if (coarsifier_) {
		coarsifier_->coarsify(coarse_pose,fine_pose);
		return true;
	} else
		return false;
}
	*/
	 //XRW_E_T1

///////////////////////////////////////////////////////////////////////////////
void
ResidueTypeSet::apply_patches(
	utility::vector1< std::string > const & filenames
)
{
	for ( Size ii=1; ii<= filenames.size(); ++ii ) {
		Patch p;
		p.read_file( filenames[ii] );
		Size const current_n_residue( residue_types_.size() );
		for ( Size i=1; i<= current_n_residue; ++i ) {
			ResidueType const & rsd_type( *residue_types_[ i ] );
			if ( p.applies_to( rsd_type ) ) {
				if ( p.replaces( rsd_type ) ) {
					residue_types_[ i ] = p.apply( rsd_type );
				} else {
					ResidueTypeOP new_rsd_type( p.apply( rsd_type ) );
					if ( new_rsd_type ) {
						residue_types_.push_back( new_rsd_type );
					}
				}
			}
		}
	}

	update_residue_maps();


}

///////////////////////////////////////////////////////////////////////////////
//private
/// @brief clear residue  maps
void
ResidueTypeSet::clear_residue_maps()
{
	aa_map_.clear();
	name3_map_.clear();
	name_map_.clear();
	nonconst_name_map_.clear();
	aas_defined_.clear();
	residue_types_const_.clear();
}


///////////////////////////////////////////////////////////////////////////////
//private
/// @details check that residue id map should be unique,
/// sort the aas_defined list and make its membe unique
///
void
ResidueTypeSet::update_residue_maps()
{

	clear_residue_maps();

	assert( residue_types_const_.empty() );

	for ( 	ResidueTypeOPs::iterator rsdtype_it(residue_types_.begin() ), rsdtype_end( residue_types_.end() );
			rsdtype_it != rsdtype_end; ++rsdtype_it ) {
	//for ( Residue int ires=1; ires<= nres; ++ires ) {  ///TODO Use an iterator
		ResidueType & rsd( **rsdtype_it );
		add_residue_type_to_maps( rsd );
	}
	aas_defined_.sort();
	aas_defined_.unique();
}

void
ResidueTypeSet::add_residue_type_to_maps( ResidueType & rsd )
{
	ResidueType const * rsd_ptr = &rsd;

	residue_types_const_.push_back( rsd_ptr );

	// name should be unique!
	if ( name_map_.count( rsd.name() ) ) {
		std::stringstream err_msg;
		err_msg << "Attempting to add a residue type with name '" + rsd.name() + "' ";
		err_msg << "but this name is already taken." << std::endl;
		err_msg << "Please check your residue parameter files." << std::endl;
		utility_exit_with_message(err_msg.str());
	}
	name_map_[ rsd.name() ] = rsd_ptr;
	nonconst_name_map_[ rsd.name() ] = &rsd;


	// map by AA
	if ( rsd.aa() != aa_unk ) {
		aa_map_[ rsd.aa() ].push_back( rsd_ptr );
	}

	// add aa type
	aas_defined_.push_back( rsd.aa() );

	// map by pdb string
	name3_map_[ rsd.name3() ].push_back( rsd_ptr );

	// For specialty amino acids, add them to the name three maps both with their PDB strings and
	// with their specialty string -- the first three letters of the residue name.
	// E.g. CYD will appear in both lists for name3_map_[ "CYS" ] and name3_map_[ "CYD" ]
	if ( rsd.name3() != rsd.name().substr(0,3) ) {
		name3_map_[ rsd.name().substr(0,3) ].push_back( rsd_ptr );
	}

}

	///@brief beginning of aas_defined_ list
std::list< AA >::const_iterator
ResidueTypeSet::aas_defined_begin() const
{
	return aas_defined_.begin();
}
	///@brief end of aas_defined_ list
std::list< AA >::const_iterator
ResidueTypeSet::aas_defined_end() const
{
	return aas_defined_.end();
}

///////////////////////////////////////////////////////////////////////////////
/// @details selection done by ResidueSelector class
///
void
ResidueTypeSet::select_residues(
	ResidueSelector const & selector,
	ResidueTypeCAPs & matches
) const
{
	for ( ResidueTypeOPs::const_iterator iter=residue_types_.begin(), iter_end = residue_types_.end(); iter!= iter_end; ++iter ) {
		if ( selector[ **iter ] ) {
			matches.push_back( (*iter)() );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
/// @details return the first match with both base ResidueType id and variant_type
/// name. Abort if there is no match
///
/// @note currently, this will not work for variant types defined as alternate
/// base residues (ie different params files)
ResidueType const &
ResidueTypeSet::get_residue_type_with_variant_added( ResidueType const & init_rsd, VariantType const & new_type ) const
{
	if ( init_rsd.has_variant_type( new_type ) ) return init_rsd;

	// find all residues with the same base name as init_rsd
	std::string const base_name( residue_type_base_name( init_rsd ) );

	// the desired set of variant types:
	utility::vector1< VariantType > target_variants( init_rsd.variant_types() );
	if ( !init_rsd.has_variant_type(new_type) ) target_variants.push_back( new_type );

	Size const nvar( target_variants.size() );

	// now look for residue_type with same base_name and the desired set of variants
	for ( ResidueTypeOPs::const_iterator iter= residue_types_.begin(), iter_end = residue_types_.end();
				iter != iter_end; ++iter ) {
		ResidueType const & rsd( **iter );
		if ( residue_type_base_name( rsd ) == base_name && rsd.variant_types().size() == nvar ) {
			bool match( true );
			for ( Size i=1; i<= nvar; ++i ) {
				if ( !rsd.has_variant_type( target_variants[i] ) ) {
					match = false;
					break;
				}
			}
			if ( match == true ) {
				return rsd;
			}
		}
	}
	utility_exit_with_message( "unable to find desired variant residue: "+init_rsd.name()+" "+base_name+" "+new_type);
	// wont get here:
	return *( name_map_.begin()->second );
}

///////////////////////////////////////////////////////////////////////////////
ResidueType const &
ResidueTypeSet::get_residue_type_with_variant_removed( ResidueType const & init_rsd, VariantType const & old_type) const
{
	if ( !init_rsd.has_variant_type( old_type ) ) return init_rsd; // already done

	// find all residues with the same base name as init_rsd
	std::string const base_name( residue_type_base_name( init_rsd ) );

	// the desired set of variant types:
	utility::vector1< VariantType > target_variants( init_rsd.variant_types() );
	target_variants.erase( std::find( target_variants.begin(), target_variants.end(), old_type ) );

	Size const nvar( target_variants.size() );

	// now look for residue_type with same base_name and the desired set of variants
	for ( ResidueTypeOPs::const_iterator iter= residue_types_.begin(), iter_end = residue_types_.end();
				iter != iter_end; ++iter ) {
		ResidueType const & rsd( **iter );
		if ( residue_type_base_name( rsd ) == base_name && rsd.variant_types().size() == nvar ) {
			bool match( true );
			for ( Size i=1; i<= nvar; ++i ) {
				if ( !rsd.has_variant_type( target_variants[i] ) ) {
					match = false;
					break;
				}
			}
			if ( match == true ) {
				return rsd;
			}
		}
	}
	utility_exit_with_message( "unable to find desired non-variant residue: "+init_rsd.name()+" "+base_name+" "+old_type);
	// wont get here:
	return *( name_map_.begin()->second );
}

///////////////////////////////////////////////////////////////////////////////
/// @details Generation of new residue types augmented by adduct atoms
///
void
ResidueTypeSet::place_adducts()
{
	// First parse the command line for requested adducts
	utility::options::StringVectorOption & add_set
		= basic::options::option[ basic::options::OptionKeys::packing::adducts ];

	// No adducts, skip out
	if( add_set.size() == 0 ) return;

	// Convert to a map that takes a string descriptor of an adduct and
	// gives the max number of adducts of that class to apply, so
	// a command line option of -adducts <adduct_type> 2 for a type that has
	// 5 entries in the rsd param file will create all combinations with up to
	// 2 total adducts.

	AdductMap add_map = parse_adduct_string( add_set );

	// Error check each requested adduct from the command line, and
	// complain if there are no examples in any residues.  This function
	// will not return if
	error_check_requested_adducts( add_map, residue_types() );

	// Set up a starting point map where the int value is the number
	// of adducts of a given type placed
		AdductMap blank_map( add_map );
		for( AdductMap::iterator add_iter = blank_map.begin(),
						end_iter = blank_map.end() ;
					add_iter != end_iter ; ++add_iter ) {
			add_iter->second = 0;
		}

	// Process the residues in turn
	for ( ResidueTypeOPs::const_iterator iter= residue_types_.begin(), iter_end = residue_types_.end();
				iter != iter_end; ++iter ) {
		ResidueType const & rsd( **iter );
		AdductMap count_map( blank_map );
		utility::vector1< bool > add_mask( rsd.defined_adducts().size(), false  );
		create_adduct_combinations( rsd, add_map, count_map, add_mask, rsd.defined_adducts().begin() );
	}

//	utility_exit_with_message( "Debug stop point \n" );

	update_residue_maps();

}

void
ResidueTypeSet::add_residue_type( ResidueTypeOP new_type )
{
	if(basic::options::option[ basic::options::OptionKeys::in::add_orbitals]){
		orbitals::AssignOrbitals add_orbitals_to_residue(new_type);
		add_orbitals_to_residue.assign_orbitals();
	}
	residue_types_.push_back( new_type );
	add_residue_type_to_maps( *new_type );
	aas_defined_.sort();
	aas_defined_.unique();
}

/// @brief Create correct combinations of adducts for a residue type
void ResidueTypeSet:: create_adduct_combinations(
	ResidueType const & rsd,
	AdductMap ref_map,
	AdductMap count_map,
	utility::vector1< bool > add_mask,
	utility::vector1< Adduct >::const_iterator work_iter
)
{

	if( work_iter == rsd.defined_adducts().end() ) {
		// Skip the 'no adduct' case - that has already been
		// made when reading in files
		if( std::find( add_mask.begin(), add_mask.end(), true ) == add_mask.end() ) {
			return;
		}
		// Make this combo and return;
//		std::cout << "Making an adduct" << std::endl;

		utility::vector1< Adduct >::const_iterator add_iter = rsd.defined_adducts().begin() ;
		foreach(bool make, add_mask){
			std::cout << "Adduct " << add_iter->adduct_name() << " make is " << make << std::endl;
			++add_iter;
		}

		// Farm this out to a helper function
		residue_types_.push_back( apply_adducts_to_residue( rsd, add_mask ) );

		return;
	}

	// Traverse the 'make' branch for this adduct if:
	// 1. The adduct is in the map of requested adducts
	// 2. we haven't exceeded the count limit for this adduct
	AdductMap::iterator test_iter =
			ref_map.find( work_iter->adduct_name() );

	if ( test_iter != ref_map.end() &&
				count_map[ test_iter->first ] < ref_map[ test_iter->first ]   ) {
		AdductMap new_count_map( count_map );
		new_count_map[ work_iter->adduct_name() ]++;
		utility::vector1< bool > new_add_mask( add_mask );
		// This following line may not work if the Adducts are no longer
		// stored in a vector
		new_add_mask[ work_iter - rsd.defined_adducts().begin() + 1 ] = true;
		create_adduct_combinations( rsd, ref_map, new_count_map, new_add_mask, work_iter+1 );
	}

	// Always traverse the 'do not make' for this adduct
	// The count is not incremented, and the mask is left at the default (false)
	AdductMap new_count_map( count_map );
	utility::vector1< bool > new_add_mask( add_mask );
	create_adduct_combinations( rsd, ref_map, new_count_map, new_add_mask, work_iter+1 );

}



///////////////////////////////////////////////////////////////////////////////

} // pose
} // core
