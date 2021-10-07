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

//function for shifting out n bits into the storage register, with their defined values
void shiftOutBits(int dataPin, int clockPin, int bitOrder, int value, int bits)
{
  for (int i = 0; i < bits; i++) // shift out n bits
  {
    if (bitOrder == LSBFIRST) //least significant bit first
    {
      digitalWrite(dataPin, (i == value)); //if increment value is same as needed value, set the dataPin HIGH
      delayMicroseconds(10);               //delay for short time
    }
    else //most significant bit first
    {
      digitalWrite(dataPin, (bits - 1 - i) == value); //if increment value is same as needed, value set the dataPin HIGH
      delayMicroseconds(10);                          //delay for short time
    }
    digitalWrite(clockPin, HIGH); //pulse the clock pin in order to store data in storage register
    digitalWrite(clockPin, LOW);
  }
}

//this function updates displayed time
void updateDisplayedTime()
{
  digitalWrite(latchPin, LOW);                            //set the latch pin LOW when shifting data to the storage register
  shiftOutBits(dataPin, clockPin, LSBFIRST, minute2, 10); //shift out minute2 value
  shiftOutBits(dataPin, clockPin, LSBFIRST, minute1, 10); //shift out minute1 value
  shiftOutBits(dataPin, clockPin, LSBFIRST, hour2, 10);   //shift out hour2 value
  shiftOutBits(dataPin, clockPin, LSBFIRST, hour1, 10);   //shift out hour1 value
  digitalWrite(latchPin, HIGH);                           //set the latch pin HIGH in order to display data on shift register outputs
}

//function for calculating first and last hour digit, first and last minute digit
void calculateTime()
{
  //separate first and second digit of hour value
  hour1 = hour / 10;
  hour2 = hour % 10;

  //separate first and second digit of minute value
  minute1 = minute / 10;
  minute2 = minute % 10;
}

/*
Function necessary for longevity of NIXIE tubes, it lights up every digit one after another and repeats it for a certain ammount of time, with frequency high enough for human eye not to notice.
This should be done as frequently as possible, but every half hour will do just fine
*/
void doCathodeRoutine(const unsigned long timeInterval) //timeInterval --> how long will cathode routine run (in milliseconds)
{
  unsigned long previousTime = millis(); //local variable for storing the starting time of cathode routine interval

  while ((millis() - previousTime) <= timeInterval) //do cathode routine until some time has passed (timeInterval)
  {
    for (int i = 0; i < 10; i++) //loop runs 10 times since every NIXIE tube has 10 digits (0...9)
    {
      digitalWrite(latchPin, LOW);                      //set the latch pin LOW when shifting data to the storage register
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10); //shifts out 10 bits while lighting up the corresponding digit once; in this case minute2 digit
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10); //shifts out 10 bits while lighting up the corresponding digit once; in this case minute1 digit
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10); //shifts out 10 bits while lighting up the corresponding digit once; in this case hour2 digit
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10); //shifts out 10 bits while lighting up the corresponding digit once; in this case hour1 digit
      digitalWrite(latchPin, HIGH);                     //set the latch pin HIGH in order to display data on shift register outputs
    }
  }
  debugln("cathode routine has completed");
}

//function that calculates current minutes, hours, seconds and their respective digits
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
    digitalWrite(hourLed, HIGH);    //turn on hourLed when setting up hours
    updateDisplayedTime();          //evey time hour value is changed update the displayed time
    if (debouncedButtonRead(0, 50)) //if setup button is pressed go to the next menu page
      setupMode++;
    if (debouncedButtonRead(1, 50)) //increment hour value when increment button is pressed
      hour = (hour + 1) % 24;       //make sure that hour value overflows when it reaches 24 and it goes back to 0
    calculateTime();                //calculate hour and minute digit values:
    debug("Set hours : ");          //print out current menu page
    debugln(hour);                  //print out hour value on serial monitor
    digitalWrite(hourLed, LOW);     //turn off hourLed when done with setting up hours
  }
}

//menu page for changing minutes
void secondMenuPage()
{
  while (setupMode == 2)
  {
    digitalWrite(minuteLed, HIGH);  //turn on minuteLed when setting up minutes
    updateDisplayedTime();          //evey time minute value is changed update the displayed time
    if (debouncedButtonRead(0, 50)) //if setup button is pressed go to the next menu page
      setupMode++;
    if (debouncedButtonRead(1, 50)) //increment minute value when increment button is pressed
      minute = (minute + 1) % 60;   //make sure that minute value overflows when it reaches 60 and it goes back to 0
    calculateTime();                //calculate hour and minute digit values:
    debug("Set minutes : ");        //print out current menu page
    debugln(minute);                //print out hour value on serial monitor
    digitalWrite(minuteLed, LOW);   //turn off minuteLed when done setting up minutes
  }
}

//last menu page that sets adjusted time in the RTC module
void lastMenuPage()
{
  rtc.adjust(DateTime(rtc.now().year(), rtc.now().month(), rtc.now().day(), hour, minute, 0)); //set adjusted time in RTC module
  setupMode = 0;                                                                               //set setupMode to 0; exits menu
}

void setup()
{
  //wait for rtc module to connect
  while (!rtc.begin())
    continue;

  //Since we can have alot of button pins, it is most effective to use a for loop that sets the pinMode of button pins.
  for (int i = 0; i < number_of_buttons; i++)
    pinMode(button[i], INPUT_PULLUP);

  pinMode(latchPin, OUTPUT);    //set latch pin as output
  pinMode(clockPin, OUTPUT);    //set clock pin as output
  pinMode(dataPin, OUTPUT);     //set data pin as output
  pinMode(masterReset, OUTPUT); //set master reset pin as output
  pinMode(hourLed, OUTPUT);     //set hourLed pin as output
  pinMode(minuteLed, OUTPUT);   //set minuteLed pin as output

  //before proceeding we will reset all shift registers to make sure that there are no undefined previous states on their data inputs (all shift register reset pints are tied together)
  digitalWrite(masterReset, LOW);
  delayMicroseconds(10);
  digitalWrite(masterReset, HIGH);
  doCathodeRoutine(5000); //when powering the clock for the first time do cathode routine

  //establish serial communication with baud rate of 9600
  debug_begin(9600);
}

void loop()
{
  //get current time
  getCurrentTime();

  //constantly check for setupButton state
  if (debouncedButtonRead(0, 50))
    setupMode++;

  switch (setupMode) //menu for changing hours and minutes, can be expanded by adding aditional cases
  {
  case 1:
    firstMenuPage(); //menu page for changing hours
  case 2:
    secondMenuPage(); //menu page for changing minutes
  case 3:
    lastMenuPage(); //last menu page that sets adjusted time in the RTC module
    break;
  }

  //chech if minute value has changed, and if it did, update displayed time
  if (minuteChange != minute)
  {
    updateDisplayedTime();
    minuteChange = minute; //save the current minute value
    minuteCounter++;       //add to minuteCounter to keep track of time (half hour)

    //check if half an hour has passed and do CathodeRoutine
    if (minuteCounter == 30)
    {
      debugln("Half an hour has passed, doing cathodeRoutine...");
      doCathodeRoutine(5000);
      minuteCounter = 0; //reset minuteCounter until next half an hour has passed
    }
  }
}