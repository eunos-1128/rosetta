#include <basic/options/keys/in.OptionKeys.gen.hh> 
#include <basic/options/keys/out.OptionKeys.gen.hh>
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
#include <ObjexxFCL/FArray2D.hh>
#include <ObjexxFCL/FArray3D.hh>
#include <ObjexxFCL/FArray4D.hh>
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
OPT_1GRP_KEY( Real , ttc, clash_dis	)
OPT_1GRP_KEY( Real , ttc, contact_dis )
OPT_1GRP_KEY( Real , ttc, intra )
OPT_1GRP_KEY( Integer , ttc, nsamp1 )
OPT_1GRP_KEY( Integer , ttc, topx )
OPT_1GRP_KEY( Integer , ttc, peak_grid_size)
OPT_1GRP_KEY( Integer , ttc, peak_grid_smooth)
OPT_1GRP_KEY( Boolean , ttc, reverse )
OPT_1GRP_KEY( Boolean , ttc, dump_pdb )
OPT_1GRP_KEY( Boolean , ttc, dump_grids )
OPT_2GRP_KEY( FileVector, ttc, icos, c2 )
OPT_2GRP_KEY( FileVector, ttc, icos, c3 )
OPT_2GRP_KEY( FileVector, ttc, icos, c5 )
OPT_2GRP_KEY( FileVector, ttc, octa, c2 )
OPT_2GRP_KEY( FileVector, ttc, octa, c3 )
OPT_2GRP_KEY( FileVector, ttc, octa, c4 )
OPT_2GRP_KEY( FileVector, ttc, tetr ,c2 )
OPT_2GRP_KEY( FileVector, ttc, tetr ,c3 )
void register_options() {
		using namespace basic::options;
		using namespace basic::options::OptionKeys;
		OPT( in::file::s );
		NEW_OPT( ttc::clash_dis	 ,"max acceptable clash dis",	3.5 );
		NEW_OPT( ttc::contact_dis ,"max acceptable contact dis", 12 );
		NEW_OPT( ttc::intra       ,"include intra 3-3 5-5 contacts", 1.0 );
		NEW_OPT( ttc::nsamp1      ,"output top X hits", 2000 );
		NEW_OPT( ttc::topx        ,"output top X hits", 10 );
		NEW_OPT( ttc::peak_grid_size   ,"peak detect grid size (2*N+1)", 24 );		
		NEW_OPT( ttc::peak_grid_smooth ,"peak detect grid smooth (0+)", 1 );		
		NEW_OPT( ttc::reverse     ,"rev.", false );	
		NEW_OPT( ttc::dump_pdb,  "dump pdb", false );	
		NEW_OPT( ttc::dump_grids,"dump grids", false );
		NEW_OPT( ttc::icos::c2,"file(s) for icos 2fold trimer", "" );
		NEW_OPT( ttc::icos::c3,"file(s) for icos 3fold trimer", "" );
		NEW_OPT( ttc::icos::c5,"file(s) for icos 5fold trimer", "" );
		NEW_OPT( ttc::octa::c2,"file(s) for octa 2fold trimer", "" );
		NEW_OPT( ttc::octa::c3,"file(s) for octa 3fold trimer", "" );
		NEW_OPT( ttc::octa::c4,"file(s) for octa 4fold trimer", "" );
		NEW_OPT( ttc::tetr::c2,"file(s) for tetr 2fold trimer", "" );
		NEW_OPT( ttc::tetr::c3,"file(s) for tetr 3fold trimer", "" );
}
template<typename T> inline T sqr(T x) { return x*x; }
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
void trans_pose( Pose & pose, Vecf const & trans ) {
	for(Size ir = 1; ir <= pose.n_residue(); ++ir) {
		for(Size ia = 1; ia <= pose.residue_type(ir).natoms(); ++ia) {
			core::id::AtomID const aid(core::id::AtomID(ia,ir));
			pose.set_xyz( aid, pose.xyz(aid) + (Vec)trans );
		}
	}
}
void rot_pose( Pose & pose, Mat const & rot ) {
	for(Size ir = 1; ir <= pose.n_residue(); ++ir) {
		for(Size ia = 1; ia <= pose.residue_type(ir).natoms(); ++ia) {
			core::id::AtomID const aid(core::id::AtomID(ia,ir));
			pose.set_xyz( aid, rot * pose.xyz(aid) );
		}
	}
}
void rot_pose( Pose & pose, Mat const & rot, Vecf const & cen ) {
	trans_pose(pose,-cen);
	rot_pose(pose,rot);
	trans_pose(pose,cen);
}
void rot_pose( Pose & pose, Vecf const & axis, double const & ang ) {
	rot_pose(pose,rotation_matrix_degrees(axis,ang));
}
void rot_pose( Pose & pose, Vecf const & axis, double const & ang, Vecf const & cen ) {
	rot_pose(pose,rotation_matrix_degrees(axis,ang),cen);
}
void alignaxis(Pose & pose, Vecf newaxis, Vecf oldaxis, Vecf cen = Vecf(0,0,0) ) {
	newaxis.normalize();
	oldaxis.normalize();
	Vecf axis = newaxis.cross(oldaxis).normalized();
	double ang = (double)-acos(numeric::max((double)-1.0,numeric::min((double)1.0,newaxis.dot(oldaxis))))*(double)180.0/numeric::constants::f::pi;
	rot_pose(pose,axis,ang,cen);
}
int cbcount_vec(vector1<Vecf> & cba, vector1<Vecf> & cbb) {
	int cbcount = 0;
	for(vector1<Vecf>::const_iterator ia = cba.begin(); ia != cba.end(); ++ia)
		for(vector1<Vecf>::const_iterator ib = cbb.begin(); ib != cbb.end(); ++ib)
			if( ib->distance_squared(*ia) < 100.0 ) cbcount++;
	return cbcount;
}
void prune_cb_pairs_dis10(vector1<Vecf> & cba, vector1<Vecf> & cbb) {
	vector1<Vecf> a,b;
	for(vector1<Vecf>::const_iterator ia = cba.begin(); ia != cba.end(); ++ia) {
		for(vector1<Vecf>::const_iterator ib = cbb.begin(); ib != cbb.end(); ++ib) {
			if( ib->distance_squared(*ia) < 100.0 ) {
				a.push_back(*ia);
				b.push_back(*ib);
			}
		}
	}
	cba = a;
	cbb = b;
}
int pose_cbcount(Pose const & a, Pose const & b) {
	int count = 0;
	for(Size i = 1; i <= a.n_residue(); ++i) {
		for(Size j = 1; j <= b.n_residue(); ++j) {
			if(a.residue(i).xyz(2).distance_squared(b.residue(j).xyz(2)) < 100.0) {
				count++;
			}
		}
	}
	return count;
}
struct Vecf2 {
	Vecf a,b;
	Vecf2() {}
	Vecf2(Vecf _a, Vecf _b) : a(_a),b(_b) {}
};


struct SICFast {
	double xmx1,xmn1,ymx1,ymn1,xmx,xmn,ymx,ymn;
	double const CTD,CLD,CTD2,CLD2,BIN;
	int xlb,ylb,xub,yub;
	SICFast() : 
		CTD(basic::options::option[basic::options::OptionKeys::ttc::contact_dis]()),
		CLD(basic::options::option[basic::options::OptionKeys::ttc::clash_dis]()),
		CTD2(sqr(CTD)),CLD2(sqr(CLD)),BIN(CLD/2.0)
	{}
	void rotate_points(vector1<Vecf> & pa, vector1<Vecf> & pb, Vecf ori) {
		// get points, rotated ro ori is 0,0,1, might already be done
		Matf rot = Matf::identity();
		if		 ( ori.dot(Vecf(0,0,1)) < -0.99999 ) rot = rotation_matrix( Vecf(1,0,0).cross(ori), (double)-acos(Vecf(0,0,1).dot(ori)) );
		else if( ori.dot(Vecf(0,0,1)) <	0.99999 ) rot = rotation_matrix( Vecf(0,0,1).cross(ori), (double)-acos(Vecf(0,0,1).dot(ori)) );
		if( rot != Matf::identity() ) {
			for(vector1<Vecf>::iterator ia = pa.begin(); ia != pa.end(); ++ia) *ia = rot*(*ia);
			for(vector1<Vecf>::iterator ib = pb.begin(); ib != pb.end(); ++ib) *ib = rot*(*ib);
		}		
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
		xlb = (int)floor(xmn/BIN)-2; xub = (int)ceil(xmx/BIN)+2; // one extra on each side for correctness,
		ylb = (int)floor(ymn/BIN)-2; yub = (int)ceil(ymx/BIN)+2; // and one extra for outside atoms
	}
	void fill_plane_hash(vector1<Vecf> & pa, vector1<Vecf> & pb, ObjexxFCL::FArray2D<Vecf2> & ha, ObjexxFCL::FArray2D<Vecf2> & hb) {
		// insert points into hashes
		int const xsize = xub-xlb+1;
		int const ysize = yub-ylb+1;
		for(vector1<Vecf>::const_iterator ia = pa.begin(); ia != pa.end(); ++ia) {
			int const ix = (int)ceil(ia->x()/BIN)-xlb;
			int const iy = (int)ceil(ia->y()/BIN)-ylb;
			if( ix < 1 || ix > xsize || iy < 1 || iy > ysize ) continue;
			if( ha(ix,iy).a.z() < ia->z() ) {
				ha(ix,iy).b = ha(ix,iy).a;
				ha(ix,iy).a = *ia;
			} else
			if( ha(ix,iy).b.z() < ia->z() ) {
				ha(ix,iy).b = *ia;			
			}
		}
		for(vector1<Vecf>::const_iterator ib = pb.begin(); ib != pb.end(); ++ib) {
			int const ix = (int)ceil(ib->x()/BIN)-xlb;
			int const iy = (int)ceil(ib->y()/BIN)-ylb;
			if( ix < 1 || ix > xsize || iy < 1 || iy > ysize ) continue;
			if( hb(ix,iy).a.z() > ib->z() ) {
				hb(ix,iy).b = hb(ix,iy).a;
				hb(ix,iy).a = *ib;			
			} else 
			if( hb(ix,iy).b.z() > ib->z() ) {
				hb(ix,iy).b = *ib;
			}
		}
		
	}
	double get_mindis_with_plane_hashes(ObjexxFCL::FArray2D<Vecf2> & ha, ObjexxFCL::FArray2D<Vecf2> & hb) {
		int const xsize=xub-xlb+1, ysize=yub-ylb+1;
		int imna=0,jmna=0,imnb=0,jmnb=0;
		double m = 9e9;
		for(int i = 1; i <= xsize; ++i) { // skip 1 and N because they contain outside atoms (faster than clashcheck?)
			for(int j = 1; j <= ysize; ++j) {
				for(int k = -2; k <= 2; ++k) {
					if(i+k < 1 || i+k > xsize) continue;
					for(int l = -2; l <= 2; ++l) {
						if(j+l < 1 || j+l > ysize) continue;
						double const xa1=ha(i,j).a.x(),ya1=ha(i,j).a.y(),xb1=hb(i+k,j+l).a.x(),yb1=hb(i+k,j+l).a.y(),d21=(xa1-xb1)*(xa1-xb1)+(ya1-yb1)*(ya1-yb1); 
						double const xa2=ha(i,j).a.x(),ya2=ha(i,j).a.y(),xb2=hb(i+k,j+l).b.x(),yb2=hb(i+k,j+l).b.y(),d22=(xa2-xb2)*(xa2-xb2)+(ya2-yb2)*(ya2-yb2); 
						double const xa3=ha(i,j).b.x(),ya3=ha(i,j).b.y(),xb3=hb(i+k,j+l).a.x(),yb3=hb(i+k,j+l).a.y(),d23=(xa3-xb3)*(xa3-xb3)+(ya3-yb3)*(ya3-yb3);
						double const xa4=ha(i,j).b.x(),ya4=ha(i,j).b.y(),xb4=hb(i+k,j+l).b.x(),yb4=hb(i+k,j+l).b.y(),d24=(xa4-xb4)*(xa4-xb4)+(ya4-yb4)*(ya4-yb4);
						if(d21<CLD2){ double const dz1=hb(i+k,j+l).a.z()-ha(i,j).a.z()-sqrt(CLD2-d21); if(dz1<m){ m=dz1; imna=i; jmna=j; imnb=i+k; jmnb=j+l; } }
						if(d22<CLD2){ double const dz2=hb(i+k,j+l).b.z()-ha(i,j).a.z()-sqrt(CLD2-d22); if(dz2<m){ m=dz2; imna=i; jmna=j; imnb=i+k; jmnb=j+l; } }
						if(d23<CLD2){ double const dz3=hb(i+k,j+l).a.z()-ha(i,j).b.z()-sqrt(CLD2-d23); if(dz3<m){ m=dz3; imna=i; jmna=j; imnb=i+k; jmnb=j+l; } }
						if(d24<CLD2){ double const dz4=hb(i+k,j+l).b.z()-ha(i,j).b.z()-sqrt(CLD2-d24); if(dz4<m){ m=dz4; imna=i; jmna=j; imnb=i+k; jmnb=j+l; } }
					}
				}
			}
		}
		return m;
	}
	double get_score(vector1<Vecf> const & cba, vector1<Vecf> const & cbb, Vecf ori, double mindis) {
		double cbcount = 0.0;
		for(vector1<Vecf>::const_iterator ia = cba.begin(); ia != cba.end(); ++ia) {
			for(vector1<Vecf>::const_iterator ib = cbb.begin(); ib != cbb.end(); ++ib) {
				double d2 = ib->distance_squared( (*ia) + (mindis*ori) );
				if( d2 < CTD2 ) {
					cbcount += sigmoid(d2, CLD, CTD );
				}
			}
		}
		return cbcount;
	}
	double slide_into_contact( vector1<Vecf> pa, vector1<Vecf> pb, vector1<Vecf> const & cba, vector1<Vecf> const & cbb, Vecf ori, double & cbcount){ 
		rotate_points(pa,pb,ori);
		get_bounds(pa,pb);
		ObjexxFCL::FArray2D<Vecf2> ha(xub-xlb+1,yub-ylb+1,Vecf2(Vecf(0,0,-9e9),Vecf(0,0,-9e9)));
		ObjexxFCL::FArray2D<Vecf2> hb(xub-xlb+1,yub-ylb+1,Vecf2(Vecf(0,0, 9e9),Vecf(0,0, 9e9)));
		fill_plane_hash(pa,pb,ha,hb);
		// check hashes for min dis
		double mindis = get_mindis_with_plane_hashes(ha,hb);
		if(cbcount != -12345.0)	cbcount = get_score(cba,cbb,ori,mindis);
		return mindis;
	}
};
void cmp11_to_cmp12(vector1<Vecf> & pts, Vecf cmp1axs_, Vecf cmp1ax2_) { 
	Matf r = rotation_matrix_degrees( cmp1axs_.cross(cmp1ax2_), angle_degrees(cmp1axs_,Vecf(0,0,0),cmp1ax2_) );
	r = r * rotation_matrix_degrees(cmp1axs_,(double)60.0);
	for(vector1<Vecf>::iterator i = pts.begin(); i != pts.end(); ++i) *i = r*(*i);
}
void cmp21_to_cmp22(vector1<Vecf> & pts, Vecf cmp2axs_, Vecf cmp2ax2_) { 
	Matf r = rotation_matrix_degrees( cmp2axs_.cross(cmp2ax2_), angle_degrees(cmp2axs_,Vecf(0,0,0),cmp2ax2_) );
	r = r * rotation_matrix_degrees(cmp2axs_,(double)36.0);
	for(vector1<Vecf>::iterator i = pts.begin(); i != pts.end(); ++i) *i = r*(*i);
}
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
struct TCDock {
	vector1<double> cmp2mnpos_,cmp1mnpos_,cmp2mnneg_,cmp1mnneg_,cmp2dspos_,cmp1dspos_,cmp2dsneg_,cmp1dsneg_;
	ObjexxFCL::FArray2D<double> cmp2cbpos_,cmp1cbpos_,cmp2cbneg_,cmp1cbneg_;
	ObjexxFCL::FArray3D<double> gradii,gscore;	
	Vecf cmp1axs_,cmp1ax2_,cmp2axs_,cmp2ax2_;
	double alpha_,sin_alpha_,tan_alpha_;
	core::pose::Pose cmp1_in_,cmp2_in_;
	vector1<Vecf> cmp1_pts_,cmp1_cbs_,cmp2_pts_,cmp2_cbs_;
	string comp1name_,comp2name_,comp1type_,comp2type_;
	TCDock( string comp1pdb, string comp2pdb, string comp1type, string comp2type ) :
		cmp2mnpos_(72,0.0),cmp1mnpos_(120,0.0),cmp2mnneg_(72,0.0),cmp1mnneg_(120,0.0),
		cmp2dspos_(72,0.0),cmp1dspos_(120,0.0),cmp2dsneg_(72,0.0),cmp1dsneg_(120,0.0),
		cmp2cbpos_(72,97,0.0),cmp1cbpos_(120,145,0.0),cmp2cbneg_(72,97,0.0),cmp1cbneg_(120,145,0.0),
		gradii(72,120,360,-9e9),gscore(72,120,360,-9e9),
		comp1type_(comp1type),comp2type_(comp2type)
	{
		using basic::options::option;
		using namespace basic::options::OptionKeys;
		core::chemical::ResidueTypeSetCAP crs=core::chemical::ChemicalManager::get_instance()->residue_type_set(core::chemical::CENTROID);
		core::import_pose::pose_from_pdb(cmp1_in_,*crs,comp1pdb);
		core::import_pose::pose_from_pdb(cmp2_in_,*crs,comp2pdb);
		comp1name_ = utility::file_basename(comp1pdb);
		comp2name_ = utility::file_basename(comp2pdb);
		if(comp1name_.substr(comp1name_.size()-3)==".gz" ) comp1name_ = comp1name_.substr(0,comp1name_.size()-3);
		if(comp2name_.substr(comp2name_.size()-3)==".gz" ) comp2name_ = comp2name_.substr(0,comp2name_.size()-3);		
		if(comp1name_.substr(comp1name_.size()-4)==".pdb") comp1name_ = comp1name_.substr(0,comp1name_.size()-4);
		if(comp2name_.substr(comp2name_.size()-4)==".pdb") comp2name_ = comp2name_.substr(0,comp2name_.size()-4);		
		make_trimer(cmp1_in_);
		make_pentamer(cmp2_in_);		
		// set up geometry

		Vecf tri1_axis1 = Vecf( 0.000000, 0.000000,1.000000).normalized();
		Vecf tri1_axis2 = Vecf(-0.333333,-0.577350,0.745356).normalized();
		Vecf pnt2_axis1 = Vecf(-0.607226, 0.000000,0.794529).normalized();
		Vecf pnt2_axis2 = Vecf(-0.491123,-0.850651,0.187593).normalized();

		cmp1axs_ = tri1_axis1;
		cmp1ax2_ = tri1_axis2; // 33.4458470159, 10.42594
		cmp2axs_ = pnt2_axis1;
		cmp2ax2_ = pnt2_axis2; // 63.4311873349, 5.706642

		alpha_ = angle_degrees(cmp1axs_,Vecf(0,0,0),cmp2axs_);
		sin_alpha_ = sin(numeric::conversions::radians(alpha_));
		tan_alpha_ = tan(numeric::conversions::radians(alpha_));
		rot_pose(cmp2_in_,Vecf(0,1,0),-alpha_,Vecf(0,0,0));
		if(option[ttc::reverse]()) rot_pose(cmp1_in_,Vecf(0,1,0),180.0);

		for(Size i = 1; i <= cmp1_in_.n_residue(); ++i) {
			cmp1_cbs_.push_back(Vecf(cmp1_in_.residue(i).xyz(2)));
			int const natom = (cmp1_in_.residue(i).name3()=="GLY") ? 4 : 5;
			for(int j = 1; j <= natom; ++j) cmp1_pts_.push_back(Vecf(cmp1_in_.residue(i).xyz(j)));
		}
		for(Size i = 1; i <= cmp2_in_.n_residue(); ++i) {
			cmp2_cbs_.push_back(Vecf(cmp2_in_.residue(i).xyz(2)));
			int const natom = (cmp2_in_.residue(i).name3()=="GLY") ? 4 : 5;
			for(int j = 1; j <= natom; ++j) cmp2_pts_.push_back(Vecf(cmp2_in_.residue(i).xyz(j)));
		}		
		
	}
	void precompute_intra() {
		double const CONTACT_D	= basic::options::option[basic::options::OptionKeys::ttc::contact_dis]();
		double const CLASH_D		= basic::options::option[basic::options::OptionKeys::ttc::	clash_dis]();
		double const CONTACT_D2 = sqr(CONTACT_D);
		double const CLASH_D2	 = sqr(CLASH_D);
		// compute high/low min dis for pent and cmp1 here, input to sicfast and don't allow any below
		cout << "precomputing pent-pent and cmp1-cmp1 interactions every 1°" << endl;
		#ifdef USE_OPENMP
		#pragma omp parallel for schedule(dynamic,1)
		#endif
		for(int icmp2 = 0; icmp2 < 72; icmp2+=1) {
			vector1<Vecf> pcmp2,cbp; // precompute these
			get_cmp2(icmp2,pcmp2,cbp);
			vector1<Vecf> ppn2(pcmp2),cb2(cbp);
			cmp21_to_cmp22(ppn2,cmp2axs_,cmp2ax2_);
			cmp21_to_cmp22( cb2,cmp2axs_,cmp2ax2_);				
			double cbcount = 0;
			double const d = SICFast().slide_into_contact(pcmp2,ppn2,cbp,cb2,(cmp2ax2_-cmp2axs_).normalized(),cbcount);
			if( d > 0 ) utility_exit_with_message("d shouldn't be > 0 for cmp2pos! "+ObjexxFCL::string_of(icmp2));
			for(vector1<Vecf>::iterator i = cb2.begin(); i != cb2.end(); ++i) *i = (*i) - d*(cmp2ax2_-cmp2axs_).normalized();
			cmp2mnpos_[icmp2+1] = -d/2.0/sin( angle_radians(cmp2ax2_,Vecf(0,0,0),cmp2axs_)/2.0 );
			cmp2dspos_[icmp2+1] = -d;
			cmp2cbpos_(icmp2+1,1) = cbcount;
			// cout << "compute CB" << endl;
			prune_cb_pairs_dis10(cbp,cb2);
			for(int i = 2; i <= 97; ++i) {
				for(vector1<Vecf>::iterator iv = cbp.begin(); iv != cbp.end(); ++iv) *iv = (*iv) + 0.1f*cmp2axs_;
				for(vector1<Vecf>::iterator iv = cb2.begin(); iv != cb2.end(); ++iv) *iv = (*iv) + 0.1f*cmp2ax2_;
				double cbc = 0.0;
				for(Size j = 1; j <= cbp.size(); ++j) 
					if(cbp[j].distance_squared(cb2[j]) < 100.0) 
						cbc += sigmoid(cbp[j].distance_squared(cb2[j]), CLASH_D, CONTACT_D );
				cmp2cbpos_(icmp2+1,i) = cbc;
				if(cbc==0) break;
			}
		}
		#ifdef USE_OPENMP
		#pragma omp parallel for schedule(dynamic,1)
		#endif
		for(int icmp2 = 0; icmp2 < 72; icmp2+=1) {
			vector1<Vecf> pcmp2,cbp; // precompute these
			get_cmp2(icmp2,pcmp2,cbp);					
			vector1<Vecf> ppn2(pcmp2),cb2(cbp);
			cmp21_to_cmp22(ppn2,cmp2axs_,cmp2ax2_);
			cmp21_to_cmp22( cb2,cmp2axs_,cmp2ax2_);				
			double cbcount = 0;
			double const d = SICFast().slide_into_contact(pcmp2,ppn2,cbp,cb2,(cmp2axs_-cmp2ax2_).normalized(),cbcount);
			if( d > 0 ) utility_exit_with_message("d shouldn't be > 0 for cmp2neg! "+ObjexxFCL::string_of(icmp2));
			for(vector1<Vecf>::iterator i = cb2.begin(); i != cb2.end(); ++i) *i = (*i) - d*(cmp2axs_-cmp2ax2_).normalized();
			cmp2mnneg_[icmp2+1] = d/2.0/sin( angle_radians(cmp2ax2_,Vecf(0,0,0),cmp2axs_)/2.0 );
			cmp2dsneg_[icmp2+1] = d;
			cmp2cbneg_(icmp2+1,1) = cbcount;
			// cout << "compute CB" << endl;
			prune_cb_pairs_dis10(cbp,cb2);
			for(int i = 2; i <= 97; ++i) {
				for(vector1<Vecf>::iterator iv = cbp.begin(); iv != cbp.end(); ++iv) *iv = (*iv) - 0.1f*cmp2axs_;
				for(vector1<Vecf>::iterator iv = cb2.begin(); iv != cb2.end(); ++iv) *iv = (*iv) - 0.1f*cmp2ax2_;
				double cbc = 0.0; 
				for(Size j = 1; j <= cbp.size(); ++j) 
					if(cbp[j].distance_squared(cb2[j]) < 100.0) 
						cbc += sigmoid(cbp[j].distance_squared(cb2[j]), CLASH_D, CONTACT_D );
				cmp2cbneg_(icmp2+1,i) = cbc;
				if(cbc==0) break;
			}
		}
		#ifdef USE_OPENMP
		#pragma omp parallel for schedule(dynamic,1)
		#endif			
		for(int icmp1 = 0; icmp1 < 120; icmp1+=1) {
			vector1<Vecf> pcmp1,cbt; // precompute these
			get_cmp1(icmp1,pcmp1,cbt);
			vector1<Vecf> ptr2(pcmp1),cb2(cbt);
			cmp11_to_cmp12(ptr2,cmp1axs_,cmp1ax2_);
			cmp11_to_cmp12( cb2,cmp1axs_,cmp1ax2_);				
			double cbcount = 0;
			double const d = SICFast().slide_into_contact(pcmp1,ptr2,cbt,cb2,(cmp1ax2_-cmp1axs_).normalized(),cbcount);
			if( d > 0 ) utility_exit_with_message("d shouldn't be > 0 for cmp1pos! "+ObjexxFCL::string_of(icmp1));
			for(vector1<Vecf>::iterator i = cb2.begin(); i != cb2.end(); ++i) *i = (*i) - d*(cmp1ax2_-cmp1axs_).normalized();
			cmp1mnpos_[icmp1+1] = -d/2.0/sin( angle_radians(cmp1ax2_,Vecf(0,0,0),cmp1axs_)/2.0 );
			cmp1dspos_[icmp1+1] = -d;
			cmp1cbpos_(icmp1+1,1) = cbcount;
			// cout << "compute CB" << endl;
			prune_cb_pairs_dis10(cbt,cb2);
			for(int i = 2; i <= 145; ++i) {
				for(vector1<Vecf>::iterator iv = cbt.begin(); iv != cbt.end(); ++iv) *iv = (*iv) + 0.1f*cmp1axs_;
				for(vector1<Vecf>::iterator iv = cb2.begin(); iv != cb2.end(); ++iv) *iv = (*iv) + 0.1f*cmp1ax2_;
				double cbc = 0.0; 
				for(Size j = 1; j <= cbt.size(); ++j) 
					if(cbt[j].distance_squared(cb2[j]) < 100.0) 
						cbc += sigmoid(cbt[j].distance_squared(cb2[j]), CLASH_D, CONTACT_D );
				cmp1cbpos_(icmp1+1,i) = cbc;
				if(cbc==0) break;
			}
		}
		#ifdef USE_OPENMP
		#pragma omp parallel for schedule(dynamic,1)
		#endif			
		for(int icmp1 = 0; icmp1 < 120; icmp1+=1) {
			vector1<Vecf> pcmp1,cbt; // precompute these
			get_cmp1(icmp1,pcmp1,cbt);
			vector1<Vecf> ptr2(pcmp1),cb2(cbt);
			cmp11_to_cmp12(ptr2,cmp1axs_,cmp1ax2_);
			cmp11_to_cmp12( cb2,cmp1axs_,cmp1ax2_);				
			double cbcount = 0;
			double const d = SICFast().slide_into_contact(pcmp1,ptr2,cbt,cb2,(cmp1axs_-cmp1ax2_).normalized(),cbcount);
			if( d > 0 ) utility_exit_with_message("d shouldn't be > 0 for cmp1neg! "+ObjexxFCL::string_of(icmp1));
			for(vector1<Vecf>::iterator i = cb2.begin(); i != cb2.end(); ++i) *i = (*i) - d*(cmp1axs_-cmp1ax2_).normalized();
			cmp1mnneg_[icmp1+1] = d/2.0/sin( angle_radians(cmp1ax2_,Vecf(0,0,0),cmp1axs_)/2.0 );
			cmp1dsneg_[icmp1+1] = d;
			cmp1cbneg_(icmp1+1,1) = cbcount;
			// cout << "compute CB" << endl;
			prune_cb_pairs_dis10(cbt,cb2);
			for(int i = 2; i <= 145; ++i) {
				for(vector1<Vecf>::iterator iv = cbt.begin(); iv != cbt.end(); ++iv) *iv = (*iv) - 0.1f*cmp1axs_;
				for(vector1<Vecf>::iterator iv = cb2.begin(); iv != cb2.end(); ++iv) *iv = (*iv) - 0.1f*cmp1ax2_;
				double cbc = 0.0; 
				for(Size j = 1; j <= cbt.size(); ++j) 
					if(cbt[j].distance_squared(cb2[j]) < 100.0) 
						cbc += sigmoid(cbt[j].distance_squared(cb2[j]), CLASH_D, CONTACT_D );
				cmp1cbneg_(icmp1+1,i) = cbc;
				if(cbc==0) break;
			}
		}
	}
	void get_cmp1(double acmp1, vector1<Vecf> & pts, vector1<Vecf> & cbs ) {
		Matf R = rotation_matrix_degrees( cmp1axs_, acmp1 );
		pts = cmp1_pts_;
		cbs = cmp1_cbs_;		
		for(vector1<Vecf>::iterator i = pts.begin(); i != pts.end(); ++i) (*i) = R*(*i);
		for(vector1<Vecf>::iterator i = cbs.begin(); i != cbs.end(); ++i) (*i) = R*(*i);
	}
	void get_cmp2(double acmp2, vector1<Vecf> & pts, vector1<Vecf> & cbs ) {
		Matf R = rotation_matrix_degrees( cmp2axs_, acmp2 );
		pts = cmp2_pts_;
		cbs = cmp2_cbs_;		
		for(vector1<Vecf>::iterator i = pts.begin(); i != pts.end(); ++i) (*i) = R*(*i);
		for(vector1<Vecf>::iterator i = cbs.begin(); i != cbs.end(); ++i) (*i) = R*(*i);
	}
	void make_dimer(core::pose::Pose & pose) {
		core::pose::Pose t2(pose);
		rot_pose(t2,Vecf(0,0,1),180.0);
		for(Size i = 1; i <= t2.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()) pose.append_residue_by_jump(t2.residue(i),1); else pose.append_residue_by_bond(t2.residue(i));
	}
	void make_trimer(core::pose::Pose & pose) {
		core::pose::Pose t2(pose),t3(pose);
		rot_pose(t2,Vecf(0,0,1),120.0);
		rot_pose(t3,Vecf(0,0,1),240.0);
		for(Size i = 1; i <= t2.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()) pose.append_residue_by_jump(t2.residue(i),1); else pose.append_residue_by_bond(t2.residue(i));
		for(Size i = 1; i <= t3.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()) pose.append_residue_by_jump(t3.residue(i),1); else pose.append_residue_by_bond(t3.residue(i));
	}
	void make_tetramer(core::pose::Pose & pose) {
		core::pose::Pose t2(pose),t3(pose),t4(pose);
		rot_pose(t2,Vecf(0,0,1), 90.0);
		rot_pose(t3,Vecf(0,0,1),180.0);
		rot_pose(t4,Vecf(0,0,1),270.0);
		for(Size i = 1; i <= t2.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()) pose.append_residue_by_jump(t2.residue(i),1); else pose.append_residue_by_bond(t2.residue(i));
		for(Size i = 1; i <= t3.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()) pose.append_residue_by_jump(t3.residue(i),1); else pose.append_residue_by_bond(t3.residue(i));
		for(Size i = 1; i <= t4.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()) pose.append_residue_by_jump(t4.residue(i),1); else pose.append_residue_by_bond(t4.residue(i));
	}
	void make_pentamer(core::pose::Pose & pose) {
		core::pose::Pose t2(pose),t3(pose),t4(pose),t5(pose);
		rot_pose(t2,Vecf(0,0,1), 72.0);
		rot_pose(t3,Vecf(0,0,1),144.0);
		rot_pose(t4,Vecf(0,0,1),216.0);
		rot_pose(t5,Vecf(0,0,1),288.0);
		for(Size i = 1; i <= t2.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()) pose.append_residue_by_jump(t2.residue(i),1); else pose.append_residue_by_bond(t2.residue(i));
		for(Size i = 1; i <= t3.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()) pose.append_residue_by_jump(t3.residue(i),1); else pose.append_residue_by_bond(t3.residue(i));
		for(Size i = 1; i <= t4.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()) pose.append_residue_by_jump(t4.residue(i),1); else pose.append_residue_by_bond(t4.residue(i));
		for(Size i = 1; i <= t5.n_residue(); ++i) if(pose.residue(i).is_lower_terminus()) pose.append_residue_by_jump(t5.residue(i),1); else pose.append_residue_by_bond(t5.residue(i));
	}
	void dump_pdb(double acmp2, double acmp1, double aori, string fname, bool sym=true) {
		using basic::options::option;
		using namespace basic::options::OptionKeys;

		vector1<Vec> pb,pa,cbb,cba;
		get_cmp1(acmp1,pa,cba);
		get_cmp2(acmp2,pb,cbb);

		Vecf sicaxis = (rotation_matrix_degrees(-Vecf(0,1,0),(double)aori) * Vecf(0,0,1)).normalized();
		double tmpcbc;
		double const d = SICFast().slide_into_contact(pb,pa,cbb,cba,sicaxis,tmpcbc);
		if(d > 0) utility_exit_with_message("ZERO!!");
		double const theta=aori, gamma=numeric::conversions::radians(theta-alpha_);
		double const sin_gamma=sin(gamma), cos_gamma=cos(gamma), x=d*sin_gamma, y=d*cos_gamma, w=x/sin_alpha_, z=x/tan_alpha_;
		double const dcmp2 = y+z, dcmp1 = w;
		double cmp2mn,cmp1mn;
		Pose t,p,symm;
		#ifdef USE_OPENMP
		#pragma omp critical
		#endif
		{
			t = cmp1_in_;
			p = cmp2_in_;
		}
		rot_pose  (p,cmp2axs_,acmp2);
		rot_pose  (t,cmp1axs_,acmp1);
		trans_pose(p,dcmp2*cmp2axs_);
		trans_pose(t,dcmp1*cmp1axs_);
		{
			symm.append_residue_by_jump(t.residue(1),1);
			for(Size i = 2; i <= t.n_residue()/3; ++i) {
				if(symm.residue(i-1).is_terminus()) symm.append_residue_by_jump(t.residue(i),1);
				else                                symm.append_residue_by_bond(t.residue(i));
			}
			symm.append_residue_by_jump(p.residue(1),1);
			for(Size i = 2; i <= p.n_residue()/5; ++i) {
				if(symm.residue(symm.n_residue()).is_terminus()) symm.append_residue_by_jump(p.residue(i),1);
				else                                             symm.append_residue_by_bond(p.residue(i));
			}
		}
		if(sym) core::pose::symmetry::make_symmetric_pose(symm);				
		core::io::pdb::dump_pdb(symm,option[out::file::o]()+"/"+fname);
		
	}
	void dump_onecomp() {
		using	namespace	basic::options;
		using	namespace	basic::options::OptionKeys;		
		using utility::file_basename;
		cout << "dumping 1D stats: " << option[out::file::o]()+"/"+comp2name_+"_POS_1D.dat" << endl;
		{ utility::io::ozstream out(option[out::file::o]()+"/"+comp2name_+"_POS_1D.dat");
			for(int i = 1; i <=	72; ++i) out << i << " " << cmp2dspos_[i] << " " << cmp2cbpos_(i,1) << endl;
			out.close(); }
		{ utility::io::ozstream out(option[out::file::o]()+"/"+comp2name_+"_NEG_1D.dat");
			for(int i = 1; i <=	72; ++i) out << i << " " << cmp2dsneg_[i] << " " << cmp2cbneg_(i,1) << endl;
			out.close(); }
		{ utility::io::ozstream out(option[out::file::o]()+"/"+comp1name_+"_POS_1D.dat");
			for(int i = 1; i <= 120; ++i) out << i << " " << cmp1dspos_[i] << " " << cmp1cbpos_(i,1) << endl;
			out.close(); }
		{ utility::io::ozstream out(option[out::file::o]()+"/"+comp1name_+"_NEG_1D.dat");
			for(int i = 1; i <= 120; ++i) out << i << " " << cmp1dsneg_[i] << " " << cmp1cbneg_(i,1) << endl;
			out.close(); }
	}
	double __dock_base__(int icmp2,int icmp1,int iori,double&dori,double&dcmp2,double&dcmp1,double&icbc,double&cmp2cbc,double&cmp1cbc,bool cache=true){
		if(!cache || gradii(icmp2+1,icmp1+1,iori+1)==-9e9 ) {
			using basic::options::option;
			using namespace basic::options::OptionKeys;
			cmp2cbc=0; cmp1cbc=0;
			vector1<Vecf> pb,cbb, pa,cba; get_cmp1(icmp1,pa,cba); get_cmp2(icmp2,pb,cbb);		
			Vecf sicaxis = (rotation_matrix_degrees(-Vecf(0,1,0),(double)iori) * Vecf(0,0,1)).normalized();
			double const d = SICFast().slide_into_contact(pb,pa,cbb,cba,sicaxis,icbc);
			dori = d;
			if(d > 0) utility_exit_with_message("ZERO!!");
			double const theta=iori, gamma=numeric::conversions::radians(theta-alpha_);
			double const sin_gamma=sin(gamma), cos_gamma=cos(gamma), x=d*sin_gamma, y=d*cos_gamma, w=x/sin_alpha_, z=x/tan_alpha_;
			dcmp2 = y+z;  dcmp1 = w;
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
				if( 0 < dp && dp <=  97 ) cmp1cbc = cmp2cbpos_(icmp2+1,dp);
				if( 0 < dt && dt <= 145 ) cmp2cbc = cmp1cbpos_(icmp1+1,dt);
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
				if( 0 < dp && dp <=  97 ) cmp1cbc = cmp2cbneg_(icmp2+1,dp);
				if( 0 < dt && dt <= 145 ) cmp2cbc = cmp1cbneg_(icmp1+1,dt);
			}
			if(icbc!=-12345.0) gscore(icmp2+1,icmp1+1,iori+1) = icbc+option[ttc::intra]()*(cmp2cbc+cmp1cbc);
			gradii(icmp2+1,icmp1+1,iori+1) = d;
		}
		return gscore(icmp2+1,icmp1+1,iori+1);
	}

	void dock_no_score(int icmp2,int icmp1,int iori){
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
	void run() {
		using basic::options::option;
		using namespace basic::options::OptionKeys;
		using namespace core::id;
		using numeric::conversions::radians;
		double const CONTACT_D	= basic::options::option[basic::options::OptionKeys::ttc::contact_dis]();
		double const CLASH_D		= basic::options::option[basic::options::OptionKeys::ttc::	clash_dis]();
		double const CONTACT_D2 = sqr(CONTACT_D);
		double const CLASH_D2	= sqr(CLASH_D);
		Pose const cmp1init(cmp1_in_);
		Pose const cmp2init(cmp2_in_);
		double max1=0;
		precompute_intra();
		// cout << "greetings from thread: " << omp_get_thread_num() << " of " << omp_get_num_threads() << endl
		
		{ // 3deg loop 
			cout << "main loop 1 over icmp2, icmp1, iori every 3 degrees" << endl; 
			double max_score = 0;
			for(int icmp2 = 0; icmp2 < 72; icmp2+=3) {
				if(icmp2%9==0 && icmp2!=0) cout<<" lowres dock "<<comp1name_<<" "<<(100*icmp2)/72<<"\% done, max_score: "<<F(14,9,max_score)<<endl;
				vector1<Vecf> pb,cbb; get_cmp2(icmp2,pb,cbb);
				#ifdef USE_OPENMP
				#pragma omp parallel for schedule(dynamic,1)
				#endif
				for(int icmp1 = 0; icmp1 < 120; icmp1+=3) {
					vector1<Vecf> pa,cba; get_cmp1(icmp1,pa,cba);
					int iori = -1, stg = 1;	bool newstage = true;
					while(stg < 5) {
						if(newstage) {
							iori = (int)(((stg>2)?270.0:90.0)+double(3)/2.0+angle_degrees(cmp1axs_,Vecf(0,0,0),cmp2axs_));
							iori = (iori / 3) * 3; // round to closest multiple of angle incr
							if(stg==2||stg==4) iori -= 3;
							newstage = false;
						} else {
							iori += (stg%2==0) ? -3 : 3;
						}						
						double const score = dock_score(icmp2,icmp1,iori);
						if( 0.0==score ) { stg++; newstage=true;	continue; }
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
			topX_3 = cbtmp[max(1,(int)cbtmp.size()-option[ttc::nsamp1]()+1)];
			cout << "scanning top "<<option[ttc::nsamp1]()<<" with cbcount3 >= " << topX_3 << endl;
			for(int icmp2 = 0; icmp2 < 72; icmp2+=3) {
				for(int icmp1 = 0; icmp1 < 120; icmp1+=3) {
					for(int iori = 0; iori < 360; iori+=3) {
						if( gscore(icmp2+1,icmp1+1,iori+1) >= topX_3) {
							vector1<int> cmp2,cmp1,ori;
							for(int i = -1; i <= 1; ++i) cmp2.push_back( (icmp2+i+ 72)% 72 );
							for(int j = -1; j <= 1; ++j) cmp1.push_back( (icmp1+j+120)%120 );
							for(int k = -1; k <= 1; ++k) ori.push_back( (iori+k+360)%360 );
							cmp2lmx.push_back(cmp2);
							cmp1lmx.push_back(cmp1);
							orilmx.push_back(ori);
						}
					}
				}
			}
		}
		
		{ //main loop 2       
			#ifdef USE_OPENMP
			#pragma omp parallel for schedule(dynamic,1)
			#endif
			for(Size ilmx = 1; ilmx <= cmp2lmx.size(); ++ilmx)  {       //  MAIN LOOP 2
				if( (ilmx-1)%(option[ttc::nsamp1]()/10)==0 && ilmx!=1) 
					cout<<" highres dock "<<comp1name_<<" "<<(double(ilmx-1)/(option[ttc::nsamp1]()/100))<<"\% done"<<endl;
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
				
		vector1<LMAX> local_maxima; {                        // get local radial disp maxima (minima, really)     
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
		}

		cout << " NUM   score pscore tscore        cmp1 ncm1  a1       r1        cmp2 ncm2  a2       r2 ori  v0.2  v0.4  v0.6";
		cout << "  v0.8  v1.0  v1.2  v1.4  v1.6  v1.8  v2.0  v2.2  v2.4  v2.6  v2.8  v3.0  v3.2  v3.4  v3.6  v3.8  v4.0 ";
		cout << " v4.2  v4.4  v4.6  v4.8  v5.0" << endl;
		for(Size ilm = 1; ilm <= min(local_maxima.size(),(Size)option[ttc::topx]()); ++ilm) { // dump top hit info 
			LMAX const & h(local_maxima[ilm]);
			double d,dcmp2,dcmp1,icbc,cmp1cbc,cmp2cbc;
			int N = option[ttc::peak_grid_size]();
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
			int ifh;
			for(ifh = 0; ifh < ffhist.size(); ++ifh) {
				flood_fill3D(N+1,N+1,N+1,grid, grid(N+1,N+1,N+1)-0.000001 - 0.2);

				double count = 0;
				int Nedge = option[ttc::peak_grid_smooth]();
				ObjexxFCL::FArray3D<double> grid2(grid);
				for(int i = 1+Nedge; i <= grid.size1()-Nedge; ++i) {
					for(int j = 1+Nedge; j <= grid.size2()-Nedge; ++j) {
						for(int k = 1+Nedge; k <= grid.size3()-Nedge; ++k) {
							bool allgood = true;
							for(int di = -Nedge; di <= Nedge; ++di) {
								for(int dj = -Nedge; dj <= Nedge; ++dj) {
									for(int dk = -Nedge; dk <= Nedge; ++dk) {
										allgood &= grid(i+di,j+dj,k+dk)==grid(N+1,N+1,N+1);
									}
								}
							}
							double w = max(0.0, 1.0 - Vecf(N+1-i,N+1-j,N+1-k).length() / (double)N );
							if( allgood ) count += w;
							// grid2(i,j,k) += allgood;
						}
					}
				}
				ffhist[ifh+1] = pow(count,1.0/3.0);
				if( ilm==6 || ilm==2 ) {
					utility::io::ozstream o(("out/grid_"+ObjexxFCL::string_of(ilm)+"_"+ObjexxFCL::string_of(ifh)+".dat.gz"));
					for(int i = 1; i <= grid.size1(); ++i) {
						for(int j = 1; j <= grid.size2(); ++j) {
							for(int k = 1; k <= grid.size3(); ++k) {
								o << grid2(i,j,k) << endl;
							}
						}
					}
					o.close();
					dump_pdb(h.icmp2,h.icmp1,h.iori,"test_"+ObjexxFCL::string_of(ilm)+".pdb");					
				}
			}
			for(ifh; ifh < ffhist.size(); ++ifh) ffhist[ifh+1] = (2*N+1)*(2*N+1)*(2*N+1);
			
			double cbc = dock_get_geom(h.icmp2,h.icmp1,h.iori,d,dcmp2,dcmp1,icbc,cmp2cbc,cmp1cbc);
			cout << "| " << I(2,ilm) << " "
			          << F(7,3,icbc) << " " 
			          << F(6,2,cmp1cbc) << " " 
			          << F(6,2,cmp2cbc) << " " 
			          << comp1name_ << " " 
			          << I(4,cmp1_in_.n_residue()) << " " 
			          << I(3,h.icmp1) << " " 
			          << F(8,3,dcmp1) << " "
			          << comp2name_ << " " 
			          << I(4,cmp2_in_.n_residue()) << " " 
			          << I(3,h.icmp2) << " " 
			          << F(8,3,dcmp2) << " "
						 << I(3,h.iori);
			for(int ifh = 1; ifh <= ffhist.size(); ++ifh) {
				cout << " " << F(5,2,ffhist[ifh]);
			}
			cout << endl;
			if(option[ttc::dump_pdb]()) dump_pdb(h.icmp2,h.icmp1,h.iori,"test_"+ObjexxFCL::string_of(ilm)+".pdb");
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
	if( option[ttc::icos::c2].user() ) { compkind.push_back("icos/c2"); compfiles.push_back(option[ttc::icos::c2]()); }
	if( option[ttc::icos::c3].user() ) { compkind.push_back("icos/c3"); compfiles.push_back(option[ttc::icos::c3]()); }
	if( option[ttc::icos::c5].user() ) { compkind.push_back("icos/c5"); compfiles.push_back(option[ttc::icos::c5]()); }		
	if( option[ttc::octa::c2].user() ) { compkind.push_back("octa/c2"); compfiles.push_back(option[ttc::octa::c2]()); }
	if( option[ttc::octa::c3].user() ) { compkind.push_back("octa/c3"); compfiles.push_back(option[ttc::octa::c3]()); }
	if( option[ttc::octa::c4].user() ) { compkind.push_back("octa/c4"); compfiles.push_back(option[ttc::octa::c4]()); }		
	if( option[ttc::tetr::c2].user() ) { compkind.push_back("tetr/c2"); compfiles.push_back(option[ttc::tetr::c2]()); }
	if( option[ttc::tetr::c3].user() ) { compkind.push_back("tetr/c3"); compfiles.push_back(option[ttc::tetr::c3]()); }
	if(compkind.size()!=2) utility_exit_with_message("must specify two components!");
	if(compkind[1].substr(0,4)!=compkind[2].substr(0,4)) utility_exit_with_message("components must be of same sym icos, tetra or octa");

	for(Size i = 1; i <= compfiles[1].size(); ++i) {
		for(Size j = 1; j <= compfiles[2].size(); ++j) {
			TCDock tcd(compfiles[1][i],compfiles[2][i],compkind[1],compkind[2]);
			tcd.run();
		}
	}
	cout << "DONE testing refactor __dock_base__" << endl;
}
