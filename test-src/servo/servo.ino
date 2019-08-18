/***************************************************
  This is an example for our Adafruit 16-channel PWM & Servo driver
  Servo test - this will drive 16 servos, one after the other

  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/products/815

  These displays use I2C to communicate, 2 pins are required to 
  interface. For Arduino UNOs, thats SCL -> Analog 5, SDA -> Analog 4

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Limor Fried/Ladyada for Adafruit Industries. 
  BSD license, all text above must be included in any redistribution
 ****************************************************/

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// called this way, it uses the default address 0x40
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
// you can also call it with a different address you want
//Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x41);

// Depending on your servo make, the pulse width min and max may vary, you
// want these to be as small/large as possible without hitting the hard stop
// for max range. You'll have to tweak them as necessary to match the servos you
// have!
#define SERVO0MIN  900 //221 // this is the 'minimum' pulse length count (out of 4096)
#define SERVO0MAX  1940 //516 // this is the 'maximum' pulse length count (out of 4096)
#define SERVO1MIN  900 // this is the 'minimum' pulse length count (out of 4096)
#define SERVO1MAX  1940 // this is the 'maximum' pulse length count (out of 4096)

// our servo # counter
uint8_t servonum = 0;

static const uint8_t SERVO_FREQ = 50;   // typical servo pulse frequency, Hz
static constexpr float US_PER_BIT = (1e6/SERVO_FREQ/4096);  // usec per bit @ 12 bit resolution

void setup() {
  Serial.begin(115200);
  Serial.println("16 channel Servo test!");

  pwm.begin();
 
  pwm.setPWMFreq(SERVO_FREQ);  // Analog servos run at ~60 Hz updates

  yield();
}


// you can use this function if you'd like to set the pulse length in seconds
// e.g. setServoPulse(0, 0.001) is a ~1 millisecond pulse width. its not precise!
void setServoPulse(uint8_t n, double pulse) {
  double pulselength;
  pulselength = 1e6;   // 1,000,000 us per second
  pulselength /= SERVO_FREQ;   // 60 Hz
  Serial.print(pulselength); Serial.println(" us per period");
  pulselength /= 4096;  // 12 bits of resolution
  Serial.print(pulselength); Serial.println(" us per bit");
  pulse /= pulselength;
  Serial.println(pulse);
  pwm.setPWM(n, 0, pulse);
}

void loop() {
  // Drive each servo to its minimum, wait 15 seconds
  servonum = 1;
  Serial.println(servonum);
  Serial.println(SERVO1MAX);
  setServoPulse(servonum,SERVO1MAX);
  /*servonum = 1;
  Serial.println(servonum);
  Serial.println(SERVO1MIN);
  pwm.setPWM(servonum, 0, SERVO1MIN);
  delay(15000);
  // Drive each servo to its maximum, wait 15 seconds
  servonum = 0;
  Serial.println(servonum);
  Serial.println(SERVO0MAX);
  pwm.setPWM(servonum, 0, SERVO0MAX);
  servonum = 1;
  Serial.println(servonum);
  Serial.println(SERVO1MAX);
  pwm.setPWM(servonum, 0, SERVO1MAX);
  */
  delay(15000);
}
