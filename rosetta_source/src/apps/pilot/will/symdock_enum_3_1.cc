// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
// :noTabs=false:tabSize=4:indentSize=4:

/*
WISH LIST
	buried unsats
		scan for rotamers that cna make H-bonds
		detect and penalize missing BB density
	strand pairs
	centriod score components ?
	disulfide-compatible positions
	low-res sc? maybe patchdock-like complimentarity measure?
	statistically "good" BB xforms? all of by good contacts?
IGNORE
	contacts by SS ?? avg. degree probably sufficient
DONE
	termini distance
	contacts weight by avg. deg.
*/
#include <apps/pilot/will/xyzStripeHashPose.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>
#include <basic/options/keys/symmetry.OptionKeys.gen.hh>
#include <basic/options/option.hh>
#include <basic/options/option_macros.hh>
#include <basic/Tracer.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/conformation/Residue.hh>
#include <core/id/AtomID.hh>
#include <core/import_pose/import_pose.hh>
#include <core/io/pdb/pose_io.hh>
#include <core/pose/Pose.hh>
#include <core/pose/symmetry/util.hh>
#include <core/pose/util.hh>
#include <core/scoring/sasa.hh>
#include <core/util/SwitchResidueTypeSet.hh>
#include <devel/init.hh>
#include <numeric/constants.hh>
#include <numeric/xyz.functions.hh>
#include <numeric/xyz.io.hh>
#include <ObjexxFCL/FArray2D.hh>
#include <ObjexxFCL/FArray3D.hh>
#include <ObjexxFCL/format.hh>
#include <ObjexxFCL/string.functions.hh>
#include <utility/io/ozstream.hh>
#include <utility/string_util.hh>
#include <utility/vector1.hh>
#ifdef USE_OPENMP
#include <omp.h>
#endif
using core::Size;
using core::pose::Pose;
using std::string;
using utility::vector1;
using ObjexxFCL::fmt::I;
using ObjexxFCL::fmt::F;
using ObjexxFCL::fmt::RJ;
using numeric::min;
using numeric::max;
using std::cout;
using std::cerr;
using std::endl;
typedef numeric::xyzVector<core::Real> Vec;
typedef numeric::xyzMatrix<core::Real> Mat;
typedef numeric::xyzVector<double> Vecf;
typedef numeric::xyzMatrix<double> Matf;
static basic::Tracer TR("symdock_enum");
OPT_1GRP_KEY( Real , tcdock, clash_dis	)
OPT_1GRP_KEY( Real , tcdock, contact_dis )
OPT_1GRP_KEY( Real , tcdock, intra )
OPT_1GRP_KEY( Real , tcdock, intra1 )
OPT_1GRP_KEY( Real , tcdock, intra2 )
OPT_1GRP_KEY( Real , tcdock, termini_weight )
OPT_1GRP_KEY( Real , tcdock, termini_cutoff )
OPT_1GRP_KEY( Real , tcdock, hash_3D_vs_2D )
OPT_1GRP_KEY( Integer , tcdock, termini_trim )
OPT_1GRP_KEY( Integer , tcdock, nsamp1 )
OPT_1GRP_KEY( Integer , tcdock, topx )
OPT_1GRP_KEY( Integer , tcdock, peak_grid_size)
OPT_1GRP_KEY( Integer , tcdock, peak_grid_smooth)
OPT_1GRP_KEY( Boolean , tcdock, reverse )
OPT_1GRP_KEY( Boolean , tcdock, dump_pdb )
OPT_1GRP_KEY( IntegerVector , tcdock, dump_pdb_grid )
OPT_1GRP_KEY( Real , tcdock, grid_delta_rot )
OPT_1GRP_KEY( Real , tcdock, grid_delta_disp )
OPT_1GRP_KEY( Integer , tcdock, grid_size_rot )
OPT_1GRP_KEY( Integer , tcdock, grid_size_disp )
OPT_1GRP_KEY( IntegerVector , tcdock, dump_peak_grids )
OPT_1GRP_KEY( IntegerVector , tcdock, justone )
OPT_1GRP_KEY( FileVector, tcdock, I2 )
OPT_1GRP_KEY( FileVector, tcdock, I3 )
OPT_1GRP_KEY( FileVector, tcdock, I5 )
OPT_1GRP_KEY( FileVector, tcdock, O2 )
OPT_1GRP_KEY( FileVector, tcdock, O3 )
OPT_1GRP_KEY( FileVector, tcdock, O4 )
OPT_1GRP_KEY( FileVector, tcdock, T2 )
OPT_1GRP_KEY( FileVector, tcdock, T3 )
OPT_1GRP_KEY( Boolean , tcdock, usen1 )
OPT_1GRP_KEY( Boolean , tcdock, usec1 )
OPT_1GRP_KEY( Boolean , tcdock, require_exposed_termini )
OPT_1GRP_KEY( Real , tcdock, term_min_expose )
OPT_1GRP_KEY( Real , tcdock, term_max_angle )

void register_options() {
		using namespace basic::options;
		using namespace basic::options::OptionKeys;
		OPT( in::file::s );
		NEW_OPT( tcdock::clash_dis	      ,"max acceptable clash dis"            ,  3.5   );
		NEW_OPT( tcdock::contact_dis      ,"max acceptable contact dis"          , 12     );
		NEW_OPT( tcdock::intra            ,"weight for intra contacts"           ,  1.0   );
		NEW_OPT( tcdock::intra1           ,"weight for comp1 intra contacts"     ,  1.0   );
		NEW_OPT( tcdock::intra2           ,"weight for conp2 intra contacts"     ,  1.0   );
		NEW_OPT( tcdock::nsamp1           ,"check around X top lowres hits"      , 2000   );
		NEW_OPT( tcdock::topx             ,"output top X hits"                   , 10     );
		NEW_OPT( tcdock::peak_grid_size   ,"peak detect grid size (2*N+1)"       , 24     );
		NEW_OPT( tcdock::peak_grid_smooth ,"peak detect grid smooth (0+)"        ,  1     );
		NEW_OPT( tcdock::reverse          ,"reverse one component"               , false  );
		NEW_OPT( tcdock::dump_pdb         ,"dump pdbs"                           , false  );
		NEW_OPT( tcdock::dump_pdb_grid    ,"dump pdb design grids"               , -1  );
		NEW_OPT( tcdock::grid_delta_rot   ,"grid samp size theta"                ,  0.5   );
		NEW_OPT( tcdock::grid_delta_disp  ,"grid samp size r"                    ,  0.5   );
		NEW_OPT( tcdock::grid_size_rot    ,"grid nsamp theta +- (2*N+1 samp)"    ,  1     );
		NEW_OPT( tcdock::grid_size_disp   ,"grid samp r"                         ,  5     );
		NEW_OPT( tcdock::dump_peak_grids  ,"dump specific grids grids"           , -1     );
		NEW_OPT( tcdock::justone          ,"dump info on one structure"          , -1     );
		NEW_OPT( tcdock::I2               ,"file(s) for icos 2fold dimer"        , ""     );
		NEW_OPT( tcdock::I3               ,"file(s) for icos 3fold trimer"       , ""     );
		NEW_OPT( tcdock::I5               ,"file(s) for icos 5fold pentamer"     , ""     );
		NEW_OPT( tcdock::O2               ,"file(s) for octa 2fold dimer"        , ""     );
		NEW_OPT( tcdock::O3               ,"file(s) for octa 3fold trimer"       , ""     );
		NEW_OPT( tcdock::O4               ,"file(s) for octa 4fold tetramer"     , ""     );
		NEW_OPT( tcdock::T2               ,"file(s) for tetr 2fold dimer"        , ""     );
		NEW_OPT( tcdock::T3               ,"file(s) for tetr 3fold trimer"       , ""     );
		NEW_OPT( tcdock::termini_cutoff   ,"tscore = w*max(0,cut-x)"             , 20.0   );
		NEW_OPT( tcdock::termini_weight   ,"tscore = w*max(0,cut-x)"             ,  0.0   );
		NEW_OPT( tcdock::termini_trim     ,"trim termini up to for termini score",  0     );
		NEW_OPT( tcdock::hash_3D_vs_2D    ,"grid spacing top 2D hash"            ,  1.3   );
		NEW_OPT( tcdock::usec1            ,"use comp1 cterm"                     , true  );
		NEW_OPT( tcdock::usen1            ,"use comp1 nterm"                     , true  );
		NEW_OPT( tcdock::term_max_angle ,""             , 50.0  );
		NEW_OPT( tcdock::term_min_expose ,""            , 0.85  );

}
template<typename T> inline T sqr(T x) { return x*x; }
void dump_points_pdb(utility::vector1<Vecf> const & p, std::string fn) {
	using namespace ObjexxFCL::fmt;
	std::ofstream o(fn.c_str());
	for(Size i = 1; i <= p.size(); ++i) {
		std::string rn = "VIZ";
		o<<"HETATM"<<I(5,i)<<' '<<" CA "<<' '<<rn<<' '<<"A"<<I(4,i)<<"    "<<F(8,3,p[i].x())<<F(8,3,p[i].y())<<F(8,3,p[i].z())<<F(6,2,1.0)<<F(6,2,1.0)<<'\n';
	}
	o.close();
}
void dump_points_pdb(utility::vector1<Vecf> const & p, Vec t, std::string fn) {
	using namespace ObjexxFCL::fmt;
	std::ofstream o(fn.c_str());
	for(Size i = 1; i <= p.size(); ++i) {
		std::string rn = "VIZ";
		o<<"HETATM"<<I(5,i)<<' '<<" CA "<<' '<<rn<<' '<<"A"<<I(4,i)<<"    "<<F(8,3,p[i].x()+t.x())<<F(8,3,p[i].y()+t.y())<<F(8,3,p[i].z()+t.z())<<F(6,2,1.0)<<F(6,2,1.0)<<'\n';
	}
	o.close();
}
void dump_points_pdb(xyzStripeHash<double>::float4 const *p, int n, Vec t, std::string fn) {
	using namespace ObjexxFCL::fmt;
	std::ofstream o(fn.c_str());
	for(Size i = 0; i < n; ++i) {
		std::string rn = "VIZ";
		o<<"HETATM"<<I(5,i)<<' '<<" CA "<<' '<<rn<<' '<<"A"<<I(4,i)<<"    "<<F(8,3,p[i].x-t.x())<<F(8,3,p[i].y-t.y())<<F(8,3,p[i].z-t.z())<<F(6,2,1.0)<<F(6,2,1.0)<<'\n';
	}
	o.close();
}
void dump_points_pdb(xyzStripeHash<double>::float4 const *p, int n, std::string fn) {
	using namespace ObjexxFCL::fmt;
	std::ofstream o(fn.c_str());
	for(Size i = 0; i < n; ++i) {
		std::string rn = "VIZ";
		o<<"HETATM"<<I(5,i)<<' '<<" CA "<<' '<<rn<<' '<<"A"<<I(4,i)<<"    "<<F(8,3,p[i].x)<<F(8,3,p[i].y)<<F(8,3,p[i].z)<<F(6,2,1.0)<<F(6,2,1.0)<<'\n';
	}
	o.close();
}
inline double sigmoid( double const & sqdist, double const & start, double const & stop ) {
	if( sqdist > stop*stop ) {
		return 0.0;
	} else if( sqdist < start*start ) {
		return 1.0;
	} else {
		double dist = sqrt( sqdist );
		return (stop-dist)/(stop-start);
		//return sqr(1.0	- sqr( (dist - start) / (stop - start) ) );
	}
}
void trans_pose( Pose & pose, Vecf const & trans, Size start=1, Size end=0 ) {
	if(0==end) end = pose.n_residue();
	for(Size ir = start; ir <= end; ++ir) {
		for(Size ia = 1; ia <= pose.residue_type(ir).natoms(); ++ia) {
			core::id::AtomID const aid(core::id::AtomID(ia,ir));
			pose.set_xyz( aid, pose.xyz(aid) + (Vec)trans );
		}
	}
}
void rot_pose( Pose & pose, Mat const & rot, Size start=1, Size end=0 ) {
	if(0==end) end = pose.n_residue();
	for(Size ir = start; ir <= end; ++ir) {
		for(Size ia = 1; ia <= pose.residue_type(ir).natoms(); ++ia) {
			core::id::AtomID const aid(core::id::AtomID(ia,ir));
			pose.set_xyz( aid, rot * pose.xyz(aid) );
		}
	}
}
void rot_pose( Pose & pose, Mat const & rot, Vecf const & cen, Size start=1, Size end=0 ) {
	trans_pose(pose,-cen,start,end);
	rot_pose(pose,rot,start,end);
	trans_pose(pose,cen,start,end);
}
void rot_pose( Pose & pose, Vecf const & axis, double const & ang, Size start=1, Size end=0 ) {
	rot_pose(pose,rotation_matrix_degrees(axis,ang),start,end);
}
void rot_pose( Pose & pose, Vecf const & axis, double const & ang, Vecf const & cen, Size start=1, Size end=0 ) {
	rot_pose(pose,rotation_matrix_degrees(axis,ang),cen,start,end);
}
void alignaxis(Pose & pose, Vecf newaxis, Vecf oldaxis, Vecf cen = Vecf(0,0,0) ) {
	newaxis.normalize();
	oldaxis.normalize();
	if(fabs(newaxis.dot(oldaxis)) < 0.9999) {
		Vecf axis = newaxis.cross(oldaxis).normalized();
		double ang = (double)-acos(numeric::max((double)-1.0,numeric::min((double)1.0,newaxis.dot(oldaxis))))*(double)180.0/numeric::constants::f::pi;
		rot_pose(pose,axis,ang,cen);
	}
}
inline Vec projperp(Vec const & u, Vec const & v) {
  return v - projection_matrix(u)*v;
}
void prune_cb_pairs(vector1<Vecf> & cba, vector1<Vecf> & cbb, vector1<double> & wa_in, vector1<double> & wb_in, double CTD2) {
	vector1<Vecf> a,b;
	vector1<double> wa,wb;
	vector1<double>::const_iterator iwa = wa_in.begin();
	for(vector1<Vecf>::const_iterator ia = cba.begin(); ia != cba.end(); ++ia,++iwa) {
		vector1<double>::const_iterator iwb = wb_in.begin();
		for(vector1<Vecf>::const_iterator ib = cbb.begin(); ib != cbb.end(); ++ib,++iwb) {
			if( ib->distance_squared(*ia) < CTD2 ) {
				a.push_back(*ia);
				b.push_back(*ib);
				wa.push_back(*iwa);
				wb.push_back(*iwb);
			}
		}
	}
	cba = a;
	cbb = b;
	wa_in = wa;
	wb_in = wb;
}
int neighbor_count(Pose const &pose, int ires, double distance_threshold=10.0) {
	core::conformation::Residue const resi( pose.residue( ires ) );
	Size resi_neighbors( 0 );
	for(Size jres = 1; jres <= pose.n_residue(); ++jres) {
		core::conformation::Residue const resj( pose.residue( jres ) );
		double const distance( resi.xyz( resi.nbr_atom() ).distance( resj.xyz( resj.nbr_atom() ) ) );
		if( distance <= distance_threshold ){
			++resi_neighbors;
		}
	}
	return resi_neighbors;
}
double brute_mindis(vector1<Vecf> const & pa, vector1<Vecf> const & pb, Vecf const ofst) {
	double mindis = 9e9;
	for(vector1<Vecf>::const_iterator i = pa.begin(); i != pa.end(); ++i) {
		for(vector1<Vecf>::const_iterator j = pb.begin(); j != pb.end(); ++j) {
			mindis = min( mindis, i->distance_squared(*j+ofst) );
		}
	}
	return mindis;
}
struct SICFast {
	double xmx1,xmn1,ymx1,ymn1,xmx,xmn,ymx,ymn;
	double CTD,CLD,CTD2,CLD2,BIN;
	int xlb,ylb,xub,yub;
	xyzStripeHashPose xh1_bb_,xh2_bb_,xh1_cb_,xh2_cb_;
	vector1<double> w1_,w2_;
	ObjexxFCL::FArray2D<Vecf> ha,hb;
	SICFast() :
		CTD(basic::options::option[basic::options::OptionKeys::tcdock::contact_dis]()),
		CLD(basic::options::option[basic::options::OptionKeys::tcdock::clash_dis]()),
		CTD2(sqr(CTD)),CLD2(sqr(CLD)),BIN(CLD*basic::options::option[basic::options::OptionKeys::tcdock::hash_3D_vs_2D]()),
		xh1_bb_(basic::options::option[basic::options::OptionKeys::tcdock::  clash_dis]()+0.5),
		xh2_bb_(basic::options::option[basic::options::OptionKeys::tcdock::  clash_dis]()+0.5),
		xh1_cb_(basic::options::option[basic::options::OptionKeys::tcdock::contact_dis]()),
		xh2_cb_(basic::options::option[basic::options::OptionKeys::tcdock::contact_dis]())
	{}
	void init(
		core::pose::Pose const & cmp1in, vector1<Vecf> cmp1cbs, vector1<double> const & cmp1wts, Vecf cmp1axs,
		core::pose::Pose const & cmp2in, vector1<Vecf> cmp2cbs, vector1<double> const & cmp2wts, Vecf cmp2axs
	) {
		xh1_bb_.init_with_pose(cmp1in,BB); // default meta is radius
		xh2_bb_.init_with_pose(cmp2in,BB);
		xh1_cb_.init_with_pose(cmp1in,cmp1wts,CB);
		xh2_cb_.init_with_pose(cmp2in,cmp2wts,CB);
		w1_ = cmp1wts;
		w2_ = cmp2wts;
		// Pose tmp1(cmp1in); trans_pose(tmp1,xh1_bb_.translation()); tmp1.dump_pdb("pose1.pdb");
		// Pose tmp2(cmp2in); trans_pose(tmp2,xh2_bb_.translation()); tmp2.dump_pdb("pose2.pdb");
		// dump_points_pdb(xh1_bb_.grid_atoms(),xh1_bb_.natom()                                            ,"xh1.pdb");
		// dump_points_pdb(xh2_bb_.grid_atoms(),xh2_bb_.natom()                                            ,"xh2.pdb");
		// dump_points_pdb(xh1_cb_.grid_atoms(),xh1_cb_.natom(),xh1_cb_.translation()-xh1_bb_.translation(),"xh1_cb.pdb");
		// dump_points_pdb(xh2_cb_.grid_atoms(),xh2_cb_.natom(),xh2_cb_.translation()-xh2_bb_.translation(),"xh2_cb.pdb");
		// dump_points_pdb(cmp1cbs                             ,xh1_bb_.translation()                      ,"cmp1cbs.pdb");
		// dump_points_pdb(cmp2cbs                             ,xh2_bb_.translation()                      ,"cmp2cbs.pdb");
		// cmp1in.dump_pdb("cmp1in.pdb");
		// cmp2in.dump_pdb("cmp2in.pdb");
		// utility_exit_with_message("hashes");
		// double xmn=9e9,ymn=9e9,zmn=9e9;
		// for(vector1<Vecf>::const_iterator i = cmp1cbs.begin(); i != cmp1cbs.end(); ++i) {
		// 	xmn = min(i->x(),xmn);
		// 	ymn = min(i->y(),ymn);
		// 	zmn = min(i->z(),zmn);
		// }
		// cout << xmn << " " << ymn << " " << zmn << endl;
		// xmn=9e9,ymn=9e9,zmn=9e9;
		// for(vector1<Vecf>::const_iterator i = cmp2cbs.begin(); i != cmp2cbs.end(); ++i) {
		// 	xmn = min(i->x(),xmn);
		// 	ymn = min(i->y(),ymn);
		// 	zmn = min(i->z(),zmn);
		// }
		// cout << xmn << " " << ymn << " " << zmn << endl;
		// cout << xh1_cb_.translation() << endl;
		// cout << xh2_cb_.translation() << endl;
		// utility_exit_with_message("arst");
	}
	void rotate_points(vector1<Vecf> & pa, vector1<Vecf> & pb, Vecf ori) {
		// // get points, rotated ro ori is 0,0,1, might already be done
		// Matf rot = Matf::identity();
		// if     ( ori.dot(Vecf(0,0,1)) < -0.99999 ) rot = rotation_matrix( Vecf(1,0,0).cross(ori), (double)-acos(Vecf(0,0,1).dot(ori)) );
		// else if( ori.dot(Vecf(0,0,1)) <  0.99999 ) rot = rotation_matrix( Vecf(0,0,1).cross(ori), (double)-acos(Vecf(0,0,1).dot(ori)) );
		// if( rot != Matf::identity() ) {
		//         for(vector1<Vecf>::iterator ia = pa.begin(); ia != pa.end(); ++ia) *ia = rot*(*ia);
		//         for(vector1<Vecf>::iterator ib = pb.begin(); ib != pb.end(); ++ib) *ib = rot*(*ib);
		// }
		Matf rot = rotation_matrix_degrees( (ori.z() < -0.99999) ? Vec(1,0,0) : (Vec(0,0,1)+ori.normalized())/2.0 , 180.0 );
		for(vector1<Vecf>::iterator ia = pa.begin(); ia != pa.end(); ++ia) *ia = rot*(*ia);
		for(vector1<Vecf>::iterator ib = pb.begin(); ib != pb.end(); ++ib) *ib = rot*(*ib);
	}
	void get_bounds(vector1<Vecf> & pa, vector1<Vecf> & pb) {
		// get bounds for plane hashes
		xmx1=-9e9,xmn1=9e9,ymx1=-9e9,ymn1=9e9,xmx=-9e9,xmn=9e9,ymx=-9e9,ymn=9e9;
		for(vector1<Vecf>::const_iterator ia = pa.begin(); ia != pa.end(); ++ia) {
			xmx1 = max(xmx1,ia->x()); xmn1 = min(xmn1,ia->x());
			ymx1 = max(ymx1,ia->y()); ymn1 = min(ymn1,ia->y());
		}
		for(vector1<Vecf>::const_iterator ib = pb.begin(); ib != pb.end(); ++ib) {
			xmx = max(xmx,ib->x()); xmn = min(xmn,ib->x());
			ymx = max(ymx,ib->y()); ymn = min(ymn,ib->y());
		}
		xmx = min(xmx,xmx1); xmn = max(xmn,xmn1);
		ymx = min(ymx,ymx1); ymn = max(ymn,ymn1);
	}
	double get_score(vector1<Vecf> const & cba, vector1<Vecf> const & cbb, Vecf ori, double mindis,
	                 double anga, double angb, Vecf axsa, Vecf axsb, bool cmp1or2_a, bool cmp1or2_b)
	{
 		vector1<double> const & wa = cmp1or2_a ? w1_ : w2_;
		vector1<double> const & wb = cmp1or2_b ? w1_ : w2_;
		xyzStripeHash<double> const & xh( cmp1or2_a ? xh1_cb_ : xh2_cb_);
		Matf R = numeric::rotation_matrix_degrees(axsa,-anga);
		float score = 0.0;
		vector1<double>::const_iterator iwb = wb.begin();
		for(vector1<Vecf>::const_iterator i = cbb.begin(); i != cbb.end(); ++i,++iwb) {
			Vecf v = R*((*i)-mindis*ori) + xh.translation();
			if( v.x() < -xh.grid_size_ || v.y() < -xh.grid_size_ || v.z() < -xh.grid_size_ ) continue; // worth it?
			if( v.x() >  xh.xmx_       || v.y() >  xh.ymx_       || v.z() >  xh.zmx_       ) continue; // worth it?
			int const ix	= (v.x()<0.0) ? 0 : (int)(numeric::min(xh.xdim_-1,(int)(v.x()/xh.grid_size_)));
			int const iy0	= (v.y()<0.0) ? 0 : (int)(v.y()/xh.grid_size_);
			int const iz0	= (v.z()<0.0) ? 0 : (int)(v.z()/xh.grid_size_);
			int const iyl = numeric::max(0,iy0-1);
			int const izl = numeric::max(0,iz0-1);
			int const iyu = numeric::min((int)xh.ydim_,     iy0+2);
			int const izu = numeric::min((int)xh.zdim_,(int)iz0+2);
			for(int iy = iyl; iy < iyu; ++iy) {
				for(int iz = izl; iz < izu; ++iz) {
					int const ig = ix+xh.xdim_*iy+xh.xdim_*xh.ydim_*iz;
					assert(ig < xh.xdim_*xh.ydim_*xh.zdim_ && ix < xh.xdim_ && iy < xh.ydim_ && iz < xh.zdim_);
					int const igl = xh.grid_stripe_[ig].x;
					int const igu = xh.grid_stripe_[ig].y;
					for(int i = igl; i < igu; ++i) {
						xyzStripeHash<double>::float4 const & a2 = xh.grid_atoms_[i];
						float const d2 = (v.x()-a2.x)*(v.x()-a2.x) + (v.y()-a2.y)*(v.y()-a2.y) + (v.z()-a2.z)*(v.z()-a2.z);
						if( d2 <= xh.grid_size2_ ) {
							score += sigmoid(d2, CLD, CTD ) * a2.w * (*iwb);
						}
					}
				}
			}
		}
		// double cbcount = 0.0;
		// vector1<double>::const_iterator iwa = wa.begin();
		// for(vector1<Vecf>::const_iterator ia = cba.begin(); ia != cba.end(); ++ia,++iwa) {
		// 	vector1<double>::const_iterator iwb = wb.begin();
		// 	for(vector1<Vecf>::const_iterator ib = cbb.begin(); ib != cbb.end(); ++ib,++iwb) {
		// 		double d2 = ib->distance_squared( (*ia) + (mindis*ori) );
		// 		if( d2 < CTD2 ) {
		// 			cbcount += sigmoid(d2, CLD, CTD ) * (*iwa) * (*iwb);
		// 		}
		// 	}
		// }
		// if( fabs(cbcount-score) > 0.0001 ) {
		// 	cout << "btute/hash score mismatch!!!! " << anga << " " << angb << " " << cbcount << " " << score << endl;
		// }
		return score;
	}
	double refine_mindis_with_xyzHash(vector1<Vecf> const & pa, vector1<Vecf> const & pb, double mindis, Vecf ori,
	                                  double anga, double angb, Vecf axsa, Vecf axsb, bool cmp1or2_a, bool cmp1or2_b)
	{

		xyzStripeHashPose const & xh( cmp1or2_a ? xh1_bb_ : xh2_bb_ );
		Matf Rori = rotation_matrix_degrees( (ori.z() < -0.99999) ? Vec(1,0,0) : (Vec(0,0,1)+ori.normalized())/2.0 , 180.0 );
		Matf R    = numeric::rotation_matrix_degrees(axsa,-anga);
		Matf Rinv = numeric::rotation_matrix_degrees(axsa, anga);
		while(true){
			double correction_hash = 9e9;
			for(vector1<Vecf>::const_iterator ib = pb.begin(); ib != pb.end(); ++ib) {
				Vecf const v = R*Rori*((*ib)-Vecf(0,0,mindis)) + xh.translation();
				Vecf const b = Rori * Rinv * v;
				if( v.x() < -xh.grid_size_ || v.y() < -xh.grid_size_ || v.z() < -xh.grid_size_ ) continue; // worth it?
				if( v.x() >  xh.xmx_       || v.y() >  xh.ymx_       || v.z() >  xh.zmx_       ) continue; // worth it?
				int const ix  = (v.x()<0) ? 0 : numeric::min(xh.xdim_-1,(int)(v.x()/xh.grid_size_));
				int const iy0 = (int)((v.y()<0) ? 0 : v.y()/xh.grid_size_);
				int const iz0 = (int)((v.z()<0) ? 0 : v.z()/xh.grid_size_);
				int const iyl = numeric::max(0,iy0-1);
				int const izl = numeric::max(0,iz0-1);
				int const iyu = numeric::min((int)xh.ydim_,     iy0+2);
				int const izu = numeric::min((int)xh.zdim_,(int)iz0+2);
				for(int iy = iyl; iy < iyu; ++iy) {
					for(int iz = izl; iz < izu; ++iz) {
						int const ig = ix+xh.xdim_*iy+xh.xdim_*xh.ydim_*iz;
						assert(ig < xh.xdim_*xh.ydim_*xh.zdim_ && ix < xh.xdim_ && iy < xh.ydim_ && iz < xh.zdim_);
						int const igl = xh.grid_stripe_[ig].x;
						int const igu = xh.grid_stripe_[ig].y;
						for(int i = igl; i < igu; ++i) {
							xyzStripeHash<double>::float4 const & a2 = xh.grid_atoms_[i];
							float const d2 = (v.x()-a2.x)*(v.x()-a2.x) + (v.y()-a2.y)*(v.y()-a2.y) + (v.z()-a2.z)*(v.z()-a2.z);
							Vecf const a = Rori * Rinv * Vecf(a2.x,a2.y,a2.z);
							double const dxy2 = (a.x()-b.x())*(a.x()-b.x()) + (a.y()-b.y())*(a.y()-b.y());
							if( dxy2 >= CLD2 ) continue;
							double const dz = b.z() - a.z() - sqrt(CLD2-dxy2);
							// cout << "HASH " << dz << endl;
							correction_hash = min(dz,correction_hash);
						}
					}
				}
			}
			mindis += correction_hash;
			if( fabs(correction_hash) < 0.001 ) break;
		}
		// double correction_safe = 9e9;
		// for(vector1<Vec>::const_iterator ib = pb.begin(); ib != pb.end(); ++ib) {
		// 	Vecf const v = ((*ib)-Vecf(0,0,mindis));
		// 	// dbg2.push_back(v);
		// 	for(vector1<Vec>::const_iterator ia = pa.begin(); ia != pa.end(); ++ia) {
		// 		// correction_safe = min(correction_safe,ia->distance_squared(v));
		// 		double const dxy2 = (ia->x()-v.x())*(ia->x()-v.x()) + (ia->y()-v.y())*(ia->y()-v.y());
		// 		if( dxy2 >= CLD2 ) continue;
		// 		// cout << "BRUTE " << dxy2 << endl;
		// 		double const dz = v.z() - ia->z() - sqrt(CLD2-dxy2);
		// 		if( dz < correction_safe) correction_safe = dz;
		// 	}
		// }
		// #ifdef USE_OPENMP
		// #pragma omp critical
		// #endif
		// if( fabs(correction_safe-correction_hash) > 0.01 ) {
		// 	cout << F(9,5,correction_hash) << " " << F(9,5,correction_safe) << " " << mindis << endl;
		// 	// utility_exit_with_message("FOO");
		// }

		return mindis;
	}
	double slide_into_contact(vector1<Vecf> pa, vector1<Vecf> pb, vector1<Vecf> const & cba, vector1<Vecf> const & cbb, Vecf ori, double & score,
	                          double anga, double angb, Vecf axsa, Vecf axsb, bool cmp1or2_a, bool cmp1or2_b)
	{
		rotate_points(pa,pb,ori);
		get_bounds(pa,pb);
		fill_plane_hash(pa,pb);
		double const mindis_approx = get_mindis_with_plane_hashes();
		double const mindis = refine_mindis_with_xyzHash(pa,pb,mindis_approx,ori,anga,angb,axsa,axsb,cmp1or2_a,cmp1or2_b);
		//cerr << brute_mindis(pa,pb,Vecf(0,0,-mindis)) << endl;
		// if( fabs(CLD2-brute_mindis(pa,pb,Vecf(0,0,-mindis))) > 0.0001 ) utility_exit_with_message("DIAF!");
		if(score != -12345.0) score = get_score(cba,cbb,ori,mindis,anga,angb,axsa,axsb,cmp1or2_a,cmp1or2_b);
		return mindis;
	}
	void fill_plane_hash(vector1<Vecf> & pa, vector1<Vecf> & pb) {
		xlb = (int)(xmn/BIN)-2; xub = (int)(xmx/BIN+0.999999999)+2; // one extra on each side for correctness,
		ylb = (int)(ymn/BIN)-2; yub = (int)(ymx/BIN+0.999999999)+2; // and one extra for outside atoms
		ha.dimension(xub-xlb+1,yub-ylb+1,Vecf(0,0,-9e9));
		hb.dimension(xub-xlb+1,yub-ylb+1,Vecf(0,0, 9e9));
		int const xsize = xub-xlb+1;
		int const ysize = yub-ylb+1;
		for(vector1<Vecf>::const_iterator ia = pa.begin(); ia != pa.end(); ++ia) {
			int const ix = (int)((ia->x()/BIN)-xlb+0.999999999);
			int const iy = (int)((ia->y()/BIN)-ylb+0.999999999);
			if( ix < 1 || ix > xsize || iy < 1 || iy > ysize ) continue;
			if( ha(ix,iy).z() < ia->z() ) ha(ix,iy) = *ia;
			// bool const test = !( ix < 1 || ix > xsize || iy < 1 || iy > ysize) && ha(ix,iy).z() < ia->z();
			// ha(ix,iy) = test ? *ia : ha(ix,iy);
		}
		for(vector1<Vecf>::const_iterator ib = pb.begin(); ib != pb.end(); ++ib) {
			int const ix = (int)((ib->x()/BIN)-xlb+0.999999999);
			int const iy = (int)((ib->y()/BIN)-ylb+0.999999999);
			if( ix < 1 || ix > xsize || iy < 1 || iy > ysize ) continue;
			if( hb(ix,iy).z() > ib->z() ) hb(ix,iy) = *ib;
			// bool const test = !( ix < 1 || ix > xsize || iy < 1 || iy > ysize ) && hb(ix,iy).z() > ib->z();
			// hb(ix,iy) = test ? *ib : hb(ix,iy);
		}

	}
	double get_mindis_with_plane_hashes() {
		int const xsize=xub-xlb+1, ysize=yub-ylb+1;
		int imna=0,jmna=0,imnb=0,jmnb=0;
		double m = 9e9;
		for(int i = 1; i <= xsize; ++i) { // skip 1 and N because they contain outside atoms (faster than clashcheck?)
			for(int j = 1; j <= ysize; ++j) {
				for(int k = -1; k <= 1; ++k) {
					if(i+k < 1 || i+k > xsize) continue;
					for(int l = -1; l <= 1; ++l) {
						if(j+l < 1 || j+l > ysize) continue;
						double const xa1=ha(i,j).x(),ya1=ha(i,j).y(),xb1=hb(i+k,j+l).x(),yb1=hb(i+k,j+l).y(),d21=(xa1-xb1)*(xa1-xb1)+(ya1-yb1)*(ya1-yb1);
						if(d21<CLD2){ double const dz=hb(i+k,j+l).z()-ha(i,j).z()-sqrt(CLD2-d21); if(dz<m) m=dz; }
					}
				}
			}
		}
		return m;
	}
};
int flood_fill3D(int i, int j, int k, ObjexxFCL::FArray3D<double> & grid, double t) {
	if( grid(i,j,k) <= t ) return 0;
	grid(i,j,k) = t;
	int nmark = 1;
	if(i>1           ) nmark += flood_fill3D(i-1,j  ,k  ,grid,t);
	if(i<grid.size1()) nmark += flood_fill3D(i+1,j  ,k  ,grid,t);
	if(j>1           ) nmark += flood_fill3D(i  ,j-1,k  ,grid,t);
	if(j<grid.size2()) nmark += flood_fill3D(i  ,j+1,k  ,grid,t);
	if(k>1           ) nmark += flood_fill3D(i  ,j  ,k-1,grid,t);
	if(k<grid.size3()) nmark += flood_fill3D(i  ,j  ,k+1,grid,t);
	return nmark;
}
struct LMAX {
	double score,radius;
	int icmp2,icmp1,iori;
	LMAX() :	score(0),radius(0),icmp2(0),icmp1(0),iori(0) {}
	LMAX(double _score, double _radius, int _icmp2, int _icmp1, int _iori) :
		score(_score),radius(_radius),icmp2(_icmp2),icmp1(_icmp1),iori(_iori) {}
};
int compareLMAX(const LMAX a,const LMAX b) {
	return a.score > b.score;
}


void termini_exposed(core::pose::Pose const & pose, bool & ntgood, bool & ctgood ) {
	using basic::options::option;
	using namespace basic::options::OptionKeys;
	Vec nt = pose.residue(        1       ).xyz("N");
	Vec ct = pose.residue(pose.n_residue()).xyz("C");
	core::Real nnt=0.0,nct=0.0,gnt=0.0,gct=0.0;
	for(int ir=1; ir<=pose.n_residue(); ++ir) {
		for(int ia=1; ia<=5; ++ia) {
			Vec x = pose.residue(ir).xyz(ia);
			if(angle_degrees(x,Vec(0,0,0),nt) < 15.0 &&  ) {
				nnt += 1.0;
				if( nt.normalized().dot(x) < nt.length() )
					gnt += 1.0;
			}
			if(angle_degrees(x,Vec(0,0,0),ct) < 15.0 ) {
				nct += 1.0;
				if( ct.normalized().dot(x) < ct.length() )
					gct += 1.0;
			}
		}
	}
	ntgood = gnt/nnt > option[tcdock::term_min_expose]() && angle_degrees(nt,Vec(0,0,0),Vec(nt.x(),nt.y(),0)) < option[tcdock::term_max_angle]();
	ctgood = gct/nct > option[tcdock::term_min_expose]() && angle_degrees(ct,Vec(0,0,0),Vec(ct.x(),ct.y(),0)) < option[tcdock::term_max_angle]();

	std::cerr << gnt/nnt << " " << angle_degrees(nt,Vec(0,0,0),Vec(nt.x(),nt.y(),0)) << endl;
	std::cerr << gct/nct << " " << angle_degrees(ct,Vec(0,0,0),Vec(ct.x(),ct.y(),0)) << endl;
	std::cerr << ntgood << endl;
	std::cerr << ctgood << endl;
}



struct TCDock {
	vector1<double> cmp2mnpos_,cmp1mnpos_,cmp2mnneg_,cmp1mnneg_,cmp2dspos_,cmp1dspos_,cmp2dsneg_,cmp1dsneg_;
	ObjexxFCL::FArray2D<double> cmp2cbpos_,cmp1cbpos_,cmp2cbneg_,cmp1cbneg_;
	ObjexxFCL::FArray3D<double> gradii,gscore;
	Vecf cmp1axs_,cmp2axs_;
	double alpha_,sin_alpha_,tan_alpha_;
	double cmp1diapos_,cmp1dianeg_,cmp2diapos_,cmp2dianeg_;
	core::pose::Pose cmp1in_,cmp2in_;
	vector1<Vecf> cmp1pts_,cmp1cbs_,cmp2pts_,cmp2cbs_;
	vector1<double> cmp1wts_,cmp2wts_;
	string cmp1name_,cmp2name_,cmp1type_,cmp2type_,symtype_;
	int cmp1nangle_,cmp2nangle_,cmp1nsub_,cmp2nsub_;
	std::map<string,Vecf> axismap_;
	std::vector<SICFast*> sics_;
	TCDock( string cmp1pdb, string cmp2pdb, string cmp1type, string cmp2type ) : cmp1type_(cmp1type),cmp2type_(cmp2type) {
		#ifdef USE_OPENMP
		cout << "OMP info: " << num_threads() << " " << thread_num() << endl;
		#endif
		using basic::options::option;
		using namespace basic::options::OptionKeys;
		core::chemical::ResidueTypeSetCAP crs=core::chemical::ChemicalManager::get_instance()->residue_type_set(core::chemical::CENTROID);
		core::import_pose::pose_from_pdb(cmp1in_,*crs,cmp1pdb);
		core::import_pose::pose_from_pdb(cmp2in_,*crs,cmp2pdb);

		cmp1diapos_=0.0,cmp1dianeg_=0.0,cmp2diapos_=0.0,cmp2dianeg_=0.0;
		for(Size i = 1; i <= cmp1in_.n_residue(); ++i) {
			int const natom = (cmp1in_.residue(i).name3()=="GLY") ? 4 : 5;
			for(int j = 1; j <= natom; ++j) {
				Vec const & v( cmp1in_.residue(i).xyz(j) );
				cmp1diapos_ = max(cmp1diapos_, v.z());
				cmp1dianeg_ = max(cmp1dianeg_,-v.z());
			}
		}
		for(Size i = 1; i <= cmp2in_.n_residue(); ++i) {
			int const natom = (cmp2in_.residue(i).name3()=="GLY") ? 4 : 5;
			for(int j = 1; j <= natom; ++j) {
				Vec const & v( cmp2in_.residue(i).xyz(j) );
				cmp2diapos_ = max(cmp2diapos_, v.z());
				cmp2dianeg_ = max(cmp2dianeg_,-v.z());
			}
		}

		if(cmp1type[1]=='2') { make_dimer   (cmp1in_); cmp1nsub_ = 2; }
		if(cmp1type[1]=='3') { make_trimer  (cmp1in_); cmp1nsub_ = 3; }
		if(cmp1type[1]=='4') { make_tetramer(cmp1in_); cmp1nsub_ = 4; }
		if(cmp1type[1]=='5') { make_pentamer(cmp1in_); cmp1nsub_ = 5; }
		if(cmp2type[1]=='2') { make_dimer   (cmp2in_); cmp2nsub_ = 2; }
		if(cmp2type[1]=='3') { make_trimer  (cmp2in_); cmp2nsub_ = 3; }
		if(cmp2type[1]=='4') { make_tetramer(cmp2in_); cmp2nsub_ = 4; }
		if(cmp2type[1]=='5') { make_pentamer(cmp2in_); cmp2nsub_ = 5; }
		if(option[tcdock::reverse]()) rot_pose(cmp1in_,Vecf(0,1,0),180.0);

		bool nt1good=1,nt2good=1,ct1good=1,ct2good=1;
		termini_exposed(cmp1in_,nt1good,ct1good);
		termini_exposed(cmp2in_,nt2good,ct2good);
		if(option[tcdock::require_exposed_termini]()) {
			if( !(ct1good&&nt2good) && !(ct2good&&nt1good) ) {
				std::cout << "no exposed termini on comp1" << std::endl;
				return;
			}
		}
		if(!option[tcdock::usec1].user()) {
			if( ct1good && nt2good ) option[tcdock::usec1](true);
			else                     option[tcdock::usec1](false);
		}
		if(!option[tcdock::usen1].user()) {
			if( ct2good && nt1good ) option[tcdock::usen1](true);
			else                     option[tcdock::usen1](false);
		}

		cmp1name_ = utility::file_basename(cmp1pdb);
		cmp2name_ = utility::file_basename(cmp2pdb);
		if(cmp1name_.substr(cmp1name_.size()-3)==".gz" ) cmp1name_ = cmp1name_.substr(0,cmp1name_.size()-3);
		if(cmp2name_.substr(cmp2name_.size()-3)==".gz" ) cmp2name_ = cmp2name_.substr(0,cmp2name_.size()-3);
		if(cmp1name_.substr(cmp1name_.size()-4)==".pdb") cmp1name_ = cmp1name_.substr(0,cmp1name_.size()-4);
		if(cmp2name_.substr(cmp2name_.size()-4)==".pdb") cmp2name_ = cmp2name_.substr(0,cmp2name_.size()-4);
		symtype_ = cmp2type.substr(0,4);

		std::map<string,int> comp_nangle;
		comp_nangle["I2"] = 180; comp_nangle["O2"] = 180; comp_nangle["T2"] = 180;
		comp_nangle["I3"] = 120; comp_nangle["O3"] = 120; comp_nangle["T3"] = 120;
		comp_nangle["I5"] =  72;
		comp_nangle["O4"] =  90;
		axismap_["I2"] = Vecf(-0.166666666666666,-0.28867500000000000, 0.87267800000000 ).normalized();
		axismap_["I3"] = Vecf( 0.000000000000000, 0.00000000000000000, 1.00000000000000 ).normalized();
		axismap_["I5"] = Vecf(-0.607226000000000, 0.00000000000000000, 0.79452900000000 ).normalized();
		axismap_["O2"] = Vecf( 1.000000000000000, 1.00000000000000000, 0.00000000000000 ).normalized();
		axismap_["O3"] = Vecf( 1.000000000000000, 1.00000000000000000, 1.00000000000000 ).normalized();
		axismap_["O4"] = Vecf( 1.000000000000000, 0.00000000000000000, 0.00000000000000 ).normalized();
		axismap_["T2"] = Vecf( 0.816496579408716, 0.00000000000000000, 0.57735027133783 ).normalized();
		axismap_["T3"] = Vecf( 0.000000000000000, 0.00000000000000000, 1.00000000000000 ).normalized();

		cmp1axs_ = axismap_[cmp1type_];
		cmp2axs_ = axismap_[cmp2type_];
		cmp1nangle_ = comp_nangle[cmp1type_];
		cmp2nangle_ = comp_nangle[cmp2type_];
		cout << cmp1type_ << " " << cmp2type_ << " nang1 " << cmp1nangle_ << " nang2 " << cmp2nangle_ << endl;

		alpha_ = angle_degrees(cmp1axs_,Vecf(0,0,0),cmp2axs_);
		sin_alpha_ = sin(numeric::conversions::radians(alpha_));
		tan_alpha_ = tan(numeric::conversions::radians(alpha_));

		// rot_pose(cmp2in_,Vec(0,1,0),-alpha_,Vecf(0,0,0));
		alignaxis(cmp1in_,cmp1axs_,Vec(0,0,1),Vec(0,0,0));
		alignaxis(cmp2in_,cmp2axs_,Vec(0,0,1),Vec(0,0,0));
		// cmp1in_.dump_pdb("out/test1.pdb");
		// cmp2in_.dump_pdb("out/test2.pdb");

		for(Size i = 1; i <= cmp1in_.n_residue(); ++i) {
			if(cmp1in_.residue(i).has("CB")) {
				cmp1cbs_.push_back(Vecf(cmp1in_.residue(i).xyz("CB")));
				cmp1wts_.push_back(min(1.0,(double)neighbor_count(cmp1in_,i)/20.0));
			}
			int const natom = (cmp1in_.residue(i).name3()=="GLY") ? 4 : 5;
			for(int j = 1; j <= natom; ++j) {
				Vec const & v( cmp1in_.residue(i).xyz(j) );
				cmp1pts_.push_back(v);
			}
		}
		for(Size i = 1; i <= cmp2in_.n_residue(); ++i) {
			if(cmp2in_.residue(i).has("CB")) {
				cmp2cbs_.push_back(Vecf(cmp2in_.residue(i).xyz("CB")));
				cmp2wts_.push_back(min(1.0,(double)neighbor_count(cmp2in_,i)/20.0));
			}
			int const natom = (cmp2in_.residue(i).name3()=="GLY") ? 4 : 5;
			for(int j = 1; j <= natom; ++j) {
				Vec const & v( cmp2in_.residue(i).xyz(j) );
				cmp2pts_.push_back(v);
			}
		}

		sics_.resize(num_threads());
		for(int i = 0; i < num_threads(); ++i) sics_[i] = new SICFast;
		for(int i = 0; i < num_threads(); ++i) sics_[i]->init(cmp1in_,cmp1cbs_,cmp1wts_,cmp1axs_,cmp2in_,cmp2cbs_,cmp2wts_,cmp2axs_);

		cmp1mnpos_.resize(cmp1nangle_,0.0);
		cmp2mnpos_.resize(cmp2nangle_,0.0);
		cmp1mnneg_.resize(cmp1nangle_,0.0);
		cmp2mnneg_.resize(cmp2nangle_,0.0);
		cmp1dspos_.resize(cmp1nangle_,0.0);
		cmp2dspos_.resize(cmp2nangle_,0.0);
		cmp1dsneg_.resize(cmp1nangle_,0.0);
		cmp2dsneg_.resize(cmp2nangle_,0.0);
		cmp1cbpos_.dimension(cmp1nangle_,200,0.0);
		cmp2cbpos_.dimension(cmp2nangle_,200,0.0);
		cmp1cbneg_.dimension(cmp1nangle_,200,0.0);
		cmp2cbneg_.dimension(cmp2nangle_,200,0.0);
		gradii.dimension(cmp2nangle_,cmp1nangle_,360,-9e9);
		gscore.dimension(cmp2nangle_,cmp1nangle_,360,-9e9);

	}
	virtual ~TCDock() {
		for(int i = 0; i < num_threads(); ++i) delete sics_[i];
	}
	int num_threads() {
		#ifdef USE_OPENMP
			return omp_get_max_threads();
		#else
			return 1;
		#endif
	}
	int thread_num() {
		#ifdef USE_OPENMP
			return omp_get_thread_num();
		#else
			return 0;
		#endif
	}
	Vecf swap_axis(vector1<Vecf> & pts,string type) {
		Vecf other_axis;
		if( type[1] == '2' ) {
			Mat r = rotation_matrix_degrees( axismap_[type[0]+string("3")],120.0);
			other_axis = r*axismap_[type];
			for(vector1<Vecf>::iterator i = pts.begin(); i != pts.end(); ++i) *i = r*(*i);
		} else {
			Mat r = rotation_matrix_degrees( axismap_[type[0]+string("2")],180.0);
			other_axis = r*axismap_[type];
			for(vector1<Vecf>::iterator i = pts.begin(); i != pts.end(); ++i) *i = r*(*i);
		}
		return other_axis.normalized();
	}
	void precompute_intra() {
		double const CONTACT_D	= basic::options::option[basic::options::OptionKeys::tcdock::contact_dis]();
		double const CLASH_D		= basic::options::option[basic::options::OptionKeys::tcdock::	clash_dis]();
		double const CONTACT_D2 = sqr(CONTACT_D);
		double const CLASH_D2	= sqr(CLASH_D);
		// compute high/low min dis for pent and cmp1 here, input to sicfast and don't allow any below
		cout << "precomputing one-component interactions every 1°" << endl;
		for(int i12 = 0; i12 < 2; ++i12) {
			Vecf const axis = (i12?cmp1axs_:cmp2axs_);
			vector1<double>             & cmpwts  ( i12?cmp1wts_  :cmp2wts_   );
			vector1<double>             & cmpmnpos( i12?cmp1mnpos_:cmp2mnpos_ );
			vector1<double>             & cmpmnneg( i12?cmp1mnneg_:cmp2mnneg_ );
			vector1<double>             & cmpdspos( i12?cmp1dspos_:cmp2dspos_ );
			vector1<double>             & cmpdsneg( i12?cmp1dsneg_:cmp2dsneg_ );
			ObjexxFCL::FArray2D<double> & cmpcbpos( i12?cmp1cbpos_:cmp2cbpos_ );
			ObjexxFCL::FArray2D<double> & cmpcbneg( i12?cmp1cbneg_:cmp2cbneg_ );
			for(int ipn = 0; ipn < 2; ++ipn) {
				#ifdef USE_OPENMP
				#pragma omp parallel for schedule(dynamic,1)
				#endif
				for(int icmp = 0; icmp < (i12?cmp1nangle_:cmp2nangle_); icmp+=1) {
					vector1<Vecf> ptsA,cbA;
					if(i12) get_cmp1(icmp,ptsA,cbA); else get_cmp2(icmp,ptsA,cbA);
					vector1<Vecf> ptsB(ptsA),cbB(cbA);
					assert( cbA.size() == cmpwts.size() && cbB.size() == cmpwts.size() );
					/*              */ swap_axis(ptsB,i12?cmp1type_:cmp2type_);
					Vecf const axis2 = swap_axis( cbB,i12?cmp1type_:cmp2type_);
					Vecf const sicaxis = ipn ? (axis2-axis).normalized() : (axis-axis2).normalized();
					double score = 0;
					double const d = sics_[thread_num()]->slide_into_contact(ptsA,ptsB,cbA,cbB,sicaxis,score,icmp,icmp,axis,axis2,i12,i12);
					if( d > 0 ) utility_exit_with_message("d shouldn't be > 0 for cmppos! "+ObjexxFCL::string_of(icmp));
					(ipn?cmpmnpos:cmpmnneg)[icmp+1] = (ipn?-1.0:1.0) * d/2.0/sin( angle_radians(axis2,Vecf(0,0,0),axis)/2.0 );
					(ipn?cmpdspos:cmpdsneg)[icmp+1] = (ipn?-1.0:1.0) * d;

					for(vector1<Vecf>::iterator iv = cbB.begin(); iv != cbB.end(); ++iv) *iv = (*iv) - d*sicaxis;
					vector1<double> wA(cmpwts),wB(cmpwts);
					prune_cb_pairs(cbA,cbB,wA,wB,CONTACT_D2); // mutates inputs!!!!!!!!!!
					double lastcbc = 9e9;
					for(int i = 1; i <= 200; ++i) {
						double cbc = 0.0;
						vector1<double>::const_iterator iwa=wA.begin(), iwb=wB.begin();
						for( vector1<Vecf>::const_iterator ia=cbA.begin(), ib=cbB.begin(); ia != cbA.end(); ++ia,++ib,++iwa,++iwb) {
							cbc += sigmoid( ia->distance_squared(*ib) , CLASH_D, CONTACT_D ) * (*iwa) * (*iwa);
						}
						assert(lastcbc >= cbc);
						lastcbc = cbc;
						(ipn?cmpcbpos:cmpcbneg)(icmp+1,i) = cbc;
						if(cbc==0.0) break;
						for(vector1<Vecf>::iterator iv = cbA.begin(); iv != cbA.end(); ++iv) *iv = (*iv) + (ipn?0.1:-0.1)*axis;
						for(vector1<Vecf>::iterator iv = cbB.begin(); iv != cbB.end(); ++iv) *iv = (*iv) + (ipn?0.1:-0.1)*axis2;
					}
				}
			}
		}
	}
	void dump_onecomp() {
		using	namespace	basic::options;
		using	namespace	basic::options::OptionKeys;
		using utility::file_basename;
		cout << "dumping 1D stats: " << option[out::file::o]()+"/"+cmp2name_+"_POS_1D.dat" << endl;
		{ utility::io::ozstream out(option[out::file::o]()+"/"+cmp2name_+"_POS_1D.dat");
			for(int i = 1; i <= cmp2nangle_; ++i) out << i << " " << cmp2dspos_[i] << " " << cmp2cbpos_(i,1) << " " << cmp2cbpos_(i,2) << " " << cmp2cbpos_(i,3) << " " << cmp2cbpos_(i,4) << endl;
			out.close(); }
		{ utility::io::ozstream out(option[out::file::o]()+"/"+cmp2name_+"_NEG_1D.dat");
			for(int i = 1; i <= cmp2nangle_; ++i) out << i << " " << cmp2dsneg_[i] << " " << cmp2cbneg_(i,1) << " " << cmp2cbneg_(i,2) << " " << cmp2cbneg_(i,3) << " " << cmp2cbneg_(i,4) << endl;
			out.close(); }
		{ utility::io::ozstream out(option[out::file::o]()+"/"+cmp1name_+"_POS_1D.dat");
			for(int i = 1; i <= cmp1nangle_; ++i) out << i << " " << cmp1dspos_[i] << " " << cmp1cbpos_(i,1) << " " << cmp1cbpos_(i,2) << " " << cmp1cbpos_(i,3) << " " << cmp1cbpos_(i,4) << endl;
			out.close(); }
		{ utility::io::ozstream out(option[out::file::o]()+"/"+cmp1name_+"_NEG_1D.dat");
			for(int i = 1; i <= cmp1nangle_; ++i) out << i << " " << cmp1dsneg_[i] << " " << cmp1cbneg_(i,1) << " " << cmp1cbneg_(i,2) << " " << cmp1cbneg_(i,3) << " " << cmp1cbneg_(i,4) << endl;
			out.close(); }
		utility_exit_with_message("1COMP!");
	}
	void get_cmp1(double acmp1, vector1<Vecf> & pts, vector1<Vecf> & cbs ) {
		Matf R = rotation_matrix_degrees( cmp1axs_, acmp1 );
		pts = cmp1pts_;
		cbs = cmp1cbs_;
		for(vector1<Vecf>::iterator i = pts.begin(); i != pts.end(); ++i) (*i) = R*(*i);
		for(vector1<Vecf>::iterator i = cbs.begin(); i != cbs.end(); ++i) (*i) = R*(*i);
	}
	void get_cmp2(double acmp2, vector1<Vecf> & pts, vector1<Vecf> & cbs ){
		Matf R = rotation_matrix_degrees( cmp2axs_, acmp2 );
		pts = cmp2pts_;
		cbs = cmp2cbs_;
		for(vector1<Vecf>::iterator i = pts.begin(); i != pts.end(); ++i) (*i) = R*(*i);
		for(vector1<Vecf>::iterator i = cbs.begin(); i != cbs.end(); ++i) (*i) = R*(*i);
	}
	void make_dimer(core::pose::Pose & pose) {
		// cerr << "make_dimer" << endl;
		core::pose::Pose t2(pose);
		rot_pose(t2,Vecf(0,0,1),180.0);
		for(Size i = 1; i <= t2.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()||pose.residue(i).is_ligand()) pose.append_residue_by_jump(t2.residue(i),1); else pose.append_residue_by_bond(t2.residue(i));
	}
	void make_trimer(core::pose::Pose & pose) {
		// cerr << "make_trimer" << endl;
		core::pose::Pose t2(pose),t3(pose);
		rot_pose(t2,Vecf(0,0,1),120.0);
		rot_pose(t3,Vecf(0,0,1),240.0);
		for(Size i = 1; i <= t2.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()||pose.residue(i).is_ligand()) pose.append_residue_by_jump(t2.residue(i),1); else pose.append_residue_by_bond(t2.residue(i));
		for(Size i = 1; i <= t3.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()||pose.residue(i).is_ligand()) pose.append_residue_by_jump(t3.residue(i),1); else pose.append_residue_by_bond(t3.residue(i));
	}
	void make_tetramer(core::pose::Pose & pose) {
		// cerr << "make_tetramer" << endl;
		core::pose::Pose t2(pose),t3(pose),t4(pose);
		rot_pose(t2,Vecf(0,0,1), 90.0);
		rot_pose(t3,Vecf(0,0,1),180.0);
		rot_pose(t4,Vecf(0,0,1),270.0);
		for(Size i = 1; i <= t2.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()||pose.residue(i).is_ligand()) pose.append_residue_by_jump(t2.residue(i),1); else pose.append_residue_by_bond(t2.residue(i));
		for(Size i = 1; i <= t3.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()||pose.residue(i).is_ligand()) pose.append_residue_by_jump(t3.residue(i),1); else pose.append_residue_by_bond(t3.residue(i));
		for(Size i = 1; i <= t4.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()||pose.residue(i).is_ligand()) pose.append_residue_by_jump(t4.residue(i),1); else pose.append_residue_by_bond(t4.residue(i));
	}
	void make_pentamer(core::pose::Pose & pose) {
		// cerr << "make_pentamer" << endl;
		core::pose::Pose t2(pose),t3(pose),t4(pose),t5(pose);
		rot_pose(t2,Vecf(0,0,1), 72.0);
		rot_pose(t3,Vecf(0,0,1),144.0);
		rot_pose(t4,Vecf(0,0,1),216.0);
		rot_pose(t5,Vecf(0,0,1),288.0);
		for(Size i = 1; i <= t2.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()||pose.residue(i).is_ligand()) pose.append_residue_by_jump(t2.residue(i),1); else pose.append_residue_by_bond(t2.residue(i));
		for(Size i = 1; i <= t3.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()||pose.residue(i).is_ligand()) pose.append_residue_by_jump(t3.residue(i),1); else pose.append_residue_by_bond(t3.residue(i));
		for(Size i = 1; i <= t4.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()||pose.residue(i).is_ligand()) pose.append_residue_by_jump(t4.residue(i),1); else pose.append_residue_by_bond(t4.residue(i));
		for(Size i = 1; i <= t5.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()||pose.residue(i).is_ligand()) pose.append_residue_by_jump(t5.residue(i),1); else pose.append_residue_by_bond(t5.residue(i));
	}
	void dump_pdb(int icmp2, int icmp1, int iori, string fname, int idx, bool sym=true) {
		using basic::options::option;
		using namespace basic::options::OptionKeys;
		using ObjexxFCL::string_of;
		double d,dcmp2,dcmp1,icbc,cmp1cbc,cmp2cbc;
		dock_get_geom(icmp2,icmp1,iori,d,dcmp2,dcmp1,icbc,cmp2cbc,cmp1cbc);
		Pose p1,p2,symm;
		#ifdef USE_OPENMP
		#pragma omp critical
		#endif
		{
			p1 = cmp1in_;
			p2 = cmp2in_;
		}
		rot_pose  (p2,cmp2axs_,icmp2);
		rot_pose  (p1,cmp1axs_,icmp1);
		trans_pose(p2,dcmp2*cmp2axs_);
		trans_pose(p1,dcmp1*cmp1axs_);
		{
			symm.append_residue_by_jump(p1.residue(1),1);
			for(Size i = 2; i <= p1.n_residue()/cmp1nsub_; ++i) {
				if(symm.residue(i-1).is_terminus()||symm.residue(i-1).is_ligand()) symm.append_residue_by_jump(p1.residue(i),1);
				else                                symm.append_residue_by_bond(p1.residue(i));
			}
			symm.append_residue_by_jump(p2.residue(1),1);
			for(Size i = 2; i <= p2.n_residue()/cmp2nsub_; ++i) {
				if(symm.residue(symm.n_residue()).is_terminus()||symm.residue(symm.n_residue()).is_ligand()) symm.append_residue_by_jump(p2.residue(i),1);
				else                                             symm.append_residue_by_bond(p2.residue(i));
			}
		}
		if(sym) {
			option[symmetry::symmetry_definition]("input/"+cmp1type_.substr(0,1)+".sym");
			core::pose::symmetry::make_symmetric_pose(symm);
		}
		core::io::pdb::dump_pdb(symm,option[out::file::o]()+"/"+fname);

		// find contacts

		vector1<int> dumpg = option[tcdock::dump_pdb_grid]();
		if( std::find(dumpg.begin(),dumpg.end(),idx)!=dumpg.end() ){
		//if(option[tcdock::dump_pdb_grid]()) {
			Pose asym;
			#ifdef USE_OPENMP
			#pragma omp critical
			#endif
			{
				asym.append_residue_by_jump(p1.residue(1),1);
				for(Size i = 2; i <= p1.n_residue(); ++i) {
				  if(asym.residue(i-1).is_terminus()||asym.residue(i-1).is_ligand()) asym.append_residue_by_jump(p1.residue(i),1);
					else                                asym.append_residue_by_bond(p1.residue(i));
				}
				asym.append_residue_by_jump(p2.residue(1),1);
				for(Size i = 2; i <= p2.n_residue(); ++i) {
				  if(asym.residue(asym.n_residue()).is_terminus()||asym.residue(asym.n_residue()).is_ligand()) asym.append_residue_by_jump(p2.residue(i),1);
					else                                             asym.append_residue_by_bond(p2.residue(i));
				}
			}
			double pr1=0.0,pr2=0.0;
			for(vector1<Vecf>::const_iterator i = cmp1pts_.begin(); i != cmp1pts_.end(); ++i) pr1 = max(pr1,projperp(cmp1axs_,*i).length());
			for(vector1<Vecf>::const_iterator i = cmp2pts_.begin(); i != cmp2pts_.end(); ++i) pr2 = max(pr2,projperp(cmp2axs_,*i).length());
			int const na = option[tcdock::grid_size_rot]();
			int const nr = option[tcdock::grid_size_disp]();
			double const da1 = numeric::conversions::degrees(asin(option[tcdock::grid_delta_rot]()*1.2/pr1));
			double const da2 = numeric::conversions::degrees(asin(option[tcdock::grid_delta_rot]()*1.2/pr1));
			double const dr1 = -option[tcdock::grid_delta_disp]()*dcmp1/d;
			double const dr2 = -option[tcdock::grid_delta_disp]()*dcmp2/d;
			rot_pose(asym,cmp1axs_,-(na+1)*da1,               1, p1.n_residue());
			rot_pose(asym,cmp2axs_, (na  )*da2,p1.n_residue()+1,asym.n_residue());
			for(int ia1 = 0; ia1 < 2*na+1; ++ia1) {
				rot_pose(asym,cmp1axs_,da1,1, p1.n_residue());
				rot_pose(asym,cmp2axs_,-(2*na+1)*da2,p1.n_residue()+1,asym.n_residue());
				for(int ia2 = 0; ia2 < 2*na+1; ++ia2) {
					rot_pose(asym,cmp2axs_,da2,p1.n_residue()+1,asym.n_residue());
					for(int ir = 0; ir < nr; ++ir) {
						#ifdef USE_OPENMP
						#pragma omp critical
						#endif
						core::io::pdb::dump_pdb(asym,option[out::file::o]()+"/"+fname+"_"+string_of(ia1)+"_"+string_of(ia2)+"_"+string_of(ir)+".pdb.gz");
						trans_pose(asym,cmp1axs_*dr1,1, p1.n_residue());
						trans_pose(asym,cmp2axs_*dr2,p1.n_residue()+1,asym.n_residue());
					}
					trans_pose(asym,cmp1axs_*-nr*dr1,1, p1.n_residue());
					trans_pose(asym,cmp2axs_*-nr*dr2,p1.n_residue()+1,asym.n_residue());
				}
			}
		}
	}
	double __dock_base__(int icmp2,int icmp1,int iori,double&dori,double&dcmp2,double&dcmp1,double&icbc,double&cmp2cbc,double&cmp1cbc,bool cache=true){
		if(!cache || gradii(icmp2+1,icmp1+1,iori+1)==-9e9 ) {
			using basic::options::option;
			using namespace basic::options::OptionKeys;
			cmp2cbc=0; cmp1cbc=0;
			vector1<Vecf> pb,cbb, pa,cba; get_cmp1(icmp1,pa,cba); get_cmp2(icmp2,pb,cbb);
			Vecf sicaxis = rotation_matrix_degrees( cmp1axs_.cross(cmp2axs_) ,(double)iori) * (cmp1axs_+cmp2axs_).normalized();
			// cerr << cmp1axs_.cross(cmp2axs_) << endl;
			// utility_exit_with_message("arstio");

			// if( min_termini_proj(icmp2,icmp1,sicaxis) > 3.0 ) {
			// 	gradii(icmp2+1,icmp1+1,iori+1) =  0;
			// 	gscore(icmp2+1,icmp1+1,iori+1) = -9e9;
			// 	//std::cout << "skip" << std::endl;
			// 	return -9e9;
			// }
			//std::cout << "calc" << std::endl;

			double const d = sics_[thread_num()]->slide_into_contact(pb,pa,cbb,cba,sicaxis,icbc,icmp2,icmp1,cmp2axs_,cmp1axs_,false,true);
			dori = d;
			if(d > 0) utility_exit_with_message("ZERO!!");
			double const theta=(double)iori;
			double const gamma=numeric::conversions::radians(theta-alpha_/2.0);
			double const sin_gamma=sin(gamma), cos_gamma=cos(gamma), x=d*sin_gamma, y=d*cos_gamma, w=x/sin_alpha_, z=x/tan_alpha_;
			dcmp2 = y+z;  dcmp1 = w;
						//
						// cerr << icmp1 << " " << icmp2 << " " << iori << " " << d << endl;
						// cerr << dcmp1 << " " << dcmp2 << endl;
						// utility_exit_with_message("FOO");
						//
			if( w > 0 ) {
				double const cmp2mn = cmp2mnpos_[icmp2+1];
				double const cmp1mn = cmp1mnpos_[icmp1+1];
				if( dcmp1 < cmp1mn || dcmp2 < cmp2mn ) {
					double const dmncmp2 = cmp2mn/(cos_gamma+sin_gamma/tan_alpha_);
					double const dmncmp1 = cmp1mn*sin_alpha_/sin_gamma;
					gradii(icmp2+1,icmp1+1,iori+1) = min(dmncmp2,dmncmp1);
					gscore(icmp2+1,icmp1+1,iori+1) = 0.0;
					return 0.0;
				}
				int dp = (int)(dcmp2-cmp2mn)*10+1;
				int dt = (int)(dcmp1-cmp1mn)*10+1;
				if( 0 < dp && dp <= 200 ) cmp1cbc = cmp2cbpos_(icmp2+1,dp);
				if( 0 < dt && dt <= 200 ) cmp2cbc = cmp1cbpos_(icmp1+1,dt);
			} else {
				double const cmp2mn = cmp2mnneg_[icmp2+1];
				double const cmp1mn = cmp1mnneg_[icmp1+1];
				if( dcmp1 > cmp1mn || dcmp2 > cmp2mn ) {
					double const dmncmp2 = cmp2mn/(cos_gamma+sin_gamma/tan_alpha_);
					double const dmncmp1 = cmp1mn*sin_alpha_/sin_gamma;
					gradii(icmp2+1,icmp1+1,iori+1) = min(dmncmp2,dmncmp1);
					gscore(icmp2+1,icmp1+1,iori+1) = 0.0;
					return 0.0;
				}
				int dp = (int)(-dcmp2+cmp2mn)*10+1;
				int dt = (int)(-dcmp1+cmp1mn)*10+1;
				if( 0 < dp && dp <= 200 ) cmp1cbc = cmp2cbneg_(icmp2+1,dp);
				if( 0 < dt && dt <= 200 ) cmp2cbc = cmp1cbneg_(icmp1+1,dt);
			}
			if(icbc!=-12345.0) {
				gscore(icmp2+1,icmp1+1,iori+1)  = icbc;
				gscore(icmp2+1,icmp1+1,iori+1) += option[tcdock::intra]()*option[tcdock::intra2]()*cmp2cbc;
				gscore(icmp2+1,icmp1+1,iori+1) += option[tcdock::intra]()*option[tcdock::intra1]()*cmp1cbc;
				gscore(icmp2+1,icmp1+1,iori+1) += termini_score(dcmp1,icmp1,dcmp2,icmp2);
			}
			gradii(icmp2+1,icmp1+1,iori+1) = d;
		}
		return gscore(icmp2+1,icmp1+1,iori+1);
	}
	void   dock_no_score(int icmp2,int icmp1,int iori){
		double dori,dcmp2,dcmp1,icbc,cmp2cbc,cmp1cbc;
		icbc = -12345.0;
		__dock_base__(icmp2,icmp1,iori,dori,dcmp2,dcmp1,icbc,cmp2cbc,cmp1cbc,true);
	}
	double dock_score(int icmp2,int icmp1,int iori,double&icbc,double&cmp2cbc,double&cmp1cbc){
		double dori,dcmp2,dcmp1;
		return __dock_base__(icmp2,icmp1,iori,dori,dcmp2,dcmp1,icbc,cmp2cbc,cmp1cbc,false);
	}
	double dock_score(int icmp2,int icmp1,int iori){
		double dori,dcmp2,dcmp1,icbc,cmp2cbc,cmp1cbc;
		return __dock_base__(icmp2,icmp1,iori,dori,dcmp2,dcmp1,icbc,cmp2cbc,cmp1cbc,false);
	}
	double dock_get_geom(int icmp2,int icmp1,int iori,double&dori,double&dcmp2,double&dcmp1,double&icbc,double&cmp2cbc,double&cmp1cbc){
		return __dock_base__(icmp2,icmp1,iori,dori,dcmp2,dcmp1,icbc,cmp2cbc,cmp1cbc,false);
	}
	double min_termini_dis(double d1, double a1, double d2, double a2){
		using basic::options::option;
		using namespace basic::options::OptionKeys;
		bool usen1 = option[tcdock::usen1]();
		bool usec1 = option[tcdock::usec1]();
		double td = 9e9;
		for(int t1 = 0; t1 <= option[tcdock::termini_trim](); ++t1) {
			Vecf n1_0 = cmp1in_.xyz(core::id::AtomID(1,1+t1));
			for(int t2 = 0; t2 <= option[tcdock::termini_trim](); ++t2) {
				Vecf n2_0 = cmp2in_.xyz(core::id::AtomID(1,1+t2));
				for(int t3 = 0; t3 <= option[tcdock::termini_trim](); ++t3) {
					Vecf c1_0 = cmp1in_.xyz(core::id::AtomID(3,cmp1in_.n_residue()-t3));
					for(int t4 = 0; t4 <= option[tcdock::termini_trim](); ++t4) {
						Vecf c2_0 = cmp2in_.xyz(core::id::AtomID(3,cmp2in_.n_residue()-t4));
						for(int i1 = 0; i1 < cmp1nsub_; ++i1){
							Vecf n1 = d1*cmp1axs_ + rotation_matrix_degrees(cmp1axs_,a1 + 360.0/(double)cmp1nsub_*(double)i1) * n1_0;
							Vecf c1 = d1*cmp1axs_ + rotation_matrix_degrees(cmp1axs_,a1 + 360.0/(double)cmp1nsub_*(double)i1) * c1_0;
							for(int i2 = 0; i2 < cmp2nsub_; ++i2){
								Vecf n2 = d2*cmp2axs_ + rotation_matrix_degrees(cmp2axs_,a2 + 360.0/(double)cmp2nsub_*(double)i2) * n2_0;
								Vecf c2 = d2*cmp2axs_ + rotation_matrix_degrees(cmp2axs_,a2 + 360.0/(double)cmp2nsub_*(double)i2) * c2_0;
								if(usen1) td = min( n1.distance(c2), td );
								if(usec1) td = min( c1.distance(n2), td );
							}
						}
					}
				}
			}
		}
		return td;
	}
	double min_termini_proj(double a1, double a2, Vec sicaxis){
		using basic::options::option;
		using namespace basic::options::OptionKeys;
//		return 0.0;
		bool usen1 = option[tcdock::usen1]();
		bool usec1 = option[tcdock::usec1]();
		double td = 9e9;
		int t1=1;
		Vecf n1_0 = cmp1in_.xyz(core::id::AtomID(1,1+t1));
		int t2=1;
		Vecf n2_0 = cmp2in_.xyz(core::id::AtomID(1,1+t2));
		int t3=0;
		Vecf c1_0 = cmp1in_.xyz(core::id::AtomID(3,cmp1in_.n_residue()-t3));
		int t4=0;
		Vecf c2_0 = cmp2in_.xyz(core::id::AtomID(3,cmp2in_.n_residue()-t4));
		for(int i1 = 0; i1 < cmp1nsub_; ++i1){
			Vecf n1 = rotation_matrix_degrees(cmp1axs_,a1 + 360.0/(double)cmp1nsub_*(double)i1) * n1_0;
			Vecf c1 = rotation_matrix_degrees(cmp1axs_,a1 + 360.0/(double)cmp1nsub_*(double)i1) * c1_0;
			for(int i2 = 0; i2 < cmp2nsub_; ++i2){
				Vecf n2 = rotation_matrix_degrees(cmp2axs_,a2 + 360.0/(double)cmp2nsub_*(double)i2) * n2_0;
				Vecf c2 = rotation_matrix_degrees(cmp2axs_,a2 + 360.0/(double)cmp2nsub_*(double)i2) * c2_0;
				if(usen1) td = min( projperp(sicaxis,n1-c2).length(), td );
				if(usec1) td = min( projperp(sicaxis,c1-n2).length(), td );
			}
		}
		//std::cerr << td << std::endl;
		return td;
	}
	double termini_score(double d1, double a1, double d2, double a2){
		using basic::options::option;
		using namespace basic::options::OptionKeys;
		double td = min_termini_dis(d1,a1,d2,a2);
		return option[tcdock::termini_weight]() * max(0.0,option[tcdock::termini_cutoff]()-td);
	}
	void justone(int icmp1, int icmp2, int iori) {
		using basic::options::option;
		using namespace basic::options::OptionKeys;
		int ilm=0;
		LMAX const h(0,0,icmp1,icmp2,iori);
		double d,dcmp2,dcmp1,icbc,cmp1cbc,cmp2cbc;
		int N = option[tcdock::peak_grid_size]();
		ObjexxFCL::FArray3D<double> grid(2*N+1,2*N+1,2*N+1,0.0);
		#ifdef USE_OPENMP
		#pragma omp parallel for schedule(dynamic,1)
		#endif
		for(int di = -N; di <= N; ++di) {
			for(int dj = -N; dj <= N; ++dj) {
				for(int dk = -N; dk <= N; ++dk) {
					if( Vecf(di,dj,dk).length() > (double)N+0.5 ) {
						grid(di+N+1,dj+N+1,dk+N+1) = -9e9;
					} else {
						int i = (h.icmp2+di+gscore.size1())%gscore.size1();
						int j = (h.icmp1+dj+gscore.size2())%gscore.size2();
						int k = (h.iori +dk+gscore.size3())%gscore.size3();
						dock_no_score(i,j,k);
						grid(di+N+1,dj+N+1,dk+N+1) = gradii(i+1,j+1,k+1);
					}
				}
			}
		}
		vector1<double> ffhist(25,0);
		// #ifdef USE_OPENMP
		// #pragma omp parallel for schedule(dynamic,1)
		// #endif
		for(int ifh = 0; ifh < ffhist.size(); ++ifh) {
			flood_fill3D(N+1,N+1,N+1,grid, grid(N+1,N+1,N+1)-0.000001 - 0.2);
			double count = 0;
			int Nedge = option[tcdock::peak_grid_smooth]();
			ObjexxFCL::FArray3D<double> grid2(grid);
			for(int i = 1+Nedge; i <= grid.size1()-Nedge; ++i) {
				for(int j = 1+Nedge; j <= grid.size2()-Nedge; ++j) {
					for(int k = 1+Nedge; k <= grid.size3()-Nedge; ++k) {
						if( grid(i,j,k)!=grid(N+1,N+1,N+1) ) continue;
						int ninside = 0;
						for(int di = -Nedge; di <= Nedge; ++di) {
							for(int dj = -Nedge; dj <= Nedge; ++dj) {
								for(int dk = -Nedge; dk <= Nedge; ++dk) {
									ninside += grid(i+di,j+dj,k+dk)==grid(N+1,N+1,N+1);
								}
							}
						}
						double w = max(0.0, 1.0 - Vecf(N+1-i,N+1-j,N+1-k).length() / (double)N );
						count += w*((double)ninside/(double)((2*Nedge+1)*(2*Nedge+1)*(2*Nedge+1)));
						// cerr << F(7,3,((double)ninside/(double)((2*Nedge+1)*(2*Nedge+1)*(2*Nedge+1)))) << " " << F(5,3,w) << " " << I(2,ninside) << endl;
						// grid2(i,j,k) += allgood;
					}
				}
			}
			ffhist[ifh+1] = pow(count,1.0/3.0);
			vector1<int> dumpg = option[tcdock::dump_peak_grids]();
			#ifdef USE_OPENMP
			#pragma omp critical
			#endif
			if( std::find(dumpg.begin(),dumpg.end(),ilm)!=dumpg.end() ) {
				utility::io::ozstream o(("out/grid_"+ObjexxFCL::string_of(ilm)+"_"+ObjexxFCL::string_of(ifh)+".dat.gz"));
				for(int i = 1; i <= grid.size1(); ++i) {
					for(int j = 1; j <= grid.size2(); ++j) {
						for(int k = 1; k <= grid.size3(); ++k) {
							o << grid2(i,j,k) << endl;
						}
					}
				}
				o.close();
				dump_pdb(h.icmp2,h.icmp1,h.iori,"test_"+ObjexxFCL::string_of(ilm)+".pdb",0,true);
			}
		}
		double score = dock_get_geom(h.icmp2,h.icmp1,h.iori,d,dcmp2,dcmp1,icbc,cmp2cbc,cmp1cbc);
		string fn = cmp1name_+"_"+cmp2name_+"_"+(option[tcdock::reverse]()?"R":"F")+"_"+ObjexxFCL::string_of(ilm);
		cout << "| " << fn << ((ilm<10)?"  ":" ")
             << F(8,3,score) << " "
		     << F(6,2,2*max(fabs(dcmp1)+(dcmp1>0?cmp1diapos_:cmp1dianeg_),fabs(dcmp2)+(dcmp2>0?cmp2diapos_:cmp2dianeg_))) << " "
		     << F(6,2, min_termini_dis(dcmp1,h.icmp1,dcmp2,h.icmp2) ) << " "
             << F(7,3,icbc) << " "
		     << F(6,2,cmp1cbc) << " "
		     << F(6,2,cmp2cbc) << " "
		     << I(4,cmp1in_.n_residue()) << " "
		     << I(3,h.icmp1) << " "
		     << F(8,3,dcmp1) << " "
		     << I(4,cmp2in_.n_residue()) << " "
		     << I(3,h.icmp2) << " "
		     << F(8,3,dcmp2) << " "
			 << I(3,h.iori);
		for(int ifh = 1; ifh <= ffhist.size(); ++ifh) {
			cout << " " << F(5,2,ffhist[ifh]);
		}
		cout << endl;
		if(option[tcdock::dump_pdb]() ) dump_pdb(h.icmp2,h.icmp1,h.iori,fn+".pdb.gz",0,true);

	}
	void run() {
		using basic::options::option;
		using namespace basic::options::OptionKeys;
		using namespace core::id;
		using numeric::conversions::radians;
		double const CONTACT_D  = basic::options::option[basic::options::OptionKeys::tcdock::contact_dis]();
		double const CLASH_D    = basic::options::option[basic::options::OptionKeys::tcdock::	clash_dis]();
		double const CONTACT_D2 = sqr(CONTACT_D);
		double const CLASH_D2   = sqr(CLASH_D);
		Pose const cmp1init(cmp1in_);
		Pose const cmp2init(cmp2in_);
		{                                                         // 3deg loop
			double max1=0;
			// dump_onecomp();
			precompute_intra();
			cout << "main loop 1 over icmp2, icmp1, iori every 3 degrees" << endl;
			double max_score = 0;
			for(int icmp1 = 0; icmp1 < cmp1nangle_; icmp1+=3) {
				if(icmp1%15==0 && icmp1!=0) cout<<" lowres dock "<<cmp1name_<<" "<<I(2,100*icmp1/cmp1nangle_)<<"% done, max_score: "<<F(10,6,max_score)<<endl;
				vector1<Vecf> pa,cba; get_cmp1(icmp1,pa,cba);
				#ifdef USE_OPENMP
				#pragma omp parallel for schedule(dynamic,1)
				#endif
				for(int icmp2 = 0; icmp2 < cmp2nangle_; icmp2+=3) {
					vector1<Vecf> pb,cbb; get_cmp2(icmp2,pb,cbb);
					int iori = -1, stg = 1;	bool newstage = true;
					while(stg < 5) {
						if(newstage) {
							iori = (int)(((stg>2)?270.0:90.0)+angle_degrees(cmp1axs_,Vecf(0,0,0),cmp2axs_)/2.0);
							iori = (iori / 3) * 3; // round to closest multiple of angle incr
							if(stg==2||stg==4) iori -= 3;
							newstage = false;
						} else {
							iori += (stg%2==0) ? -3 : 3;
						}
//					for(int iori = 0; iori < 360; ++iori) {
						double const score = dock_score(icmp2,icmp1,iori);
						if( 0.0==score ) { stg++; newstage=true; continue; }
						#ifdef USE_OPENMP
						#pragma omp critical
						#endif
						if(score > max_score) max_score = score;
					}
				}
			}
			if(max_score<0.00001) utility_exit_with_message("cmp1 or cmp2 too large, no contacts!");
			cout << "MAX3 " << max_score << endl;
		}
		utility::vector1<vector1<int> > cmp2lmx,cmp1lmx,orilmx; { // set up work for main loop 2
			double topX_3 = 0;
			vector1<double> cbtmp;
			for(Size i = 0; i < gscore.size(); ++i) if(gscore[i] > 0) cbtmp.push_back(gscore[i]);
			std::sort(cbtmp.begin(),cbtmp.end());
			topX_3 = cbtmp[max(1,(int)cbtmp.size()-option[tcdock::nsamp1]()+1)];
			cout << "scanning top "<<option[tcdock::nsamp1]()<<" with score3 >= " << topX_3 << endl;
			for(int icmp2 = 0; icmp2 < cmp2nangle_; icmp2+=3) {
				for(int icmp1 = 0; icmp1 < cmp1nangle_; icmp1+=3) {
					for(int iori = 0; iori < 360; iori+=3) {
						if( gscore(icmp2+1,icmp1+1,iori+1) >= topX_3) {
							vector1<int> cmp2,cmp1,ori;
							for(int i = -1; i <= 1; ++i) cmp2.push_back( (icmp2+i+ cmp2nangle_)% cmp2nangle_ );
							for(int j = -1; j <= 1; ++j) cmp1.push_back( (icmp1+j+cmp1nangle_)%cmp1nangle_ );
							for(int k = -1; k <= 1; ++k) ori.push_back( (iori+k+360)%360 );
							cmp2lmx.push_back(cmp2);
							cmp1lmx.push_back(cmp1);
							orilmx.push_back(ori);
						}
					}
				}
			}
		}
		{                                                         //main loop 2
			#ifdef USE_OPENMP
			#pragma omp parallel for schedule(dynamic,1)
			#endif
			for(Size ilmx = 1; ilmx <= cmp2lmx.size(); ++ilmx)  {       //  MAIN LOOP 2
				if( (ilmx-1)%(option[tcdock::nsamp1]()/10)==0 && ilmx!=1) cout<<" highres dock "<<cmp1name_<<" "<<(double(ilmx-1)/(option[tcdock::nsamp1]()/100))<<"% done"<<endl;
				for(vector1<int>::const_iterator picmp2 = cmp2lmx[ilmx].begin(); picmp2 != cmp2lmx[ilmx].end(); ++picmp2) {
					int icmp2 = *picmp2; vector1<Vecf> pb,cbb; get_cmp2(icmp2,pb,cbb);
					for(vector1<int>::const_iterator picmp1 = cmp1lmx[ilmx].begin(); picmp1 != cmp1lmx[ilmx].end(); ++picmp1) {
						int icmp1 = *picmp1; vector1<Vecf> pa,cba; get_cmp1(icmp1,pa,cba);
						for(vector1<int>::const_iterator piori = orilmx[ilmx].begin(); piori != orilmx[ilmx].end(); ++piori) {
							int iori = *piori;
							dock_no_score(icmp2,icmp1,iori);
						}
					}
				}
			}
		}
		vector1<LMAX> local_maxima;	{                             // get local radial disp maxima (minima, really)
			double highscore = -9e9;
			for(int i = 1; i <= gradii.size1(); ++i){
				for(int j = 1; j <= gradii.size2(); ++j){
					for(int k = 1; k <= gradii.size3(); ++k){
						double const val = gradii(i,j,k);
						if( val < -9e8 ) continue;
						double nbmax = -9e9;
						int nedge = 0;
						for(int di = -1; di <= 1; ++di){
							for(int dj = -1; dj <= 1; ++dj){
								for(int dk = -1; dk <= 1; ++dk){
									if(di==0 && dj==0 && dk==0) continue;
									int i2 = (i+di+gradii.size1()-1)%gradii.size1()+1;
									int j2 = (j+dj+gradii.size2()-1)%gradii.size2()+1;
									int k2 = (k+dk+gradii.size3()-1)%gradii.size3()+1;
									double const nbval = gradii(i2,j2,k2);
									nbmax = max(nbmax,nbval);
								}
							}
						}
						if( nbmax != -9e9 && val >= nbmax ) {
							double score = dock_score(i-1,j-1,k-1);
							local_maxima.push_back( LMAX(score,gradii(i,j,k),i-1,j-1,k-1) );
							highscore = max(score,highscore);
						}
					}
				}
			}
			std::sort(local_maxima.begin(),local_maxima.end(),compareLMAX);
			cout << "N maxima: " << local_maxima.size() << ", best score: " << highscore << endl;
			string nc1=cmp1type_.substr(1,1), nc2=cmp2type_.substr(1,1);
			cout << "                          tag     score   diam   tdis   inter    ";
			cout << "sc"+nc1+"    sc"+nc2+"  nr"+nc1+"  a"+nc1+"       r"+nc1+"  nr"+nc2+"  a"+nc2+"       r"+nc2+" ori";
			cout << "  v0.2  v0.4  v0.6  v0.8  v1.0  v1.2  v1.4  v1.6  v1.8  v2.0";
			cout << "  v2.2  v2.4  v2.6  v2.8  v3.0  v3.2  v3.4  v3.6  v3.8  v4.0  v4.2  v4.4  v4.6  v4.8  v5.0" << endl;
		}
		for(Size ilm = 1; ilm <= min(local_maxima.size(),(Size)option[tcdock::topx]()); ++ilm) { // dump top hit info
			LMAX const & h(local_maxima[ilm]);
			double d,dcmp2,dcmp1,icbc,cmp1cbc,cmp2cbc;
			int N = option[tcdock::peak_grid_size]();
			ObjexxFCL::FArray3D<double> grid(2*N+1,2*N+1,2*N+1,0.0);
			#ifdef USE_OPENMP
			#pragma omp parallel for schedule(dynamic,1)
			#endif
			for(int di = -N; di <= N; ++di) {
				for(int dj = -N; dj <= N; ++dj) {
					for(int dk = -N; dk <= N; ++dk) {
						if( Vecf(di,dj,dk).length() > (double)N+0.5 ) {
							grid(di+N+1,dj+N+1,dk+N+1) = -9e9;
						} else {
							int i = (h.icmp2+di+gscore.size1())%gscore.size1();
							int j = (h.icmp1+dj+gscore.size2())%gscore.size2();
							int k = (h.iori +dk+gscore.size3())%gscore.size3();
							dock_no_score(i,j,k);
							grid(di+N+1,dj+N+1,dk+N+1) = gradii(i+1,j+1,k+1);
						}
					}
				}
			}
			vector1<double> ffhist(25,0);
			// #ifdef USE_OPENMP
			// #pragma omp parallel for schedule(dynamic,1)
			// #endif
			for(int ifh = 0; ifh < ffhist.size(); ++ifh) {
				flood_fill3D(N+1,N+1,N+1,grid, grid(N+1,N+1,N+1)-0.000001 - 0.2);
				double count = 0;
				int Nedge = option[tcdock::peak_grid_smooth]();
				ObjexxFCL::FArray3D<double> grid2(grid);
				for(int i = 1+Nedge; i <= grid.size1()-Nedge; ++i) {
					for(int j = 1+Nedge; j <= grid.size2()-Nedge; ++j) {
						for(int k = 1+Nedge; k <= grid.size3()-Nedge; ++k) {
							if( grid(i,j,k)!=grid(N+1,N+1,N+1) ) continue;
							int ninside = 0;
							for(int di = -Nedge; di <= Nedge; ++di) {
								for(int dj = -Nedge; dj <= Nedge; ++dj) {
									for(int dk = -Nedge; dk <= Nedge; ++dk) {
										ninside += grid(i+di,j+dj,k+dk)==grid(N+1,N+1,N+1);
									}
								}
							}
							double w = max(0.0, 1.0 - Vecf(N+1-i,N+1-j,N+1-k).length() / (double)N );
							count += w*((double)ninside/(double)((2*Nedge+1)*(2*Nedge+1)*(2*Nedge+1)));
							// cerr << F(7,3,((double)ninside/(double)((2*Nedge+1)*(2*Nedge+1)*(2*Nedge+1)))) << " " << F(5,3,w) << " " << I(2,ninside) << endl;
							// grid2(i,j,k) += allgood;
						}
					}
				}
				ffhist[ifh+1] = pow(count,1.0/3.0);
				vector1<int> dumpg = option[tcdock::dump_peak_grids]();
				#ifdef USE_OPENMP
				#pragma omp critical
				#endif
				if( std::find(dumpg.begin(),dumpg.end(),ilm)!=dumpg.end() ) {
					utility::io::ozstream o(("out/grid_"+ObjexxFCL::string_of(ilm)+"_"+ObjexxFCL::string_of(ifh)+".dat.gz"));
					for(int i = 1; i <= grid.size1(); ++i) {
						for(int j = 1; j <= grid.size2(); ++j) {
							for(int k = 1; k <= grid.size3(); ++k) {
								o << grid2(i,j,k) << endl;
							}
						}
					}
					o.close();
				}
			}
			double score = dock_get_geom(h.icmp2,h.icmp1,h.iori,d,dcmp2,dcmp1,icbc,cmp2cbc,cmp1cbc);
			string fn = cmp1name_+"_"+cmp2name_+"_"+(option[tcdock::reverse]()?"R":"F")+"_"+ObjexxFCL::string_of(ilm);
			cout << "| " << fn << ((ilm<10)?"  ":" ")
                 << F(8,3,score) << " "
			     << F(6,2,2*max(fabs(dcmp1)+(dcmp1>0?cmp1diapos_:cmp1dianeg_),fabs(dcmp2)+(dcmp2>0?cmp2diapos_:cmp2dianeg_))) << " "
			     << F(6,2, min_termini_dis(dcmp1,h.icmp1,dcmp2,h.icmp2) ) << " "
                 << F(7,3,icbc) << " "
			     << F(6,2,cmp1cbc) << " "
			     << F(6,2,cmp2cbc) << " "
			     << I(4,cmp1in_.n_residue()) << " "
			     << I(3,h.icmp1) << " "
			     << F(8,3,dcmp1) << " "
			     << I(4,cmp2in_.n_residue()) << " "
			     << I(3,h.icmp2) << " "
			     << F(8,3,dcmp2) << " "
				 << I(3,h.iori);
			for(int ifh = 1; ifh <= ffhist.size(); ++ifh) {
				cout << " " << F(5,2,ffhist[ifh]);
			}
			cout << endl;
			vector1<int> dumpg = option[tcdock::dump_pdb_grid]();
			if(option[tcdock::dump_pdb]() || std::find(dumpg.begin(),dumpg.end(),ilm)!=dumpg.end() ) dump_pdb(h.icmp2,h.icmp1,h.iori,fn+".pdb.gz",ilm,true);
		}
	}

};
int main (int argc, char *argv[]) {
	register_options();
	devel::init(argc,argv);
	using basic::options::option;
	using namespace basic::options::OptionKeys;
	vector1<string> compkind;
	vector1<vector1<string> > compfiles;
	if( option[tcdock::I5].user() ) { compkind.push_back("I5"); compfiles.push_back(option[tcdock::I5]()); }
	if( option[tcdock::I3].user() ) { compkind.push_back("I3"); compfiles.push_back(option[tcdock::I3]()); }
	if( option[tcdock::I2].user() ) { compkind.push_back("I2"); compfiles.push_back(option[tcdock::I2]()); }
	if( option[tcdock::O4].user() ) { compkind.push_back("O4"); compfiles.push_back(option[tcdock::O4]()); }
	if( option[tcdock::O3].user() ) { compkind.push_back("O3"); compfiles.push_back(option[tcdock::O3]()); }
	if( option[tcdock::O2].user() ) { compkind.push_back("O2"); compfiles.push_back(option[tcdock::O2]()); }
	if( option[tcdock::T3].user() ) { compkind.push_back("T3"); compfiles.push_back(option[tcdock::T3]()); }
	if( option[tcdock::T2].user() ) { compkind.push_back("T2"); compfiles.push_back(option[tcdock::T2]()); }
	if(compkind.size()!=2) utility_exit_with_message("must specify two components!");
	if(compkind[1][0]!=compkind[2][0]) utility_exit_with_message("components must be of same sym icos, tetra or octa");

	for(Size i = 1; i <= compfiles[1].size(); ++i) {
		for(Size j = 1; j <= compfiles[2].size(); ++j) {
			TCDock tcd(compfiles[1][i],compfiles[2][i],compkind[1],compkind[2]);
			if(option[tcdock::justone].user()) {
				int icm1 = option[tcdock::justone]()[1];
				int icm2 = option[tcdock::justone]()[2];
				int iori = option[tcdock::justone]()[3];
				tcd.justone(icm2,icm1,iori);
			} else {
				tcd.run();
			}
		}
	}
	// cout << "DONE testing: nothing really" << endl;
}
































