static double epoc = 2015.0;
const double c0[13][13]={
{	0.0,	-29438.2,	-2444.5,	1351.8,	907.5,	-232.9,	69.4,	81.7,	24.2,	5.5,	-2.0,	3.0,	-2.0,	},
{	4796.3,	-1493.5,	3014.7,	-2351.6,	814.8,	360.1,	67.7,	-75.9,	8.9,	8.8,	-6.1,	-1.4,	-0.1,	},
{	-2842.4,	-638.8,	1679.0,	1223.6,	117.8,	191.7,	72.3,	-7.1,	-16.9,	3.0,	0.2,	-2.3,	0.5,	},
{	-113.7,	246.5,	-537.4,	582.3,	-335.6,	-141.3,	-129.1,	52.2,	-3.1,	-3.2,	0.6,	2.1,	1.2,	},
{	283.3,	-188.6,	180.7,	-330.0,	69.7,	-157.2,	-28.4,	15.0,	-20.7,	0.6,	-0.5,	-0.8,	-0.9,	},
{	46.9,	196.5,	-119.9,	16.0,	100.6,	7.7,	13.6,	9.1,	13.3,	-13.2,	1.8,	0.6,	0.9,	},
{	-20.1,	32.8,	59.1,	-67.1,	8.1,	61.9,	-70.3,	-3.0,	11.6,	-0.1,	-0.7,	-0.7,	0.1,	},
{	-54.3,	-19.5,	6.0,	24.5,	3.5,	-27.7,	-2.9,	5.9,	-16.3,	8.7,	2.2,	0.1,	0.6,	},
{	10.1,	-18.3,	13.3,	-14.5,	16.2,	6.0,	-9.2,	2.4,	-2.1,	-9.1,	2.4,	1.7,	-0.4,	},
{	-21.8,	10.7,	11.8,	-6.8,	-6.9,	7.9,	1.0,	-3.9,	8.5,	-10.4,	-1.8,	-0.2,	-0.5,	},
{	3.3,	-0.4,	4.6,	4.4,	-7.9,	-0.6,	-4.2,	-2.9,	-1.1,	-8.8,	-3.6,	0.4,	0.2,	},
{	-0.0,	2.1,	-0.6,	-1.1,	0.7,	-0.2,	-2.1,	-1.5,	-2.6,	-2.0,	-2.3,	3.5,	-0.9,	},
{	-1.0,	0.3,	1.8,	-2.2,	0.3,	0.7,	-0.1,	0.3,	0.2,	-0.9,	-0.2,	0.8,	-0.0,	},
};
const double cd0[13][13]={
{	0.0,	7.0,	-11.0,	2.4,	-0.8,	-0.3,	-0.8,	-0.3,	-0.1,	-0.1,	0.0,	-0.0,	0.0,	},
{	-30.2,	9.0,	-6.2,	-5.7,	-0.9,	0.6,	-0.5,	-0.2,	0.2,	-0.1,	-0.0,	0.0,	0.0,	},
{	-29.6,	-17.3,	0.3,	2.0,	-6.5,	-0.8,	-0.1,	-0.3,	-0.2,	-0.0,	-0.1,	-0.0,	-0.0,	},
{	6.5,	-0.8,	-2.0,	-11.0,	5.2,	0.1,	1.6,	0.9,	0.5,	0.4,	0.2,	0.0,	0.0,	},
{	-0.4,	5.8,	3.8,	-3.5,	-4.0,	1.2,	-1.6,	0.1,	-0.1,	-0.4,	-0.1,	-0.0,	-0.1,	},
{	0.2,	2.3,	-0.0,	3.3,	-0.6,	1.4,	0.0,	-0.6,	0.4,	0.0,	-0.2,	-0.1,	-0.0,	},
{	0.3,	-1.5,	-1.2,	0.4,	0.2,	1.3,	1.2,	-0.9,	0.4,	0.3,	-0.0,	0.0,	0.0,	},
{	0.6,	0.5,	-0.8,	-0.2,	-1.1,	0.1,	0.2,	0.7,	-0.1,	0.0,	-0.1,	-0.0,	-0.0,	},
{	-0.4,	0.6,	-0.1,	0.6,	-0.2,	-0.5,	0.5,	0.1,	0.4,	-0.0,	-0.2,	-0.0,	0.0,	},
{	-0.3,	0.1,	-0.4,	0.3,	0.1,	-0.0,	-0.1,	0.5,	0.2,	-0.3,	-0.1,	-0.1,	-0.0,	},
{	0.0,	0.1,	-0.2,	0.1,	-0.1,	0.1,	-0.0,	-0.1,	0.2,	-0.0,	-0.0,	-0.0,	-0.0,	},
{	0.0,	0.1,	0.0,	0.1,	-0.0,	-0.0,	0.1,	-0.0,	-0.1,	-0.0,	-0.1,	-0.1,	-0.0,	},
{	-0.0,	0.0,	-0.1,	0.1,	-0.0,	0.0,	-0.0,	0.0,	0.0,	-0.0,	0.0,	-0.1,	-0.1,	},
};