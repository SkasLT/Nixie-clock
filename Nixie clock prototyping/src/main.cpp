#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

// debugging
#define DEBUG 1 // choose to debug or not; 1 is debugging 0 is not

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#define debug_begin(x) Serial.begin(x)
#else
#define debug(x)
#define debugln(x)
#define debug_begin(x)
#endif

// Control variables:
const int number_of_buttons = 3;                    // number of buttons connected
const int button[number_of_buttons] = {6, 7, 8};    // array that stores button pins
int buttonState[number_of_buttons] = {1, 1, 1};     // array that stores initial button states
int lastButtonState[number_of_buttons] = {1, 1, 1}; // array that stores last button states
int setupMode = 0;                                  // variable which saves the current value of setup mode

// Variables for controling shift registers and indicator leds:
const int latchPin = 9;     // Pin connected to RCK of TPIC6B595
const int masterReset = 10; // Pin connected to SRCLR of all TPIC6B595 IC-s
const int dataPin = 11;     // Pin connected to SERIAL IN of TPIC6B595
const int clockPin = 12;    // Pin connected to SRCK of TPIC6B595
const int hourLed = 4;      // Pin connected a led that will light up when adjusting hours
const int minuteLed = 5;    // Pin connected a led that will light up whem adjusting minutes
int hourLedState = HIGH;    // variable that stores current hourLed state
int minuteLedState = HIGH;  // variable that stores current minuteLed state

// motion detection Variables
const int sensorPin = 3;
const int displayControlPin = 2;
unsigned long previousTime = 0;

// Timing variables:
int minuteCounter = -1;
int minuteChange = 100; // set to 100 so that it's impossible for minute value to be same as minute change during startup
int hour, minute, second;
int hour1, hour2, minute1, minute2;

// define values for blanking digits
#define hour_1 1
#define hour_2 2
#define minute_1 3
#define minute_2 4

RTC_DS3231 rtc;

/**
 * Function for debouncing multiple buttons
 * @param buttonIndex the index of the button array to be read (button pins are storred in array)
 * @param debounceDelay time to delay between bounces (in milliseconds)
 */
bool debouncedButtonRead(int buttonIndex, const unsigned long debounceDelay)
{
  // Read the state of the button pin into a local variable:
  int reading = digitalRead(button[buttonIndex]);

  // Local variable used to store time between button pin state changes:
  unsigned long previousTime;

  /*
  Check to see if you just pressed the button
  (i.e. the input went from HIGH to LOW), and you've waited long enough
  since the last press to ignore any noise:
  */
  // If button state changed, due to noise or pressing:
  if (reading != lastButtonState[buttonIndex])
    previousTime = millis(); // reset the debouncing timer

  if ((millis() - previousTime) > debounceDelay)
  {
    /*
    Whatever the reading is at, it's been there for longer than the debounce
    delay, so take it as the actual current state:
    */

    // If the button state has changed:
    if (reading != buttonState[buttonIndex])
    {
      buttonState[buttonIndex] = reading;

      if (buttonState[buttonIndex] == LOW) // only return true if buttin state is LOW, change this to HIGH if you have pulldown resistors on button pins
        return true;
    }
  }

  // Save the reading. Next time through the loop, it'll be the lastButtonState:
  lastButtonState[buttonIndex] = reading;
  return false;
}

/**
 * function for shifting out the nth bit into the storage register (value determines which bit)
 * @param dataPin pin connected to serial data input for shift registers
 * @param clockPin clock pin connected to clock input for shift registers
 * @param bitOrder order of bits (LSB, MSB)
 * @param value sequential bit number to be shifted out
 * @param bits number of bits to shift out
 * @param enableBlanking when this is enabled all bitts that are shifted out will be zeroes (no digit will light up)
 */
void shiftOutBits(int dataPin, int clockPin, int bitOrder, int value, int bits, bool enableBlanking)
{
  if (enableBlanking == false)
  {
    for (int i = 0; i < bits; i++)
    {
      if (bitOrder == LSBFIRST)
        digitalWrite(dataPin, (i == value));
      else
        digitalWrite(dataPin, (bits - 1 - i) == value);

      digitalWrite(clockPin, HIGH);
      digitalWrite(clockPin, LOW);
    }
  }
  else
  {
    digitalWrite(dataPin, LOW);
    for (int i = 0; i < bits; i++)
    {
      digitalWrite(clockPin, HIGH);
      digitalWrite(clockPin, LOW);
    }
  }
}

/**
 * This function updates displayed time
 * @param blankDigit which digit is blanked (put "false" if no digits are to be blanked)
 */
void updateDisplayedTime(int blankDigit)
{
  digitalWrite(latchPin, LOW);
  shiftOutBits(dataPin, clockPin, LSBFIRST, minute2, 10, blankDigit == minute_2);
  shiftOutBits(dataPin, clockPin, LSBFIRST, minute1, 10, blankDigit == minute_1);
  shiftOutBits(dataPin, clockPin, LSBFIRST, hour2, 10, blankDigit == hour_2);
  shiftOutBits(dataPin, clockPin, LSBFIRST, hour1, 10, blankDigit == hour_1);
  digitalWrite(latchPin, HIGH);
}

// Function for calculating first and last hour digit, first and last minute digit
void calculateTime()
{
  // separate first and second digit of hour value
  hour1 = hour / 10;
  hour2 = hour % 10;

  // separate first and second digit of minute value
  minute1 = minute / 10;
  minute2 = minute % 10;
}

/**
 * Function necessary for longevity of NIXIE tubes, it lights up every digit one after another and repeats it for a certain ammount of time
 * This should be done as frequently as possible, but every 15 minutes will be ok
 * @param timeInterval how long will cathode routine run (in milliseconds)
 * @param digitDelay time between digit changes (in milliseconds) make sure digitDelay is large relative to timeInterval because cathode routine will run longer
 * than digitDelay
 */
void doCathodeRoutine(const unsigned long timeInterval, const unsigned long digitDelay)
{
  unsigned long previousTime = millis();

  while ((millis() - previousTime) <= timeInterval)
  {
    for (int i = 0; i < 10; i++) // loop runs 10 times since every NIXIE tube has 10 digits (0...9)
    {
      digitalWrite(latchPin, LOW);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10, false);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10, false);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10, false);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10, false);
      digitalWrite(latchPin, HIGH);
      delay(digitDelay);
    }
    for (int i = 8; i > 0; i--) // loop runs 10 times since every NIXIE tube has 10 digits (9...0)
    {
      digitalWrite(latchPin, LOW);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10, false);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10, false);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10, false);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10, false);
      digitalWrite(latchPin, HIGH);
      delay(digitDelay);
    }
  }
  debugln("cathode routine has completed");
}

// function that calculates current minutes, hours, seconds and their respective digits
void getCurrentTime()
{
  DateTime now = rtc.now();

  // store values of hours and minutes in their respecitive varables
  hour = now.hour();
  minute = now.minute();

  // calculate hour and minute digit values
  calculateTime();

  // print out time from rtc module on seral monitor
  if (second != now.second() && DEBUG == 1)
  {
    char buffer[10];
    sprintf(buffer, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    debugln(buffer);
    second = now.second();
  }
}

/**
 * Function that detects motion and turns on or off nixie display after some time of inactivity
 * @param timeDelay after how many minutes of inactivity will nixie display turn off
 */
void motionDetection(const unsigned long timeDelay)
{
  int trigger = digitalRead(sensorPin);

  if (trigger == HIGH)
  {
    previousTime = millis();
    digitalWrite(displayControlPin, HIGH);
    debugln("Motion has been detected!");
  }

  if (millis() - previousTime >= (timeDelay * 60000))
  {
    digitalWrite(displayControlPin, LOW);
    previousTime = millis();
    debug(timeDelay);
    debugln(" minutes have passed and no motion has beed detected");
  }
}

// menu page for changing hours
void firstMenuPage()
{
  while (setupMode == 1)
  {
    digitalWrite(hourLed, HIGH);
    if (hour < 10) // blank first minute digit when time is 04:00 --> 4:00
      updateDisplayedTime(hour_1);
    else
      updateDisplayedTime(false);
    if (debouncedButtonRead(0, 50))
      setupMode++;
    if (debouncedButtonRead(1, 50))
      hour = (hour + 1) % 24;
    if (debouncedButtonRead(2, 50))
    {
      if (hour > 0)
        hour--;
      else if (hour == 0)
        hour = 23;
    }
    calculateTime();
    debug("Set hours : ");
    debugln(hour);
    digitalWrite(hourLed, LOW);
  }
}

// menu page for changing minutes
void secondMenuPage()
{
  while (setupMode == 2)
  {
    digitalWrite(minuteLed, HIGH);
    if (hour < 10) // blank first minute digit when time is 04:00 --> 4:00
      updateDisplayedTime(hour_1);
    else
      updateDisplayedTime(false);
    if (debouncedButtonRead(0, 50))
      setupMode++;
    if (debouncedButtonRead(1, 50))
      minute = (minute + 1) % 60;
    if (debouncedButtonRead(2, 50))
    {
      if (minute > 0)
        minute--;
      else if (minute == 0)
        minute = 59;
    }
    calculateTime();
    debug("Set minutes : ");
    debugln(minute);
    digitalWrite(minuteLed, LOW);
  }
}

// last menu page that sets adjusted time in the RTC module
void lastMenuPage()
{
  rtc.adjust(DateTime(rtc.now().year(), rtc.now().month(), rtc.now().day(), hour, minute, 0));
  setupMode = 0;
}

/**
 * check if minute value has changed, and if it did, update displayed time
 * @param timeToPass cathodeRoutine will run every timeToPass (in minutes)
 */
void timeChange(int timeToPass)
{
  if (minuteChange != minute)
  {
    if (hour < 10) // blank first minute digit when time is 04:00 --> 4:00
      updateDisplayedTime(hour_1);
    else
      updateDisplayedTime(false);
    minuteChange = minute;
    minuteCounter++;

    // check if half an hour has passed and do CathodeRoutine
    if (minuteCounter == timeToPass)
    {
      debug(timeToPass);
      debugln(" minutes have passed, doing cathodeRoutine...");
      doCathodeRoutine(3000, 25);
      if (hour < 10) // blank first minute digit when time is 04:00 --> 4:00
        updateDisplayedTime(hour_1);
      else
        updateDisplayedTime(false);
      minuteCounter = 0;
    }
  }
}

void setup()
{
  // wait for rtc module to connect
  while (!rtc.begin())
    continue;

  for (int i = 0; i < number_of_buttons; i++)
    pinMode(button[i], INPUT_PULLUP);

  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(masterReset, OUTPUT);
  pinMode(hourLed, OUTPUT);
  pinMode(minuteLed, OUTPUT);
  pinMode(displayControlPin, OUTPUT);
  pinMode(sensorPin, INPUT);

  digitalWrite(masterReset, LOW);
  delayMicroseconds(10);
  digitalWrite(masterReset, HIGH);
  doCathodeRoutine(2000, 25);
  digitalWrite(displayControlPin, HIGH);

  // serial communication for debugging
  debug_begin(9600);
}

void loop()
{
  // get current time
  getCurrentTime();
  // check for time change
  timeChange(15);
  // check for motion
  motionDetection(60);
  // check for menu button press
  if (debouncedButtonRead(0, 50))
    setupMode++;

  switch (setupMode)
  {
  case 1:
    firstMenuPage();
  case 2:
    secondMenuPage();
  case 3:
    lastMenuPage();
    break;
  }
}