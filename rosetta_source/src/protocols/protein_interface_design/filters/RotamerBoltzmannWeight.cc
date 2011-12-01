// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 sw=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @author Sarel Fleishman (sarelf@uw.edu)
#include <protocols/protein_interface_design/filters/RotamerBoltzmannWeight.hh>
#include <protocols/protein_interface_design/filters/RotamerBoltzmannWeightFilterCreator.hh>

#include <core/chemical/AA.hh>
#include <core/pose/Pose.hh>
#include <core/pose/PDBInfo.hh>
#include <core/conformation/Residue.hh>
#include <utility/tag/Tag.hh>
#include <protocols/filters/Filter.hh>
#include <protocols/moves/DataMap.hh>
#include <protocols/moves/RigidBodyMover.hh>
#include <core/kinematics/MoveMap.hh>
// AUTO-REMOVED #include <protocols/protein_interface_design/movers/BuildAlaPose.hh>
// AUTO-REMOVED #include <protocols/moves/MakePolyXMover.hh>
#include <protocols/flxbb/SelectResiduesByLayer.hh>
#include <basic/Tracer.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/task/PackerTask.hh>
#include <protocols/rosetta_scripts/util.hh>
#include <core/pack/rotamer_set/RotamerSet.hh>
// AUTO-REMOVED #include <core/pack/rotamer_set/RotamerSets.hh>
#include <core/pack/rotamer_set/RotamerSetFactory.hh>
#include <protocols/toolbox/pose_metric_calculators/RotamerBoltzCalculator.hh>
#include <protocols/moves/MinMover.hh>
#include <utility/vector1.hh>
#include <boost/foreach.hpp>
#define foreach BOOST_FOREACH
#include <protocols/toolbox/task_operations/DesignAroundOperation.hh>
#include <core/pack/task/operation/TaskOperations.hh>
#include <core/graph/Graph.hh>
#include <core/pack/pack_rotamers.hh>
// AUTO-REMOVED #include <core/optimization/MinimizerOptions.hh>
#include <core/scoring/Energies.hh>

// Neil headers 110621
// AUTO-REMOVED #include <core/conformation/symmetry/util.hh>
#include <core/pose/symmetry/util.hh>
#include <core/pack/make_symmetric_task.hh>
#include <protocols/simple_moves/symmetry/SymMinMover.hh>

#include <utility/vector0.hh>

//Auto Headers
#include <core/scoring/EnergyGraph.hh>
#include <protocols/simple_filters/DdgFilter.hh>
#include <protocols/simple_filters/ScoreTypeFilter.hh>
#include <protocols/simple_filters/AlaScan.hh>
namespace protocols {
namespace protein_interface_design{
namespace filters {

static basic::Tracer TR( "protocols.protein_interface_design.filters.RotamerBoltzmannWeight" );

///@brief default ctor
RotamerBoltzmannWeight::RotamerBoltzmannWeight() :
	parent( "RotamerBoltzmannWeight" ),
	task_factory_( NULL ),
	rb_jump_( 1 ),
	unbound_( true ),
	scorefxn_( NULL ),
	temperature_( 0.8 ),
	ddG_threshold_( 1.5 ),
	repacking_radius_( 6.0 ),
	energy_reduction_factor_( 0.5 ),
	compute_entropy_reduction_( false ),
	repack_( true ),
	type_( "" ),
	skip_ala_scan_( false ),
	fast_calc_(false)
{
	threshold_probabilities_per_residue_.assign( core::chemical::num_canonical_aas, 1 );
}

void
RotamerBoltzmannWeight::repack( bool const repack ){
	repack_ = repack;
}

bool
RotamerBoltzmannWeight::repack() const
{
	return repack_;
}

void
RotamerBoltzmannWeight::threshold_probability( core::chemical::AA const aa_type, core::Real const probability )
{
	threshold_probabilities_per_residue_[ aa_type ] = probability;
}

core::Real
RotamerBoltzmannWeight::threshold_probability( core::chemical::AA const aa_type ) const
{
	return( threshold_probabilities_per_residue_[ aa_type ] );
}

void
RotamerBoltzmannWeight::energy_reduction_factor( core::Real const factor )
{
	energy_reduction_factor_ = factor;
}

core::Real
RotamerBoltzmannWeight::energy_reduction_factor() const
{
	return( energy_reduction_factor_ );
}

core::Real
RotamerBoltzmannWeight::temperature() const{
	return temperature_;
}

void
RotamerBoltzmannWeight::temperature( core::Real const temp )
{
	temperature_ = temp;
}

core::pack::task::TaskFactoryOP
RotamerBoltzmannWeight::task_factory() const
{
	return task_factory_;
}

void
RotamerBoltzmannWeight::task_factory( core::pack::task::TaskFactoryOP task_factory )
{
	task_factory_ = task_factory;
}

void
RotamerBoltzmannWeight::rb_jump( core::Size const jump )
{
	runtime_assert( jump );
	rb_jump_ = jump;
}

core::Size
RotamerBoltzmannWeight::rb_jump() const
{
	return rb_jump_;
}

void
RotamerBoltzmannWeight::repacking_radius( core::Real const rad )
{
	repacking_radius_ = rad;
}

core::Real
RotamerBoltzmannWeight::repacking_radius() const
{
	return repacking_radius_;
}

bool
RotamerBoltzmannWeight::apply(core::pose::Pose const & ) const
{
	return( true );
}

/// @details iterate over all packable residues according to the task factory and for each
/// determine whether it contributes to binding at least ddG_threshold R.e.u. (using alanine scanning
/// This list will later be used to do the Boltzmann weight calculations.
utility::vector1< core::Size >
RotamerBoltzmannWeight::first_pass_ala_scan( core::pose::Pose const & pose ) const
{
	runtime_assert( task_factory() );
	TR<<"----------First pass alanine scanning to identify hot spot residues------------"<<std::endl;
	utility::vector1< core::Size > hotspot_residues;
	hotspot_residues.clear();
	protocols::simple_filters::AlaScan ala_scan;
	ala_scan.repack( repack() );
	if( repack() )
		ala_scan.repeats( 3 );
	else
		ala_scan.repeats( 1 );
	core::Size const jump( rb_jump() );
	ala_scan.jump( jump );
	ala_scan.scorefxn( scorefxn() );
	if( repack() )
		TR<<"All energy calculations will be computed subject to repacking in bound and unbound states (ddG).";
	else
		TR<<"All energy calculations will be computed subject to no repacking in the bound and unboud states (dG)";
	core::Real orig_ddG(0.0);
	if( !skip_ala_scan() ){
		protocols::simple_filters::DdgFilter ddg_filter( 100/*ddg_threshold*/, scorefxn(), jump, 1 /*repeats*/ );
		ddg_filter.repack( repack() );
		orig_ddG = ddg_filter.compute( pose );
		TR<<"\nOriginal complex ddG "<<orig_ddG<<std::endl;
	}
	core::pack::task::PackerTaskCOP packer_task( task_factory()->create_task_and_apply_taskoperations( pose ) );
	for( core::Size resi=1; resi<=pose.total_residue(); ++resi ){
		if( packer_task->being_packed( resi ) ){
			if( skip_ala_scan() ){
				TR<<"Adding residue "<<resi<<" to hotspot list\n";
				hotspot_residues.push_back( resi );
			}
			else{
				core::Real const ala_scan_ddG( ala_scan.ddG_for_single_residue( pose, resi ) );
				core::Real const ddG( ala_scan_ddG - orig_ddG );
				TR<<"ddG for resi "<<pose.residue( resi ).name3()<<resi<<" is "<<ddG<<std::endl;
				ddGs_[ resi ] = ddG;
				if( ddG >= ddG_threshold() )
					hotspot_residues.push_back( resi );
			}
		}
	}
	TR<<"----------Done with first pass alanine scanning to identify hot spot residues------------"<<std::endl;
	return( hotspot_residues );
}


core::Real
RotamerBoltzmannWeight::compute( core::pose::Pose const & const_pose ) const{
	ddGs_.clear();
	rotamer_probabilities_.clear();

	utility::vector1< core::Size > hotspot_res;
	core::pose::Pose unbound_pose( const_pose );
	if( type_ == "monomer" ) {
		protocols::flxbb::SelectResiduesByLayer srb( true, true, false );
		utility::vector1< core::chemical::AA > select_aa_types;
		select_aa_types.push_back( core::chemical::aa_tyr );
		select_aa_types.push_back( core::chemical::aa_phe );
		select_aa_types.push_back( core::chemical::aa_trp );
		select_aa_types.push_back( core::chemical::aa_leu );
		select_aa_types.push_back( core::chemical::aa_ile );
		srb.restrict_aatypes_for_selection( select_aa_types );
		hotspot_res = srb.compute( const_pose );
	} else {
		if( unbound() ){
			using namespace protocols::moves;
			RigidBodyTransMoverOP translate( new RigidBodyTransMover( unbound_pose, rb_jump() ) );
			translate->step_size( 1000.0 );
			translate->apply( unbound_pose );
		}
		hotspot_res = first_pass_ala_scan( const_pose );
	}

	if( hotspot_res.size() == 0 ){
		TR<<"No hot-spot residues detected in first pass alanine scan. Doing nothing"<<std::endl;
		return( 0 );
	}
	else
		TR<<hotspot_res.size()<<" hot-spot residues detected."<<std::endl;

	protocols::toolbox::pose_metric_calculators::RotamerBoltzCalculator rotboltz_calc( this->scorefxn(), this->temperature(), this->repacking_radius() );

	foreach( core::Size const hs_res, hotspot_res ){
		core::Real const boltz_weight( fast_calc_ ? rotboltz_calc.computeBoltzWeight( unbound_pose, hs_res ) : compute_Boltzmann_weight( unbound_pose, hs_res ) );
		TR<<const_pose.residue( hs_res ).name3()<<hs_res<<" "<<boltz_weight<<'\n';
		rotamer_probabilities_[ hs_res ] = boltz_weight;
	}
	TR.flush();
	return( 0.0 );
}

core::Real
RotamerBoltzmannWeight::compute_Boltzmann_weight( core::pose::Pose const & const_pose, core::Size const resi ) const
{
	using namespace core::pack::rotamer_set;
	using namespace core::pack::task;
	using namespace core::conformation;

	TR<<"-----Computing Boltzmann weight for residue "<<resi<<std::endl;
/// build a rotamer set for resi while pruning clashes against a poly-alanine background
// NK 110621
	core::pose::Pose pose( const_pose );
//	core::pose::Pose ala_pose( pose );
//	if( type_ == "monomer" ) {
//		protocols::moves::MakePolyXMover bap( "ALA", false, true, false );
//		bap.apply( ala_pose );
//	} else {
//		protocols::protein_interface_design::movers::BuildAlaPose bap( true/*part1*/, true/*part2*/, 100000/*dist_cutoff*/);
//		bap.apply( ala_pose );
//	}
// NK 110621

	Residue const & res = pose.residue( resi );
	RotamerSetFactory rsf;
	RotamerSetOP rotset = rsf.create_rotamer_set( res );
	rotset->set_resid( resi );
	TaskFactoryOP tf = new core::pack::task::TaskFactory;
	tf->push_back( new core::pack::task::operation::InitializeFromCommandline);
	tf->push_back( new core::pack::task::operation::IncludeCurrent );
	PackerTaskOP ptask( tf->create_task_and_apply_taskoperations( pose ) );
	if (core::pose::symmetry::is_symmetric(pose)) {
		core::pack::make_symmetric_PackerTask(pose, ptask); // NK 110621
	}
	ResidueLevelTask & restask( ptask->nonconst_residue_task( resi ) );
	restask.restrict_to_repacking();
	core::graph::GraphOP packer_graph = new core::graph::Graph( pose.total_residue() );
	ptask->set_bump_check( true );
	rotset->build_rotamers( pose, *scorefxn_, *ptask, packer_graph, false );

/// des_around will mark all residues around resi for design, the rest for packing.
	protocols::toolbox::task_operations::DesignAroundOperationOP des_around = new protocols::toolbox::task_operations::DesignAroundOperation;
	des_around->design_shell( repacking_radius() );
	des_around->include_residue( resi );
	tf->push_back( des_around );
	PackerTaskOP task = tf->create_task_and_apply_taskoperations( pose );
	if (core::pose::symmetry::is_symmetric(pose)) {
		core::pack::make_symmetric_PackerTask(pose, task); // NK 110621
	}
	protocols::simple_filters::ScoreTypeFilter const stf( scorefxn_, core::scoring::total_score, 0 );

	core::kinematics::MoveMapOP mm = new core::kinematics::MoveMap;
	if (core::pose::symmetry::is_symmetric(pose)) {
		core::pose::symmetry::make_symmetric_movemap( pose, *mm ); // NK 110621
	}
	mm->set_bb( false );
	if( unbound() ) // the complex was split, don't minimize rb dof
		mm->set_jump( false );
	else // minimize rb if bound
		mm->set_jump( rb_jump(), true );
	for( core::Size i=1; i<=pose.total_residue(); ++i ){
		if( task->being_designed( i ) ){
			task->nonconst_residue_task( i ).restrict_to_repacking(); // mark all des around to repacking only
			mm->set_chi( i, true );
		}
		else{
			task->nonconst_residue_task( i ).prevent_repacking(); /// mark all non-desaround positions for no repacking
			mm->set_chi( i, false );
		}
	}
	task->nonconst_residue_task( resi ).prevent_repacking();
	core::pack::pack_rotamers( pose, *scorefxn_, task );
	protocols::moves::MoverOP min_mover;
	if (core::pose::symmetry::is_symmetric(pose)) {
		min_mover = new protocols::simple_moves::symmetry::SymMinMover( mm, scorefxn_, "dfpmin_armijo_nonmonotone", 0.01, true, false, false ); // NK 110621
	} else {
		min_mover = new protocols::moves::MinMover( mm, scorefxn_, "dfpmin_armijo_nonmonotone", 0.01, true, false, false ); // NK 110621
	}
	min_mover->apply( pose );
	core::pose::Pose const const_min_pose( pose );
	core::Real const init_score( stf.compute( const_min_pose ) );
	TR<<"Total score for input pose: "<<init_score<<std::endl;
	utility::vector1< core::Real > scores;
	for( Rotamers::const_iterator rotamer = rotset->begin(); rotamer != rotset->end(); ++rotamer ){
		pose = const_min_pose;
		pose.replace_residue( resi, **rotamer, false/*orient bb*/ );
		core::pack::pack_rotamers( pose, *scorefxn(), task );
		min_mover->apply( pose );
		core::Real const score( stf.compute( pose ) );
		TR<<"This rotamer has score "<<score<<std::endl;
		scores.push_back( score );
	}
	core::Real boltz_sum ( 0.0 );
	foreach( core::Real const score, scores )
		boltz_sum += exp(( init_score - score )/temperature());

	return( 1/boltz_sum );
}

core::Real
RotamerBoltzmannWeight::ddG_threshold() const
{
	return ddG_threshold_;
}

void
RotamerBoltzmannWeight::ddG_threshold( core::Real const ddG )
{
	ddG_threshold_ = ddG;
}

core::Real
RotamerBoltzmannWeight::report_sm( core::pose::Pose const & pose ) const
{
	compute( pose );
	return( compute_modified_ddG( pose, TR ));
}

core::Real
RotamerBoltzmannWeight::interface_interaction_energy( core::pose::Pose const & pose, core::Size const res ) const
{
	using namespace core::graph;
	using namespace core::scoring;

	core::pose::Pose nonconst_pose( pose );
	(*scorefxn())(nonconst_pose);
	EnergyMap const curr_weights = nonconst_pose.energies().weights();

	core::Size const res_chain( pose.residue( res ).chain() );
	core::Real total_residue_energy( 0.0 );
	for( EdgeListConstIterator egraph_it = nonconst_pose.energies().energy_graph().get_node( res )->const_edge_list_begin(); egraph_it != nonconst_pose.energies().energy_graph().get_node( res )->const_edge_list_end(); ++egraph_it){
  	core::Size const int_resi = (*egraph_it)->get_other_ind( res );
		core::Size const int_resi_chain( pose.residue( int_resi ).chain() );
    if( int_resi_chain != res_chain ){ // only sum over interaction energies with the residue's non-host chain
    	EnergyEdge const * Eedge = static_cast< EnergyEdge const * > (*egraph_it);
    	core::Real const intE = Eedge->dot( curr_weights );
			total_residue_energy += intE;
    }//fi
	}//for each egraph_it
	return( total_residue_energy );
}


void
RotamerBoltzmannWeight::report( std::ostream & out, core::pose::Pose const & pose ) const
{

	if( type_ == "monomer" ) {
		out<<"RotamerBoltzmannWeightFilter returns "<<compute( pose )<<std::endl;
		out<<"RotamerBoltzmannWeightFilter final report\n";
		out<<"Residue"<<'\t'<<"ddG"<<'\t'<<"RotamerProbability"<<'\n';
		for( std::map< core::Size, core::Real >::const_iterator rot( rotamer_probabilities_.begin() ); rot != rotamer_probabilities_.end(); ++rot ){
			core::Size const res( rot->first );
			core::Real const prob( rot->second );
			core::Real const ddG( 0.0 );
			out<<pose.residue( res ).name3()<< pose.pdb_info()->number( res )<<'\t'<<ddG<<'\t'<<prob<<'\t';
			out<<'\n';
		}
	} else
		compute_modified_ddG( pose, out );
}

void
RotamerBoltzmannWeight::parse_my_tag( utility::tag::TagPtr const tag,
		protocols::moves::DataMap & data,
		protocols::filters::Filters_map const &,
		protocols::moves::Movers_map const &,
		core::pose::Pose const & )
{
	using namespace utility::tag;

	TR << "RotamerBoltzmannWeightFilter"<<std::endl;
	task_factory( protocols::rosetta_scripts::parse_task_operations( tag, data ) );
	repacking_radius( tag->getOption< core::Real >( "radius", 6.0 ) );
	type_ = tag->getOption< std::string >( "type", "" );
	rb_jump( tag->getOption< core::Size >( "jump", 1 ) );
	unbound( tag->getOption< bool >( "unbound", 1 ) );
	ddG_threshold( tag->getOption< core::Real >( "ddG_threshold", 1.5 ) );
	temperature( tag->getOption< core::Real >( "temperature", 0.8 ) );
	std::string const scorefxn_name( tag->getOption< std::string >( "scorefxn", "score12" ) );
	scorefxn( data.get< core::scoring::ScoreFunction * >( "scorefxns", scorefxn_name ) );
	energy_reduction_factor( tag->getOption< core::Real >( "energy_reduction_factor", 0.5 ) );
	compute_entropy_reduction( tag->getOption< bool >( "compute_entropy_reduction", 0 ) );
	repack( tag->getOption< bool >( "repack", 1 ) );
	utility::vector0< TagPtr > const branch( tag->getTags() );
	foreach( TagPtr const tag, branch ){
		using namespace core::chemical;

		std::string const residue_type( tag->getName() );
		AA const aa( aa_from_name( residue_type ) );
		core::Real const threshold_probability_input( tag->getOption< core::Real >( "threshold_probability" ) );

		threshold_probability( aa, threshold_probability_input );
	}

	fast_calc_ = tag->getOption< bool >("fast_calc",0);

	skip_ala_scan( tag->getOption< bool >( "skip_ala_scan", 0 ) );
	TR<<"with options repacking radius: "<<repacking_radius()<<" and jump "<<rb_jump()<<" unbound "<<unbound()<<" ddG threshold "<<ddG_threshold()<<" temperature "<<temperature()<<" energy reduction factr "<<energy_reduction_factor()<<" entropy_reduction "<<compute_entropy_reduction()<<" repack "<<repack()<<" skip_ala_scan "<<skip_ala_scan()<<std::endl;
}

protocols::filters::FilterOP
RotamerBoltzmannWeight::fresh_instance() const{
	return new RotamerBoltzmannWeight();
}

RotamerBoltzmannWeight::~RotamerBoltzmannWeight(){}

protocols::filters::FilterOP
RotamerBoltzmannWeight::clone() const{
	return new RotamerBoltzmannWeight( *this );
}

void
RotamerBoltzmannWeight::scorefxn( core::scoring::ScoreFunctionOP scorefxn ){
	scorefxn_ = scorefxn;
}

core::scoring::ScoreFunctionOP
RotamerBoltzmannWeight::scorefxn() const{
	return scorefxn_;
}

bool
RotamerBoltzmannWeight::unbound() const{
	return unbound_;
}

void
RotamerBoltzmannWeight::unbound( bool const u ){
	unbound_ = u;
}

bool
RotamerBoltzmannWeight::compute_entropy_reduction() const{
	return( compute_entropy_reduction_ );
}

void
RotamerBoltzmannWeight::compute_entropy_reduction( bool const cer ){
	compute_entropy_reduction_ = cer ;
}

protocols::filters::FilterOP
RotamerBoltzmannWeightFilterCreator::create_filter() const { return new RotamerBoltzmannWeight; }

std::string
RotamerBoltzmannWeightFilterCreator::keyname() const { return "RotamerBoltzmannWeight"; }

bool
RotamerBoltzmannWeight::skip_ala_scan() const
{
	return skip_ala_scan_;
}

void
RotamerBoltzmannWeight::skip_ala_scan( bool const s )
{
	skip_ala_scan_ = s;
}

std::string
RotamerBoltzmannWeight::type() const
{
	return type_;
}

void
RotamerBoltzmannWeight::type(std::string const & s)
{
	type_ = s;
}

/// Note that compute( pose ) needs to have been run first. This merely sums over the probabilities
core::Real
RotamerBoltzmannWeight::compute_modified_ddG( core::pose::Pose const & pose, std::ostream & out ) const
{
	protocols::simple_filters::DdgFilter ddg_filter( 100/*ddg_threshold*/, scorefxn(), rb_jump(), repack() ? 3 : 1 /*repeats*/ );
	ddg_filter.repack( repack() );

	core::Real const ddG_in( ddg_filter.compute( pose ) );
	core::Real modified_ddG( ddG_in );
	out<<"Residue"<<'\t'<<"ddG"<<'\t'<<"RotamerProbability"<<'\t'<<"EnergyReduction\n";
	for( std::map< core::Size, core::Real >::const_iterator rot( rotamer_probabilities_.begin() ); rot != rotamer_probabilities_.end(); ++rot ){
		core::Size const res( rot->first );
		core::Real const prob( rot->second );
		core::Real const ddG( ddGs_[ res ] );
		out<<pose.residue( res ).name3()<< pose.pdb_info()->number( res )<<'\t'<<ddG<<'\t'<<prob<<'\t';
		core::Real const threshold_prob( threshold_probability( pose.residue( res ).aa() ) );

		//out<<"prob: "<<prob<<"and threshold: " <<threshold_prob<<"\n";
		if( prob <= threshold_prob ){
			//core::Real const int_energy( interface_interaction_energy( pose, res ) );
			//core::Real const energy_reduction( energy_reduction_factor() * int_energy );
			core::Real const energy_reduction( -1 * temperature() * log( prob ) * energy_reduction_factor() );
			//out<<"after if statemnet energy reduction: "<<energy_reduction<<"this is prob: "<<prob<<"and this is threshold: " <<threshold_prob<<"\n";
			out<<energy_reduction;
			modified_ddG += energy_reduction;
		}
		else
			out<<0;
		out<<'\n';
	}
	out<<"ddG before, after modification: "<<ddG_in<<", "<<modified_ddG<<std::endl;
	return( modified_ddG );
}


} // filters
} // protein_interface_design
} // protocols
