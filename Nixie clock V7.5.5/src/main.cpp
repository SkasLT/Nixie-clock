#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

//debugging
#define DEBUG 0 //choose to debug or not; 1 is debugging 0 is not

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#define debug_begin(x) Serial.begin(x)
#else
#define debug(x)
#define debugln(x)
#define debug_begin(x)
#endif

//Control variables:
const int number_of_buttons = 2;                 //number of buttons connected
const int button[number_of_buttons] = {7, 2};    //array that stores button pins
int buttonState[number_of_buttons] = {1, 1};     //array that stores initial button states
int lastButtonState[number_of_buttons] = {1, 1}; //array that stores last button states
int setupMode = 0;                               //variable which saves the current value of setup mode

//Variables for controling shift registers and indicator leds:
const int latchPin = 9;     //Pin connected to RCK of TPIC6B595
const int masterReset = 10; //Pin connected to SRCLR of all TPIC6B595 IC-s
const int dataPin = 11;     //Pin connected to SERIAL IN of TPIC6B595
const int clockPin = 12;    //Pin connected to SRCK of TPIC6B595
const int hourLed = 4;      //Pin connected a led that will light up when adjusting hours
const int minuteLed = 5;    //Pin connected a led that will light up whem adjusting minutes
int hourLedState = HIGH;    //variable that stores current hourLed state
int minuteLedState = HIGH;  //variable that stores current minuteLed state

//Timing variables:
int minuteCounter = -1;             //used to determine when half an hour has passed
int minuteChange = 100;             //set to 100 so that it's impossible for minute value to be same as minute change during startup
int hour, minute, second;           //variables for storing hour values, minute values and second values
int hour1, hour2, minute1, minute2; //variables for storing each digit for hours and minutes

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

  //Local variable used to store time between button pin state changes:
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

      if (buttonState[buttonIndex] == LOW) //only return true if buttin state is LOW, change this to HIGH if you have pulldown resistors on button pins
        return true;
    }
  }

  // Save the reading. Next time through the loop, it'll be the lastButtonState:
  lastButtonState[buttonIndex] = reading;
  return false;
}

/**
* function for shifting out n bits into the storage register, with their defined values
* @param dataPin pin connected to serial data input for shift registers
* @param clockPin clock pin connected to clock input for shift registers
* @param bitOrder order of bits (LSB, MSB)
* @param value sequential bit number to be shifted out
* @param bits number of bits to shift out
*/
void shiftOutBits(int dataPin, int clockPin, int bitOrder, int value, int bits)
{
  for (int i = 0; i < bits; i++)
  {
    if (bitOrder == LSBFIRST)
    {
      digitalWrite(dataPin, (i == value));
      delayMicroseconds(10);
    }
    else
    {
      digitalWrite(dataPin, (bits - 1 - i) == value);
      delayMicroseconds(10);
    }
    digitalWrite(clockPin, HIGH);
    digitalWrite(clockPin, LOW);
  }
}

//This function updates displayed time
void updateDisplayedTime()
{
  digitalWrite(latchPin, LOW);
  shiftOutBits(dataPin, clockPin, LSBFIRST, minute2, 10);
  shiftOutBits(dataPin, clockPin, LSBFIRST, minute1, 10);
  shiftOutBits(dataPin, clockPin, LSBFIRST, hour2, 10);
  shiftOutBits(dataPin, clockPin, LSBFIRST, hour1, 10);
  digitalWrite(latchPin, HIGH);
}

//Function for calculating first and last hour digit, first and last minute digit
void calculateTime()
{
  //separate first and second digit of hour value
  hour1 = hour / 10;
  hour2 = hour % 10;

  //separate first and second digit of minute value
  minute1 = minute / 10;
  minute2 = minute % 10;
}

/**
* Function necessary for longevity of NIXIE tubes, it lights up every digit one after another and repeats it for a certain ammount of time, with frequency high enough for human eye not to notice.
* This should be done as frequently as possible, but every half hour will do just fine
* @param timeInterval how long will cathode routine run (in milliseconds)
* @param timeDelay time between digit changes (in milliseconds)
*/
void doCathodeRoutine(const unsigned long timeInterval, const unsigned long timeDelay)
{
  unsigned long previousTime = millis();

  while ((millis() - previousTime) <= timeInterval)
  {
    for (int i = 0; i < 10; i++) //loop runs 10 times since every NIXIE tube has 10 digits (0...9)
    {
      digitalWrite(latchPin, LOW);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10);
      digitalWrite(latchPin, HIGH);
      delay(timeDelay);
    }
    for (int i = 8; i > 0; i--) //loop runs 10 times since every NIXIE tube has 10 digits (9...0)
    {
      digitalWrite(latchPin, LOW);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10);
      digitalWrite(latchPin, HIGH);
      delay(timeDelay);
    }
  }
  debugln("cathode routine has completed");
}

//unction that calculates current minutes, hours, seconds and their respective digits
void getCurrentTime()
{
  DateTime now = rtc.now();

  //store values of hours and minutes in their respecitive varables
  hour = now.hour();
  minute = now.minute();

  //calculate hour and minute digit values
  calculateTime();

  //print out time from rtc module on seral monitor
  if (second != now.second() && DEBUG == 1)
  {
    char buffer[10];
    sprintf(buffer, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
    debugln(buffer);
    second = now.second();
  }
}

//menu page for changing hours
void firstMenuPage()
{
  while (setupMode == 1)
  {
    digitalWrite(hourLed, HIGH);
    updateDisplayedTime();
    if (debouncedButtonRead(0, 50))
      setupMode++;
    if (debouncedButtonRead(1, 50))
      hour = (hour + 1) % 24;
    calculateTime();
    debug("Set hours : ");
    debugln(hour);
    digitalWrite(hourLed, LOW);
  }
}

//menu page for changing minutes
void secondMenuPage()
{
  while (setupMode == 2)
  {
    digitalWrite(minuteLed, HIGH);
    updateDisplayedTime();
    if (debouncedButtonRead(0, 50))
      setupMode++;
    if (debouncedButtonRead(1, 50))
      minute = (minute + 1) % 60;
    calculateTime();
    debug("Set minutes : ");
    debugln(minute);
    digitalWrite(minuteLed, LOW);
  }
}

//last menu page that sets adjusted time in the RTC module
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
    updateDisplayedTime();
    minuteChange = minute;
    minuteCounter++;

    //check if half an hour has passed and do CathodeRoutine
    if (minuteCounter == timeToPass)
    {
      debugln("Half an hour has passed, doing cathodeRoutine...");
      doCathodeRoutine(3000, 25);
      minuteCounter = 0;
    }
  }
}

void setup()
{
  //wait for rtc module to connect
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

  digitalWrite(masterReset, LOW);
  delayMicroseconds(10);
  digitalWrite(masterReset, HIGH);
  doCathodeRoutine(2000, 25);

  debug_begin(9600);
}

void loop()
{
  //get current time
  getCurrentTime();
  //check for time change
  timeChange(15);

  //constantly check for setupButton state
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