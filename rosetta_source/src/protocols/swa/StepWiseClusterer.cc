// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file StepWiseClusterer
/// @brief Not particularly fancy, just filters a list of poses.
/// @detailed
/// @author Rhiju Das


//////////////////////////////////
#include <protocols/swa/StepWiseClusterer.hh>
#include <protocols/swa/StepWiseUtil.hh>

//////////////////////////////////
#include <core/types.hh>
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/io/silent/SilentStruct.hh>
#include <core/io/silent/SilentFileData.hh>
#include <core/import_pose/pose_stream/PoseInputStream.hh>
#include <core/import_pose/pose_stream/PoseInputStream.fwd.hh>
#include <core/import_pose/pose_stream/SilentFilePoseInputStream.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/rms_util.tmpl.hh>
#include <core/pose/datacache/CacheableDataType.hh>
#include <basic/datacache/BasicDataCache.hh>
#include <basic/datacache/CacheableString.hh>
#include <basic/Tracer.hh>
#include <utility/vector1.hh>
#include <utility/tools/make_vector1.hh>

#include <ObjexxFCL/format.hh>
#include <ObjexxFCL/string.functions.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>

#include <list>

using namespace core;
using core::Real;

static basic::Tracer TR( "protocols.swa.stepwise_clusterer" ) ;
using namespace basic::options;
using namespace basic::options::OptionKeys;

namespace protocols {
namespace swa {


  //////////////////////////////////////////////////////////////////////////
  //constructor!
  StepWiseClusterer::StepWiseClusterer( utility::vector1< std::string > const & silent_files_in )
  {
		initialize_parameters_and_input();
		input_->set_record_source( true );
		input_->filenames( silent_files_in ); //triggers read in of files, too.
  }

  StepWiseClusterer::StepWiseClusterer( std::string const & silent_file_in )
	{
		initialize_parameters_and_input();
		input_->set_record_source( true );

		utility::vector1< std::string > silent_files_;
		silent_files_.push_back( silent_file_in );
		input_->filenames( silent_files_ ); //triggers read in of files, too.
	}

  StepWiseClusterer::StepWiseClusterer( core::io::silent::SilentFileDataOP & sfd )
	{
		initialize_parameters_and_input();
		input_->set_silent_file_data( sfd ); // triggers reordering by energy and all that.
	}


  //////////////////////////////////////////////////////////////////////////
  //destructor
  StepWiseClusterer::~StepWiseClusterer()
  {}

  //////////////////////////////////////////////////////////////////////////
	void
  StepWiseClusterer::initialize_parameters_and_input(){
		max_decoys_ = 400;
		cluster_radius_ = 1.5;
		cluster_by_all_atom_rmsd_ = true;
		score_diff_cut_ = 1000000.0;
		auto_tune_ = false;
		rename_tags_ = false;
		force_align_ = false;

		score_min_ =  0.0 ;
		score_min_defined_ = false;

		input_  = new core::import_pose::pose_stream::SilentFilePoseInputStream();
		input_->set_order_by_energy( true );

		cluster_rmsds_to_try_with_auto_tune_ = utility::tools::make_vector1( 0.1, 0.2, 0.25, 0.3, 0.4, 0.5, 0.6, 0.8, 1.0, 1.2, 1.5, 1.75, 2.0 );
		hit_score_cutoff_ = false;
		initialized_atom_id_map_for_rmsd_ = false;

		rsd_type_set_ = option[ in::file::residue_type_set ]();
	}

  //////////////////////////////////////////////////////////////////////////
	void
  StepWiseClusterer::cluster()
	{
		using namespace core::scoring;
		using namespace core::import_pose::pose_stream;
		using namespace core::chemical;
		using namespace core::pose;

		rsd_set_ = core::chemical::ChemicalManager::get_instance()->residue_type_set( rsd_type_set_ );

		// basic initialization
		initialize_cluster_list();

		if ( auto_tune_ ) {
			cluster_with_auto_tune();
		} else {
			do_some_clustering();
		}

	}


	/////////////////////////////////////////////////////////////////////
	void
	StepWiseClusterer::initialize_corresponding_atom_id_map( core::pose::Pose const & pose ){
		using namespace core::scoring;

			// Only need to do this once!!!
		if( cluster_by_all_atom_rmsd_ ) {
			setup_matching_heavy_atoms( pose, pose, corresponding_atom_id_map_ );
		} else {
			setup_matching_protein_backbone_heavy_atoms( pose, pose, corresponding_atom_id_map_ );
		}
		initialized_atom_id_map_for_rmsd_ = true;
	}

	/////////////////////////////////////////////////////////////////////
	void
	StepWiseClusterer::do_some_clustering() {

		using namespace core::pose;

		hit_score_cutoff_ = false;
		while ( input_->has_another_pose() ) {

			PoseOP pose_op( new Pose );
			core::io::silent::SilentStructOP silent_struct( input_->next_struct() );
			silent_struct->fill_pose( *pose_op, *rsd_set_ );

			Real score( 0.0 );
			getPoseExtraScores( *pose_op, "score", score );

			if ( !score_min_defined_ ){
				score_min_ = score;
				score_min_defined_ = true;
			}

			if ( score > score_min_ + score_diff_cut_ ) {
				hit_score_cutoff_ = true;
				break;
			}

			std::string tag( silent_struct->decoy_tag() );
			TR << "CHECKING " << tag << " with score " << score << " against list of size " << pose_output_list_.size();

			bool const OK = check_for_closeness( pose_op );
			if ( OK )  {
				TR << " ... added. " << std::endl;
				if ( pose_output_list_.size() >= max_decoys_ ) break;
				tag_output_list_.push_back(  tag );
				pose_output_list_.push_back(  pose_op );
				silent_struct_output_list_.push_back(  silent_struct  );
			} else{
				TR << " ... not added. " << std::endl;
			}
		}

	  TR << "After clustering, number of decoys: " << pose_output_list_.size() << std::endl;
		return;

	}


	/////////////////////////////////////////////////////////////////////
	void
	StepWiseClusterer::initialize_cluster_list() {

		pose_output_list_.clear();
		tag_output_list_.clear();
		silent_struct_output_list_.clear();

		score_min_ =  0.0 ;
		score_min_defined_ = false;

		hit_score_cutoff_ = false;
	}

	/////////////////////////////////////////////////////////////////////
	void
	StepWiseClusterer::cluster_with_auto_tune() {

		for ( Size n = 1; n <= cluster_rmsds_to_try_with_auto_tune_.size(); n++ ) {

			cluster_radius_ = cluster_rmsds_to_try_with_auto_tune_[ n ];

			//can current cluster center list be shrunk, given the new cluster_radius cutoff?
			recluster_current_pose_list();

			do_some_clustering();

			if ( hit_score_cutoff_ ) break;
			if ( !input_->has_another_pose() ) break;
		}

	}


	/////////////////////////////////////////////////////////////////////
	void
	StepWiseClusterer::recluster_current_pose_list() {

		utility::vector1< core::pose::PoseOP > old_pose_output_list = pose_output_list_;
		utility::vector1< std::string > old_tag_output_list = tag_output_list_;
		utility::vector1< core::io::silent::SilentStructOP > old_silent_struct_output_list = silent_struct_output_list_;

		pose_output_list_.clear();
		tag_output_list_.clear();
		silent_struct_output_list_.clear();

		for ( Size i = 1; i <= old_pose_output_list.size(); i++ ) {

			core::pose::PoseOP pose_op = old_pose_output_list[ i ];

			bool const OK = check_for_closeness( pose_op );
			if ( OK )  {
				tag_output_list_.push_back(  old_tag_output_list[ i ] );
				pose_output_list_.push_back(  old_pose_output_list[ i ] );
				silent_struct_output_list_.push_back(  old_silent_struct_output_list[ i ]  );
			}

		}

		TR <<  "After reclustering with rmsd " << cluster_radius_ << ", number of clusters reduced from " <<
			old_pose_output_list.size() << " to " << pose_output_list_.size() << std::endl;

	}


	///////////////////////////////////////////////////////////////
	bool
	StepWiseClusterer::check_for_closeness( core::pose::PoseOP const & pose_op )
	{
		using namespace core::scoring;

		if ( !initialized_atom_id_map_for_rmsd_ ) initialize_corresponding_atom_id_map( *pose_op );

		// go through the list backwards, because poses may be grouped by similarity --
		// the newest pose is probably closer to poses at the end of the list.
		for ( Size n = pose_output_list_.size(); n >= 1; n-- ) {

			Real rmsd( 0.0 );

			//			std::cout << "checking agaist pose " << n << " out of " << pose_output_list_.size() << std::endl;
			//			std::cout << " pose_output_list_[ n ]->size " << pose_output_list_[ n ]->total_residue() << std::endl;
			//			std::cout << " pose_op->size " << pose_op->total_residue() << std::endl;

			if ( calc_rms_res_.size() == 0 ) {
				rmsd = rms_at_corresponding_atoms( *(pose_output_list_[ n ]), *pose_op, corresponding_atom_id_map_ );
			} else if ( force_align_ ) {
				rmsd = rms_at_corresponding_atoms( *(pose_output_list_[ n ]), *pose_op, corresponding_atom_id_map_, calc_rms_res_ );
			} else {
				// assumes prealignment of poses!!!
				rmsd = rms_at_corresponding_atoms_no_super( *(pose_output_list_[ n ]), *pose_op,
																										corresponding_atom_id_map_, calc_rms_res_ );
			}

			if ( rmsd < cluster_radius_ )	{
				//std::cout << "[ " << rmsd << " inside cutoff " << cluster_radius_ << " ] " ;
				//				std::cout << "FAIL! " << n << " " << rmsd << " " << cluster_radius_ << std::endl;
				return false;
			}
		}
		return true;
	}


	/////////////////////////////////////////////////////////////////////////////////////////
	void
	StepWiseClusterer::output_silent_file( std::string const & silent_file ){

		using namespace core::io::silent;

		SilentFileData silent_file_data;

		for ( Size n = 1 ; n <= silent_struct_output_list_.size(); n++ ) {

			SilentStructOP & s( silent_struct_output_list_[ n ] );

			if ( rename_tags_ ){
				s->add_comment( "PARENT_TAG", s->decoy_tag() );
				std::string const tag = "S_"+ ObjexxFCL::string_of( n-1 /* start with zero */);
				s->set_decoy_tag( tag );
			}

			silent_file_data.write_silent_struct( *s, silent_file, false /*write score only*/ );

		}

	}

	/////////////////////////////////////////////////////////////////////////////////////////
	core::io::silent::SilentFileDataOP
	StepWiseClusterer::silent_file_data(){

		using namespace core::io::silent;

		SilentFileDataOP silent_file_data = new SilentFileData;
		for ( Size n = 1 ; n <= silent_struct_output_list_.size(); n++ ) {
			silent_file_data->add_structure( silent_struct_output_list_[ n ] );
		}
		return silent_file_data;
	}

	//////////////////////////////////////////////
	PoseList
	StepWiseClusterer::clustered_pose_list(){

		PoseList pose_list;

		for ( Size n = 1 ; n <= pose_output_list_.size(); n++ ) {
			pose_list[ tag_output_list_[n] ] = pose_output_list_[ n ];
		}

		return pose_list;
	}


  //////////////////////////////////////////////////////////////////////////
	void
	StepWiseClusterer::set_calc_rms_res( utility::vector1< core::Size > const & calc_rms_res ){
		calc_rms_res_ = calc_rms_res;
	}

  //////////////////////////////////////////////////////////////////////////
	void
	StepWiseClusterer::set_silent_file_data( core::io::silent::SilentFileDataOP & sfd ){
		input_->set_silent_file_data( sfd );
	}


}
}
