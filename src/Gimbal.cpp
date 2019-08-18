// class to control two motors to track a target az and el using the Adafruit I2C interface


#include "Gimbal.h"

/* issue raw motor command in microseconds pulse width, clamped at limit
 */
void Gimbal::setMotorPosition (uint8_t motn, uint16_t newpos)
{
	if (motn >= NMOTORS || !gimbal_found)
	    return;
	MotorInfo *mip = &motor[motn];

	mip->atmin = (newpos <= mip->min);
	if (mip->atmin)
	    newpos = mip->min;
	mip->atmax = (newpos >= mip->max);
	if (mip->atmax)
	    newpos = mip->max;

	mip->del_pos = (int)newpos - (int)mip->pos;
	mip->pos = newpos;
	pwm->setPWM(mip->servo_num, 0, mip->pos/US_PER_BIT);
	// Serial.print(mip->servo_num); Serial.print(" "); Serial.println (newpos);
}

/* constructor 
 */
Gimbal::Gimbal ()
{
	// first confirm whether controller is present
	resetWatchdog();
	Wire.beginTransmission(GIMBAL_I2C_ADDR);
	gimbal_found = (Wire.endTransmission() == 0);
	if (!gimbal_found) {
	    Serial.println (F("PWM controller not found"));
	    return;
	}
	Serial.println (F("PWM controller found ok"));

	// instantiate PWM controller
	pwm = new Adafruit_PWMServoDriver(GIMBAL_I2C_ADDR);
	pwm->begin();
	pwm->setPWMFreq(SERVO_FREQ);

	// record axis assignments
	motor[0].servo_num = MOT1_UNIT;
	motor[1].servo_num = MOT2_UNIT;

	// init each motor state
	nv->get();
	motor[0].min = nv->mot0min;
	motor[0].max = nv->mot0max;
	motor[0].pos = 0;
	motor[0].atmin = false;
	motor[0].atmax = false;
	motor[0].az_scale = 0;
	motor[0].el_scale = 0;

	motor[1].min = nv->mot1min;
	motor[1].max = nv->mot1max;
	motor[1].pos = 0;
	motor[1].atmin = false;
	motor[1].atmax = false;
	motor[1].az_scale = 0;
	motor[1].el_scale = 0;

	// init to arbitrary, but at least defined, state
	init_step = 0;
	best_azmotor = 0;
	last_update = 0;
	prevfast_az = prevfast_el = -1000;
	prevstop_az = prevstop_el = -1000;
}

/* move motors towards the given new target az and el 
 */
void Gimbal::moveToAzEl (float az_t, float el_t)
{
	// only update every UPD_PERIOD
	uint32_t now = millis();
	if (now - last_update < UPD_PERIOD)
	    return;
	last_update = now;

	// read current sensor direction
	float az_s, el_s;
	sensor->getAzEl (&az_s, &el_s);

	// only check further when motion has stopped as evidenced by stable sensor values
	if (fabs (azDist (prevfast_az, az_s)) < MAX_SETTLE && fabs (el_s - prevfast_el) < MAX_SETTLE) {

	    // calibrate if not already else seek target
	    if (!calibrated())

		calibrate (az_s, el_s);

	    else

		seekTarget (az_t, el_t, az_s, el_s);
	    

	    // preserve for next stopped iteration
	    prevstop_az = az_s;
	    prevstop_el = el_s;

	}

	// preserve for next fast iteration
	prevfast_az = az_s;
	prevfast_el = el_s;
}

/* run the next step of the initial scale calibration series.
 * steps proceed using init_step up to N_INIT_STEPS
 */
void Gimbal::calibrate (float& az_s, float& el_s)
{
	// handy step ranges
	uint16_t range0 = motor[0].max - motor[0].min;
	uint16_t range1 = motor[1].max - motor[1].min;

	switch (init_step++) {

	case 0:

	    // move near min of each range
	    setMotorPosition (0, motor[0].min + (1-CAL_FRAC)/2*range0);
	    setMotorPosition (1, motor[1].min + (1-CAL_FRAC)/2*range1);
	    break;

	case 1:

	    // move just motor 0 a subtantial distance
	    /*
	    Serial.print(F("Init 1: Mot 0 starts at:\t"));
		Serial.print(az_s); Serial.print(F("\t"));
		Serial.print (el_s); Serial.print(F("\tMoves\t"));
		Serial.println(CAL_FRAC*range0, 0);
	    */
	    setMotorPosition (0, motor[0].pos + CAL_FRAC*range0);
	    break;

	case 2:

	    // calculate scale of motor 0
	    motor[0].az_scale = CAL_FRAC*range0/azDist(prevstop_az, az_s);
	    motor[0].el_scale = CAL_FRAC*range0/(el_s - prevstop_el);
	    /*
	    Serial.print(F("Init 2: Mot 0 ended  at:\t"));
		Serial.print(az_s); Serial.print(F("\t"));
		Serial.print (el_s); Serial.print(F("\tusec:\t"));
		Serial.print (CAL_FRAC*range0); Serial.print (F("\tDel usec/Deg:\t"));
		Serial.print (motor[0].az_scale); Serial.print (F("\t"));
		Serial.println (motor[0].el_scale);
	    */

	    // repeat procedure for motor 1
	    /*
	    Serial.print(F("Init 2: Mot 1 starts at:\t"));
		Serial.print(az_s); Serial.print(F("\t"));
		Serial.print (el_s); Serial.print(F("\tMoves\t"));
		Serial.println(CAL_FRAC*range1, 0);
	    */
	    setMotorPosition (1, motor[1].pos + CAL_FRAC*range1);
	    break;

	case 3:

	    // calculate scale of motor 1
	    motor[1].az_scale = CAL_FRAC*range1/azDist(prevstop_az, az_s);
	    motor[1].el_scale = CAL_FRAC*range1/(el_s - prevstop_el);
	    /*
	    Serial.print(F("Init 3: Mot 1 ended  at:\t"));
		Serial.print(az_s); Serial.print(F("\t"));
		Serial.print (el_s); Serial.print(F("\tusec:\t"));
		Serial.print (CAL_FRAC*range1); Serial.print (F("\tDel usec/Deg:\t"));
		Serial.print (motor[1].az_scale); Serial.print (F("\t"));
		Serial.println (motor[1].el_scale);
	    */

	    // select best motor for az
	    best_azmotor = fabs(motor[0].az_scale) < fabs(motor[1].az_scale) ? 0 : 1;
	    Serial.print (F("Best Az motor:\t"));
		Serial.print (best_azmotor); Serial.print (F("\tScale:\t"));
		Serial.print (motor[best_azmotor].az_scale);
		Serial.print (F("\tEl motor:\t"));
		Serial.print (!best_azmotor); Serial.print (F("\tScale:\t"));
		Serial.println (motor[!best_azmotor].el_scale);

	    // report we have finished calibrating
	    target->setTrackingState (true);
	    break;

	default:

	    webpage->setUserMessage (F("BUG! Bogus init_step"));
	    break;
	}
}

/* run the next step of seeking the given target given the current stable az/el sensor values
 */
void Gimbal::seekTarget (float& az_t, float& el_t, float& az_s, float& el_s)
{
	// find pointing error in each dimension as a move from sensor to target
	float az_err = azDist (az_s, az_t);
	float el_err = el_t - el_s;

	// correct each error using motor with most effect in that axis
	MotorInfo *azmip = &motor[best_azmotor];
	MotorInfo *elmip = &motor[!best_azmotor];

	/*
	Serial.print (F("Az:\t"));
	    Serial.print(az_s); Serial.print(F("\t"));
	    Serial.print(azmip->pos); Serial.print (F("\t"));
	    Serial.print(az_err, 1); Serial.print (F("\t"));
	    Serial.print(az_err*azmip->az_scale, 0);
	Serial.print (F("\tEl:\t"));
	    Serial.print(el_s); Serial.print(F("\t"));
	    Serial.print(elmip->pos); Serial.print (F("\t"));
	    Serial.print(el_err, 1); Serial.print (F("\t"));
	    Serial.println(el_err*elmip->el_scale, 0);
	*/


	// tweak scale if move was substantial and sanity check by believing only small changes
	const float MIN_ANGLE = 30;		// min acceptable move
	const float MAX_CHANGE = 0.1;		// max fractional scale change
	float az_move = azDist (prevstop_az, az_s);
	if (fabs(az_move) >= MIN_ANGLE) {
	    float new_az_scale = azmip->del_pos/az_move;
	    if (fabs((new_az_scale - azmip->az_scale)/azmip->az_scale) < MAX_CHANGE) {
		Serial.print (F("New Az scale\t"));
		    Serial.print (azmip->az_scale); Serial.print (F("\t->\t"));
		    Serial.println(new_az_scale);
		azmip->az_scale = new_az_scale;
	    }
	}
	float el_move = el_s - prevstop_el;
	if (fabs(el_move) >= MIN_ANGLE) {
	    float new_el_scale = elmip->del_pos/el_move;
	    if (fabs((new_el_scale - elmip->el_scale)/elmip->el_scale) < MAX_CHANGE) {
		Serial.print (F("New El scale\t"));
		    Serial.print (elmip->el_scale); Serial.print (F("\t->\t"));
		    Serial.println(new_el_scale);
		elmip->el_scale = new_el_scale;
	    }
	}


	// move each motor to reduce error, but if at Az limit then swing back to near opposite limit
	if (azmip->atmin) {
	    // Serial.println (F("At Az Min"));
	    setMotorPosition (best_azmotor, azmip->min + 0.8*(azmip->max - azmip->min));
	} else if (azmip->atmax) {
	    // Serial.println (F("At Az Max"));
	    setMotorPosition (best_azmotor, azmip->min + 0.2*(azmip->max - azmip->min));
	} else
	    setMotorPosition (best_azmotor, azmip->pos + az_err*azmip->az_scale);
	setMotorPosition (!best_azmotor, elmip->pos + el_err*elmip->el_scale);

}

/* given two azimuth values, return path length going shortest direction
 */
float Gimbal::azDist (float &from, float &to)
{
	float d = to - from;
	if (d < -180)
	    d += 360;
	else if (d > 180)
	    d -= 360;
	return (d);
}

/* send latest web values.
 * N.B. must match id's in main web page
 */
void Gimbal::sendNewValues (WiFiClient client)
{
	if (!gimbal_found) {
	    client.println (F("G_Status=Not found!"));
	    return;
	}

	client.print (F("G_Mot1Pos="));
	if (init_step > 0)
	    client.println (motor[0].pos);
	else
	    client.println (F(""));
	client.print (F("G_Mot1Min=")); client.println (motor[0].min);
	client.print (F("G_Mot1Max=")); client.println (motor[0].max);

	client.print (F("G_Mot2Pos="));
	if (init_step > 0)
	    client.println (motor[1].pos);
	else
	    client.println (F(""));
	client.print (F("G_Mot2Min=")); client.println (motor[1].min);
	client.print (F("G_Mot2Max=")); client.println (motor[1].max);

	client.print (F("G_Status="));
	    if (motor[0].atmin)
		client.println (F("1 at Min!"));
	    else if (motor[0].atmax)
		client.println (F("1 at Max!"));
	    else if (motor[1].atmin)
		client.println (F("2 at Min!"));
	    else if (motor[1].atmax)
		client.println (F("2 at Max!"));
	    else
		client.println (F("Ok+"));

}

/* process name = value.
 * return whether we recognize it
 */
bool Gimbal::overrideValue (char *name, char *value)
{
	const __FlashStringHelper *nog = F("No gimbal!");

	if (!strcmp (name, "G_Mot1Pos")) {
	    if (gimbal_found) {
		target->setTrackingState(false);
		setMotorPosition (0, atoi(value));
	    } else
		webpage->setUserMessage (nog);
	    return (true);
	}
	if (!strcmp (name, "G_Mot1Min")) {
	    if (gimbal_found) {
		nv->mot0min = motor[0].min = atoi(value);
		nv->put();
		webpage->setUserMessage (F("Servo 1 minimum saved in EEPROM+"));
	    } else
		webpage->setUserMessage (nog);
	    return (true);
	}
	if (!strcmp (name, "G_Mot1Max")) {
	    if (gimbal_found) {
		nv->mot0max = motor[0].max = atoi(value);
		nv->put();
		webpage->setUserMessage (F("Servo 1 maximum saved in EEPROM+"));
	    } else
		webpage->setUserMessage (nog);
	    return (true);
	}
	if (!strcmp (name, "G_Mot2Pos")) {
	    if (gimbal_found) {
		target->setTrackingState(false);
		setMotorPosition (1, atoi(value));
	    } else
		webpage->setUserMessage (nog);
	    return (true);
	}
	if (!strcmp (name, "G_Mot2Min")) {
	    if (gimbal_found) {
		nv->mot1min = motor[1].min = atoi(value);
		nv->put();
		webpage->setUserMessage (F("Servo 2 minimum saved in EEPROM+"));
	    } else
		webpage->setUserMessage (nog);
	    return (true);
	}
	if (!strcmp (name, "G_Mot2Max")) {
	    if (gimbal_found) {
		nv->mot1max = motor[1].max = atoi(value);
		nv->put();
		webpage->setUserMessage (F("Servo 2 maximum saved in EEPROM+"));
	    } else
		webpage->setUserMessage (nog);
	    return (true);
	}
	return (false);
}
