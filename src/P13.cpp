//
// P13.cpp
//
// An implementation of Plan13 in C++ by Mark VandeWettering
//
// Plan13 is an algorithm for satellite orbit prediction first formulated
// by James Miller G3RUH.  I learned about it when I saw it was the basis 
// of the PIC based antenna rotator project designed by G6LVB.
//
// http://www.g6lvb.com/Articles/LVBTracker2/index.htm
//
// I ported the algorithm to Python, and it was my primary means of orbit
// prediction for a couple of years while I operated the "Easy Sats" with 
// a dual band hand held and an Arrow antenna.
//
// I've long wanted to redo the work in C++ so that I could port the code
// to smaller processors including the Atmel AVR chips.  Bruce Robertson,
// VE9QRP started the qrpTracker project to fufill many of the same goals,
// but I thought that the code could be made more compact and more modular,
// and could serve not just the embedded targets but could be of more
// use for more general applications.  And, I like the BSD License a bit
// better too.
//
// So, here it is!
//

#include "P13.h"

// here are a bunch of constants that will be used throughout the
// code, but which will probably not be helpful outside.

// Updated with 2014 values from
// http://www.amsat.org/amsat/articles/g3ruh/111.html

static const float RE = 6378.137f ;
static const float FL = 1.f/298.257224f ;
static const float GM = 3.986E5f ;
static const float J2 = 1.08263E-3f ;
static const float YM = 365.25f ;
static const float YT = 365.2421874f ;
static const float WW = 2.f*M_PI/YT ;
static const float WE = 2.f*M_PI+ WW ;
static const float W0 = WE/86400.f ;
static const float YG = 2014.f ;
static const float G0 = 99.5828f ;
static const float MAS0 = 356.4105f ;
static const float MASD = 0.98560028f ;
static const float EQC1 = 0.03340 ;
static const float EQC2 = 0.00035 ;
static const float INS = (23.4375f)*M_PI/180.0 ;
static const float CNS = cos(INS) ;
static const float SNS = sin(INS) ;


float
RADIANS(float deg)
{
    return deg * M_PI / 180. ;
}

float
DEGREES(float rad)
{
    return rad * 180. / M_PI ;
}


//----------------------------------------------------------------------
//     _              ___       _      _____ _           
//  __| |__ _ ______ |   \ __ _| |_ __|_   _(_)_ __  ___ 
// / _| / _` (_-<_-< | |) / _` |  _/ -_)| | | | '  \/ -_)
// \__|_\__,_/__/__/ |___/\__,_|\__\___||_| |_|_|_|_\___|
//                                                       
//----------------------------------------------------------------------

static long
fnday(int y, uint8_t m, uint8_t d)
{
    if (m < 3) {
	m += 12 ;
	y -- ;
    }
    return (long) (y * YM) + (long) ((m+1)*30.6f) + (long)d - 428L ;
}

static void
fndate(int &y, uint8_t &m, uint8_t &d, long dt)
{
    dt += 428L ;
    y = (int) ((dt-122.1)/365.25) ;
    dt -= (long) (y*365.25) ;
    m = (uint8_t) (dt / 30.61) ;
    dt -= (long) (m*30.6) ;
    m -- ;
    if (m > 12) {
	m -= 12 ;
	y++ ;
    }
    d = dt ;
}

DateTime::DateTime(int year, uint8_t month, uint8_t day, uint8_t h, uint8_t m, uint8_t s) 
{
    settime(year, month, day, h, m, s) ;
}


// copy constructor
DateTime::DateTime(const DateTime &dt)
{
    DN = dt.DN ;
    TN = dt.TN ;
}

// default constructor
DateTime::DateTime()
{
   DN = 0L ;
   TN = 0. ;
}

// overload assignment
DateTime &DateTime::operator= (const DateTime &source)
{
    DN = source.DN;
    TN = source.TN;
    return *this;
}

void
DateTime::gettime(int &year, uint8_t &month, uint8_t &day, uint8_t &h, uint8_t &m, uint8_t &s)
{
    fndate(year, month, day, DN) ;
    float t = TN ;
    t *= 24. ;
    h = (uint8_t) t ;
    t -= h ;
    t *= 60 ;
    m = (uint8_t) t ;
    t -= m ;
    t *= 60 ;
    s = (uint8_t) (t+0.5) ;
    if (s == 60)
	s = 59;

}

void
DateTime::settime(int year, uint8_t month, uint8_t day, uint8_t h, uint8_t m, uint8_t s) 
{
    DN = fnday(year, month, day) ;
    TN = ((float) h + m / 60. + s / 3600.) / 24. ;
}

void
DateTime::add(float days)
{
    TN += days ;
    DN += (long) TN ;
    TN -= (long) TN ;
}

void
DateTime::add(long seconds)
{
    TN += seconds/(24.0*3600.0);
    DN += (long) TN ;
    TN -= (long) TN ;
}

// return (t0 - this) in days
float
DateTime::diff (DateTime& t0)
{
    long ddn = t0.DN - DN;
    float dtn = t0.TN - TN;
    return (ddn + dtn);
}

//----------------------------------------------------------------------
//     _               ___  _                            
//  __| |__ _ ______  / _ \| |__ ___ ___ _ ___ _____ _ _ 
// / _| / _` (_-<_-< | (_) | '_ (_-</ -_) '_\ V / -_) '_|
// \__|_\__,_/__/__/  \___/|_.__/__/\___|_|  \_/\___|_|  
//                                                      
//----------------------------------------------------------------------

Observer::Observer(float lat, float lng, float hgt)
{
    LA = RADIANS(lat) ;
    LO = RADIANS(lng) ;
    HT = hgt / 1000 ;

    U[0] = cos(LA)*cos(LO) ;
    U[1] = cos(LA)*sin(LO) ;
    U[2] = sin(LA) ;

    E[0] = -sin(LO) ;
    E[1] =  cos(LO) ;
    E[2] =  0. ;

    N[0] = -sin(LA)*cos(LO) ;
    N[1] = -sin(LA)*sin(LO) ;
    N[2] =  cos(LA) ;

    float RP = RE * (1 - FL) ;
    float XX = RE * RE ;
    float ZZ = RP * RP ;
    float D = sqrt(XX*cos(LA)*cos(LA) + 
	            ZZ*sin(LA)*sin(LA)) ;
    float Rx = XX / D + HT ;
    float Rz = ZZ / D + HT ;

    O[0] = Rx * U[0] ;
    O[1] = Rx * U[1] ;
    O[2] = Rz * U[2] ;

    V[0] = -O[1] * W0 ;
    V[1] =  O[0] * W0 ;
    V[2] =  0 ;
}

//----------------------------------------------------------------------
//     _              ___       _       _ _ _ _       
//  __| |__ _ ______ / __| __ _| |_ ___| | (_) |_ ___ 
// / _| / _` (_-<_-< \__ \/ _` |  _/ -_) | | |  _/ -_)
// \__|_\__,_/__/__/ |___/\__,_|\__\___|_|_|_|\__\___|
//
//----------------------------------------------------------------------

static float
getfloat(const char *c, int i0, int i1)
{
    char buf[20] ;
    int i ;
    for (i=0; i0+i<i1; i++) 
	buf[i] = c[i0+i] ;
    buf[i] = '\0' ;
    return strtod(buf, NULL) ;
}

static long
getlong(const char *c, int i0, int i1)
{
    char buf[20] ;
    int i ;
    for (i=0; i0+i<i1; i++) 
	buf[i] = c[i0+i] ;
    buf[i] = '\0' ;
    return atol(buf) ;
}

Satellite::Satellite(const char *l1, const char *l2)
{
    tle(l1, l2) ;
}

Satellite::~Satellite()
{
}

void
Satellite::tle(const char *l1, const char *l2)
{
    // direct quantities from the orbital elements

    N = getlong(l2, 2, 7) ;
    YE = getlong(l1, 18, 20) ;
    if (YE < 58)
	YE += 2000 ;
    else
	YE += 1900 ;

    TE = getfloat(l1, 20, 32) ;
    M2 = RADIANS(getfloat(l1, 33, 43)) ;

    IN = RADIANS(getfloat(l2, 8, 16)) ;
    RA = RADIANS(getfloat(l2, 17, 25)) ;
    EC = getfloat(l2, 26, 33)/1e7f ;
    WP = RADIANS(getfloat(l2, 34, 42)) ;
    MA = RADIANS(getfloat(l2, 43, 51)) ;
    MM = 2.0f * M_PI * getfloat(l2, 52, 63) ;
    RV = getlong(l2, 63, 68) ;

    // derived quantities from the orbital elements 

    // convert TE to DE and TE 
    DE = fnday(YE, 1, 0) + (long) TE ;
    TE -= (long) TE ;
    N0 = MM/86400 ;
    A_0 = pow(GM/(N0*N0), 1./3.) ;
    B_0 = A_0*sqrt(1.-EC*EC) ;
    PC = RE*A_0/(B_0*B_0) ;
    PC = 1.5f*J2*PC*PC*MM ;
    float CI = cos(IN) ;
    QD = -PC*CI ;
    WD =  PC*(5*CI*CI-1)/2 ;
    DC = -2*M2/(3*MM) ;
}
void
Satellite::predict(const DateTime &dt)
{
    long DN = dt.DN ;
    float TN = dt.TN ;

    float TEG = DE - fnday(YG, 1, 0) + TE ;

    float GHAE = RADIANS(G0) + TEG * WE ;

    float T = (float) (DN - DE) + (TN-TE) ;
    float DT = DC * T / 2. ;
    float KD = 1. + 4. * DT ;
    float KDP = 1. - 7. * DT ;
  
    float M = MA + MM * T * (1. - 3. * DT) ;
    float DR = (long) (M / (2. * M_PI)) ;
    M -= DR * 2. * M_PI ;
    float EA = M ;

    float DNOM, C_EA, S_EA ;

    for (;;) {
	C_EA = cos(EA) ;
	S_EA = sin(EA) ;
	DNOM = 1. - EC * C_EA ;
	float D = (EA-EC*S_EA-M)/DNOM ;
	EA -= D ;
	if (fabs(D) < 1e-5)
	    break ;
    }

    float A = A_0 * KD ;
    float B = B_0 * KD ;
    RS = A * DNOM ;

    float Vx, Vy ;
    float Sx, Sy ;
    Sx = A * (C_EA - EC) ;
    Sy = B * S_EA ;

    Vx = -A * S_EA / DNOM * N0 ;
    Vy =  B * C_EA / DNOM * N0 ;

    float AP = WP + WD * T * KDP ;
    float CW = cos(AP) ;
    float SW = sin(AP) ;

    float RAAN = RA + QD * T * KDP ;
 
    float CQ = cos(RAAN) ;
    float SQ = sin(RAAN) ;

    float CI = cos(IN) ;
    float SI = sin(IN) ;

    // CX, CY, and CZ form a 3x3 matrix
    // that converts between orbit coordinates,
    // and celestial coordinates.

    Vec3 CX, CY, CZ ;
   
    CX[0] =  CW * CQ - SW * CI * SQ ;
    CX[1] = -SW * CQ - CW * CI * SQ ;
    CX[2] =  SI * SQ ;

    CY[0] =  CW * SQ + SW * CI * CQ ;
    CY[1] = -SW * SQ + CW * CI * CQ ;
    CY[2] = -SI * CQ ;

    CZ[0] = SW * SI ;
    CZ[1] = CW * SI ;
    CZ[2] = CI ;

    // satellite in celestial coords

    SAT[0] = Sx * CX[0] + Sy * CX[1] ;
    SAT[1] = Sx * CY[0] + Sy * CY[1] ;
    SAT[2] = Sx * CZ[0] + Sy * CZ[1] ;

    VEL[0] = Vx * CX[0] + Vy * CX[1] ;
    VEL[1] = Vx * CY[0] + Vy * CY[1] ;
    VEL[2] = Vx * CZ[0] + Vy * CZ[1] ;

    // and in geocentric coordinates

    float GHAA = (GHAE + WE * T) ;
    float CG = cos(-GHAA) ;
    float SG = sin(-GHAA) ;

    S[0] = SAT[0] * CG - SAT[1] * SG ;
    S[1] = SAT[0] * SG + SAT[1] * CG ;
    S[2] = SAT[2] ;

    V[0] = VEL[0] * CG - VEL[1]* SG ;
    V[1] = VEL[0] * SG + VEL[1]* CG ;
    V[2] = VEL[2] ;
}

void
Satellite::topo(const Observer *obs, float &alt, float &az, float &range, float &range_rate)
{
    Vec3 R ;
    R[0] = S[0] - obs->O[0] ;
    R[1] = S[1] - obs->O[1] ;
    R[2] = S[2] - obs->O[2] ;
    range = sqrt(R[0]*R[0]+R[1]*R[1]+R[2]*R[2]) ;
    R[0] /= range ;
    R[1] /= range ;
    R[2] /= range ;

    range_rate = 1000*((V[0]-obs->V[0])*R[0] + (V[1]-obs->V[1])*R[1] + V[2]*R[2]);	// m/s

    float u = R[0] * obs->U[0] + R[1] * obs->U[1] + R[2] * obs->U[2] ;
    float e = R[0] * obs->E[0] + R[1] * obs->E[1] + R[2] * obs->E[2] ;
    float n = R[0] * obs->N[0] + R[1] * obs->N[1] + R[2] * obs->N[2] ;

    az = DEGREES(atan2(e, n)) ;
    if (az < 0.) az += 360. ;
    alt = DEGREES(asin(u)) ;

    /* N.B. ignore refraction
     */
}

bool
Satellite::eclipsed(Sun *sp)
{
    float CUA = -(SAT[0]*sp->SUN[0]+SAT[1]*sp->SUN[1]+SAT[2]*sp->SUN[2])/RS;
    float UMD = RS*sqrt(1-CUA*CUA)/RE;
    return (UMD<=1 && CUA>=0);
       
}

//----------------------------------------------------------------------

Sun::Sun()
{
}

void
Sun::predict(const DateTime &dt)
{
    long DN = dt.DN ;
    float TN = dt.TN ;

    float T = (float) (DN - fnday(YG, 1, 0)) + TN ;
    float GHAE = RADIANS(G0) + T * WE ;
    float MRSE = RADIANS(G0) + T * WW + M_PI ;
    float MASE = RADIANS(MAS0 + T * MASD) ;
    float TAS = MRSE + EQC1*sin(MASE) + EQC2*sin(2.*MASE) ;
    float C, S ;

    C = cos(TAS) ;
    S = sin(TAS) ;
    SUN[0]=C ;
    SUN[1]=S*CNS ;
    SUN[2]=S*SNS ;
    C = cos(-GHAE) ; 
    S = sin(-GHAE) ; 
    H[0]=SUN[0]*C - SUN[1]*S ;
    H[1]=SUN[0]*S + SUN[1]*C ;
    H[2]=SUN[2] ;
}
