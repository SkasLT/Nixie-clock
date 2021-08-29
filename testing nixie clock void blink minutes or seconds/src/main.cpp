#include <Arduino.h>
#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;

const int latchPin = 9;     //Pin connected to RCK of TPIC6B595
const int masterReset = 10; //Pin connected to SRCLR of all TPIC6B595 IC-s
const int dataPin = 11;     //Pin connected to SERIAL IN of TPIC6B595
const int clockPin = 12;    //Pin connected to RCK of 7PIC6B595
int minuteChange = 100;     //set to 100 so that it's impossible for minute value to be same as minute change during startup
// variables for reading and diplaying time
int hour, minute, second;
int hour1, hour2, minute1, minute2;
//variables for selecting if we want to blink hour or minute digits
const int selectHours = 2;
const int selectMinutes = 4;
//we do this so we can display time on serial monitor
char *time = (char *)malloc(sizeof(char) * 9);
//function for shifting out n bits into the shift register, with their defined values
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
//this void updates displayed time
void updateDisplayedTime()
{
  digitalWrite(latchPin, LOW);
  shiftOutBits(dataPin, clockPin, LSBFIRST, minute2, 10);
  shiftOutBits(dataPin, clockPin, LSBFIRST, minute1, 10);
  shiftOutBits(dataPin, clockPin, LSBFIRST, hour2, 10);
  shiftOutBits(dataPin, clockPin, LSBFIRST, hour1, 10);
  digitalWrite(latchPin, HIGH);
}
/*
Function necessary for longevity of NIXIE tubes, it lights up every digit one after another and repeats it 1000 times, with frequency high enough for human eye not to notice.
This should be done as frequently as possible, but every half hour will do just fine
*/
void blinkDisplayedTime(int hoursOrMinutes, const unsigned long blinkDelay)
{
  unsigned long previousDelayTime = millis(); //used for blinkind minutes or sencods
  digitalWrite(latchPin, LOW);

  if (hoursOrMinutes == 2 && (millis() - previousDelayTime <= blinkDelay))
  {
    Serial.println(millis() - previousDelayTime);
    shiftOutBits(dataPin, clockPin, LSBFIRST, minute2, 10);
    shiftOutBits(dataPin, clockPin, LSBFIRST, minute1, 10);
    digitalWrite(dataPin, LOW);

    for (int i = 0; i < 20; i++)
    {
      digitalWrite(clockPin, HIGH);
      digitalWrite(clockPin, LOW);
    }
  }
  digitalWrite(latchPin, HIGH);
}

void doCathodeRoutine()
{
  unsigned long previousTime = millis();
  const unsigned long timeInterval = 5000; //how long will cathode routine run (in milliseconds)

  while ((millis() - previousTime) <= timeInterval)
  {
    for (int i = 0; i < 10; i++)
    {
      digitalWrite(latchPin, LOW);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10);
      shiftOutBits(dataPin, clockPin, LSBFIRST, i, 10);
      digitalWrite(latchPin, HIGH);
    }
  }
  Serial.println("cathode routine has completed");
}

void setup()
{
  //wait for rtc module to connect
  while (!rtc.begin())
  {
  }
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); //sets date and time to date and time the program was uploaded
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(masterReset, OUTPUT);
  //before proceeding we will reset all shift registers to make sure that there are no undefined previous states on their data inputs (all shift register reset pints are tied together)
  digitalWrite(masterReset, LOW);
  delayMicroseconds(10);
  digitalWrite(masterReset, HIGH);
  //doCathodeRoutine();
  //establish serial communication with boud rate of 9600 bits per second
  Serial.begin(9600);
}

void loop()
{

  DateTime now = rtc.now();
  //store values of hours and minutes in their respecitive varables
  hour = now.hour();
  minute = now.minute();
  //separate first and second digit of hour value
  hour1 = hour / 10;
  hour2 = hour % 10;
  //separate first and second digit of minute value
  minute1 = minute / 10;
  minute2 = minute % 10;
  //update displayed time every minute

  blinkDisplayedTime(selectHours, 200);
}