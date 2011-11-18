// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file StepWiseRNA_Minimizer
/// @brief Not particularly fancy, just minimizes a list of poses.
/// @detailed
/// @author Rhiju Das


//////////////////////////////////
#include <protocols/swa/rna/StepWiseRNA_Clusterer.hh>
#include <protocols/swa/rna/StepWiseRNA_Util.hh>
#include <protocols/swa/StepWiseUtil.hh>

//////////////////////////////////
#include <core/types.hh>
//#include <core/id/AtomID.hh>
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/scoring/rms_util.tmpl.hh>


#include <core/import_pose/pose_stream/PoseInputStream.hh>
#include <core/import_pose/pose_stream/PoseInputStream.fwd.hh>
#include <core/import_pose/pose_stream/SilentFilePoseInputStream.hh>

#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/ChemicalManager.hh>

#include <basic/Tracer.hh>
#include <core/io/silent/SilentFileData.fwd.hh>
#include <core/io/silent/SilentFileData.hh>
#include <core/io/silent/BinaryRNASilentStruct.hh>

#include <core/chemical/VariantType.hh>
#include <core/chemical/util.hh>
#include <core/chemical/AtomType.hh> //Need this to prevent the compiling error: invalid use of incomplete type 'const struct core::chemical::AtomType Oct 14, 2009

#include <ObjexxFCL/format.hh>
#include <ObjexxFCL/string.functions.hh>

#include <utility/exit.hh>
#include <time.h>

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <core/io/pdb/pose_io.hh>
#include <core/pose/Pose.hh>
#include <core/pose/PDBInfo.hh>
//#include <utility/io/ozstream.hh>
//#include <utility/io/izstream.hh>
//#include <basic/basic.hh>


using namespace core;
using core::Real;
using io::pdb::dump_pdb;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Cluster_Pose generated by StepWiseRNA_ResidueSampler
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static basic::Tracer TR( "protocols.swa.stepwise_rna_clusterer" ) ;

namespace protocols {
namespace swa {
namespace rna {

  //////////////////////////////////////////////////////////////////////////
  //constructor!
  StepWiseRNA_Clusterer::StepWiseRNA_Clusterer(utility::vector1< std::string > const & input_silent_file_list):
		input_silent_file_list_(input_silent_file_list),
		num_pose_kept_(432 ),
	  cluster_rmsd_(1.0 ),
		cluster_mode_("all_atom"),
		output_silent_file_(""),
		verbose_(true)
  {
  }

  //////////////////////////////////////////////////////////////////////////
  //destructor
  StepWiseRNA_Clusterer::~StepWiseRNA_Clusterer()
  {}
	/////////////////////////////////////////////////////////////////////////////

	utility::vector1< pose_data_struct2 >
	StepWiseRNA_Clusterer::cluster(){


		using namespace core::scoring;
		using namespace core::import_pose::pose_stream;
		using namespace core::chemical;
		using namespace core::pose;


			Output_title_text("Enter center_pose function");
	  	if(verbose_){
  			std::cout << "cluster_mode= " << cluster_mode_ << std::endl;
				std::cout << "(cluster_mode_==\"individual_residue\")= "; Output_boolean(cluster_mode_=="individual_residue");
				std::cout << ::std::endl;
  		}

		  core::chemical::ResidueTypeSetCAP rsd_set; 
			rsd_set = core::chemical::ChemicalManager::get_instance()->residue_type_set( "rna" );


			SilentFilePoseInputStreamOP input  = new SilentFilePoseInputStream();
			input->set_order_by_energy( true );
			input->filenames( input_silent_file_list_ ); //triggers read in of files, too.


			//Might want to cluster over individual residues and/or only over a subset of the residues
			if(cluster_mode_=="individual_residue" || cluster_mode_=="specified_residue_list"){
				Check_residue_list_parameters();
			}

			Size num_cluster_found_so_far=0;
			Size pose_ID=0;

			utility::vector1< pose_data_struct2 > cluster_pose_data_list; //Initially empty;

			while( input->has_another_pose() ) {
				pose_ID++;

				pose_data_struct2 pose_data;
				pose_data.pose_OP=new pose::Pose;

				core::io::silent::SilentStructOP silent_struct( input->next_struct() );
				silent_struct->fill_pose( *pose_data.pose_OP, *rsd_set );

				Real score( 0.0 );
				getPoseExtraScores( *pose_data.pose_OP, "score", score);
				pose_data.score = score; //Problem with converting from float to Real variable


				pose_data.tag= silent_struct->decoy_tag();

				if(verbose_) TR << "tag= " << pose_data.tag << " score " << pose_data.score << " pose_ID " << pose_ID << " num_cluster_found_so_far= " << num_cluster_found_so_far << std::endl;

				if(Is_new_cluster_center(pose_data, cluster_pose_data_list)==true){
					 num_cluster_found_so_far++;
					 pose_data.tag[0]='C'; //Indicate that the pose has been clustered
					 cluster_pose_data_list.push_back(pose_data);
					 std::cout << "Add pose " << pose_data.tag << " to cluster_pose_list" << std::endl;
				}

				//Break once find enough clusters
				if(num_cluster_found_so_far==num_pose_kept_) {
					std::cout << "Final: " << num_cluster_found_so_far  << " pose clusters center " << "from " << pose_ID << " poses" << std::endl;
					break;
				}

			}

			if(output_silent_file_!="") Output_pose_data_list(cluster_pose_data_list, output_silent_file_, false /*write_score_only*/);

			//Testing
			if(verbose_){
				for(Size n=1; n<=cluster_pose_data_list.size(); n++){
					pose_data_struct2 const & pose_data=cluster_pose_data_list[n];
			 		dump_pdb( *pose_data.pose_OP, pose_data.tag + ".pdb");
				}
			}

			return cluster_pose_data_list;

		 	std::cout << "Exit central_cluster_pose  function"  << std::endl;

			Output_title_text("Exit center_pose function");

		}

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool
	StepWiseRNA_Clusterer::Individual_residue_clustering(pose::Pose const & current_pose, pose::Pose const & cluster_center_pose) const{

		utility::vector1< Real > rmsd_list(cluster_residue_list_.size(), 9999.99);

		for(Size i=1; i<=cluster_residue_list_.size(); i++){
 			Size const full_seq_num= cluster_residue_list_[i].seq_num;
 			Size const seq_num=res_map_.find(full_seq_num)->second;
			bool Is_prepend=Is_prepend_map_.find(full_seq_num)->second;

			rmsd_list[i]=suite_rmsd(current_pose, cluster_center_pose, seq_num, Is_prepend);

			if(rmsd_list[i]>cluster_rmsd_) return false;
		}

		if(verbose_){
			for(Size i=1; i<=cluster_residue_list_.size(); i++){

 			Size const full_seq_num= cluster_residue_list_[i].seq_num;
 			Size const seq_num=res_map_.find(full_seq_num)->second;
			bool Is_prepend=Is_prepend_map_.find(full_seq_num)->second;

				std::cout << "full_seq_num= " << full_seq_num << " seq_num= " << seq_num << " Is_prepend= "; Output_boolean(Is_prepend);
				std::cout << " rmsd_list[" << i << "]= " << rmsd_list[i] << std::endl;
			}
		}

		return true;
	}


		//return false, if part of existing cluster. return true is a new cluster center
		bool
		StepWiseRNA_Clusterer::Is_new_cluster_center(pose_data_struct2 const & current_pose_data, utility::vector1< pose_data_struct2> const & cluster_pose_data_list) const {

			using namespace core::scoring; //for the all_atom_rmsd function

			pose::Pose & current_pose=*(current_pose_data.pose_OP);

			bool Is_new_cluster_center=true;

			for(Size ID=1; ID<=cluster_pose_data_list.size(); ID++){

				if(cluster_mode_=="individual_residue"){

					if(Individual_residue_clustering(current_pose, (*cluster_pose_data_list[ID].pose_OP))==true) Is_new_cluster_center=false;

				} else if(cluster_mode_=="specified_residue_list"){

					if(rmsd_over_residue_list(current_pose, (*cluster_pose_data_list[ID].pose_OP),
															 cluster_residue_list_, res_map_, Is_prepend_map_, false)<cluster_rmsd_) Is_new_cluster_center=false;

				} else if(cluster_mode_=="all_atom"){

					if(all_atom_rmsd( current_pose, (*cluster_pose_data_list[ID].pose_OP))<cluster_rmsd_) Is_new_cluster_center=false;

				}else{

					std::cout << "In Is_new_cluster_center function, Invalid cluster_mode= " << cluster_mode_ << std::endl;
					exit(1);
				}

				if(Is_new_cluster_center==false){
					std::cout << current_pose_data.tag << " is a neighbor of " << cluster_pose_data_list[ID].tag << std::endl;
					return false;
				}

			}

			return true; //new cluster center!

		}

		/////////////////////////////////////////////////////////////////////////////////////////////////////////////


		//Check that all the parameters need for the individual_residue and specified_residue_list exist.
		void
		StepWiseRNA_Clusterer::Check_residue_list_parameters() const {


			if(verbose_) {
				TR << " Check_residue_list_parameters function" << std::endl;
				Size const max_seq_num=get_max_seq_num_from_res_map(res_map_);
				std::cout << std::setw(30) << "cluster:  "; Output_residue_list(cluster_residue_list_);
				output_res_map(res_map_ , max_seq_num);
				output_is_prepend_map(Is_prepend_map_ , max_seq_num);
			 }



			//check if the cluster_residue_list is empty;
			if(cluster_residue_list_.size()==0){
				TR << "cluster_residue_list_.size()=0" << std::endl;
				exit(1);
			}

			for(Size n=1; n<=cluster_residue_list_.size(); n++){
				Size const seq_num=cluster_residue_list_[n].seq_num;

				//Check the res_map_
				if(res_map_.find(seq_num)==res_map_.end()){
					TR << "res_map_.find(" << seq_num << ")==std::map::end" << std::endl;
					exit(1);
				}

				//Check the Is_prepend_map_
				if(Is_prepend_map_.find(seq_num)==Is_prepend_map_.end()){
					TR << "Is_prepend_map_.find(" << seq_num << ")==std::map::end" << std::endl;
					exit(1);
				}
			}

		}



		///////////////////////////////////////////////////////////////////////////////////////////////////////////////

		void
		StepWiseRNA_Clusterer::set_res_map(std::map< core::Size, core::Size > const & res_map){
			res_map_=res_map;
		}

		void
		StepWiseRNA_Clusterer::set_is_prepend_map(std::map< core::Size, bool > const & Is_prepend_map){
			Is_prepend_map_=Is_prepend_map;
		}


		void
		StepWiseRNA_Clusterer::create_cluster_residue_list(utility::vector1< Size > const & cluster_res_seq_num_list,
																											 std::string const & full_fasta_sequence){

			utility::vector1< Residue_info > full_residue_list=Get_residue_list_from_fasta(full_fasta_sequence);

			for(Size n=1; n<=cluster_res_seq_num_list.size(); n++){
				Size const seq_num=cluster_res_seq_num_list[n];
				cluster_residue_list_.push_back(Get_residue_from_seq_num(seq_num, full_residue_list));
			}

			sort_residue_list(cluster_residue_list_);

			if(verbose_) {
				TR << " create_cluster_residue_list function" << std::endl;
				std::cout << std::setw(30) << "full: "; Output_residue_list(full_residue_list);
				std::cout << std::setw(30) << "cluster:  "; Output_residue_list(cluster_residue_list_);
			 }
		}

	  void
  	StepWiseRNA_Clusterer::set_output_silent_file( std::string const & output_silent_file){
    	output_silent_file_ = output_silent_file;
  	}

	  void
  	StepWiseRNA_Clusterer::set_cluster_mode( std::string const & cluster_mode){
    	cluster_mode_ = cluster_mode;
  	}

}
}
}


