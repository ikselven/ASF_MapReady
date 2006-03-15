#include "asf.h"
#include "geolocate.h"

#ifndef pi
#define pi M_PI
#endif

static double er_target;

static vector latLon2cart(GEOLOCATE_REC *g,double elev,
			  double deg_lat,double deg_lon)
{
/*Convert latitude and longitude to radians */
	double lat=deg_lat*D2R;
	double lon=deg_lon*D2R;
	/* e2==Earth eccentricity, squared */
	double e2=1.0-(g->rp*g->rp)/(g->re*g->re);
	double cosLat=cos(lat), sinLat=sin(lat);
	/* "prime vertical radius of curvature" at our latitude */
	double N=g->re/sqrt(1-e2*sinLat*sinLat);
	vector ret;
	/* Cartesian to spherical, in a coordinate system stretched along Z by (1-e2) */
	ret.x=(N+elev)*cosLat*cos(lon);
	ret.y=(N+elev)*cosLat*sin(lon);
	ret.z=(N*(1-e2)+elev)*sinLat;
	return ret;
}


/************ SAR Geolocation Algorithm ************/

static vector getLocCart(GEOLOCATE_REC *g,double range,double dop)
{
	double yaw=0,look=0;
	vector target;
	getLookYaw(g,range,dop,&look,&yaw);
	getDoppler(g,look,yaw,NULL,NULL,&target,NULL);
	return target;
}

static void getLatLon(GEOLOCATE_REC *g,double range,double dop,  /*  Inputs.*/
		      double *out_lat,double *out_lon,double *earthRadius) /*Outputs.*/
{
	double lat,lon;
	cart2sph(getLocCart(g,range,dop),earthRadius,&lat,&lon);
	
/*Convert longitude to (-pi, pi].*/
	if (lon < -pi)
		lon += 2*pi;
	
/*Convert latitude to geodetic, from geocentric.*/
	lat=atan(tan(lat)*(g->re/g->rp)*(g->re/g->rp));
	if (out_lon) *out_lon = lon*R2D;
	if (out_lat) *out_lat = lat*R2D;
}

static GEOLOCATE_REC * meta_make_geolocate(meta_parameters *meta,
				    double time,double elev)
{
        stateVector st=meta_get_stVec(meta,time);
        GEOLOCATE_REC *g=init_geolocate_meta(&st,meta);
        g->re+=elev;
        g->rp+=elev;
        return g;
}

static double getPixSize(meta_parameters *meta,int axis,int *loc,
			 float elev,float dop) {

	GEOLOCATE_REC *g1, *g2;
	int shift=1.0;
	vector t1,t2,diff;
	dop*=1.0/meta->geo->azPixTime; /* multiply by PRF */
	g1=meta_make_geolocate(meta,meta_get_time(meta,loc[1],loc[0]),elev);
	t1=getLocCart(g1,meta_get_slant(meta,loc[1],loc[0]),meta_get_dop(meta,loc[1],loc[0])+dop);
	loc[axis]+=shift;
	g2=meta_make_geolocate(meta,meta_get_time(meta,loc[1],loc[0]),elev);
	t2=getLocCart(g2,meta_get_slant(meta,loc[1],loc[0]),meta_get_dop(meta,loc[1],loc[0])+dop);
	loc[axis]-=shift;
	er_target=vecMagnitude(t1); /* magnitude of target location==earth radius */
	vecSub(t2,t1,&diff);
	return vecMagnitude(diff)/shift;
}

static double vecCosAng(vector a,vector b)
{
	vecNormalize(&a); vecNormalize(&b);
	return vecDot(a,b);
}

void xpyp_getPixSizes(meta_parameters *meta, float *range_pixsiz, float *az_pixsiz)
{
  int sar_x = (int) meta->general->sample_count / 2;
  int sar_y=  (int) meta->general->line_count / 2;

  int loc[2];

  meta_get_original_line_sample(meta,sar_y,sar_x, &loc[1],&loc[0]);

  *range_pixsiz = getPixSize(meta,0,loc,0,0);
  *az_pixsiz = getPixSize(meta,1,loc,0,0);
}

void xpyp_getVelocities(meta_parameters *meta, float *pp_velocity,
						float *corrected_velocity)
{
	double azSize, azTime, azVel;
	double t,h,r,c,dt,sc_vel,earth_rad,sc_rad,cos_earth_ang,swath_nr;
	stateVector scFix,scGEI,ts;
	vector target1,target2,targVel; double tv,v;
//		printf("Azimuth velocity estimation at topleft:\n");
/* Use meta routines to find target point at time t and t+dt */
	//meta_get_orig((void *)&sar_ddr,0,sar_ddr.ns/2,&loc[1],&loc[0]);
	meta_get_original_line_sample(meta,
					    0,meta->general->sample_count/2,
					    &loc[1],&loc[0]);
	t=meta_get_time(meta,loc[1],loc[0]);
	scFix=meta_get_stVec(meta,t);
	target1=getLocCart(  /* body-fixed position of target at time t */
		meta_make_geolocate(meta,t,0),
		meta_get_slant(meta,loc[1],loc[0]),
		meta_get_dop(meta,loc[1],loc[0])
	);
	dt=0.001;
	target2=getLocCart(  /* body-fixed position of target at time t+dt */
		meta_make_geolocate(meta,t+dt,0),
		meta_get_slant(meta,loc[1],loc[0]),
		meta_get_dop(meta,loc[1],loc[0])
	);
	vecSub(target2,target1,&targVel); /* velocity of target point */
	vecScale(&targVel,1.0/dt);
	tv=vecMagnitude(targVel);
//		printf("  ASF geolocate azimuth velocity: %.3f m/s\n",tv);
	
/* Use getPixSize to doublecheck target velocity */
	azSize=getPixSize(meta,1,loc,0.0,0.0);
	azTime=meta->geo->azPixTime;
	azVel=azSize/azTime;
//		printf("  xpix_ypix target azimuth velocity: %.3f m/s = %.3f m / %.6f s\n",azVel,azSize,azTime);

/* Find spacecraft vectors and use Precision Processor / Tom Bicknell approach */
	scGEI=scFix; fixed2gei(&scGEI,0.0); /* inertial velocities */
	//printf("  Orbital velocity: %.3f m/s fixed, %.3f m/s inertial\n",
	//	vecMagnitude(scFix.vel), vecMagnitude(scGEI.vel));
	//printf("  Orbit velocity cross position: %.4f deg\n",
	//	acos(vecCosAng(scGEI.pos,scGEI.vel))*180.0/M_PI);
#define gxMe 3.986005e14 /*Gravitational constant times mass of Earth (g times Me, o
r gxMe) */
	h=vecMagnitude(scGEI.pos);
	c=gxMe/(h*h); /* acceleration downward, from gMM/r^2 */
	r=vecMagnitude(scGEI.vel)*vecMagnitude(scGEI.vel)/c; /* r = v^2/a for uniform circ. motion */
	//printf("  Orbit radius of curvature: %.3f m  (vs ht %.3f m)\n",r,h);
	sc_vel=vecMagnitude(scGEI.vel); /* GEI velocity */
	sc_rad=vecMagnitude(scGEI.pos); /* distance from center of earth to spacecraft */
	earth_rad=vecMagnitude(target1); /* target earth radius */
	cos_earth_ang=vecCosAng(scFix.pos,target1); /* cosine of target earth angle */
	swath_nr=sc_vel*(earth_rad/sc_rad)*cos_earth_ang; /* target velocity, ignoring earth rotation */
	//printf("  PP swath velocity: %.3f m/s = %.3f m/s * (%.3f/%.3f) * %.6f\n",
	//	swath_nr,sc_vel,earth_rad,sc_rad,cos_earth_ang);

	ts.pos=target1; /* target1 fixed-earth position */
	ts.vel=scGEI.vel; /* direction: same as spacecraft */
	vecScale(&ts.vel,swath_nr/vecMagnitude(ts.vel)); /* scale: swath_nr long */

	gei2fixed(&ts,0.0); /* convert to fixed-earth velocity */
	v=vecMagnitude(ts.vel);
	//printf("  Fixed-earth swath magnitude: %.3f m/s (%.2f%% error)\n",v,100.0*(v-tv)/tv);
	
	c=vecCosAng(targVel,scGEI.vel);
	//printf("  Angle between real swath vel and GEI-derived velocity: %.3f deg, %.3f-%.3f m/s\n",
	//	acos(c)*180.0/M_PI,v/c,v*c);
	c=vecCosAng(targVel,ts.vel);
	//printf("  Angle between real swath vel and fixed-earth velocity: %.3f deg, %.3f-%.3f m/s\n",
	//	acos(c)*180.0/M_PI,v/c,v*c);
	
	*pp_velocity = swath_nr;
	*corrected_velocity = tv;
}
