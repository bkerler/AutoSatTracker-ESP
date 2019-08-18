/* information about observing circumstances including time and location from GPS or manually entered.
 * use Serial1 hardware RX1/TX1 for Adafruit GPS shield on Mega.
 */

#include "Circum.h"

// serial configuration for GPS
#define	GPS_TX_PIN	13			// not needed
#define	GPS_RX_PIN	12
#define	GPS_INVERT	false
#define	GPS_BUFSIZE	512
#define	GPS_BAUD	9600


/* constructor
 */
Circum::Circum ()
{
	// create serial connection to GPS board
	resetWatchdog();
	//ss = new SoftwareSerial (GPS_RX_PIN, GPS_TX_PIN, GPS_INVERT, GPS_BUFSIZE);
	//ss->begin(GPS_BAUD);
  ss = new SoftwareSerial ();
  ss->begin(GPS_BAUD, GPS_RX_PIN, GPS_TX_PIN, SWSERIAL_8N1, false, 512);
  
	// create GPS parser
	resetWatchdog();
	GPS = new TinyGPS();

	// init values
	resetWatchdog();
	latitude = 30.0;
	longitude = -110.0;
	altitude = 700.0;
	obs = NULL;
	newObserver (latitude, longitude, altitude);
	hdop = 99.0;
	nsats = 0;
	setnow (2018, 1, 1, 0, 0, 0);
	magdecl (latitude, longitude, altitude, decimalYear(), &magdeclination);

	// init flags
	gps_lock = false;
	gps_ok = false;
	time_overridden = false;
	loc_overridden = false;

}

/* return current time as a decimal year
 */
float Circum::decimalYear()
{
	// get time now, sets dt_now.DN and dt_now.TN
	int year; uint8_t month, day, h, m, s;
	getnow (year, month, day, h, m, s);

	// get time on jan 1 this year
	DateTime y0 (year, 1, 1, 0, 0, 0);

	// n days this year
	int nd = (year%4) ? 365 : 366;

	// return year and fraction
	return (year + ((dt_now.DN + dt_now.TN) - (y0.DN + y0.TN))/nd);
}

/* send latest values to web page.
 * N.B. names must match ids in web page
 */
void Circum::sendNewValues (WiFiClient client)
{

	int year; uint8_t month, day, h, m, s;
	getnow (year, month, day, h, m, s);

	client.print (F("GPS_Date="));
	printDate (client, year, month, day);
	    printPL (client, time_overridden ? BADNEWS : NORMAL);

	client.print (F("GPS_UTC="));
	printHMS (client, h, m, s);
	    printPL (client, time_overridden ? BADNEWS : NORMAL);

	client.print (F("GPS_Status="));
	if (gps_lock) {
	    if (time_overridden || loc_overridden)
		client.println (F("Overridden!"));
	    else
		client.println (F("Locked+"));
	} else if (gps_ok)
	    client.println (F("No lock!"));
	else
	    client.println (F("Not found!"));

	client.print (F("GPS_Enable="));
	if (gps_lock && (time_overridden || loc_overridden))
	    client.println (F("true"));
	else
	    client.println (F("false"));

	client.print (F("GPS_Lat=")); client.print (latitude, 3);
	    printPL (client, loc_overridden ? BADNEWS : NORMAL);
	client.print (F("GPS_Long=")); client.print (longitude, 3);
	    printPL (client, loc_overridden ? BADNEWS : NORMAL);
	client.print (F("GPS_Alt=")); client.print (altitude);
	    printPL (client, loc_overridden ? BADNEWS : NORMAL);

	client.print (F("GPS_MagDecl=")); client.println (magdeclination);
	client.print (F("GPS_HDOP=")); client.println (hdop);
	client.print (F("GPS_NSat=")); client.println (nsats);
}

/* print value v in sexagesimal format, can be negative
 */
void Circum::printSexa (WiFiClient client, float v)
{
	bool isneg = v < 0;
	if (isneg) {
	    v = -v;
	    client.print (F("-"));
	}

        uint8_t h = (uint8_t) v;
        v = (v - h)*60;
        uint8_t m = (uint8_t) v;
        v = (v - m)*60;
        uint8_t s = (uint8_t) v;

	printHMS (client, h, m, s);
}

/* print time
 */
void Circum::printHMS (WiFiClient client, uint8_t h, uint8_t m, uint8_t s)
{
	client.print (h);

	if (m < 10)
	    client.print (F(":0"));
	else
	    client.print (F(":"));
	client.print (m);

	if (s < 10)
	    client.print (F(":0"));
	else
	    client.print (F(":"));
	client.print (s);
}

/* print the given date
 */
void Circum::printDate (WiFiClient client, int y, uint8_t m, uint8_t d)
{
	client.print(y);
	client.print(F(" "));
	client.print(m);
	client.print(F(" "));
	client.print(d);
}

/* end a value with the given print level code
 */
void Circum::printPL (WiFiClient client, PrintLevel pl)
{
	switch (pl) {
	case BADNEWS:
	    client.println (F("!"));
	    break;
	case GOODNEWS:
	    client.println (F("+"));
	    break;
	default:
	    client.println (F(""));
	    break;
	}
}

/* process name = value.
 * return whether we recognize it
 */
bool Circum::overrideValue (char *name, char *value)
{
	if (!strcmp (name, "GPS_UTC")) {
	    // op is setting new UTC
	    int year; uint8_t month, day, h, m, s;
	    getnow (year, month, day, h, m, s);		// start with now
	    char *mat, *sat;				// start of min and sec, if any
	    h = strtol (value, &mat, 10);		// override hour
	    if (mat != value) {				// if new minute ...
		m = strtol (mat, &sat, 10);		// override minutes
		if (sat != mat)				// if new seconds ...
		    s = strtol (sat, NULL, 10);		// override seconds
	    }
	    setnow (year, month, day, h, m, s);		// set system time to new value
	    time_overridden = true;			// set flag that op has overridden GPS time
	    target->updateTopo();			// update target to new time
	    target->findNextPass();			// update pass from now
	    target->computeSkyPath();			// and show
	    return (true);
	}
	if (!strcmp (name, "GPS_Date")) {
	    // op is setting new date
	    int year; uint8_t month, day, h, m, s;
	    getnow (year, month, day, h, m, s);		// start with now
	    char *mat, *dat;				// start of month and day, if any
	    year = strtol (value, &mat, 10);		// override year
	    if (mat != value) {				// if new month ...
		month = strtol (mat, &dat, 10);		// override month
		if (dat != mat)				// if new day ...
		    day = strtol (dat, NULL, 10);	// override day
	    }
	    setnow (year, month, day, h, m, s);		// set system time to new value
	    time_overridden = true;			// set flag that op has overridden GPS time
	    target->updateTopo();			// update target to new time
	    target->findNextPass();			// update pass from now
	    target->computeSkyPath();			// and show
	    return (true);
	}
	if (!strcmp (name, "GPS_Lat")) {
	    latitude = atof (value);
	    newObserver (latitude, longitude, altitude);
	    loc_overridden = true;			// set flag that op has overridden GPS loc
	    target->updateTopo();			// update target to new time
	    target->findNextPass();			// update pass from here
	    target->computeSkyPath();			// and show
	    magdecl (latitude, longitude, altitude, decimalYear(), &magdeclination);
	    return (true);
	}
	if (!strcmp (name, "GPS_Long")) {
	    longitude = atof (value);
	    newObserver (latitude, longitude, altitude);
	    loc_overridden = true;			// set flag that op has overridden GPS loc
	    target->updateTopo();			// update target to new time
	    target->findNextPass();			// update pass from here
	    target->computeSkyPath();			// and show
	    magdecl (latitude, longitude, altitude, decimalYear(), &magdeclination);
	    return (true);
	}
	if (!strcmp (name, "GPS_Alt")) {
	    altitude = atof (value);
	    newObserver (latitude, longitude, altitude);
	    loc_overridden = true;			// set flag that op has overridden GPS loc
	    target->updateTopo();			// update target to new time
	    target->findNextPass();			// update pass from here
	    target->computeSkyPath();			// and show
	    magdecl (latitude, longitude, altitude, decimalYear(), &magdeclination);
	    return (true);
	}
	if (!strcmp (name, "GPS_Enable")) {
	    time_overridden = false;			// resume GPS values
	    loc_overridden = false;			// resume GPS values
	}

	return (false);	// not one of ours
}

/* call occasionally to sync our system time from GPS, if it is running ok.
 */
void Circum::checkGPS()
{
	resetWatchdog();

	// read more from GPS, process when new message complete
	while (ss->available()) {

	    // Serial.print((char)ss->peek());

	    if (GPS->encode(ss->read())) {

		// note we receiving GPS sentences ok
		gps_ok = true;

		// get location and age
		float new_lat, new_lng, new_alt;
		unsigned long loc_fix_age;
		GPS->f_get_position(&new_lat, &new_lng, &loc_fix_age);
		new_alt = GPS->f_altitude();

		// get time and age
		unsigned long time_fix_age;
		int new_year; byte new_mon, new_day, new_hr, new_min, new_sec, new_hund;
		GPS->crack_datetime (&new_year, &new_mon, &new_day, &new_hr, &new_min, &new_sec,
				&new_hund, &time_fix_age);

		// determine whether data are up to date
		if (loc_fix_age == TinyGPS::GPS_INVALID_AGE || time_fix_age  == TinyGPS::GPS_INVALID_AGE) {
		    gps_lock = false;
		    Serial.println("No fix detected");
		} else {
		    gps_lock = true;
		    if (loc_fix_age > 5000 || time_fix_age > 5000)
			Serial.println("Warning: possible stale data!");
		}

		if (gps_lock) {

		    // update system time from GPS unless op has overridden
		    if (!time_overridden)
			setnow (new_year, new_mon, new_day, new_hr, new_min, new_sec);

		    // update location from GPS, unless op has overridden or within allowed jitter
		    if (!loc_overridden 
			    && (fabs(latitude - new_lat) > 0.01
				|| fabs(longitude - new_lng) > 0.01
				|| fabs (altitude - new_alt) > 100)
			    ) {

			latitude = new_lat;
			longitude = new_lng;
			altitude = new_alt;

			newObserver (latitude, longitude, altitude);
			magdecl (latitude, longitude, altitude, decimalYear(), &magdeclination);
		    }

		    // get fix quality info
		    hdop = 0.01*GPS->hdop();
		    nsats = (int)GPS->satellites();

		}
	    }
	}
}

/* get time from dt_now advanced to current millis()
 */
void Circum::getnow(int &year, uint8_t &month, uint8_t &day, uint8_t &h, uint8_t &m, uint8_t &s)
{
	dt_now.TN = dt_TN0;
	dt_now.DN = dt_DN0;
	dt_now.add ((long)((millis() - dt_m0)/1000));
	dt_now.gettime(year, month, day, h, m, s);
}

/* init dt_now based on current millis()
 */
void Circum::setnow(int year, uint8_t month, uint8_t day, uint8_t h, uint8_t m, uint8_t s)
{
	dt_m0 = millis();
	dt_now.settime(year, month, day, h, m, s);
	dt_TN0 = dt_now.TN;
	dt_DN0 = dt_now.DN;
}

/* return age of satellite elements in days
 */
float Circum::age (Satellite *sat)
{
	return ((dt_now.DN + dt_now.TN) - (sat->DE + sat->TE));

}

/* install a new Observer
 */
void Circum::newObserver (float lat, float lng, float hgt)
{
	if (obs)
	    delete (obs);
	obs = new Observer (lat, lng, hgt);
}

/* return the current Observer
 */
Observer *Circum::observer()
{
	return (obs);
}
