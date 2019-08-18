/* define and track a target
 */

#include "Target.h"

/* constructor
 */
Target::Target()
{
	// init values
	resetWatchdog();
	az = el = range = rate = 0;
	memset (TLE_L0, 0, sizeof(TLE_L0));
	memset (TLE_L1, 0, sizeof(TLE_L1));
	memset (TLE_L2, 0, sizeof(TLE_L2));
	sat = new Satellite();
	sun = new Sun();

	// init flags
	tle_ok = false;
	tracking = false;
	overridden = false;
	set_ok = rise_ok = trans_ok = false;
	nskypath = 0;
}

/* update target if valid, and move gimbal if tracking
 */
void Target::track()
{
	resetWatchdog();

	// update ephemerides
	updateTopo();

	// update gimbal if tracking
	if (tracking)
	    gimbal->moveToAzEl (az, el);
}

/* update target info
 */
void Target::updateTopo()
{
	if (tle_ok && !overridden) {
	    DateTime now (circum->now());
	    sat->predict (now);
	    sun->predict (now);
	    sat->topo (circum->observer(), el, az, range, rate);
	}
}

/* turn tracking on or off if it makes sense to do so
 */
void Target::setTrackingState (bool want_on)
{
	if (want_on) {
	    if (!gimbal->connected()) {
		webpage->setUserMessage (F("Can not track without a gimbal!"));
		tracking = false;
	    } else if (!gimbal->calibrated()) {
		webpage->setUserMessage (F("Calibrating gimbal motor scales!"));
		tracking = true;
	    } else if (!sensor->connected()) {
		webpage->setUserMessage (F("Can not track without a position sensor!"));
		tracking = false;
	    } else if (overridden) {
		webpage->setUserMessage (F("Now tracking overridden Az and El+"));
		tracking = true;
	    } else if (tle_ok) {
		webpage->setUserMessage (F("Now tracking: "), TLE_L0, '+');
		tracking = true;
	    } else {
		webpage->setUserMessage (F("First Upload a TLE or override Target Az and El!"));
		tracking = false;
	    }
	} else {
	    webpage->setUserMessage (F("Tracking is off"));
	    tracking = false;
	}
}

/* send latest values to web page.
 * N.B. names must match ids in web page
 */
void Target::sendNewValues (WiFiClient client)
{

	const __FlashStringHelper *zerostr = F("");

	// update pass info just after main events
	// N.B. transit jiggles back and forth too much to recompute after it
	DateTime now (circum->now());
	bool just_rose = rise_ok && now.diff(rise_time) < 0;
	bool just_sat = set_ok && now.diff(set_time) < 0;
	if (just_rose || just_sat) {
	    updateTopo();
	    findNextPass();
	    computeSkyPath();
	}

	if (tle_ok || overridden) {
	    client.print (F("T_Az="));
	    client.print (az);
	    displayAsWarning (client, overridden);
	    client.print (F("T_El="));
	    client.print (el);
	    displayAsWarning (client, overridden);
	}

	if (tle_ok && !overridden) {
	    float age = circum->age(sat);
	    client.print (F("T_Age="));
	    client.print (age);
	    displayAsWarning (client, age < -10 || age > 10);

	    client.print (F("T_Sunlit="));
	    if (sat->eclipsed (sun))
		client.println (F("No"));
	    else
		client.println (F("Yes"));

	    client.print (F("T_Range=")); client.println (range);
	    client.print (F("T_RangeR=")); client.println (rate);
	    client.print (F("T_VHFDoppler=")); client.println (-rate*144000/3e8); // want kHz
	    client.print (F("T_UHFDoppler=")); client.println (-rate*440000/3e8); // want kHz


	    client.print (F("T_NextRise="));
	    if (rise_ok) {
		float dt = 24*now.diff(rise_time);
		circum->printSexa (client, dt);
		circum->printPL (client, (dt < 1.0/60.0) ? Circum::GOODNEWS : Circum::NORMAL);
	    } else
		client.println (F("??? !"));		// beware trigraphs

	    client.print (F("T_RiseAz="));
	    if (rise_ok)
		client.println (rise_az);
	    else
		client.println (F("??? !"));

	    const __FlashStringHelper *transin, *transaz, *transel;
	    client.print (F("T_NextTrans="));
	    if (trans_ok) {
		float dt = 24*now.diff(trans_time);
		circum->printSexa (client, dt);
		circum->printPL (client, Circum::NORMAL);
		if (dt < 0) {
		    transin = F("This Transited ago");
		    transaz = F("This Transit Azimuth");
		    transel = F("This Transit Elevation");
		} else {
		    transin = F("Next Transit in");
		    transaz = F("Next Transit Azimuth");
		    transel = F("Next Transit Elevation");
		}
	    } else {
		client.println (F("??? !"));
		transin = zerostr;
		transaz = zerostr;
		transel = zerostr;
	    }
	    client.print ("T_NextTrans_l=");
	    client.println (transin);

	    client.print (F("T_TransAz="));
	    if (trans_ok)
		client.println (trans_az);
	    else
		client.println (F("??? !"));
	    client.print ("T_TransAz_l=");
	    client.println (transaz);

	    client.print (F("T_TransEl="));
	    if (trans_ok)
		client.println (trans_el);
	    else
		client.println (F("??? !"));
	    client.print ("T_TransEl_l=");
	    client.println (transel);


	    client.print (F("T_NextSet="));
	    if (set_ok) {
		float dt = 24*now.diff(set_time);
		circum->printSexa (client, dt);
		circum->printPL (client, Circum::NORMAL);
	    } else
		client.println (F("??? !"));

	    client.print (F("T_SetAz="));
	    if (rise_ok)
		client.println (set_az);
	    else
		client.println (F("??? !"));

	    const __FlashStringHelper *tup;
	    client.print ("T_Up=");
	    if (rise_ok && set_ok) {
		float up = rise_time.diff(set_time);
		if (up > 0) {
		    circum->printSexa (client, up*24);			// next whole pass
		    circum->printPL (client, Circum::NORMAL);
		    tup= F("Next pass duration");
		} else {
		    up = 24*now.diff(set_time);				// this pass remaining
		    circum->printSexa (client, up);
		    circum->printPL (client, (up < 1.0/60.0) ? Circum::BADNEWS : Circum::NORMAL);
		    tup= F("This pass Ends in");
		}
	    } else {
		tup= zerostr;
		client.println (zerostr);
	    }
	    client.print ("T_Up_l=");
	    client.println (tup);
	}

	client.print (F("T_TLE="));
	if (tle_ok) {
	    client.println (TLE_L0);
	    client.println (TLE_L1);
	    client.println (TLE_L2);
	} else {
	    client.println (zerostr);
	    client.println (zerostr);
	    client.println (zerostr);
	}

	if (overridden) {
	    client.println (F("T_Age="));
	    client.println (F("T_Sunlit="));
	    client.println (F("T_Range="));
	    client.println (F("T_RangeR="));
	    client.println (F("T_VHFDoppler="));
	    client.println (F("T_UHFDoppler="));
	    client.println (F("T_NextRise="));
	    client.println (F("T_RiseAz="));
	    client.println (F("T_NextTrans="));
	    client.println (F("T_TransAz="));
	    client.println (F("T_TransEl="));
	    client.println (F("T_NextSet="));
	    client.println (F("T_SetAz="));
	}

	client.print (F("T_Status="));
	    client.println (tle_ok || overridden ? (el > 0 ? "Up+" : "Down") : "No target!");

	client.print (F("tracking="));
	if (tracking)
	    client.println (F("On"));
	else
	    client.println (F("Off"));

	client.print(F("skypath="));
	for (uint8_t i = 0; i < nskypath; i++) {
	    client.print(skypath[i].az);
	    client.print(F(","));
	    client.print(skypath[i].el);
	    client.print(F(";"));
	}
	client.println(zerostr);
}

/* optionally add code to display the value as a warning
 */
void Target::displayAsWarning (WiFiClient client, bool mark)
{
	if (mark)
	    client.println (F("!"));
	else
	    client.println (F(""));
}

/* process name = value other than T_TLE (that's done with setTLE()).
 * return whether we recognize it
 */
bool Target::overrideValue (char *name, char *value)
{
	if (!strcmp (name, "T_Az")) {
	    az = atof(value);
	    while (az < 0)
		az += 360;
	    while (az >= 360)
		az -= 360;
	    overridden = true;
	    tle_ok = false;
	    nskypath = 0;
	    return (true);
	}
	if (!strcmp (name, "T_El")) {
	    el = fmax (fmin (atof(value), 90), 0);
	    overridden = true;
	    tle_ok = false;
	    nskypath = 0;
	    return (true);
	}
	return (false);
}

/* record a TLE for possible tracking, set tle_ok if valid
 */
void Target::setTLE (char *l1, char *l2, char *l3)
{
	if (tleValidChecksum (l2) && tleValidChecksum(l3)) {
	    tle_ok = true;
	    sat->tle (l2, l3);
	    strncpy (TLE_L0, l1, sizeof(TLE_L0)-1);
	    strncpy (TLE_L1, l2, sizeof(TLE_L1)-1);
	    strncpy (TLE_L2, l3, sizeof(TLE_L2)-1);
	    Serial.println (TLE_L0);
	    Serial.println (TLE_L1);
	    Serial.println (TLE_L2);
	    overridden = false;
	    tracking = false;
	    updateTopo();
	    findNextPass();		// init for track()
	    computeSkyPath();
	    webpage->setUserMessage (F("New TLE uploaded successfully for "), TLE_L0, '+');
	} else {
	    webpage->setUserMessage (F("Uploaded TLE is invalid!"));
	    tle_ok = false;
	}
}

/* find next pass for sat, if currently valid
 */
void Target::findNextPass()
{
	if (!tle_ok || overridden) {
	    set_ok = rise_ok = trans_ok = false;
	    return;
	}

	const int8_t COARSE_DT = 60;	// seconds/step forward for course search
	const int8_t FINE_DT = -1;	// seconds/step backward for refined search
	float pel = 0, ppel = 0;	// previous 2 elevations
	float paz = 0;			// previous az
	int8_t dt = COARSE_DT;		// search step size, seconds
	DateTime t(circum->now());	// search time, init with now

	// search no more than two days ahead
	set_ok = rise_ok = trans_ok = false;
	while ((!set_ok || !rise_ok || !trans_ok) && circum->now().diff(t) < 2) {

	    // find circumstances at time t
	    float tel, taz, trange, trate;
	    sat->predict (t);
	    sat->topo (circum->observer(), tel, taz, trange, trate);
	    // Serial.print (24*60*circum->now().diff(t)); Serial.print(" ");
	    // Serial.print (tel, 6); Serial.print(" ");
	    // Serial.print (rise_ok); Serial.print (trans_ok); Serial.println (set_ok);

	    // check for a visible transit event
	    // N.B. too flat to use FINE_DT
	    if (dt == COARSE_DT && tel > 0 && ppel > 0 && ppel < pel && pel > tel) {
		// found a coarse transit at previous time, good enough
		trans_time = t;
		trans_time.add ((long)(-COARSE_DT));
		trans_az = paz;
		trans_el = pel;
		trans_ok = true;
	    }

	    // check for rising events
	    // N.B. invalidate ppel after turning around
	    if (tel > 0 && pel < 0) {
		if (dt == FINE_DT) {
		    // going backwards so found a refined set event, record and resume course forward time
		    set_time = t;
		    set_az = taz;
		    set_ok = true;
		    dt = COARSE_DT;
		    pel = tel;
		} else if (!rise_ok) {
		    // found a coarse rise event, go back slower looking for better set
		    dt = FINE_DT;
		    pel = tel;
		}
	    }

	    // check for setting events
	    // N.B. invalidate ppel after turning around
	    if (tel < 0 && pel > 0) {
		if (dt == FINE_DT) {
		    // going backwards so found a refined rise event, record and resume course forward time
		    rise_time = t;
		    rise_az = taz;
		    rise_ok = true;
		    dt = COARSE_DT;
		    pel = tel;
		} else if (!set_ok) {
		    // found a coarse set event, go back slower looking for better rise
		    dt = FINE_DT;
		    pel = tel;
		}
	    }

	    // next time step
	    paz = taz;
	    ppel = pel;
	    pel = tel;
	    t.add ((long)dt);
	}
}

/* compute sky path of current pass.
 * if up now just plot until set because rise_time will be for subsequent pass
 */
void Target::computeSkyPath()
{
        if (!set_ok || !rise_ok)
            return;

        DateTime t;

        if (el > -1)	// allow for a bit of rise/set round off
            t = circum->now();
        else if (rise_time.diff(set_time) > 0)
            t = rise_time;
        else {
            // rise or set is unknown or for different passes
            nskypath = 0;
            return;
        }


        long secsup = (long)(t.diff(set_time)*24*3600);
        long stepsecs = secsup/(MAXSKYPATH-1);  // inclusive

        for (nskypath = 0; nskypath < MAXSKYPATH; nskypath++) {
            float srange, srate;
            sat->predict (t);
            sat->topo (circum->observer(), skypath[nskypath].el, skypath[nskypath].az, srange, srate);
            t.add (stepsecs);
        }
}

/* return whether the given line appears to be a valid TLE
 * only count digits and '-' counts as 1
 */
bool Target::tleValidChecksum (const char *line)
{
	// sum first 68 chars
	int sum = 0;
	for (uint8_t i = 0; i < 68; i++) {
	    char c = *line++;
	    if (c == '-')
		sum += 1;
	    else if (c == '\0')
		return (false);		// too short
	    else if (c >= '0' && c <= '9')
		sum += c - '0';
	}

	// last char is sum of previous modulo 10
	return ((*line - '0') == (sum%10));
}
