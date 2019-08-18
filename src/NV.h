/* Class to organize variables stored in EEPROM.
 * The public class variables are mirrored in RAM, so change nv then call put(), or call get() then access nv.
 * The validity of EEPROM is checked using a magic constant that must match. All values will be inited to 0.
 */

#ifndef _NV_H
#define _NV_H

#include <ESP8266WiFi.h>
#include <EEPROM.h>

#include "AutoSatTracker-ESP.h"

class NV {

    private:

	enum {
	    MAGIC  = 0x5a5aa5a5,
	    NBNO055CALBYTES = 22,
	    EEBYTES = 250,
	};

    public:

	// these variables are stored in EEPROM
	uint32_t magic;
	IPAddress IP, GW, NM;
	char ssid[64];
	char pw[64];
	uint16_t mot0min, mot0max, mot1min, mot1max;
	uint8_t BNO055cal[NBNO055CALBYTES];

	NV() {
	    EEPROM.begin(EEBYTES);
	}

	void get() {
	    // fill this object from EEPROM
	    byte *this_addr = (byte *)this;
	    for (size_t i = 0; i < sizeof(*this); i++)
		this_addr[i] = EEPROM.read(i);
	    // it no magic cookie, init and save in EEPROM
	    if (magic != MAGIC) {
		memset (this, 0, sizeof(*this));
		magic = MAGIC;
		put();
	    }
	}

	void put() {
	    // save this object in EEPROM
	    byte *this_addr = (byte *)this;
	    for (size_t i = 0; i < sizeof(*this); i++)
		EEPROM.write(i, this_addr[i]);
	    EEPROM.commit();
	}
};

extern NV *nv;

#endif // _NV_H
