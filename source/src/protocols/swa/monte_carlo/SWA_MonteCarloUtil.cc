// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file RNA_DeleteMover
/// @brief Torsions an RNA residue from a chain terminus.
/// @detailed
/// @author Rhiju Das

#include <protocols/swa/monte_carlo/SWA_MonteCarloUtil.hh>
#include <protocols/swa/monte_carlo/SWA_Move.hh>

// libRosetta headers
#include <core/types.hh>
#include <core/chemical/VariantType.hh>
#include <core/chemical/rna/RNA_Util.hh>
#include <core/kinematics/FoldTree.hh>
#include <core/pose/Pose.hh>
#include <core/pose/full_model_info/FullModelInfo.hh>
#include <core/pose/full_model_info/FullModelInfoUtil.hh>
#include <core/pose/util.hh>

#include <protocols/moves/MonteCarlo.hh>

#include <utility/tools/make_vector1.hh>

#include <basic/Tracer.hh>

#include <map>

#include <numeric/random/random.hh>

static numeric::random::RandomGenerator RG(239111);  // <- Magic number, do not change it!

static basic::Tracer TR( "protocols.swa.monte_carlo.SWA_MonteCarloUtil" ) ;

using namespace core;

namespace protocols {
namespace swa {
namespace monte_carlo {

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	get_delete_move_elements( pose::Pose & pose,
															 utility::vector1< SWA_Move > & swa_moves ) {

		swa_moves.clear();
		get_terminal_move_elements( pose, swa_moves, DELETE );

		// don't delete a multi_residue_move_element if its the only one!
		remove_from_consideration_first_multi_residue_move_element( swa_moves, false /*remove_even_if_not_singlet*/ );
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	get_resample_terminal_move_elements( pose::Pose & pose,
																					utility::vector1< SWA_Move > & swa_moves ) {

		swa_moves.clear();
		get_terminal_move_elements( pose, swa_moves, RESAMPLE );
		remove_from_consideration_first_multi_residue_move_element( swa_moves, true /*remove_even_if_not_singlet*/ );

	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// The point of this function is to allow big move_elements to remain fixed. It may not be entirely necessary.
	void
	remove_from_consideration_first_multi_residue_move_element( utility::vector1< SWA_Move > & swa_moves,
																																bool remove_even_if_not_singlet ){

		Size number_of_multi_residue_move_elements( 0 ), first_multi_residue_move_element( 0 );
		for ( Size n = 1; n <= swa_moves.size(); n++ ) {
			if ( swa_moves[ n ].move_element().size() > 1 ) {
				number_of_multi_residue_move_elements++;
				if ( first_multi_residue_move_element == 0 ) first_multi_residue_move_element = n;
			}
		}
		if ( number_of_multi_residue_move_elements == 0 ) return;

		runtime_assert( number_of_multi_residue_move_elements > 0 );

		if ( remove_even_if_not_singlet || (number_of_multi_residue_move_elements == 1) ){

			utility::vector1< SWA_Move > swa_moves_new;

			for ( Size n = 1; n <= swa_moves.size(); n++ ) {
				if ( n == first_multi_residue_move_element ) continue;
				swa_moves_new.push_back( swa_moves[ n ] );
			}
			swa_moves = swa_moves_new;

		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	get_terminal_move_elements( pose::Pose & pose,
																utility::vector1< SWA_Move > & swa_moves,
																MoveType const & move_type ) {

		using namespace core::pose::full_model_info;

		utility::vector1< MoveElement > const & move_elements = get_move_elements_from_full_model_info( pose );

		Size const num_move_elements( move_elements.size() );
		for ( Size n = 1; n <= num_move_elements; n++ ){

			MoveElement const & move_element = move_elements[ n ];
			utility::vector1< Attachment > attachments = get_attachments( pose, move_element );

			// at least for now, each move_element should be connected to another one.
			if ( attachments.size() == 0 ) runtime_assert( num_move_elements == 1 );

			if ( attachments.size() > 1 ) continue; // not a terminal

			swa_moves.push_back( SWA_Move( move_element, attachments, move_type ) );
		}

	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	get_resample_internal_move_elements( pose::Pose & pose,
																					utility::vector1< SWA_Move > & swa_moves ) {

		get_internal_move_elements( pose, swa_moves, RESAMPLE_INTERNAL_LOCAL );

		// don't delete a multi_residue_move_element if its the only one!
		remove_from_consideration_first_multi_residue_move_element( swa_moves, false /*remove_even_if_not_singlet*/ );
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	get_internal_move_elements( pose::Pose & pose,
																 utility::vector1< SWA_Move > & swa_moves,
																 MoveType const & move_type ) {

		using namespace core::pose::full_model_info;

		utility::vector1< MoveElement > const & move_elements = get_move_elements_from_full_model_info( pose );

		Size const num_move_elements( move_elements.size() );
		for ( Size n = 1; n <= num_move_elements; n++ ){

			MoveElement const & move_element = move_elements[ n ];
			utility::vector1< Attachment > attachments = get_attachments( pose, move_element );

			// at least for now, each move_element should be connected to another one.
			if ( attachments.size() != 2 ) continue;

			// note that PREVIOUS should be before NEXT.
			if ( attachments[1].attachment_type() == ATTACHED_TO_PREVIOUS &&
					 attachments[2].attachment_type() == ATTACHED_TO_NEXT ) {
				swa_moves.push_back( SWA_Move( move_element, attachments, move_type ) );
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	Attachments
	get_attachments( pose::Pose & pose, Size const & moving_res ){
		return get_attachments( pose, utility::tools::make_vector1( moving_res ) );
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	Attachments
	get_attachments( pose::Pose & pose, MoveElement const & move_element ){

		using namespace core::pose::full_model_info;

		Size const & nres( pose.total_residue() );
		kinematics::FoldTree const & fold_tree( pose.fold_tree() );
		utility::vector1< Size > const & res_list = get_res_list_from_full_model_info( pose );

		Attachments attachments_to_previous, attachments_to_next, attachments;

		// need to find number of attachment points of this move_element to rest of pose.
		for ( Size k = 1; k <= move_element.size(); k++ ){

			Size const i_full = move_element[ k ];
			Size const i = res_list.index( i_full );

			// potential connection to previous residue in rest of pose
			if ( i > 1 && ! move_element.has_value( res_list[ i - 1 ] ) && !fold_tree.is_cutpoint( i - 1 ) )  {
				attachments_to_previous.push_back( Attachment( i_full - 1, ATTACHED_TO_PREVIOUS ) );
			}

			// potential connection to next residue in rest of pose
			if ( i < nres && ! move_element.has_value( res_list[ i + 1 ] ) && !fold_tree.is_cutpoint( i ) ){
				attachments_to_next.push_back( Attachment( i_full + 1, ATTACHED_TO_NEXT ) );
			}
		}

		for ( Size k = 1; k <= attachments_to_previous.size(); k++ ) attachments.push_back( attachments_to_previous[ k ] );
		for ( Size k = 1; k <= attachments_to_next.size()    ; k++ ) attachments.push_back( attachments_to_next[ k ] );

		return attachments;

	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	get_add_move_elements( pose::Pose & pose,
														utility::vector1< SWA_Move > & swa_moves ){

		using namespace core::pose::full_model_info;

		Size const & nres( pose.total_residue() );
		kinematics::FoldTree const & fold_tree( pose.fold_tree() );

		FullModelInfo const & full_model_info = const_full_model_info_from_pose( pose );
		utility::vector1< Size > const & res_list = get_res_list_from_full_model_info( pose );
		utility::vector1< Size > const & cutpoint_open_in_full_model = full_model_info.cutpoint_open_in_full_model();
		Size nres_full = full_model_info.full_sequence().size();

		utility::vector1< bool > is_cutpoint_in_full_pose;
		for ( Size i = 1; i <= nres_full; i++ ) is_cutpoint_in_full_pose.push_back( false );
		for ( Size n = 1; n <= cutpoint_open_in_full_model.size(); n++ ) is_cutpoint_in_full_pose[ cutpoint_open_in_full_model[n] ] = true;

		for ( Size i = 1; i <= nres; i++ ){

			if ( ( i == nres ) ||
					 ( fold_tree.is_cutpoint( i ) && (res_list[ i ]+1 < res_list[ i+1 ]) ) ) { // could be a 3' chain terminus

				Size const i_full = res_list[ i ] ;
				if ( !is_cutpoint_in_full_pose[ i_full ] && i_full < nres_full ){ // good, there's still a gap!
					swa_moves.push_back( SWA_Move( i_full + 1, Attachment( i_full, ATTACHED_TO_PREVIOUS ), ADD ) );
				}

			}
		}

		for ( Size i = 1; i <= nres; i++ ){

			if ( ( i == 1 ) ||
					 ( fold_tree.is_cutpoint( i-1 ) && (res_list[ i ]-1 > res_list[ i-1 ]) ) ) { // could be a 5' chain terminus

				Size const i_full = res_list[ i ];
				if ( i_full > 1 && !is_cutpoint_in_full_pose[ i_full-1 ] ) { // good, there's still a gap!
					swa_moves.push_back( SWA_Move( i_full - 1, Attachment( i_full, ATTACHED_TO_NEXT), ADD ) );
				}

			}
		}

	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	get_random_move_element_at_chain_terminus( pose::Pose & pose,
																			SWA_Move & swa_move,
																			bool const disallow_delete,
																			bool const disallow_resample ){

		utility::vector1< Size > sample_res; /*leave empty if no filter*/
		get_random_move_element_at_chain_terminus( pose, swa_move,
																				disallow_delete, disallow_resample,
																				sample_res );
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	get_random_move_element_at_chain_terminus( pose::Pose & pose,
																			SWA_Move & swa_move,
																			bool const disallow_delete,
																			bool const disallow_resample,
																			utility::vector1< Size > const & sample_res /*leave empty if no filter*/) {

		using namespace core::pose::full_model_info;

		utility::vector1< SWA_Move >  swa_moves;

		if ( !disallow_resample )  get_resample_terminal_move_elements( pose, swa_moves );
		if ( !disallow_delete )    get_delete_move_elements( pose, swa_moves );

		get_add_move_elements( pose, swa_moves );

		if ( sample_res.size() > 0 ) filter_by_sample_res( swa_moves,
																											 sample_res, get_res_list_from_full_model_info( pose ) );
		for ( Size n = 1; n <= swa_moves.size(); n++ )	TR.Debug << swa_moves[ n ] << std::endl;

		if ( swa_moves.size() == 0 ){
			swa_move = SWA_Move();
			return;
		}

		swa_move =  RG.random_element( swa_moves );

	}


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	void
	filter_by_sample_res( utility::vector1< SWA_Move > & swa_moves,
											  utility::vector1< Size > const & sample_res,
											  utility::vector1< Size > const & res_list ){

		if ( sample_res.size() == 0 ) return;

		utility::vector1< SWA_Move > swa_moves_filtered;

		for ( Size i = 1; i <= swa_moves.size(); i++ ){

			SWA_Move const & swa_move = swa_moves[ i ];

			if ( swa_move.move_type() == ADD ){
				if ( ! sample_res.has_value( swa_move.moving_res() ) ) continue;
			}

			swa_moves_filtered.push_back( swa_move );
		}

		swa_moves = swa_moves_filtered;
	}


}
}
}
