/* inline World Magnetic Model good from 2015 through 2020.
 * from http://www.ngdc.noaa.gov
 */


#include <math.h>
#include <stdio.h>
#include <string.h>

#include "AutoSatTracker-ESP.h"
#include "wmm.h"

static int E0000(int *maxdeg, double alt,
double glat, double glon, double t, double *dec, double *mdp, double *ti,
double *gv)
{
     /* N.B. storage types are a tradeoff between stack and program memory.
      * N.B. the algorithm modifies c[][] and cd[][] IN PLACE so they must be inited each time upon entry.
      */
     static int maxord,n,m,j,D1,D2,D3,D4;
     static double c[13][13],cd[13][13],tc[13][13],dp[13][13];
     static double snorm[169],sp[13],cp[13],fn[13],fm[13],pp[13],k[13][13],dtr,a,b,re,
	  a2,b2,c2,a4,b4,c4,flnmj,
	  dt,rlon,rlat,srlon,srlat,crlon,crlat,srlat2,
	  crlat2,q,q1,q2,ct,st,r2,r,d,ca,sa,aor,ar,br,bt,bp,bpp,
	  par,temp1,temp2,parp,bx,by,bz,bh;
     static double *p = snorm;


// GEOMAG:

/* INITIALIZE CONSTANTS */
      maxord = *maxdeg;
      sp[0] = 0.0;
      cp[0] = *p = pp[0] = 1.0;
      dp[0][0] = 0.0;
      a = 6378.137;
      b = 6356.7523142;
      re = 6371.2;
      a2 = a*a;
      b2 = b*b;
      c2 = a2-b2;
      a4 = a2*a2;
      b4 = b2*b2;
      c4 = a4 - b4;

      resetWatchdog();
      for (int i = 0; i < 13; i++) {
	  for (int j = 0; j < 13; j++) {
	      c[i][j] = c0[i][j];
	      cd[i][j] = cd0[i][j];
	  }
      }


/* CONVERT SCHMIDT NORMALIZED GAUSS COEFFICIENTS TO UNNORMALIZED */
      *snorm = 1.0;
      for (n=1; n<=maxord; n++) 
      {
	*(snorm+n) = *(snorm+n-1)*(double)(2*n-1)/(double)n;
	j = 2;
	for (m=0,D1=1,D2=(n-m+D1)/D1; D2>0; D2--,m+=D1) 
	{
	  k[m][n] = (double)(((n-1)*(n-1))-(m*m))/(double)((2*n-1)*(2*n-3));
	  if (m > 0) 
	  {
	    flnmj = (double)((n-m+1)*j)/(double)(n+m);
	    *(snorm+n+m*13) = *(snorm+n+(m-1)*13)*sqrt(flnmj);
	    j = 1;
	    c[n][m-1] = *(snorm+n+m*13)*c[n][m-1];
	    cd[n][m-1] = *(snorm+n+m*13)*cd[n][m-1];
	  }
	  c[m][n] = *(snorm+n+m*13)*c[m][n];
	  cd[m][n] = *(snorm+n+m*13)*cd[m][n];
	}
	fn[n] = (double)(n+1);
	fm[n] = (double)n;
      }
      k[1][1] = 0.0;

/*************************************************************************/

// GEOMG1:

      dt = t - epoc;
      if (dt < 0.0 || dt > 5.0) {
	  *ti = epoc;			/* pass back base time for diag msg */
	  return (-1);
      }

      dtr = M_PI/180.0;
      rlon = glon*dtr;
      rlat = glat*dtr;
      srlon = sin(rlon);
      srlat = sin(rlat);
      crlon = cos(rlon);
      crlat = cos(rlat);
      srlat2 = srlat*srlat;
      crlat2 = crlat*crlat;
      sp[1] = srlon;
      cp[1] = crlon;

/* CONVERT FROM GEODETIC COORDS. TO SPHERICAL COORDS. */
      q = sqrt(a2-c2*srlat2);
      q1 = alt*q;
      q2 = ((q1+a2)/(q1+b2))*((q1+a2)/(q1+b2));
      ct = srlat/sqrt(q2*crlat2+srlat2);
      st = sqrt(1.0-(ct*ct));
      r2 = (alt*alt)+2.0*q1+(a4-c4*srlat2)/(q*q);
      r = sqrt(r2);
      d = sqrt(a2*crlat2+b2*srlat2);
      ca = (alt+d)/r;
      sa = c2*crlat*srlat/(r*d);
      for (m=2; m<=maxord; m++) 
      {
	sp[m] = sp[1]*cp[m-1]+cp[1]*sp[m-1];
	cp[m] = cp[1]*cp[m-1]-sp[1]*sp[m-1];
      }
      aor = re/r;
      ar = aor*aor;
      br = bt = bp = bpp = 0.0;
      for (n=1; n<=maxord; n++) 
      {
	ar = ar*aor;
	for (m=0,D3=1,D4=(n+m+D3)/D3; D4>0; D4--,m+=D3) 
	{
/*
   COMPUTE UNNORMALIZED ASSOCIATED LEGENDRE POLYNOMIALS
   AND DERIVATIVES VIA RECURSION RELATIONS
*/
	  if (n == m) 
	  {
	    *(p+n+m*13) = st**(p+n-1+(m-1)*13);
	    dp[m][n] = st*dp[m-1][n-1]+ct**(p+n-1+(m-1)*13);
	    goto S50;
	  }
	  if (n == 1 && m == 0) 
	  {
	    *(p+n+m*13) = ct**(p+n-1+m*13);
	    dp[m][n] = ct*dp[m][n-1]-st**(p+n-1+m*13);
	    goto S50;
	  }
	  if (n > 1 && n != m) 
	  {
	    if (m > n-2) *(p+n-2+m*13) = 0.0;
	    if (m > n-2) dp[m][n-2] = 0.0;
	    *(p+n+m*13) = ct**(p+n-1+m*13)-k[m][n]**(p+n-2+m*13);
	    dp[m][n] = ct*dp[m][n-1] - st**(p+n-1+m*13)-k[m][n]*dp[m][n-2];
	  }
S50:
/*
    TIME ADJUST THE GAUSS COEFFICIENTS
*/
	  tc[m][n] = c[m][n]+dt*cd[m][n];
	  if (m != 0) tc[n][m-1] = c[n][m-1]+dt*cd[n][m-1];
/*
    ACCUMULATE TERMS OF THE SPHERICAL HARMONIC EXPANSIONS
*/
	  par = ar**(p+n+m*13);
	  if (m == 0) 
	  {
	    temp1 = tc[m][n]*cp[m];
	    temp2 = tc[m][n]*sp[m];
	  }
	  else 
	  {
	    temp1 = tc[m][n]*cp[m]+tc[n][m-1]*sp[m];
	    temp2 = tc[m][n]*sp[m]-tc[n][m-1]*cp[m];
	  }
	  bt = bt-ar*temp1*dp[m][n];
	  bp += (fm[m]*temp2*par);
	  br += (fn[n]*temp1*par);
/*
    SPECIAL CASE:  NORTH/SOUTH GEOGRAPHIC POLES
*/
	  if (st == 0.0 && m == 1) 
	  {
	    if (n == 1) pp[n] = pp[n-1];
	    else pp[n] = ct*pp[n-1]-k[m][n]*pp[n-2];
	    parp = ar*pp[n];
	    bpp += (fm[m]*temp2*parp);
	  }
	}
      }
      if (st == 0.0) bp = bpp;
      else bp /= st;
/*
    ROTATE MAGNETIC VECTOR COMPONENTS FROM SPHERICAL TO
    GEODETIC COORDINATES
*/
      bx = -bt*ca-br*sa;
      by = bp;
      bz = bt*sa-br*ca;
/*
    COMPUTE DECLINATION (DEC), INCLINATION (DIP) AND
    TOTAL INTENSITY (TI)
*/
      bh = sqrt((bx*bx)+(by*by));
      *ti = sqrt((bh*bh)+(bz*bz));
      *dec = atan2(by,bx)/dtr;
      *mdp = atan2(bz,bh)/dtr;
/*
    COMPUTE MAGNETIC GRID VARIATION IF THE CURRENT
    GEODETIC POSITION IS IN THE ARCTIC OR ANTARCTIC
    (I.E. GLAT > +55 DEGREES OR GLAT < -55 DEGREES)

    OTHERWISE, SET MAGNETIC GRID VARIATION TO -999.0
*/
      *gv = -999.0;
      if (fabs(glat) >= 55.) 
      {
	if (glat > 0.0 && glon >= 0.0) *gv = *dec-glon;
	if (glat > 0.0 && glon < 0.0) *gv = *dec+fabs(glon);
	if (glat < 0.0 && glon >= 0.0) *gv = *dec+glon;
	if (glat < 0.0 && glon < 0.0) *gv = *dec-fabs(glon);
	if (*gv > +180.0) *gv -= 360.0;
	if (*gv < -180.0) *gv += 360.0;
      }
      return (0);
}


/* compute magnetic declination for given location, elevation and time.
 * sign is such that mag bearing = true az + mag deviation.
 * return 0 if ok.
 */
int
magdecl (
double l, double L,		/* geodesic lat, +N, long, +E, degrees */
double e,			/* elevation, m */
double y,			/* time, decimal year */
double *mdp			/* return magnetic deviation, degrees E of N */
)
{
	double alt = e/1000.;
	double dp, ti, gv;
	int maxdeg = 12;

	int ok = E0000(&maxdeg,alt,l,L,y,mdp,&dp,&ti,&gv);
	return (ok);
}

#ifdef TEST_MAIN

#include <stdlib.h>

/* stand-alone test program
 */

int main (int ac, char *av[])
{
	if (ac != 5) {
	    char *slash = strrchr (av[0], '/');
	    char *base = slash ? slash+1 : av[0];
	    fprintf (stderr, "Purpose: test stand-alone magnetic declination model.\n");
	    fprintf (stderr, "Usage: %s lat_degsN lng_degsE elevation_m decimal_year\n", base);
	    exit(1);
	}

	double l = atof (av[1]);
	double L = atof (av[2]);
	double e = atof (av[3]);
	double y = atof (av[4]);
	double mdp;

	int ok = magdecl (l, L, e, y, &mdp);

	printf ("%d %g\n", ok, mdp);

	return (!!ok);
}

#endif // TEST_MAIN
