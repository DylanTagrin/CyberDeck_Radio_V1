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


// ---------------- Controls ----------------
const int ENC_A = 32;
const int ENC_B = 33;
const int ENC_SW = 27;

const int VOL_POT = 34;  // input-only ADC pin, good for potentiometer

// ---------------- Encoder state ----------------
int lastEncA = HIGH;
unsigned long lastEncoderMoveMs = 0;
const unsigned long encoderDebounceMs = 3;

// ---------------- Button debounce ----------------
int lastButtonState = HIGH;
unsigned long lastButtonPressMs = 0;
const unsigned long buttonDebounceMs = 250;

// ---------------- Potentiometer smoothing ----------------
int lastVolumeFromPot = -1;
unsigned long lastPotReadMs = 0;
const unsigned long potReadIntervalMs = 80;




void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("ESP32 Si4703 Radio Interface");
  Serial.println("============================");

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

  // Controls
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);

  // ADC setup for ESP32
  analogReadResolution(12);  // 0-4095
  analogSetPinAttenuation(VOL_POT, ADC_11db);

  lastEncA = digitalRead(ENC_A);


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

  showHelp();
  displayInfo();
}

void loop()
{
  handleSerial();
  handleEncoder();
  handleEncoderButton();
  handleVolumePot();
  displayInfo();
}

// ---------------- Serial controls ----------------

void handleSerial()
{
  if (Serial.available())
  {
    char ch = Serial.read();

    if (ch == 'T' || ch == 't')
    {
      int requestedChannel = Serial.parseInt();

      if (requestedChannel >= 875 && requestedChannel <= 1080)
      {
        setChannelDirect(requestedChannel, "Serial");
      }
      else
      {
        Serial.println("Invalid channel. Use 875 to 1080, e.g. T955 for 95.5 MHz.");
        modeText = "Bad input";
        drawOLED();
      }
    }
    else if (ch == 'u')
    {
      seekUp();
    }
    else if (ch == 'd')
    {
      seekDown();
    }
    else if (ch == '+')
    {
      setVolumeDirect(volume + 1, "Serial Vol+");
    }
    else if (ch == '-')
    {
      setVolumeDirect(volume - 1, "Serial Vol-");
    }
    else if (ch == 'r')
    {
      readRDS();
    }
    else if (ch == 'h')
    {
      showHelp();
    }
  }
}


// ---------------- Encoder frequency control ----------------

void handleEncoder()
{
  int encA = digitalRead(ENC_A);

  // Detect a falling edge on A.
  if (lastEncA == HIGH && encA == LOW)
  {
    if (millis() - lastEncoderMoveMs >= encoderDebounceMs)
    {
      lastEncoderMoveMs = millis();

      int encB = digitalRead(ENC_B);

      if (encB == HIGH)
      {
        // One direction
        setChannelDirect(channel + 1, "Tune +");
      }
      else
      {
        // Other direction
        setChannelDirect(channel - 1, "Tune -");
      }
    }
  }

  lastEncA = encA;
}


// ---------------- Encoder pushbutton ----------------

void handleEncoderButton()
{
  int buttonState = digitalRead(ENC_SW);

  if (lastButtonState == HIGH && buttonState == LOW)
  {
    if (millis() - lastButtonPressMs >= buttonDebounceMs)
    {
      lastButtonPressMs = millis();

      // Button action: seek up
      seekUp();
    }
  }

  lastButtonState = buttonState;
}


// ---------------- Potentiometer volume control ----------------

void handleVolumePot()
{
  if (millis() - lastPotReadMs < potReadIntervalMs)
  {
    return;
  }

  lastPotReadMs = millis();

  int raw = analogRead(VOL_POT);  // 0-4095

  // Convert 0-4095 to volume 0-15.
  int newVolume = map(raw, 0, 4095, 0, 15);

  if (newVolume < 0) newVolume = 0;
  if (newVolume > 15) newVolume = 15;

  // Only update if the value actually changed.
  // This prevents tiny ADC jitter from constantly sending volume commands.
  if (newVolume != lastVolumeFromPot)
  {
    lastVolumeFromPot = newVolume;

    if (newVolume != volume)
    {
      setVolumeDirect(newVolume, "Pot Vol");
    }
  }
}


// --------- Radio Command Functions -----------

void setChannelDirect(int newChannel, String newMode)
{
  if (newChannel > 1080) newChannel = 875;
  if (newChannel < 875) newChannel = 1080;

  channel = newChannel;
  modeText = newMode;

  radio.setChannel(channel);
  displayInfo();
}


void setVolumeDirect(int newVolume, String newMode)
{
  if (newVolume < 0) newVolume = 0;
  if (newVolume > 15) newVolume = 15;

  volume = newVolume;
  modeText = newMode;

  radio.setVolume(volume);
  displayInfo();
}


void seekUp()
{
  modeText = "Seek up";
  drawOLED();

  channel = radio.seekUp();

  modeText = "Seek up";
  displayInfo();
}


void seekDown()
{
  modeText = "Seek down";
  drawOLED();

  channel = radio.seekDown();

  modeText = "Seek down";
  displayInfo();
}


void readRDS()
{
  Serial.println("RDS listening...");
  modeText = "RDS wait";
  drawOLED();

  radio.readRDS(rdsBuffer, 15000);

  Serial.print("RDS heard: ");
  Serial.println(rdsBuffer);

  modeText = "RDS done";
  drawOLED();
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


void showHelp() {
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  T1037  tune to 103.7 MHz");
  Serial.println("  T1005  tune to 100.5 MHz");
  Serial.println("  u      seek up");
  Serial.println("  d      seek down");
  Serial.println("  +      volume up");
  Serial.println("  -      volume down");
  Serial.println("  r      read RDS");
  Serial.println("  h      help");
  Serial.println();
}