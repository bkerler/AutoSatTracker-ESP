/* this class contains information about the 9 dof spatial sensor
 */

#include "Sensor.h"

/* class constructor
 */
Sensor::Sensor()
{
	// instantiate, discover and initialize
	resetWatchdog();
	bno = new Adafruit_BNO055(-1, BNO055_I2CADDR);
	resetWatchdog();
	sensor_found = bno->begin(Adafruit_BNO055::OPERATION_MODE_NDOF);
	if (sensor_found)
	    Serial.println (F("Sensor found ok"));
	else
	    Serial.println (F("Sensor not found"));
	installCalibration();
}

/* read the current temperature, in degrees C
 */
int8_t Sensor::getTempC()
{
	if (sensor_found)
	    return bno->getTemp();
	return (-1);
}

/* return whether sensor is connected and calibrated
 */
bool Sensor::calibrated(uint8_t& sys, uint8_t& gyro, uint8_t& accel, uint8_t& mag)
{
	if (!sensor_found)
	    return (false);

	sys = 0;
	gyro = 0;
	accel = 0;
	mag = 0;;
	bno->getCalibration(&sys, &gyro, &accel, &mag);
	return (sys >= 1 && gyro >= 1 && accel >= 1 && mag >= 1);
}

/* read the current az and el, corrected for mag decl but not necessarily calibrated.
 * N.B. we assume this will only be called if we know the sensor is connected.
 * N.B. Adafruit board:
 *   the short dimension is parallel to the antenna boom,
 *   the populated side of the board faces upwards and
 *   the side with the control signals (SDA, SCL etc) points in the rear direction of the antenna pattern.
 * Note that az/el is a left-hand coordinate system.
 */
void Sensor::getAzEl (float *azp, float *elp)
{
	Wire.setClockStretchLimit(2000);
	imu::Vector<3> euler = bno->getVector(Adafruit_BNO055::VECTOR_EULER);
	*azp = myfmod (euler.x() + circum->magdeclination + 540, 360);
	*elp = euler.z();
}

/* process name = value pair
 * return whether we recognize it
 */
bool Sensor::overrideValue (char *name, char *value)
{
	if (!strcmp (name, "SS_Save")) {
	    saveCalibration();
	    webpage->setUserMessage (F("Sensor calibrations saved to EEPROM+"));
	    return (true);
	}

	return (false);
}

/* send latest values to web page
 * N.B. labels must match ids in wab page
 */
void Sensor::sendNewValues (WiFiClient client)
{
	if (!sensor_found) {
	    client.println (F("SS_Status=Not found!"));
	    client.println (F("SS_Save=false"));
	    return;
	}

	float az, el;
	getAzEl (&az, &el);
	client.print (F("SS_Az=")); client.println (az);
	client.print (F("SS_El=")); client.println (el);

	uint8_t sys, gyro, accel, mag;
	bool calok = calibrated (sys, gyro, accel, mag);
	if (calok)
	    client.println (F("SS_Status=Ok+"));
	else
	    client.println (F("SS_Status=Uncalibrated!"));
	client.print (F("SS_SStatus=")); client.println (sys);
	client.print (F("SS_GStatus=")); client.println (gyro);
	client.print (F("SS_MStatus=")); client.println (mag);
	client.print (F("SS_AStatus=")); client.println (accel);

	client.print (F("SS_Save="));
	if (calok && sys == 3 && gyro == 3 && accel == 3 && mag == 3) 
	    client.println (F("true"));
	else
	    client.println (F("false"));

	client.print (F("SS_Temp="));
	client.println (getTempC());
}

/* read the sensor calibration values and save into EEPROM.
 * Wanted to stick with stock Adafruit lib so pulled from
 * post by protonstorm at https://forums.adafruit.com/viewtopic.php?f=19&t=75497
 */
void Sensor::saveCalibration()
{
	// put into config mode
	bno->setMode (Adafruit_BNO055::OPERATION_MODE_CONFIG);
	delay(25);

	// request all bytes starting with the ACCEL
	byte nbytes = (byte)sizeof(nv->BNO055cal);
	Wire.beginTransmission((uint8_t)BNO055_I2CADDR);
	Wire.write((uint8_t)(Adafruit_BNO055::ACCEL_OFFSET_X_LSB_ADDR));
	Wire.endTransmission();
	Wire.requestFrom((uint8_t)BNO055_I2CADDR, nbytes);

	// wait for all 22 bytes to be available
	while (Wire.available() < nbytes);

	// copy to NV
	Serial.println (F("Saving sensor values"));
	for (uint8_t i = 0; i < nbytes; i++) {
	    nv->BNO055cal[i] = Wire.read();
	    Serial.println (nv->BNO055cal[i]);
	}

	// restore NDOF mode
	bno->setMode (Adafruit_BNO055::OPERATION_MODE_NDOF);
	delay(25);

	// save in EEPROM
	nv->put();
}

/* install previously stored calibration data from EEPROM if it looks valid.
 * Wanted to stick with stock Adafruit lib so pulled from
 * post by protonstorm at https://forums.adafruit.com/viewtopic.php?f=19&t=75497
 */
void Sensor::installCalibration()
{
	resetWatchdog();
	byte nbytes = (byte)sizeof(nv->BNO055cal);

	// read from EEPROM, qualify
	nv->get();
	uint8_t i;
	for(i = 0; i < nbytes; i++) {
	    if (nv->BNO055cal[i] != 0)
		break;
	}
	if (i == nbytes)
	    return;		// all zeros can't be valid

	// put into config mode
	bno->setMode (Adafruit_BNO055::OPERATION_MODE_CONFIG);
	delay(25);

	// set from NV
	// Serial.println (F("Restoring sensor values"));
	for(uint8_t i = 0; i < nbytes; i++) {
	    Wire.beginTransmission((uint8_t)BNO055_I2CADDR);
	    Wire.write((Adafruit_BNO055::adafruit_bno055_reg_t)
	    		((uint8_t)(Adafruit_BNO055::ACCEL_OFFSET_X_LSB_ADDR)+i));
	    Wire.write(nv->BNO055cal[i]);
	    // Serial.println (nv->BNO055cal[i]);
	    Wire.endTransmission();
	}

	// restore NDOF mode
	bno->setMode (Adafruit_BNO055::OPERATION_MODE_NDOF);
	delay(25);
}
