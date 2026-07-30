#include <Wire.h>
#include <SparkFunSi4703.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>



// ---------- I2C addresses ----------
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint8_t SI4703_ADDRESS = 0x10;

// ----------- Radio ---------
int resetPin = 25;
int SDIO = 21;
int SCLK = 22;
int STC = 26;
Si4703_Breakout radio(resetPin, SDIO, SCLK, STC);

int channel = 1037;  // default: 100.5 MHz
int volume = 5;
char rdsBuffer[10];

// ---------------- OLED ----------------
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int OLED_RESET = -1;
// OLED second I2C bus
TwoWire OLEDWire = TwoWire(1);
const int OLED_SDA = 16;
const int OLED_SCL = 17;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &OLEDWire, OLED_RESET);

// ----- Initial Vars -----
String modeText = "Boot";
bool oledOK = false;
bool radioStarted = false;



void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();

  // Default Wire bus for radio
  Wire.begin(SDIO, SCLK);
  Wire.setClock(100000);

  // Separate OLED bus
  OLEDWire.begin(OLED_SDA, OLED_SCL);
  OLEDWire.setClock(100000);
  scanOLEDBus();
  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS);
  if (oledOK) {
    modeText = "OLED OK";
    drawOLED();
    Serial.println("OLED succeeded to initialize.");
  } else {
    Serial.println("OLED failed to initialize.");
  }




  Serial.println("\n\nSi4703 ESP32 Radio Test");
  Serial.println("=======================");
  Serial.println("Commands:");
  Serial.println("  T937   tune to 937 MHz");
  Serial.println("  T1005  tune to 100.5 MHz");
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

void loop() {
  if (Serial.available()) {
    char ch = Serial.read();

    if (ch == 'T' || ch == 't') {
      // Parse the number after T, e.g. T955 = 95.5 MHz
      int requestedChannel = Serial.parseInt();

      if (requestedChannel >= 875 && requestedChannel <= 1080) {
        channel = requestedChannel;
        radio.setChannel(channel);
        displayInfo();
        drawOLED();
      } else {
        Serial.println("Invalid channel. Use 875 to 1080, e.g. T955 for 95.5 MHz.");
      }
    } else if (ch == 'u') {
      channel = radio.seekUp();
      displayInfo();
    } else if (ch == 'd') {
      channel = radio.seekDown();
      displayInfo();
    } else if (ch == '+') {
      volume++;
      if (volume > 15) volume = 15;
      radio.setVolume(volume);
      displayInfo();
      drawOLED();
    } else if (ch == '-') {
      volume--;
      if (volume < 0) volume = 0;
      radio.setVolume(volume);
      displayInfo();
      drawOLED();
    } else if (ch == 'r') {
      Serial.println("RDS listening...");
      radio.readRDS(rdsBuffer, 15000);
      Serial.print("RDS heard: ");
      Serial.println(rdsBuffer);
    }
  }
}


// --------- Functions -------

void drawOLED() {
  if (!oledOK) return;

  unsigned long totalSeconds = millis() / 1000;
  unsigned long hours = totalSeconds / 3600;
  unsigned long minutes = (totalSeconds % 3600) / 60;
  unsigned long seconds = totalSeconds % 60;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("FM RADIO");

  display.setCursor(0, 13);
  display.print("Freq: ");
  display.print(channel / 10.0, 1);
  display.println(" MHz");

  display.setCursor(0, 25);
  display.print("Volume: ");
  display.print(volume);
  display.println(" / 15");

  display.setCursor(0, 37);
  display.print("Mode: ");
  display.println(modeText);

  display.setCursor(0, 49);
  display.print("Run: ");

  if (hours < 10) display.print("0");
  display.print(hours);
  display.print(":");

  if (minutes < 10) display.print("0");
  display.print(minutes);
  display.print(":");

  if (seconds < 10) display.print("0");
  display.print(seconds);

  display.display();
}


void scanOLEDBus() {
  Serial.println("Scanning OLEDWire bus...");

  int found = 0;

  for (uint8_t addr = 1; addr < 127; addr++) {
    OLEDWire.beginTransmission(addr);
    uint8_t error = OLEDWire.endTransmission();

    if (error == 0) {
      Serial.print("Found device at 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }

  if (found == 0) {
    Serial.println("No devices found on OLEDWire.");
  }
}

void displayInfo() {
  Serial.print("Channel: ");
  Serial.print(channel / 10.0, 1);
  Serial.print(" MHz");

  Serial.print(" | Raw channel: ");
  Serial.print(channel);

  Serial.print(" | Volume: ");
  Serial.println(volume);
}