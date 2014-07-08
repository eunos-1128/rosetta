// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file    protocols/grafting/AnchoredGraftMover.cc
/// @brief   Method definitions for AnchoredGraftMover
/// @author  Jared Adolf-Bryfogle (jadolfbr@gmail.com)
/// @author  Steven Lewis (smlewi@gmail.com)

//Unit Headers
#include <protocols/grafting/AnchoredGraftMover.hh>
#include <protocols/grafting/GraftMoverBase.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/grafting/util.hh>

//Core Headers
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/constraints/ConstraintSet.hh>

#include <core/kinematics/FoldTree.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/task/PackerTask.hh>

#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/ResidueType.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/VariantType.hh>

//Protocol Headers
#include <protocols/simple_moves/MinMover.hh>
#include <protocols/simple_moves/BackboneMover.hh>
#include <protocols/simple_moves/ReturnSidechainMover.hh>
#include <protocols/simple_moves/SwitchResidueTypeSetMover.hh>
#include <protocols/simple_moves/PackRotamersMover.hh>

#include <protocols/loops/loop_closure/ccd/CcdLoopClosureMover.hh>
#include <protocols/loops/Loop.hh>
#include <protocols/loops/Loops.hh>
#include <protocols/loops/FoldTreeFromLoopsWrapper.hh>
#include <protocols/moves/MonteCarlo.hh>

//Option Headers
#include <basic/options/keys/relax.OptionKeys.gen.hh>
#include <basic/options/option.hh>

//Basic and Utility
#include <basic/Tracer.hh>
#include <core/scoring/rms_util.hh>
#include <map>
#include <utility/PyAssert.hh>

static basic::Tracer TR("protocols.grafting.AnchoredGraftMover");
namespace protocols {
namespace grafting {
	using namespace core::scoring;
	using namespace core::pose;
	using namespace core::pack::task;
	using namespace basic::options;
	using namespace protocols::loops;
	using namespace protocols::simple_moves;
	
	
AnchoredGraftMover::AnchoredGraftMover(const Size start, Size const end)
	:GraftMoverBase(start, end, "AnchoredGraftMover")

{
	//moves::Mover("GraftMover"),
	set_defaults();
}

AnchoredGraftMover::AnchoredGraftMover(const Size start, const Size end, core::pose::Pose const & piece, Size Nter_overhang_length, Size Cter_overhang_length)
	:GraftMoverBase(start, end, "AnchoredGraftMover", piece, Nter_overhang_length, Cter_overhang_length)
	
{
    set_defaults();
}

AnchoredGraftMover::~AnchoredGraftMover() {}

void
AnchoredGraftMover::set_defaults(){
	cen_scorefxn_ = NULL;
	fa_scorefxn_ = NULL;
	movemap_ = NULL;
	scaffold_movemap_ =NULL;
	insert_movemap_ = NULL;
	
	set_skip_sampling(false);
	set_mintype("dfpmin_armijo_nonmonotone");
	set_test_control_mode(false);
	use_default_movemap_ = true;
	set_scaffold_flexibility(2, 2);
	set_insert_flexibility(0, 0);
	set_cycles(300);
	stop_at_closure(true);
	final_repack(true);
	neighbor_dis(4.0);
}

void
AnchoredGraftMover::set_scaffold_flexibility(const Size Nter_scaffold_flexibility, const Size Cter_scaffold_flexibility){
    //Insert flexibility stays at default values (0)
    Nter_scaffold_flexibility_=Nter_scaffold_flexibility;
    Cter_scaffold_flexibility_=Cter_scaffold_flexibility;
}

Size
AnchoredGraftMover::get_nterm_scaffold_flexibility(){
	return Nter_scaffold_flexibility_;
}

Size
AnchoredGraftMover::get_cterm_scaffold_flexibility(){
	return Cter_scaffold_flexibility_;
}

void
AnchoredGraftMover::set_insert_flexibility(Size Nter_insert_flexibility, Size Cter_insert_flexibility)
{
	Nter_insert_flexibility_=Nter_insert_flexibility;
	Cter_insert_flexibility_=Cter_insert_flexibility;
}

void
AnchoredGraftMover::set_cycles(Size cycles){
	cycles_=cycles;
}

void
AnchoredGraftMover::set_mintype(std::string mintype){
	mintype_ = mintype;
}

void
AnchoredGraftMover::set_skip_sampling(bool skip_sampling){
	skip_sampling_ = skip_sampling;
}

core::Size
AnchoredGraftMover::get_Cter_loop_end(){
	return Cter_loop_end_;
}

void
AnchoredGraftMover::set_test_control_mode(bool test_control_mode){
	test_control_mode_=test_control_mode;
}


void
AnchoredGraftMover::set_movemaps(MoveMapCOP scaffold_mm, MoveMapCOP insert_mm){
    
	scaffold_movemap_ = scaffold_mm;
	insert_movemap_ = insert_mm;

}

void
AnchoredGraftMover::setup_scorefunction(){
    if (! cen_scorefxn_){
	this->set_default_cen_scorefunction();
    }
    
    if (! fa_scorefxn_){
	this->set_default_fa_scorefunction();
    }
}

void
AnchoredGraftMover::set_cen_scorefunction(ScoreFunctionCOP score){
	cen_scorefxn_=score->clone();
	cen_scorefxn_->set_weight_if_zero(chainbreak, 20.0);
	cen_scorefxn_->set_weight_if_zero(linear_chainbreak, 20.0);
}

void
AnchoredGraftMover::set_fa_scorefunction(ScoreFunctionCOP score){
	//Just in case modifications are made to it.
	fa_scorefxn_=score->clone();
	fa_scorefxn_->set_weight_if_zero(chainbreak, 20.0);
	fa_scorefxn_->set_weight_if_zero(linear_chainbreak, 20.0);
}

void
AnchoredGraftMover::set_default_cen_scorefunction(){
	//cen_scorefxn_=protocols::loops::get_cen_scorefxn();
	cen_scorefxn_ = new ScoreFunction();
	cen_scorefxn_->set_weight( chainbreak,        20.00);
	cen_scorefxn_->set_weight( linear_chainbreak, 20.00);
	cen_scorefxn_->set_weight( cbeta_smooth,      1.0 );
	cen_scorefxn_->set_weight( vdw,               1.0 );
	cen_scorefxn_->set_weight( cen_pair_smooth,   1.0 );
	cen_scorefxn_->set_weight( cenpack_smooth,    1.0 );
	cen_scorefxn_->set_weight( cen_env_smooth,    1.0);
	cen_scorefxn_->set_weight( rama,              5.0 );
	cen_scorefxn_->set_weight( hbond_lr_bb,       1.0 );
	cen_scorefxn_->set_weight( hbond_sr_bb,       1.0 );
	cen_scorefxn_->set_weight( omega,             5.0 );
}
void
AnchoredGraftMover::set_default_fa_scorefunction(){
	fa_scorefxn_=get_score_function();
	fa_scorefxn_->set_weight_if_zero(chainbreak, 20.0);
	fa_scorefxn_->set_weight_if_zero(linear_chainbreak, 20.0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
///MOVEMAP and REGION SETUP
//////////////////////////////////////////////////////////////////////////////////////////////////////

void
AnchoredGraftMover::setup_movemap_and_regions(Pose & pose){
	if (scaffold_movemap_ && insert_movemap_){
		movemap_ = protocols::grafting::combine_movemaps_post_insertion(scaffold_movemap_, insert_movemap_, 
				end(), original_end(), insertion_length());
		
		set_regions_from_movemap(pose);
	}
	else{
		set_regions_from_flexibility();
		set_default_movemap();
	}

	PyAssert((Nter_loop_start_!=Cter_loop_end_), "Start and end of remodeling region the same.  Please check movemap");
}

void
AnchoredGraftMover::set_default_movemap(){
    TR <<"Setting default movemap"<<std::endl;
	movemap_ = new core::kinematics::MoveMap();
	for (Size i=Nter_loop_start_; i<=Nter_loop_end_; ++i){
		movemap_->set_bb(i, true);
		movemap_->set_chi(i, true);
	}
	for (Size i=Cter_loop_start_; i<=Cter_loop_end_; ++i){
		movemap_->set_bb(i, true);
		movemap_->set_chi(i, true);
	}
}

void
AnchoredGraftMover::set_regions_from_flexibility(){
	Nter_loop_start_ = start()-Nter_scaffold_flexibility_+1;//(loop_start/insert_loop_start)First Flexible loop residue
	Nter_loop_end_ = start()+Nter_insert_flexibility_;

	Cter_loop_start_=start()+insertion_length()+1-Cter_insert_flexibility_;
	Cter_loop_end_ = start()+insertion_length()+Cter_scaffold_flexibility_;//(loop_end) Last Flexible loop residue

	TR.Debug <<"Nter_loop_start: "<<Nter_loop_start_<<std::endl;
	TR.Debug <<"Nter_loop_end: "<<Nter_loop_end_<<std::endl;
	TR.Debug <<"Cter_loop_start:  "<<Cter_loop_start_<<std::endl;
	TR.Debug <<"Cter_loop_end:  "<<Cter_loop_end_<<std::endl;
}


void
AnchoredGraftMover::set_regions_from_movemap(Pose & pose){
    //N terminal
    for (Size i=1; i<=start(); ++i){
        if (movemap_->get_bb(i)){
            Nter_loop_start_=i;
            break;
        }
    }

    //C terminal end
    for (Size i=pose.total_residue(); i>=Nter_loop_start_; --i){
        if (movemap_->get_bb(i)){
        	TR <<i<<std::endl;
            Cter_loop_end_=i;
            break;
        }
    }

    //Insertion default flexiblity.  Will not effect single_loop foldtrees.
    Nter_insert_flexibility_=0;
    for (Size i = start()+1; i<=start()+insertion_length(); ++i){
        if (movemap_->get_bb(i)){
            ++Nter_insert_flexibility_;
        }
        else{
            Nter_loop_end_=start()+Nter_insert_flexibility_;

            break;
        }
    }

    Cter_insert_flexibility_=0;
    for (Size i = start()+insertion_length(); i>=1; --i){
        if (movemap_->get_bb(i)){
            ++Cter_insert_flexibility_;
        }
        else{
            Cter_loop_start_=start()+insertion_length()-Cter_insert_flexibility_;
            break;
        }
    }
    //TR <<"Nter_insert_flex: "<<Nter_insert_flexibility_<<std::endl;
    //TR <<"Nter_loop_start: "<<Nter_loop_start_<<std::endl;
    //TR <<"Nter_loop_end: "<<Nter_loop_end_<<std::endl;
    //TR <<"Cter_loop_start:  "<<Cter_loop_start_<<std::endl;
    //TR <<"Cter_loop_end:  "<<Cter_loop_end_<<std::endl;
}

MinMoverOP
AnchoredGraftMover::setup_default_min_mover(){

	MinMoverOP min_mover =  new MinMover(movemap_, cen_scorefxn_, mintype_, 0.01, true /*use_nblist*/ );
	return min_mover;
}

SmallMoverOP
AnchoredGraftMover::setup_default_small_mover(){
	SmallMoverOP small =  new SmallMover(movemap_, 10, 200); //huge moves for sampling
	small->angle_max( 'H', 180.0 );
	small->angle_max( 'E', 180.0 );
	small->angle_max( 'L', 180.0 );
	return small;
}

void
AnchoredGraftMover::stop_at_closure(bool stop_at_closure){
    stop_at_closure_ = stop_at_closure;
}
bool
AnchoredGraftMover::stop_at_closure(){
    return stop_at_closure_;
}

void
AnchoredGraftMover::final_repack(bool final_repack){
    final_repack_ = final_repack;
}
bool
AnchoredGraftMover::final_repack(){
    return final_repack_;
}

void
AnchoredGraftMover::apply(Pose & pose){
	using core::pose::add_variant_type_to_pose_residue;
	using namespace core::id;
	using protocols::moves::MonteCarlo;
	using protocols::moves::MonteCarloOP;
	
	//TR <<"Beginning of anchored graft mover" <<std::endl;
	//pose.constraint_set()->show(TR);
	setup_movemap_and_regions(pose);
    
	//Run the insertion.
	Pose combined = insert_piece(pose);
	core::kinematics::FoldTree original_ft = combined.fold_tree();
	//Setup for the remodeling
	core::Size const insert_start(start()+1); //this will be the first residue of the insert
	core::Size const insert_end(start()+insertion_length()); //this will be the last residue of the insert


	setup_scorefunction();
	
	///Add variants, create the loops and set the foldtree that will be used for CCD.
	
	
	Loops loops;
	
	setup_single_loop_single_arm_remodeling_foldtree(combined, Nter_loop_start_, Cter_loop_end_);
	Loop Cter_loop = Loop(Nter_loop_start_, Cter_loop_end_, Cter_loop_end_-1);      
	loops.add_loop(Cter_loop);
	
	loop_closure::ccd::CcdLoopClosureMoverOP ccd_mover = new loop_closure::ccd::CcdLoopClosureMover(Cter_loop, movemap_);
        
	add_cutpoint_variants_for_ccd(combined, loops);
	
	idealize_combined_pose(combined, movemap_, start(), insert_start, insert_end, Nter_loop_start_, Cter_loop_end_);
	movemap_->set( TorsionID(insert_start, BB, phi_torsion), true);
	movemap_->set( TorsionID(insert_end, BB, psi_torsion), true);
	
	//centroidize the pose before we do stuff to it - sidechains are expensive and unnecessary
	protocols::simple_moves::SwitchResidueTypeSetMover typeset_swap(core::chemical::CENTROID);
	protocols::simple_moves::ReturnSidechainMoverOP return_sidechains = new  protocols::simple_moves::ReturnSidechainMover(combined );
	typeset_swap.apply( combined );

	//TR <<"After type swap" <<std::endl;
	
	//combined.dump_pdb("combined_preclose_cen.pdb");

	MinMoverOP min_mover = setup_default_min_mover();
	SmallMoverOP small = setup_default_small_mover();
	

	if (test_control_mode_){perturb_backbone_for_test(combined, movemap_);}
	MonteCarlo mc(combined, (*cen_scorefxn_), 0.8);

	/////////////////////////Protocol//////////////////////////////////////////////////////////
	TR << "start " << ((*cen_scorefxn_))(combined) << std::endl;

	bool skip_mc = false;
	for( core::Size i(1); i<=cycles_; ++i){
		TR <<"round "<<i <<std::endl;
		if (!skip_sampling_){small->apply(combined);}
		
		ccd_mover->apply(combined);
		combined.conformation().insert_ideal_geometry_at_polymer_bond(Cter_loop.cut()); //Not sure if this really helps, but Steven had it, so it probably does.
		min_mover->apply(combined);
		combined.conformation().insert_ideal_geometry_at_polymer_bond(Cter_loop.cut());
		
		if (stop_at_closure() && graft_closed(combined, loops)){
			TR << "Graft Closed early - returning" << std::endl;
			skip_mc = true;
			TR << i << " " << ((*cen_scorefxn()))(combined) << std::endl;
			break;
		}
		if(mc.boltzmann(combined)) TR << i << " " << ((*cen_scorefxn()))(combined) << std::endl;
	}
        
	if (! skip_mc) mc.recover_low(combined);
	

	
	TR << "finish " << ((*cen_scorefxn_))(combined) << std::endl;
	//combined.conformation().insert_ideal_geometry_at_polymer_bond(Cter_loop.cut());
	
	return_sidechains->apply( combined );
	
	//Remove cutpoints that were required for CCD.
	remove_cutpoint_variants_for_ccd(combined, loops);
	combined.fold_tree(original_ft);
	
	if (final_repack_){
		repack_connection_and_residues_in_movemap_and_piece_and_neighbors( pose, fa_scorefxn_, 
			start(), end(), movemap_, neighbor_dis());
	}
	TR <<"Graft meets ideal geometry " << std::boolalpha << graft_closed(combined, loops) << std::endl;
	
	TR << "Complete"<<std::endl;
	pose = combined;

}



///////Accessors and Mutators of private data for derived classes

void AnchoredGraftMover::movemap(MoveMapOP movemap){movemap_ = movemap;}
MoveMapOP AnchoredGraftMover::movemap(){return movemap_;}

void AnchoredGraftMover::scaffold_movemap(MoveMapCOP scaffold_movemap){
    scaffold_movemap_ = scaffold_movemap;
}

MoveMapCOP AnchoredGraftMover::scaffold_movemap() {
    return scaffold_movemap_;
}

void AnchoredGraftMover::insert_movemap(MoveMapCOP insert_movemap){
    insert_movemap_ = insert_movemap;
}
MoveMapCOP AnchoredGraftMover::insert_movemap(){
    return insert_movemap_;
}
ScoreFunctionOP AnchoredGraftMover::cen_scorefxn(){
    return cen_scorefxn_;
}
ScoreFunctionOP AnchoredGraftMover::fa_scorefxn(){
    return fa_scorefxn_;
}
void AnchoredGraftMover::use_default_movemap(bool use_default_movemap){
    use_default_movemap_ = use_default_movemap;
}
bool AnchoredGraftMover::use_default_movemap(){
    return use_default_movemap_;
}
bool AnchoredGraftMover::test_control_mode(){
    return test_control_mode_;
}
Size AnchoredGraftMover::Nter_insert_flexibility(){
    return Nter_insert_flexibility_;
}
Size AnchoredGraftMover::Cter_insert_flexibility(){
    return Cter_insert_flexibility_;
}
Size AnchoredGraftMover::Nter_scaffold_flexibility(){
    return Nter_scaffold_flexibility_;
}
Size AnchoredGraftMover::Cter_scaffold_flexibility(){
    return Cter_scaffold_flexibility_;
}
Size AnchoredGraftMover::Nter_loop_start(){
    return Nter_loop_start_;
}
Size AnchoredGraftMover::Nter_loop_end(){
    return Nter_loop_end_;
}
Size AnchoredGraftMover::Cter_loop_start(){
    return Cter_loop_start_;
}
Size AnchoredGraftMover::Cter_loop_end(){
    return Cter_loop_end_;
}
std::string AnchoredGraftMover::mintype(){
    return mintype_;
}

Size AnchoredGraftMover::cycles(){
    return cycles_;
}

bool AnchoredGraftMover::skip_sampling(){
    return skip_sampling_;
}

void AnchoredGraftMover::neighbor_dis(core::Real dis){
	neighbor_dis_ = dis;
}
core::Real AnchoredGraftMover::neighbor_dis(){
	return neighbor_dis_;
}


} //Grafting
} //Protocols
