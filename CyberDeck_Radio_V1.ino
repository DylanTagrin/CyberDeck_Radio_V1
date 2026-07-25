#include <Wire.h>
#include <SparkFunSi4703.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>



// ---------- ESP32 / I2C pins ----------
constexpr int SDA_PIN = 21;
constexpr int SCL_PIN = 22;


// ---------- I2C addresses ----------
constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr uint8_t SI4703_ADDRESS = 0x10;


// ----------- Radio Pins --------
const int RADIO_SDIO_PIN  = 21;
const int RADIO_SCLK_PIN  = 22;
const int RADIO_RESET_PIN = 25;
// Do NOT wire anything to ESP32 GPIO3 just because of this.
const int STC_PIN = 3;

// ---------------- OLED ----------------
const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;
const int OLED_RESET = -1;
const uint8_t OLED_ADDR = 0x3C;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------------- Radio ----------------
Si4703_Breakout radio(RADIO_RESET_PIN, RADIO_SDIO_PIN, RADIO_SCLK_PIN, STC_PIN);

int channel = 1027;   // 102.7 MHz default.
int volume = 6;      // 0-15
char rdsBuffer[10];

String modeText = "Boot";
bool oledOK = false;
bool radioStarted = false;

unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL_MS = 5000;

// Serial command buffer
String inputLine = "";


// ---------------- Utility ----------------

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


// ---------- Serial/debug ----------

void printInfo() {
  Serial.print("Channel: ");
  Serial.print(channel / 10.0, 1);
  Serial.print(" MHz");

  Serial.print(" | Raw: ");
  Serial.print(channel);

  Serial.print(" | Volume: ");
  Serial.print(volume);

  Serial.print(" | Mode: ");
  Serial.println(modeText);
}


void showHelp() {
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  T1027  tune to 102.7 MHz");
  Serial.println("  T955   tune to 95.5 MHz");
  Serial.println("  u      seek up");
  Serial.println("  d      seek down");
  Serial.println("  +      volume up");
  Serial.println("  -      volume down");
  Serial.println("  r      read RDS");
  Serial.println("  h      help");
  Serial.println();
}



// ---------- Radio control ----------

void tuneToChannel(int newChannel) {

  if (newChannel < 875 || newChannel > 1080) {
    Serial.println("Invalid channel. Use 875 to 1080, e.g. T1027 for 102.7 MHz.");
    modeText = "Bad input";
    drawOLED();
    return;
  }

  channel = newChannel;
  radio.setChannel(channel);
  modeText = "Direct";

  printInfo();
  drawOLED();
}


void setRadioVolume(int newVolume) {
  if (newVolume < 0) newVolume = 0;
  if (newVolume > 15) newVolume = 15;

  volume = newVolume;
  radio.setVolume(volume);
  modeText = "Volume";

  printInfo();
  drawOLED();
}



void seekUp() {
  modeText = "Seek up";
  drawOLED();

  channel = radio.seekUp();

  modeText = "Seek up";
  printInfo();
  drawOLED();
}


void seekDown() {
  modeText = "Seek down";
  drawOLED();

  channel = radio.seekDown();

  modeText = "Seek down";
  printInfo();
  drawOLED();
}


void readRDS() {
  Serial.println("RDS listening for up to 15 seconds...");
  modeText = "RDS wait";
  drawOLED();

  radio.readRDS(rdsBuffer, 15000);

  Serial.print("RDS heard: ");
  Serial.println(rdsBuffer);

  modeText = "RDS done";
  drawOLED();
}


// ---------- Command parsing ----------

void processCommand(String cmd) {
  cmd.trim();

  if (cmd.length() == 0) {
    return;
  }

  Serial.print("Command received: ");
  Serial.println(cmd);

  char command = cmd.charAt(0);

  if (command == 'T' || command == 't') {
    String numberPart = cmd.substring(1);
    numberPart.trim();

    int requestedChannel = numberPart.toInt();

    if (requestedChannel == 0) {
      Serial.println("Bad tune command. Example: T1027 for 102.7 MHz.");
      modeText = "Bad cmd";
      drawOLED();
      return;
    }

    tuneToChannel(requestedChannel);
  }

  else if (command == 'u') {
    seekUp();
  }

  else if (command == 'd') {
    seekDown();
  }

  else if (command == '+') {
    setRadioVolume(volume + 1);
  }

  else if (command == '-') {
    setRadioVolume(volume - 1);
  }

  else if (command == 'r') {
    readRDS();
  }

  else if (command == 'h') {
    showHelp();
    modeText = "Help";
    drawOLED();
  }

  else {
    Serial.println("Unknown command. Type h for help.");
    modeText = "Unknown";
    drawOLED();
  }
}


void readSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\r') {
      // Ignore carriage return.
      continue;
    }

    if (c == '\n') {
      processCommand(inputLine);
      inputLine = "";
    } else {
      inputLine += c;
    }
  }
}



// ---------------- Arduino setup/loop ----------------
// ---------- Arduino setup/loop ----------

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("ESP32 + Si4703 + OLED Radio Test");
  Serial.println("================================");

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);

  if (oledOK) {
    modeText = "OLED OK";
    drawOLED();
  } else {
    Serial.println("OLED failed to initialize.");
  }

  Serial.println("Powering on radio...");
  modeText = "Radio boot";
  drawOLED();

  radio.powerOn();

  volume = 6;
  radio.setVolume(volume);

  radio.setChannel(channel);
  modeText = "Direct";

  showHelp();
  printInfo();
  drawOLED();
}


void loop() {
  readSerialCommands();

  // Slow runtime refresh only.
  // This reduces digital ticking compared to updating the OLED every 250 ms.
  if (millis() - lastDisplayUpdate >= DISPLAY_INTERVAL_MS) {
    lastDisplayUpdate = millis();
    drawOLED();
  }
}