// class to control two motors to track a target az and el using the Adafruit I2C interface

#ifndef _GIMBAL_H
#define _GIMBAL_H

#include <Wire.h>
#include <WiFiClient.h>
#include <Adafruit_PWMServoDriver.h>

#include "AutoSatTracker-ESP.h"
#include "Target.h"
#include "Sensor.h"

class Gimbal {

    private:

	// I2C servo interface
	Adafruit_PWMServoDriver *pwm;
	static const uint8_t GIMBAL_I2C_ADDR = 0x40;	// I2C bus address of servo controller
	static const uint8_t SERVO_FREQ = 50;		// typical servo pulse frequency, Hz
	static constexpr float US_PER_BIT = (1e6/SERVO_FREQ/4096);	// usec per bit @ 12 bit resolution
	static const uint8_t MOT1_UNIT = 0;		// motor 1 I2C unit number
	static const uint8_t MOT2_UNIT = 1;		// motor 2 I2C unit number
	bool gimbal_found;				// whether PWM controller is present

	// motor info
	typedef struct {
	    float az_scale, el_scale;			// az and el scale: steps (del usec) per degree
	    uint16_t min, max;				// position limits, usec
	    uint16_t pos;				// last commanded position, usec
	    int16_t del_pos;				// change in pos since previous move
	    bool atmin, atmax;				// (would have been commanded to) limit
	    uint8_t servo_num;				// I2C bus address 0..15
	} MotorInfo;
	static const uint8_t NMOTORS = 2;		// not easily changed
	MotorInfo motor[NMOTORS];

	// search info
	// N.B.: max az physical motion must be < 180/CAL_FRAC
	static const uint16_t UPD_PERIOD = 500;		// ms between updates
	static constexpr float MAX_SETTLE = 5.0;	// considered stopped, degs
	static const uint8_t N_INIT_STEPS = 4;		// number of init_steps
	static constexpr float CAL_FRAC = 0.333;	// fraction of full range to move for calibration
							// N.B.: max physical motion must be < 180/CAL_FRAC
	uint8_t init_step;				// initialization sequencing
	uint8_t best_azmotor;				// after cal, motor[] index with most effect in az
	uint32_t last_update;				// millis() time of last move
	float prevfast_az, prevfast_el;			// previous pointing position
	float prevstop_az, prevstop_el;			// previous stopped position

	void setMotorPosition (uint8_t motn, uint16_t newpos);
	void calibrate (float &az_s, float &el_s);
	void seekTarget (float& az_t, float& el_t, float& az_s, float& el_s);
	float azDist (float &from, float &to);

    public:

	Gimbal();

	void moveToAzEl (float az_t, float el_t);
	void sendNewValues (WiFiClient client);
	bool overrideValue (char *name, char *value);
	bool connected() { return (gimbal_found); };
	bool calibrated() { return (init_step >= N_INIT_STEPS); }
};

extern Gimbal *gimbal;

#endif // _GIMBAL_H
