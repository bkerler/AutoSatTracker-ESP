// Sidereal and Solar data. Valid to year ~2030
static const float YG = 2014.f ; //GHAA, Year YG, Jan 0.0
static const float G0 = 99.5828f ;

//MA Sun and rate, deg, deg/day
static const float MAS0 = 356.4105f ;
static const float MASD = 0.98560028f ;

//Sun's Equation of centre terms
static const float EQC1 = 0.03340 ;
static const float EQC2 = 0.00035 ;

//Sun inclination
static const float INS = (23.4375f)*M_PI/180.0 ;
static const float CNS = cos(INS) ;
static const float SNS = sin(INS) ;

