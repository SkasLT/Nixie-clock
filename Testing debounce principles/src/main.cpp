#include <Arduino.h>

const int buttonPin1 = 7;
const int buttonPin2 = 2;
int buttonState1;
int buttonState2;
int lastButtonState1 = HIGH;
int lastButtonState2 = HIGH;
unsigned long previousTime1;
unsigned long previousTime2;
int counter1 = 0;
int counter2 = 0;

void debouncedButton1Read(int pin, const unsigned long debounceDelay)
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
        counter1++;
    }
  }
  lastButtonState1 = reading1;
}

void debouncedButton2Read(int pin, const unsigned long debounceDelay)
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
        counter2++;
    }
  }
  lastButtonState2 = reading2;
}

void setup()
{
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  Serial.begin(9600);
}

void loop()
{
  debouncedButton1Read(buttonPin1, 50);
  debouncedButton2Read(buttonPin2, 50);
  Serial.print("Counter 1: ");
  Serial.print(counter1);
  Serial.print("\tcounter 2: ");
  Serial.println(counter2);
}