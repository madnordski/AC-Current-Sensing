// [ . . . ]

//////////// Current Sensors
#define CSOFFSET 0.1068 // default zero offset (not used)
#define CSSCALE  0.185  // this is sensitivity, see the AC712 datasheet p.2
#define RMSCF 0.3526    // rms conversion factor from peak-to-peak current
#define STANDING_THRESHOLD 0.450   // current draw above which a train
				   // is powered in neutral
#define RUNNING_THRESHOLD 1.300    // current draw above which a train
				   // has it's motors energized

// as determined by thresholds, each block can have one of three states
#define BLOCK_OFF      0
#define TRAIN_STANDING 1
#define TRAIN_RUNNING  2

// deployed 4 sensors
#define NUM_CS 4

// we have four blocks, two on the outer loop and two on the inner
// the blocks are indexed as follows:
//  0 = outer loop block 1   A12
//  1 = outer loop block 2   A13
//  2 = inner loop block 1   A14
//  3 = inner loop block 2   A15

// the pins on the Mega used for current sensing (4 sensors)
int csPin[NUM_CS] = {A12, A13, A14, A15};

// we keep a zero offset in volts for each sensor (determined by a
// calibration routine at startup)
float csOffset[NUM_CS] = {CSOFFSET, CSOFFSET, CSOFFSET, CSOFFSET};

// this global is used to track the state of each block as determined
// by the thresholds
int blockState[NUM_CS] = {BLOCK_OFF, BLOCK_OFF, BLOCK_OFF, BLOCK_OFF};

// translates integer states into strings for convenience
char blockStatus[3][9] = {"OFF", "STANDING", "RUNNING"};

// flag indicating which trains need a status update
int trainUpdate = 0; 

/////////// current sensing convenience functions

int acRead(int sensor) {
  /** acRead gets a peak to peak difference among readings
      for 60 hz we read for at least 50ms to ensure we get 3 complete cycles
      1 / 60 * 3 = 0.050 = 50 ms
      @param sensor - the arduino pin to read
      @return an integer value representing difference between the
      highest and lowest peak
  */
  int minR, maxR, r, i;
  int nReads = 5;
  unsigned long startTimer;
  
  r = aveRead(csPin[sensor], nReads); // reduce noise by taking an average

  // find the min and max over about 50 ms and return the difference
  for(startTimer = millis(), minR = r, maxR = r; millis() - startTimer < 52;
      r = aveRead(csPin[sensor], nReads), i++) {
    if ( r < minR ) minR = r;
    if ( r > maxR ) maxR = r;
  }
  return(maxR - minR);
}

int aveRead(int pin, int count) {
  /** aveRead takes an average of count reads
      @param pin - the pin to read
      @param count - the number of readings to average
      @return the average
  */
  unsigned long sum = 0;
  for (int i=0; i<count; i++)
    sum += analogRead(pin);
  return(sum/count);
}

float getCurrent(int sensor) {
  /** getCurrent gets the current reading from a sensor
      @param sensor - the pin from which the sensor value is read
      @return the current in amps
  */
  int r = acRead(sensor);
  return(currentFromVolts(sensor, voltsFromRead(r)));
}

float voltsFromRead(int reading) {
  /** voltsFromRead converts a bit reading from a 10-bit analog pin to
      a voltage, applies to 5v microprocessors, e.g. Arduino Uno or Mega
      @return voltage in volts
  */
  return(reading * (5.0 / 1024.0)); // analog pin gives 5 volts over 2^10 bits
}

float currentFromVolts(int sensor, float volts) {
  /** currentFromVolts gets the RMS current from a voltage reading
      @param sensor - the sensor used, this is required because each
      sensor can have a different voltage offset
      @param volts - the voltage read from the sensor
      @return the RMS AC current
  */
  return((float)RMSCF * ((volts - csOffset[sensor]) / CSSCALE));
}

float csCalibrate(int sensor) {
  /** csCalibrate gets the zero voltage for a current sensor,
      typically run at startup when no trains are powered
      @param sensor - the pin to which the sensor is connected
      @return the calibration voltage
  */
  unsigned long sum = 0;
  int bitOfZero;
  int iters = 10;
  for (int i=0; i<iters; i++)
    sum += acRead(sensor);
  bitOfZero = sum / iters;
  return(voltsFromRead(bitOfZero));
}

// [ . . . ]

void setup() {

  // [ . . . ]

  // configure the current sensor pins
  for (int i=0; i<NUM_CS; i++)
    pinMode(csPin[i], INPUT);

  // [ . . . ]
  
  // calibrate current sensors

  for (int i=0; i<NUM_CS; i++) {
    csOffset[i] = csCalibrate(i);
    Serial.print("Current sensor "); Serial.print(i);
    Serial.print(" calibration = "); Serial.println(csOffset[i], 3);
  }

  // [ . . . ]
}

void loop() {

  // [ . . . ]

    // report on which blocks of track are active
  for (int i=0; i < NUM_CS; i++) {
    char info[24];
    int blockIs = BLOCK_OFF;
    float amps = getCurrent(i);
    int mA;
    if ( amps > STANDING_THRESHOLD ) blockIs = TRAIN_STANDING;
    if ( amps > RUNNING_THRESHOLD ) blockIs = TRAIN_RUNNING;
    if ( blockIs != blockState[i] ) {
      blockState[i] = blockIs;
      mA = (int)(amps * 1000);
      sprintf(info, "BLOCK %d %s %d mA", i+1, blockStatus[blockIs], mA);
      // send the message to the tablet
      uno.longPutString(info);
      // for a train all the blocks are the same, combine them
      trainUpdate |= i < 2 ? 1 : 2;
    }   
  }
  // two blocks to check for train status (highest state level)
  if ( trainUpdate != 0 ) { // 1 means 1, 2 means 2, 3 means both
    char info[32];
    int state;
    for (int track = 0; track < 2; track++) {
      if ( trainUpdate & track+1 ) {
	state = blockState[track*2] > blockState[track*2+1]
	  ? blockState[track*2] : blockState[track*2+1];
	sprintf(info, "TRAIN %d STATUS %s", track+1, blockStatus[state]);
	// the following sends message to Android app (via Arduino Uno)
	uno.longPutString(info);
      }
    }
    trainUpdate = 0;
  }

  // [ . . . ]
}

