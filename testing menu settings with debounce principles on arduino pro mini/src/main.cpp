#include <Arduino.h>
#define number_of_buttons 2

int button[number_of_buttons] = {7, 2};
int buttonState[number_of_buttons] = {1, 1};
int lastButtonState[number_of_buttons] = {1, 1};
int setupMode = 0;
int counter = 0;

bool debouncedButtonRead(int buttonIndex, const unsigned long debounceDelay)
{
  int reading = digitalRead(button[buttonIndex]);
  unsigned long previousTime;

  if (reading != lastButtonState[buttonIndex])
    previousTime = millis();

  if ((millis() - previousTime) > debounceDelay)
  {

    if (reading != buttonState[buttonIndex])
    {
      buttonState[buttonIndex] = reading;

      if (buttonState[buttonIndex] == LOW)
        return true;
    }
  }
  lastButtonState[buttonIndex] = reading;
  return false;
}

void setup()
{
  for (int i = 0; i < number_of_buttons; i++)
    pinMode(button[i], INPUT_PULLUP);

  Serial.begin(9600);
}

void loop()
{
  if (debouncedButtonRead(0, 50))
    setupMode++;
  Serial.println(setupMode);

  switch (setupMode)
  {
  case 1:
    while (setupMode == 1)
    {
      if (debouncedButtonRead(0, 50))
        setupMode++;
      if (debouncedButtonRead(1, 50))
        counter++;
      Serial.print("1 : ");
      Serial.println(counter);
    }
    break;
  case 2:
    counter = 0;
    while (setupMode == 2)
    {
      if (debouncedButtonRead(0, 50))
        setupMode++;
      if (debouncedButtonRead(1, 50))
        counter++;
      Serial.print("2 : ");
      Serial.println(counter);
    }
    break;
  case 3:
    counter = 0;
    while (setupMode == 3)
    {
      if (debouncedButtonRead(0, 50))
        setupMode++;
      if (debouncedButtonRead(1, 50))
        counter++;
      Serial.print("3 : ");
      Serial.println(counter);
    }
    break;
  default:
    setupMode = 0;
  }
}