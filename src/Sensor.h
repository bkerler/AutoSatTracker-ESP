/* this class contains information about the 9 dof spatial sensor
 */

#ifndef _SENSOR_H
#define _SENSOR_H

#include <Wire.h>
#include <WiFiClient.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>


#include "AutoSatTracker-ESP.h"
#include "Circum.h"

class Sensor {

    private:

	Adafruit_BNO055 *bno;		// sensor detail
	bool sensor_found;		// whether sensor is connected
	bool calibrated(uint8_t& sys, uint8_t& gyro, uint8_t& accel, uint8_t& mag);
	enum {
	    BNO055_I2CADDR = 0x28,	// I2C bus address of BNO055
	};

    public:

	Sensor();
	int8_t getTempC();
	void getAzEl (float *azp, float *elp);
	void sendNewValues (WiFiClient client);
	bool connected() { return sensor_found; };
	void saveCalibration(void);
	void installCalibration(void);
	bool overrideValue (char *name, char *value);

};

extern Sensor *sensor;

#endif // _SENSOR_H
