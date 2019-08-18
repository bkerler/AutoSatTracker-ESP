/* define and track a target
 */

#ifndef _TARGET_H
#define _TARGET_H

#include <WiFiClient.h>

#include "Circum.h"
#include "Sensor.h"
#include "Gimbal.h"
#include "Webpage.h"

#include "AutoSatTracker-ESP.h"
#include "P13.h"

class Target {

    private:

	// target
	float az, el;		// from TLE or op if overridden
	float range, rate;
	Satellite *sat;
	Sun *sun;

	// rise set transit state
	DateTime rise_time;
	DateTime set_time;
	DateTime trans_time;
	float rise_az, set_az;
	float trans_az, trans_el;
	bool set_ok, rise_ok, trans_ok;

	// current TLE lines
	char TLE_L0[30];	// name is arbitrarily truncated to this length
	char TLE_L1[70];	// 69 + '\0'
	char TLE_L2[70];	// 69 + '\0'

	// flags
	bool tle_ok;		// whether TLE and myobj are valid
	bool tracking;		// whether currently tracking
	bool overridden;	// whether target az or el has been overridden

	// skypath for displaying graph of a pass on an all-sky map
	enum {MAXSKYPATH = 20};
	struct {
	    float az, el;
	} skypath[MAXSKYPATH];
	uint8_t nskypath;

	// handy
	void displayAsWarning (WiFiClient client, bool mark);

    public:

	Target();
	void track();
	bool tleValidChecksum (const char *line);
	void setTrackingState (bool on);
        void sendNewValues (WiFiClient client);
        bool overrideValue (char *name, char *value);
	void setTLE (char *l1, char *l2, char *l3);
	void updateTopo(void);
	void findNextPass(void);
	void computeSkyPath(void);

};

extern Target *target;

#endif // _TARGET_H
