#include <Arduino.h>

const int buttonPin = 7;     //implemented
const int incrementPin = 2;  //implemented
int buttonState1;            //implemented   setupButtonState
int buttonState2;            //implemented   incrementButtonState
int lastButtonState1 = HIGH; //implemented   lastSetupButtonState
int lastButtonState2 = HIGH; //implemented   lastIncrementButtonState
unsigned long previousTime1; //implemented   previousSetupButtonTime
unsigned long previousTime2; //implemented   previousIncrementButtonTime
int setupMode = 0;           //implemented
int counter = 0;

void debouncedButton1Read(int pin, const unsigned long debounceDelay) //implemented
{
  int reading1 = digitalRead(pin);

  if (reading1 != lastButtonState1)
    previousTime1 = millis();

  if ((millis() - previousTime1) > debounceDelay)
  {

    if (reading1 != buttonState1)
    {
      buttonState1 = reading1;

      if (buttonState1 == LOW)
        setupMode++;
    }
  }
  lastButtonState1 = reading1;
}

void debouncedButton2Read(int pin, const unsigned long debounceDelay) //implemented
{
  int reading2 = digitalRead(pin);

  if (reading2 != lastButtonState2)
    previousTime2 = millis();

  if ((millis() - previousTime2) > debounceDelay)
  {

    if (reading2 != buttonState2)
    {
      buttonState2 = reading2;

      if (buttonState2 == LOW)
        counter++;
    }
  }
  lastButtonState2 = reading2;
}

void setup()
{
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(incrementPin, INPUT_PULLUP);
  Serial.begin(9600);
}

void loop()
{
  debouncedButton1Read(buttonPin, 50);
  Serial.println(setupMode);

  switch (setupMode)
  {
  case 1:
    while (setupMode == 1)
    {
      debouncedButton1Read(buttonPin, 50);
      debouncedButton2Read(incrementPin, 50);
      Serial.print("1 : ");
      Serial.println(counter);
    }
    break;
  case 2:
    counter = 0;
    while (setupMode == 2)
    {
      debouncedButton1Read(buttonPin, 50);
      debouncedButton2Read(incrementPin, 50);
      Serial.print("2 : ");
      Serial.println(counter);
    }
    break;
  case 3:
    counter = 0;
    while (setupMode == 3)
    {
      debouncedButton1Read(buttonPin, 50);
      debouncedButton2Read(incrementPin, 50);
      Serial.print("3 : ");
      Serial.println(counter);
    }
    break;
  default:
    setupMode = 0;
  }
}