#include <SparkFunSi4703.h>
#include <Wire.h>

int resetPin = 25;
int SDIO = 21;
int SCLK = 22;
int STC = 3;

Si4703_Breakout radio(resetPin, SDIO, SCLK, STC);

int channel = 889; // default: 88.9 MHz, change this
int volume = 5;
char rdsBuffer[10];

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\nSi4703 ESP32 Radio Test");
  Serial.println("=======================");
  Serial.println("Commands:");
  Serial.println("  T889   tune to 88.9 MHz");
  Serial.println("  T1011  tune to 101.1 MHz");
  Serial.println("  u      seek up");
  Serial.println("  d      seek down");
  Serial.println("  +      volume up");
  Serial.println("  -      volume down");
  Serial.println("  r      read RDS");

  radio.powerOn();

  volume = 5;
  radio.setVolume(volume);

  radio.setChannel(channel);
  displayInfo();
}

void loop()
{
  if (Serial.available())
  {
    char ch = Serial.read();

    if (ch == 'T' || ch == 't')
    {
      // Parse the number after T, e.g. T955 = 95.5 MHz
      int requestedChannel = Serial.parseInt();

      if (requestedChannel >= 875 && requestedChannel <= 1080)
      {
        channel = requestedChannel;
        radio.setChannel(channel);
        displayInfo();
      }
      else
      {
        Serial.println("Invalid channel. Use 875 to 1080, e.g. T955 for 95.5 MHz.");
      }
    }
    else if (ch == 'u') 
    {
      channel = radio.seekUp();
      displayInfo();
    } 
    else if (ch == 'd') 
    {
      channel = radio.seekDown();
      displayInfo();
    } 
    else if (ch == '+') 
    {
      volume++;
      if (volume > 15) volume = 15;
      radio.setVolume(volume);
      displayInfo();
    } 
    else if (ch == '-') 
    {
      volume--;
      if (volume < 0) volume = 0;
      radio.setVolume(volume);
      displayInfo();
    } 
    else if (ch == 'r')
    {
      Serial.println("RDS listening...");
      radio.readRDS(rdsBuffer, 15000);
      Serial.print("RDS heard: ");
      Serial.println(rdsBuffer);      
    }
  }
}

void displayInfo()
{
  Serial.print("Channel: ");
  Serial.print(channel / 10.0, 1);
  Serial.print(" MHz");

  Serial.print(" | Raw channel: ");
  Serial.print(channel);

  Serial.print(" | Volume: ");
  Serial.println(volume);
}