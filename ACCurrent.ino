/*********
Read the AC Current draw of an Lionel Locomotive
JK 24 Jan 2023
Using the DEVMO 5A Range AC and DC Current Sensor Module
*********/

// decide which OLED display
#define SINEWAVE
#define RMS

//////////// for OLED 1.5 Inch Color

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1351.h>

// known to work with nano 33 iot (31 dec 2022)
//#define NANO33IOT
#ifdef NANO33IOT
 #define SCLK_PIN 13
 #define MOSI_PIN 11
 #define RST_PIN 3
 #define DC_PIN 5
 #define CS_PIN 4
#else                 // UNO
 #define MOSI_PIN 11
 #define SCLK_PIN 13
 #define CS_PIN   10
 #define DC_PIN    7
 #define RST_PIN   8
#endif

#define SWIDTH 128
#define SHEIGHT 128

Adafruit_SSD1351 display(SWIDTH, SHEIGHT, &SPI, CS_PIN, DC_PIN, RST_PIN);

#define BLACK           0x0000
#define BLUE            0x001F
#define RED             0xF800
#define GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0  
#define WHITE           0xFFFF
#define GREY            0x8410
#define ORANGE          0xE880

/////////// end OLED defines

#define SW1 3 // a reed switch, use with internal pullup
#define LED  2
#define CS   A0 // current sensor using magnetism

// defines for the ACS712 5 amps full scale
/* According to the ACS712 chip datasheet:
      Variation in V IOUT(Q) can be attributed to the
      resolution of the Allegro linear IC quiescent voltage trim and
      thermal drift.
*/
#define CSOFFSET 0.1068 // empirically determined in volts
                   // a. 0.08 seemed to give more accurate results for a time
                   // b. 0.1068 determined by linear
                   // c. 0.2619 regression after installing the sensor in a
                   //    metal box at the time this compared well with the
                   //    zero meter reading, 0.27
                   // d. things changed back to (b) when things were rewired...

#define CSSCALE  0.185 // this is sensitivity, see the AC712 datasheet p.2
#define RMSCF 0.3526  // rms conversion factor from peak-to-peak current
#define FULLSCALE_SINE 400.0
#define DETECT_THRESHOLD 0.16

unsigned long timeZero;
unsigned long nReads;
int captures = 0;
int lastRead;

void setup() {
  Serial.begin(9600);
  display.begin();
  display.fillScreen(BLACK);
  display.setRotation(2);
  display.setTextColor(WHITE, BLACK);
  display.setCursor(0, 0);
#ifdef RMS
  display.setTextSize(3);
#else  
  display.setTextSize(0);
#endif
  
  // initialize analog input for the Current Sensor
  pinMode(CS, INPUT);
  pinMode(LED, OUTPUT);
  pinMode(SW1, INPUT_PULLUP);
  
  delay(100);
  nReads = 0;
  captures = 0;
  timeZero = millis();
}

#define MAXBUF 270
float pkCurrent, acVolts, acCurrent;
int capture[MAXBUF];
int lastSW1 = LOW;

void loop() {
  float amps;
  if ( digitalRead(SW1) == HIGH ) {
    if ( lastSW1 != HIGH ) {
      display.fillScreen(BLACK);
    }
    display.setTextSize(3);
    amps = rmsAmps();
    display.setTextSize(2);
    display.setCursor(16, 94);
    display.print(amps > DETECT_THRESHOLD ? "I'm home!" : "I'm away!");
    lastSW1 = HIGH;
  }
  else {
    display.setTextSize(0);
    amps = sineWave();
    lastSW1 = LOW;
  }
  if ( amps > DETECT_THRESHOLD )
    digitalWrite(LED, HIGH);
  else
    digitalWrite(LED, LOW);
}

#ifdef RMS
float rmsAmps() {
  int n, r, acR;
  unsigned long sum;
  float amps;
  acR = acRead(CS, 500);
  acVolts = voltsFromRead(acR);
  pkCurrent = currentFromVolts(acVolts);
  amps = pkCurrent * RMSCF;
  //Serial.print(" AC Read "); Serial.print(acR);
  //Serial.print(" Peak Current "); Serial.print(pkCurrent);
  //Serial.print(" pkVolts "); Serial.print(acVolts);
  //Serial.print(" Amps "); Serial.println(amps);
  display.setCursor(20,50);
  display.print("      ");
  display.setCursor(20,50);
  if ( amps >= 0 ) display.print(" ");
  display.print(amps);
  delay(100);
  return(amps);
}
#endif

float swAmps;

#ifdef SINEWAVE
float sineWave() {
  int r;
  unsigned long cycleTime;
  float amps;
  
  cycleTime = millis() - timeZero;
  r = aveRead(CS, 5) - 511;          // make the wave pass through zero
  if ( (lastRead >= 0 && r < 0) || cycleTime > 1000 ) {
    // toggle between capture and display
    captures++;
    if ( captures > 3 ) captures = 0;
    timeZero = millis();
  }
  lastRead = r;
  
  if ( captures < 3 && nReads < MAXBUF ) { // capture 2 full cycles
    capture[nReads] = r;
    nReads++;
  }
  else if ( nReads > 0 ) {
    // display sine wave on OLED screen
    int x, y, lastX, lastY;
    float xS, yS, amps;
    
    display.fillScreen(BLACK);
    display.setCursor(94, 120);
    display.print(1000/cycleTime); display.print(" Hz");
    xS = (float)SWIDTH / (float)nReads;
    yS = SHEIGHT / FULLSCALE_SINE;
    display.drawLine(0, SHEIGHT/2, SWIDTH, SHEIGHT/2, WHITE);
    for (int i=0; i<nReads; i++) {
      x = (int)(i * xS);
      y = SHEIGHT - (int)((capture[i]+(FULLSCALE_SINE/2.0)) * yS);
      if ( i == 0 ) {
	lastX = x;
	lastY = y;
      }
      display.drawLine(lastX, lastY, x, y, RED); // costs memory
      lastX = x;
      lastY = y;
    }
    swAmps = currentFromVolts(voltsFromRead(pkToPk(capture, nReads)))
      * RMSCF;

    nReads = 0;
  }
  return(swAmps);
}
#endif

float voltsFromRead(int reading) {
  return(reading * (5.0 / 1024.0));
}

float currentFromVolts(float volts) {
  return((volts - CSOFFSET) / CSSCALE);
}

int acRead(int pin, int count) {
  // get a peak to peak difference over count samples
  int minR;
  int maxR;
  int i;
  int r = analogRead(pin);

  for(minR = r, maxR = r, i = 0; i < count; r = analogRead(pin), i++) {
    if ( r < minR ) minR = r;
    if ( r > maxR ) maxR = r;
  }
  return(maxR - minR);
}

int pkToPk(int *reads, int count) {
  int minR, maxR, i, r;
  r = reads[0];
  
  for(minR = r, maxR = r, i = 0; i < count; r = reads[i], i++) {
    if ( r < minR ) minR = r;
    if ( r > maxR ) maxR = r;
  }
  return(maxR - minR);
}

int aveRead(int pin, int count) {
  unsigned long sum = 0;
  for (int i=0; i<count; i++)
    sum += analogRead(pin);
  return(sum/count);
}
    
