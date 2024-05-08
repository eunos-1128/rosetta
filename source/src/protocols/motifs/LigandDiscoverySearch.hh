// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file
/// @brief aginsparg, ipatel, sthyme; this script uses a target receptor, list of motifs, and input ligand(s) to attempt to fit ligands against the receptor against a specified residue index


#include <core/types.hh>
#include <core/chemical/ResidueType.fwd.hh>
#include <core/chemical/MutableResidueType.fwd.hh>
#include <core/chemical/ResidueTypeSet.fwd.hh>
#include <core/chemical/ResidueTypeFinder.fwd.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/scoring/ScoreFunctionFactory.fwd.hh>
#include <core/id/AtomID.fwd.hh>
#include <protocols/dna/RestrictDesignToProteinDNAInterface.fwd.hh>
#include <protocols/motifs/LigandMotifSearch.fwd.hh>
#include <protocols/motifs/MotifLibrary.fwd.hh>
#include <core/conformation/Residue.fwd.hh>
#include <core/conformation/Conformation.fwd.hh>
#include <core/chemical/ChemicalManager.fwd.hh>
#include <protocols/motifs/Motif.fwd.hh>
#include <protocols/motifs/MotifHit.fwd.hh>
#include <core/pack/rotamer_set/RotamerSet.fwd.hh>
#include <core/pack/rotamer_set/RotamerSetFactory.fwd.hh>
#include <protocols/motifs/BuildPosition.fwd.hh>
#include <core/scoring/methods/EnergyMethodOptions.fwd.hh>
#include <core/kinematics/MoveMap.fwd.hh>
#include <core/scoring/constraints/ConstraintSet.fwd.hh>
#include <protocols/minimization_packing/MinMover.fwd.hh>
#include <core/scoring/Energies.fwd.hh>
#include <core/scoring/EnergyMap.fwd.hh>
#include <core/scoring/hbonds/HBondSet.fwd.hh>
#include <core/chemical/AtomTypeSet.fwd.hh>
#include <core/chemical/ChemicalManager.hh>

#include <core/pose/PDBInfo.fwd.hh>

#include <core/chemical/GlobalResidueTypeSet.fwd.hh>

#include <protocols/dna/PDBOutput.fwd.hh>
#include <protocols/dna/util.hh>
#include <protocols/dna/util.hh>
#include <protocols/dna/util.hh>


#include <basic/prof.fwd.hh>
#include <basic/Tracer.fwd.hh>



// Utility Headers
#include <utility/io/ozstream.fwd.hh>
#include <utility/file/FileName.fwd.hh>
#include <utility/vector1.fwd.hh>
#include <utility/excn/Exceptions.fwd.hh>

// c++ headers
#include <fstream>
#include <iostream>
#include <string>
#include <queue>
#include <functional>
#include <map>

// option key includes

#include <numeric/xyzVector.fwd.hh>
#include <protocols/ligand_docking/InterfaceScoreCalculator.fwd.hh>

#include <protocols/ligand_docking/MoveMapBuilder.fwd.hh>
#include <protocols/ligand_docking/LigandArea.fwd.hh>
#include <protocols/ligand_docking/InterfaceBuilder.fwd.hh>
#include <protocols/ligand_docking/MoveMapBuilder.fwd.hh>
#include <core/optimization/MinimizerOptions.fwd.hh>
#include <core/kinematics/MoveMap.fwd.hh>
#include <protocols/ligand_docking/HighResDocker.fwd.hh>
#include <protocols/ligand_docking/LigandDockProtocol.fwd.hh>
#include <core/scoring/func/HarmonicFunc.fwd.hh>
#include <core/chemical/PoseResidueTypeSet.fwd.hh>

#include <ObjexxFCL/FArray1D.fwd.hh>


// Time profiling header
#include <time.h>

//for data type debugging
#include <typeinfo>


////////////////////////////////////////////////////////////////////////////////

class LigandDiscoverySearch
{

public:

	////////////////////////////////////////////////////////////////////////////
	//functions/constructors to set up protocol

	// @brief destructor
	~LigandDiscoverySearch();

	// @brief parameterized constructor to load in motif library, pdb, and ligand library
	//LigandDiscoverySearch(core::pose::PoseOP pose_from_PDB, protocols::motifs::MotifCOPs motif_library, utility::vector1<core::conformation::ResidueOP> all_residues, core::Size working_position);
	LigandDiscoverySearch(core::pose::PoseOP pose_from_PDB, protocols::motifs::MotifCOPs motif_library, utility::vector1<core::conformation::ResidueOP> all_residues, utility::vector1<core::Size> working_position);

	// @brief function to load in a library for the search protocol
	void set_motif_library(protocols::motifs::MotifCOPs motif_library);

	// @brief function to load in a library for the search protocol; read in MotifLibrary object and convert to MotifCOPs
	void set_motif_library(protocols::motifs::MotifLibrary motif_library);

	// @brief return contents of motif_library_
	protocols::motifs::MotifCOPs get_motif_library();

	// @brief function to define the residue index that we will use for applying motifs for ligand placements
	void set_working_position(core::Size working_position);

	// @brief return contents of working_position_
	core::Size get_working_position();

	// @brief function to define the vector of residue indices that we will use for applying motifs for ligand placements
	void set_working_positions(utility::vector1<core::Size> working_position);

	// @brief return contents of working_positions_
	utility::vector1<core::Size>  get_working_positions();

	// @brief function to load in a pose for the receptor
	void set_working_pose(core::pose::PoseOP pose_from_PDB);

	// @brief function to load in a pose for the receptor, use a regular pose and convert to pointer
	void set_working_pose(core::pose::Pose pose_from_PDB);

	// @brief function to get working_pose_
	core::pose::PoseOP get_working_pose();

	// @brief function to set ligand residues in all_residues_
	void set_all_residues(utility::vector1<core::conformation::ResidueOP> all_residues);

	// @brief function to get all ligand residues
	utility::vector1<core::conformation::ResidueOP> get_all_residues();

	// @brief function to set value of verbose_ at desire of user when using an LDS object
	int get_verbose();

	// @brief function to set value of verbose_ at desire of user when using an LDS object
	void set_verbose(int verbosity);

	////////////////////////////////////////////////////////////////////////////
	//operation functions

	// @brief main function to run ligand discovery operations
	//needs to have values set for working_pose_, motif_library_, and all_residues_
	//parameter is a string to be a prefix name to use for outputted file names
	core::Size discover(std::string output_prefix);

	// @brief function to get a sub-library of motifs from the main library, based on the residue being used (only get for select residue)
	//function may have use outside discover, so public can use it
	protocols::motifs::MotifCOPs get_motif_sublibrary_by_aa(std::string residue_name);

	// @brief function to hash out an input motif library into a standard map of motifCOPs
	//inputs are initial motif library and map that is to be filled out
	//map keys are tuples of 7 strings, which is the residue involved in the motif and then the names of the atoms involved (3 atoms on both sides of motif; we don't care about ligand name in key)
	std::string hash_motif_library_into_map(protocols::motifs::MotifCOPs & input_library, std::map<std::tuple<std::string, std::string, std::string, std::string, std::string, std::string, std::string>,protocols::motifs::MotifCOPs> & mymap);

private:

	// @brief default constructor
	//will need to use class functions to seed values for input pose, motif library, and ligand library
	//should only use parameterized constructor
	LigandDiscoverySearch();

	// @brief functions to be called in discover() function
	//function to set values for the score functions
	//void seed_score_functions();

	// @brief create protein_representation_matrix_
	//uses working_pose to make the matrix
	void create_protein_representation_matrix(core::Size & x_shift, core::Size & y_shift, core::Size & z_shift, int & x_bound_int, int & y_bound_int, int & z_bound_int);

	// @brief create protein_representation_matrix_space_fill_
	//uses working_pose to make the matrix
	//void LigandDiscoverySearch::create_protein_representation_matrix_space_fill(core::Size & x_shift, core::Size & y_shift, core::Size & z_shift, core::Size & x_bound_int, core::Size & y_bound_int, core::Size & z_bound_int, int & resolution_increase_factor,
		//core::Size & sub_x_min, core::Size & sub_x_max, core::Size & sub_y_min, core::Size & sub_y_max,	core::Size & sub_z_min, core::Size & sub_z_max, core::Real & occupied_ratio, core::Real & sub_occupied_ratio)
	//condensing arguments in function to use vectors to hold xyz trios
	void create_protein_representation_matrix_space_fill(utility::vector1<core::Size> & xyz_shift, utility::vector1<core::Size> & xyz_bound, int & resolution_increase_factor,
		utility::vector1<core::Size> & sub_xyz_min, utility::vector1<core::Size> & sub_xyz_max, utility::vector1<core::Real> & occupied_ratios, utility::vector1<core::Size> & matrix_data_counts);

	// @brief function to run a clash check of the placed ligand against the protein target
	bool ligand_clash_check(core::conformation::ResidueOP ligresOP, core::Size x_shift, core::Size y_shift, core::Size z_shift, int x_bound_int, int y_bound_int, int z_bound_int);

	// @brief function to determine if the placed ligand is satisfactory at filling the binding pocket in question
	utility::vector1<utility::vector1<utility::vector1<core::Size>>> space_fill_analysis(core::conformation::ResidueOP ligresOP, utility::vector1<core::Size> & xyz_shift, utility::vector1<core::Size> & xyz_bound, int & resolution_increase_factor,
		utility::vector1<core::Size> & sub_xyz_min, utility::vector1<core::Size> & sub_xyz_max, utility::vector1<core::Real> & occupied_ratios, utility::vector1<core::Size> & matrix_data_counts);

	// @brief debugging function to export a space fill matrix as a pdb. Occupied cells are represented as a nitrogen and unoccupied cells are represented as an oxygen (considering making one for a clash matrix too)
	// if printing the whole matrix and not just the sub-area, occupied cells are represented by hydrogens and unoccupied are represented by carbon
	core::pose::Pose export_space_fill_matrix_as_C_H_O_N_pdb(utility::vector1<utility::vector1<utility::vector1<core::Size>>> space_fill_matrix, utility::vector1<core::Size> & xyz_shift, utility::vector1<core::Size> & xyz_bound, int & resolution_increase_factor,
			utility::vector1<core::Real> & occupied_ratios, std::string pdb_name_prefix, core::chemical::MutableResidueType dummylig_mrt);

	// @brief function to be used to convert a base 10 number to base 62 (as a string with characters represented by base_62_cipher_)
	//used in export_space_fill_matrix_as_C_H_O_N_pdb to assign a unique name to an atom (due to limitations in atom icoor data, an atom name can be no longer than 4 characters)
	std::string base_10_to_base_62(core::Size starting_num);

	//class variables

	// @brief receptor pose to work with
	core::pose::PoseOP working_pose_;
	// @brief motif library (all motifs for all residues)
	protocols::motifs::MotifCOPs motif_library_;
	// @brief motifs library for select residue
	protocols::motifs::MotifCOPs motif_library_for_select_residue_;
	// @brief ligand library
	utility::vector1<core::conformation::ResidueOP> all_residues_;
	// @brief residue index of protein pdb to attempt to place ligands off of
	core::Size working_position_;
	// @brief vector to hold list of all indices to investigate/use as anchor residues, used to set value of working_position_
	utility::vector1<core::Size> working_positions_;

	// @brief 3D matrix to represent voxelized copy of atoms in pose, used in clash check of placement for quick ruling out of bad placements
	utility::vector1<utility::vector1<utility::vector1<bool>>> protein_representation_matrix_;

	// @brief 3D matrix to represent voxelized copy of atoms in pose in a more geometrically-accurate means than protein_representation_matrix_, used in checking space fill analysis (of binding pocket)
	utility::vector1<utility::vector1<utility::vector1<core::Size>>> protein_representation_matrix_space_fill_;

	// @brief 1D vector to hold a list of residue indices to be considered in space fill function
	utility::vector1<core::Size> target_residues_sf_;

	// @brief 1D vector to hold a list of residue indices to be considered in residue-ligand contacts function
	//utility::vector1<core::Size> target_residues_contact_;

	// @brief a boolean value that controls the verbosity of text output while using the object
	//default to be false, but can be made true by using the motifs::verbose flag; can also use set_verbose()
	int verbose_;

	// @brief a vector to act as a cipher for the space fill matrix with naming atoms with a unique name
	//due to there being a limit in an atom name only having up to 4 characters and ther ebeign potential for a full representation matrix to have 1e5-1e6 atoms, using a base62 system for unique names
	//base 62 allows over 14 million unique names
	//represented as alphanumeric characters 0-9,a-z,A-Z
	utility::vector1<std::string> base_62_cipher_{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", 
		"o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", 
		"U", "V", "W", "X", "Y", "Z", };
};
