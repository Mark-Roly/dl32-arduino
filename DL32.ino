/*

  DL32 Aduino by Mark Booth
  For use with Wemos S3 and DL32 S3 hardware rev 20240812 or later
  Last updated 24/05/2025
  https://github.com/Mark-Roly/dl32-arduino

  Board Profile: ESP32S3 Dev Module
  Upload settings:
    USB CDC on Boot: Enabled
    CPU frequency: 240Mhz (WiFi)
    USB DFU on boot: Disabled
    USB Firmware MSC on boot: Disabled
    Partition Scheme: Custom (partitions.csv provided on Github)
    Upload Mode: UART0/Hardware CDC
    Upload speed: 921600
    USB Mode: Hardware CDC and JTAG   
  
  DIP Switch settings:
    DS01 = Offline mode
    DS02 = FailSafe Strike mode
    DS03 = Silent mode
    DS04 = Garage mode

  SD card pins:
    CD DAT3 CS 34
    CMD DI DIN MOSI 36
    CLK SCLK 38
    DAT0 D0 MISO 35

*/

#define codeVersion 20250524
#define ARDUINOJSON_ENABLE_COMMENTS 1

// Include Libraries
#include <Arduino.h>            // Arduino by Arduino https://github.com/arduino/ArduinoCore-avr/blob/master/cores/arduino
#include <WiFi.h>               // WiFi by Ivan Grokhotkov https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFi
#include <WebServer.h>          // WebServer by Ivan Grokhotkov https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer
#include <SPI.h>                // SPI by Hristo Gochkov https://github.com/espressif/arduino-esp32/blob/master/libraries/SPI
#include <PubSubClient.h>       // PubSubClient by Nick O'leary https://github.com/knolleary/pubsubclient
#include <Adafruit_NeoPixel.h>  // Adafruit_NeoPixel by Adafruit https://github.com/adafruit/Adafruit_NeoPixel
#include <Wiegand.h>            // YetAnotherArduinoWiegandLibrary by paula-raca https://github.com/paulo-raca/YetAnotherArduinoWiegandLibrary
#include <ArduinoJson.h>        // ArduinoJSON by Benoit Blanchon https://arduinojson.org/?utm_source=meta&utm_medium=library.properties
#include <Ticker.h>             // Ticker by Bert Melis https://github.com/espressif/arduino-esp32/blob/master/libraries/Ticker
#include <uri/UriRegex.h>       // UriRegex by espressif https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer
#include <uptime_formatter.h>   // Uptime-Library by YiannisBourkelis https://github.com/YiannisBourkelis/Uptime-Library
#include <FS.h>                 // FS by Ivan Grokhotkov https://github.com/espressif/arduino-esp32/blob/master/libraries/FS
#include <FFat.h>               // FFAT by espressif https://github.com/espressif/arduino-esp32/blob/master/libraries/FFat
#include <SD.h>                 // SD by espressif https://github.com/espressif/arduino-esp32/blob/master/libraries/SD
#include <SimpleFTPServer.h>    // SimpleFTPServer by Renzo Mischianti https://mischianti.org/category/my-libraries/simple-ftp-server/
#include <Update.h>             // Update by Hristo Gochkov https://github.com/espressif/arduino-esp32/tree/master/libraries/Update
#include <WiFiClientSecure.h>   // WifiClientSecure by Evandro Luis Copercini https://git.liberatedsystems.co.uk/jacob.eva/arduino-esp32/src/commit/bcd6dcf5f6f8a4670db53a495ad8c8b556c2a8fa/libraries/WiFiClientSecure
#include <vector>               // vector library from C++ standard library https://github.com/espressif/arduino-esp32
#include <algorithm>            // algorithm library from C++ standard library https://github.com/espressif/arduino-esp32

// Hardware Rev 20240812 pins [Since codeVersion 20240819]
#define buzzer_pin 14
#define neopix_pin 47
#define lockRelay_pin 1
#define AUXButton_pin 6
#define exitButton_pin 21
#define bellButton_pin 17
#define magSensor_pin 15
#define attSensor_pin 9
#define wiegand_0_pin 16
#define wiegand_1_pin 18
#define DS01 33
#define DS02 37
#define DS03 5
#define DS04 10
#define GH01 2
#define GH02 4
#define GH03 12
#define GH04 13
#define GH05 8
#define GH06 11
#define IO0 0
#define SD_CS_PIN 34
#define SD_CLK_PIN 38
#define SD_MOSI_PIN 36
#define SD_MISO_PIN 35
#define SD_CD_PIN 7

// Define struct for storing configuration
struct Config {
  bool wifi_enabled;
  char wifi_ssid[32];
  char wifi_password[32];
  bool mqtt_enabled;
  char mqtt_server[64];
  int mqtt_port;
  char mqtt_topic[32];
  char mqtt_cmnd_topic[32];
  char mqtt_stat_topic[32];
  char mqtt_keys_topic[32];
  char mqtt_client_name[32];
  bool mqtt_auth;
  bool mqtt_tls;
  char mqtt_user[32];
  char mqtt_password[32];
  bool magsr_present;
  bool ftp_enabled;
  int ftp_port;
  char ftp_user[32];
  char ftp_password[32];
  int exitUnlockDur_S;
  int httpUnlockDur_S;
  int keyUnlockDur_S;
  int cmndUnlockDur_S;
  int badKeyLockoutDur_S;
  int exitButMinThresh_Ms;
  int keyWaitDur_Ms;
  int garageMomentaryDur_Ms;
  int addKeyTimeoutDur_S;
};

struct NoteEvent {
  int tick;
  note_t note;
  int octave;
  int dur;
};

// Define struct for storing addressing configuration
struct Addressing {
  char localIP[16];
  char subnetMask[16];
  char gatewayIP[16];
};

#define WDT_TIMEOUT_S 60

// Number of neopixels used
#define NUMPIXELS 1

// Format FFat on boot?
#define FORMAT_FFAT false

unsigned long lastMsg = 0;
unsigned long disconCount = 0;
unsigned long lastMQTTConnectAttempt = 0;
unsigned long lastWifiConnectAttempt = 0;
unsigned long wifiReconnectInterval = 180000;
unsigned long mqttReconnectInterval = 180000;
int add_count = 0;
int seqTmr = 0;
int hwRev = 0;
String scannedKey = "";
String serialCmd;

unsigned long ota_progress_millis = 0;

boolean validKeyRead = false;
boolean forceOffline = false;
boolean invalidKeyRead = false;
boolean SD_present = false;
boolean SD_mounted = false;
boolean FFat_present = false;
boolean staticIP = false;
boolean doorOpen = true;
boolean failSecure = true;
boolean add_mode = false;
boolean garage_mode = false;
boolean magsr_present = false;

// String holder for html webpage content
String pageContent = "";

// Filenames
const char* config_filename = "/dl32.json";
const char* addressing_filename = "/addressing.json";
const char* keys_filename = "/keys.txt";
const char* bell_filename = "/current.bell_";
const char* caCrt_filename = "/ca.crt";
const char* clCrt_filename = "/client.crt";
const char* clKey_filename = "/client.key";

// Values for URL OTA (Once Releases are published)
const char* releaseRepoUrl = "www.github.com";
uint16_t releaseRepoPort = 80;
const char* releaseRepoPath = "/Mark-Roly/dl32-arduino/releases/latest/download/DL32.bin";

// TLS buffer pointers
char* ca_cert = nullptr;
char* client_cert = nullptr;
char* client_key = nullptr;

// buzzer settings
int freq = 2000;
int channel = 0;
int resolution = 8;
int tickMs = 5;
String trackName;
String bellFile;

// integer for watchdog counter
volatile int watchdogCount = 0;

// Define onboard and offvoard neopixels
Adafruit_NeoPixel pixel = Adafruit_NeoPixel(NUMPIXELS, neopix_pin, NEO_GRB + NEO_KHZ800);

// instantiate objects for Configuration struct, addressing struct, wifi client, webserver, mqtt client, Wiegand reader and WDT timer
Config config;
Addressing addressing;
WiFiClient wifiClient;
WiFiClientSecure wifiClient_tls;
WebServer webServer(80);
File uploadFile;
Wiegand wiegand;
Ticker secondTick;
FtpServer ftpSrv;
PubSubClient mqttClient(wifiClient);
PubSubClient mqttClient_tls(wifiClient_tls);

// --- Watchdog Functions --- Watchdog Functions --- Watchdog Functions --- Watchdog Functions --- Watchdog Functions --- Watchdog Functions ---

void restart() {
  Serial.println("Restarting...");
  mqttPublish(config.mqtt_stat_topic, "Restarting...");
  delay(1000);
  ESP.restart();
}

void ISRwatchdog() {
  watchdogCount++;
  if (watchdogCount == WDT_TIMEOUT_S) {
    Serial.println("Watchdog invoked!");
    ESP.restart();
  }
}

// --- Stats Functions --- Stats Functions --- Stats Functions --- Stats Functions --- Stats Functions --- Stats Functions --- Stats Functions ---

int rssiToPercent(long rssi) {
  if (rssi <= -100) return 0;
  if (rssi >= -50) return 100;
  return 2 * (rssi + 100);
}

// Convert RSSI to human-readable signal quality
String rssiToQuality(long rssi) {
  if (rssi >= -60) return "Excellent";
  else if (rssi >= -67) return "Good";
  else if (rssi >= -75) return "Fair";
  else return "Weak";
}

String formatNumber(long n) {
  String result = "";
  String numStr = String(n);
  int len = numStr.length();
  int count = 0;

  for (int i = len - 1; i >= 0; i--) {
    result = numStr[i] + result;
    count++;
    if (count == 3 && i != 0) {
      result = "," + result;
      count = 0;
    }
  }
  return result;
}

void printStats() {
  String statsPayload;
  statsPayload += "Hardware revision: " + String(hwRev) + "\n";
  statsPayload += "Firmware version: " + String(codeVersion) + "\n";
  statsPayload += "IP address: " + WiFi.localIP().toString() + "\n";
  statsPayload += "MAC address: " + WiFi.macAddress() + "\n";
  statsPayload += "Wifi strength: " + String(WiFi.RSSI()) + "dBm (" + rssiToPercent(WiFi.RSSI()) + "% - " + rssiToQuality(WiFi.RSSI()) + ")" + "\n";
  statsPayload += "Uptime: " + uptime_formatter::getUptime() + "\n";
  if (digitalRead(SD_CD_PIN) == LOW) {
    statsPayload += "SD Card present: true\n";
  } else {
    statsPayload += "SD Card present: false\n";
  }
  statsPayload += "FFat free space: " + formatNumber(FFat.usedBytes()/1024) + "kB of " +  formatNumber(FFat.totalBytes()/1024) + "kB used" + "\n";
  if (digitalRead(SD_CD_PIN) == LOW) {
    statsPayload += "SD space: " + formatNumber(SD.usedBytes()/1024) + "kB of " + formatNumber(SD.totalBytes()/1024) + "kB used";
  }
  Serial.println(statsPayload);
}

void statsPanel() {
  String statsPayload;
  statsPayload += "Hardware revision: " + String(hwRev) + "\n";
  statsPayload += "Firmware version: " + String(codeVersion) + "\n";
  statsPayload += "IP address: " + WiFi.localIP().toString() + "\n";
  statsPayload += "Wifi strength: " + String(WiFi.RSSI()) + "dBm (" + rssiToPercent(WiFi.RSSI()) + "% - " + rssiToQuality(WiFi.RSSI()) + ")" + "\n";
  statsPayload += "MAC address: " + WiFi.macAddress() + "\n";
  statsPayload += "Uptime: " + uptime_formatter::getUptime() + "\n";
  if (digitalRead(SD_CD_PIN) == LOW) {
    statsPayload += "SD Card present: true\n";
  } else {
    statsPayload += "SD Card present: false\n";
  }
  statsPayload += "FFat free space: " + formatNumber(FFat.usedBytes()/1024) + "kB of " +  formatNumber(FFat.totalBytes()/1024) + "kB used" + "\n";
  if (digitalRead(SD_CD_PIN) == LOW) {
    statsPayload += "SD space: " + formatNumber(SD.usedBytes()/1024) + "kB of " + formatNumber(SD.totalBytes()/1024) + "kB used" + "\n";
  }
  pageContent += "      <textarea class='statsBox' readonly>\n";
  pageContent += statsPayload;
  pageContent += "      </textarea><br/>\n";
}

void publishStats() {
  String statsPayload;
  statsPayload += "Hardware revision: " + String(hwRev) + "\n";
  statsPayload += "Firmware version: " + String(codeVersion) + "\n";
  statsPayload += "IP address: " + WiFi.localIP().toString() + "\n";
  statsPayload += "Wifi strength: " + String(WiFi.RSSI()) + "dBm (" + rssiToPercent(WiFi.RSSI()) + "% - " + rssiToQuality(WiFi.RSSI()) + ")" + "\n";
  statsPayload += "MAC address: " + WiFi.macAddress() + "\n";
  statsPayload += "Uptime: " + uptime_formatter::getUptime();
  mqttPublish(config.mqtt_stat_topic, statsPayload.c_str());
  delay(250);
}

void publishStorage() {
  delay(250);
  String storagePayload;
  if (digitalRead(SD_CD_PIN) == LOW) {
    storagePayload += "SD Card present: true\n";
  } else {
    storagePayload += "SD Card present: false\n";
  }
  storagePayload += "FFat free space: " + formatNumber(FFat.usedBytes()/1024) + "kB of " +  formatNumber(FFat.totalBytes()/1024) + "kB used" + "\n";
  if (digitalRead(SD_CD_PIN) == LOW) {
    storagePayload += "SD space: " + formatNumber(SD.usedBytes()/1024) + "kB of " + formatNumber(SD.totalBytes()/1024) + "kB used";
  }
  mqttPublish(config.mqtt_stat_topic, storagePayload.c_str());
}

// --- Wiegand Functions --- Wiegand Functions --- Wiegand Functions --- Wiegand Functions --- Wiegand Functions --- Wiegand Functions --- Wiegand Functions ---

// function that checks if a provided key is present in the authorized list
boolean keyAuthorized(String key) {
  boolean verboseScanOutput = false;
  File keysFile = FFat.open(keys_filename, "r");
  if (!keysFile) {
    Serial.println("Failed to open keys file.");
    return false;
  }
  Serial.print("Checking key: ");
  Serial.println(key);
  while (keysFile.available()) {
    String fileKey = keysFile.readStringUntil('\n');
    fileKey.trim();  // Removes trailing \r, \n, and spaces
    if (verboseScanOutput) {
      Serial.print("Comparing input key [");
      Serial.print(key);
      Serial.print("] with file key [");
      Serial.print(fileKey);
      Serial.println("]");
    }
    if (fileKey.equals(key)) {
      keysFile.close();
      if (verboseScanOutput) Serial.println("MATCH FOUND");
      return true;
    }
  }
  keysFile.close();
  if (verboseScanOutput) Serial.println("NO MATCH");
  return false;
}

void writeKey(String key) {
  if (keyAuthorized(key)) {
    add_mode = false;
    Serial.print("Key ");
    Serial.print(key);
    Serial.println(" is already authorized");
    mqttPublish(config.mqtt_stat_topic, "Key is already authorized!");
    playUnauthorizedTone();
  } else if ((key.length() < 3)||(key.length() > 10)) {
    add_mode = false;
    Serial.print("Key ");
    Serial.print(key);
    Serial.println(" is an invalid length");
    mqttPublish(config.mqtt_stat_topic, "Key is an invalid length");
  } else {
    appendlnFile(FFat, keys_filename, key.c_str());
    add_mode = false;
    Serial.print("Added key ");
    Serial.print(key);
    Serial.println(" to authorized list");
    mqttPublish(config.mqtt_stat_topic, "Key added authorized to authorized list");
    playTwinkleUpTone();
  }
}

void removeKey(String key) {
  if (!FFat.exists(keys_filename)) {
    Serial.println("Key file not present. Cancelling operation.");
    return;
  }
  File keysFile = FFat.open(keys_filename, "r");
  if (!keysFile) {
    Serial.println("Failed to open keys file for reading.");
    return;
  }
  if (FFat.exists("/keys_temp")) {
    deleteFile(FFat, "/keys_temp");
  }
  File tempFile = FFat.open("/keys_temp", "w");
  if (!tempFile) {
    Serial.println("Failed to open temp file for writing.");
    keysFile.close();
    return;
  }
  const size_t bufferLimit = 1024; // 1KB buffer
  String writeBuffer = "";
  bool found = false;
  while (keysFile.available()) {
    String fileKey = keysFile.readStringUntil('\n');
    fileKey.trim(); // Clean up trailing whitespace/newlines
    if (fileKey.length() == 0) continue; // Skip empty lines
    if (fileKey == key) {
      found = true;
      continue; // Skip writing this key
    }
    writeBuffer += fileKey + "\n";
    if (writeBuffer.length() >= bufferLimit) {
      tempFile.print(writeBuffer); // Dump buffer to file
      writeBuffer = ""; // Clear buffer
    }
  }
  // Write any remaining buffer content
  if (writeBuffer.length() > 0) {
    tempFile.print(writeBuffer);
  }
  keysFile.close();
  tempFile.close();
  if (found) {
    if (FFat.exists("/keys_old")) {
      deleteFile(FFat, "/keys_old");
    }
    renameFile(FFat, keys_filename, "/keys_old");
    renameFile(FFat, "/keys_temp", keys_filename);
    playTwinkleDownTone();
    Serial.print("Removed key ");
    Serial.print(key);
    Serial.println(" from authorized list.");
  } else {
    deleteFile(FFat, "/keys_temp");
    Serial.print("Key ");
    Serial.print(key);
    Serial.println(" not found in authorized list.");
  }
}

void previewFile(String file) {
  if (FFat_present) {
    String path = "/" + file;
    File preview = FFat.open(path);
    if (preview) {
      webServer.sendHeader("Content-Type", "text/plain");
      webServer.streamFile(preview, "text/plain");
      preview.close();
      Serial.print("Previewing file: ");
      Serial.println(file);
      return;
    } else {
      Serial.println("Failed to open file for preview: " + file);
    }
  }
  webServer.send(404, "text/plain", "File not found");
}

// Polled function to check if a key has been recently read
void checkKey() {
  noInterrupts();
  wiegand.flush();
  interrupts();
  String keypadBuffer;
  int keypadCounter = 0;
  if (scannedKey == "") {
    return;
  //Othwerwise if the key pressed is 0A (*/ESC), then ring the bell and finish
  } else if ((scannedKey == "0A") && (add_mode == false)) {
    ringBell();
    scannedKey = "";
    return;
  } else if ((scannedKey == "0B")) {
    scannedKey = "";
    add_mode = false;
    return;
  }
  //function to recognize keypad input
  if ((scannedKey.length()) == 2) {
    Serial.println("Keypad entry...");
    keypadBuffer += scannedKey.substring(1);
    scannedKey = "";
    while ((keypadCounter < (config.keyWaitDur_Ms/100)) && (keypadBuffer.length() < 12)) {
      noInterrupts();
      wiegand.flush();
      interrupts();
      delay(100);
      keypadCounter++;
      //If the input was a keypad digit, then reset the inter-key timeout and wait for another one
      if ((scannedKey.length()) == 2) {
        keypadCounter = 0;
        //If the key pressed is 0B (#/ENT, then exit the loop of waiting for input
        if (scannedKey == "0B") {
          keypadCounter = (config.keyWaitDur_Ms/100);
        } else if (scannedKey == "0A") {
          Serial.println("Cancelling keypad input");
          add_mode = false;
          scannedKey = "";
          return;
        } else {
          keypadBuffer += scannedKey.substring(1);
        }
        scannedKey = "";
      }
    }
    if (keypadBuffer.length() < 4) {
      Serial.println("Key too short - discarded");
      add_mode = false;
      playUnauthorizedTone();
      return;
    } else if (keypadBuffer.length() > 10) {
      Serial.println("Key too long - discarded");
      add_mode = false;
      playUnauthorizedTone();
      return;
    } else {
      scannedKey = keypadBuffer;
    }
  }

  if (config.mqtt_tls && mqttConnected()) {
    mqttClient_tls.publish(config.mqtt_keys_topic, scannedKey.c_str());
  } else if (mqttConnected()) {
    mqttClient.publish(config.mqtt_keys_topic, scannedKey.c_str());
  }

  bool match_found = keyAuthorized(scannedKey);

  if ((match_found == false) and (add_mode)) {
    writeKey(scannedKey);
  } else if (match_found and (add_mode)) {
    add_mode = false;
    Serial.print("Key ");
    Serial.print(scannedKey);
    Serial.println(" is already authorized!");
    mqttPublish(config.mqtt_stat_topic, "Key is already authorized!");
    playUnauthorizedTone();
  } else if (match_found and (add_mode == false)) {
    Serial.print("Key ");
    Serial.print(scannedKey);
    Serial.println(" is authorized!");
    mqttPublish(config.mqtt_stat_topic, "Key is authorized!");
    unlock(config.keyUnlockDur_S);
  } else {
    add_mode = false;
    Serial.print("Key ");
    Serial.print(scannedKey);
    Serial.println(" is unauthorized!");
    mqttPublish(config.mqtt_stat_topic, "Key is unauthorized!");
    setPixRed();
    playUnauthorizedTone();
    delay(config.badKeyLockoutDur_S*1000);
    setPixBlue();
  }
  scannedKey = "";
  return;
}

// When any of the pins have changed, update the state of the wiegand library
void pinStateChanged() {
  wiegand.setPin0State(digitalRead(wiegand_0_pin));
  wiegand.setPin1State(digitalRead(wiegand_1_pin));
}

// Notifies when a reader has been connected or disconnected.
// Instead of a message, the seconds parameter can be anything you want -- Whatever you specify on `wiegand.onStateChange()`
void stateChanged(bool plugged, const char* message) {
  Serial.print(message);
  Serial.println(plugged ? "CONNECTED" : "DISCONNECTED");
}

// IRQ function for reading keys
void receivedData(uint8_t* data, uint8_t bits, const char* message) {
  String key = "";
  String key_buff = "";
  add_count = (config.addKeyTimeoutDur_S * 100);
  //Print value in HEX
  uint8_t bytes = (bits+7)/8;
  for (int i=0; i<bytes; i++) {
    String FirstNum = (String (data[i] >> 4, 16));
    String SecondNum = (String (data[i] & 0xF, 16));
    key_buff = key;
    key = (FirstNum + SecondNum + key_buff);
  }
  scannedKey = key;
  scannedKey.toUpperCase();
  return;
}

// Notifies when an invalid transmission is detected
void receivedDataError(Wiegand::DataError error, uint8_t* rawData, uint8_t rawBits, const char* message) {
  Serial.print(message);
  Serial.print(Wiegand::DataErrorStr(error));
  Serial.print(" - Raw data: ");
  Serial.print(rawBits);
  Serial.print("bits / ");
  //Print value in HEX
  uint8_t bytes = (rawBits+7)/8;
  for (int i=0; i<bytes; i++) {
    Serial.print(rawData[i] >> 4, 16);
    Serial.print(rawData[i] & 0xF, 16);
  }
  Serial.println();
}

// --- SD Functions --- SD Functions --- SD Functions --- SD Functions --- SD Functions --- SD Functions --- SD Functions ---

void sd_setup() {
  Serial.print("Mounting SD filesystem...");
  SPI.begin(SD_CLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if(!SD.begin(SD_CS_PIN)){
    Serial.println("FAILED");
    return;
  }
  Serial.println("OK");
  SD_mounted = true;
}

void checkSDPresent(int verbose) {
  if ((SD_present == false) && (digitalRead(SD_CD_PIN) == LOW)) {
    SD_present = true;
    if (verbose == 1) {
      Serial.println("SD Card inserted");
      sd_setup();
    }
  } else if ((SD_present == true) && (digitalRead(SD_CD_PIN) == HIGH)) {
    SD_present = false;
    if (verbose == 1) {
      Serial.println("SD Card ejected");
    }
  }
}

// --- FATFS Functions --- FATFS Functions --- FATFS Functions --- FATFS Functions --- FATFS Functions --- FATFS Functions --- FATFS Functions ---

bool fatfs_setup() {
  Serial.print("Mounting FFAT filesystem...");
  if (FORMAT_FFAT) {
    FFat.format();
  }  
  if(!FFat.begin(true)){
    Serial.println("FAILED");
    return false;
  }
  Serial.println("OK");
  FFat_present = true;
  return true;
}

bool listDir(fs::FS & fs, const char * dirname, uint8_t levels){
  Serial.printf("Listing directory: %s... ", dirname);
  File root = fs.open(dirname);
  if(!root){
    Serial.println("failed to open directory");
    return false;
  }
  if(!root.isDirectory()){
    Serial.println("not a directory");
    return false;
  }
  File file = root.openNextFile();
  while(file){
    if(file.isDirectory()){
      Serial.print("  DIR : ");
      Serial.println(file.name());
      if(levels){
        listDir(fs, file.path(), levels -1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
  return true;
}

bool readFile(fs::FS & fs, const char * path){
  Serial.printf("Reading file: %s... ", path);
  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("failed to open file for reading");
    return false;
  }
  Serial.println("- read from file:");
  while(file.available()){
    Serial.write(file.read());
  }
  file.close();
  return true;
}

bool writeFile(fs::FS & fs, const char * path, const char * message){
  Serial.printf("Writing file: %s... ", path);
  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("failed to open file for writing");
    return false;
  }
  if(file.print(message)){
      Serial.println("file written");
  } else {
    Serial.println("write failed");
    file.close();
    return false;
  }
  file.close();
  return true;
}

bool appendlnFile(fs::FS & fs, const char * path, const char * message){
  //Serial.printf("Appending line to file: %s... ", path);
  File file = fs.open(path, FILE_APPEND);
  if(!file){
    Serial.println("- failed to open file for appending line");
    return false;
  }
  if(file.println(message)){
    //Serial.println("message line appended");
  } else {
    Serial.println("append line failed");
    return false;
  }
  file.close();
  return true;
}

bool renameFile(fs::FS & fs, const char * path1, const char * path2){
  //Serial.printf("Renaming file %s to %s... ", path1, path2);
  if (fs.rename(path1, path2)) {
    Serial.println("file renamed");
  } else {
    Serial.println("rename failed");
    return false;
  }
  return true;
}

bool deleteFile(fs::FS & fs, const char * path) {
  //Serial.printf("Deleting file: %s...", path);
  if (fs.remove(path)) {
    playTwinkleDownTone();
    Serial.print("Deleted file ");
    Serial.println(path);
  } else {
    Serial.print("Failed to delete file ");
    Serial.println(path);
    return false;
  }
  return true;
}

bool createDir(fs::FS & fs, const char * path) {
  //Serial.printf("Creating Dir: %s\n", path);
  if (fs.mkdir(path)) {
    Serial.println("Dir created");
  } else {
    Serial.println("mkdir failed");
    return false;
  }
  return true;
}

bool removeDir(fs::FS & fs, const char * path) {
  //Serial.printf("Removing Dir: %s\n", path);
  if (fs.rmdir(path)) {
    Serial.println("Dir removed");
    return true;
  } else {
    Serial.println("rmdir failed");
    return false;
  }
}

bool copyFileFFat(const char* sourcePath, const char* destinationPath) {
  File sourceFile = FFat.open(sourcePath, "r");
  if (!sourceFile) {
    Serial.print("Failed to open source file: ");
    Serial.println(sourcePath);
    return false;
  }

  File destFile = FFat.open(destinationPath, "w");
  if (!destFile) {
    Serial.print("Failed to create destination file: ");
    Serial.println(destinationPath);
    sourceFile.close();
    return false;
  }

  // Copy contents
  const size_t bufferSize = 512;
  uint8_t buffer[bufferSize];
  size_t bytesRead;
  while ((bytesRead = sourceFile.read(buffer, bufferSize)) > 0) {
    destFile.write(buffer, bytesRead);
  }

  sourceFile.close();
  destFile.close();

  // Serial.print("Copied ");
  // Serial.print(sourcePath);
  // Serial.print(" to ");
  // Serial.println(destinationPath);
  return true;
}

// Loads the configuration from a file
bool loadFSJSON_config(const char* config_filename, Config& config) {
  // Open file for reading
  File config_file = FFat.open(config_filename);

  // Allocate a temporary JsonDocument
  JsonDocument config_doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(config_doc, config_file);
  if (error) {
    Serial.println(F("FAILED: Using defaults"));
  } else {
    Serial.println("OK");
  }
  config.wifi_enabled = config_doc["wifi_enabled"] | false;
  strlcpy(config.wifi_ssid, config_doc["wifi_ssid"] | "null_wifi_ssid", sizeof(config.wifi_ssid));
  strlcpy(config.wifi_password, config_doc["wifi_password"] | "null_wifi_pass", sizeof(config.wifi_password));
  config.mqtt_enabled = config_doc["mqtt_enabled"] | false;
  strlcpy(config.mqtt_server, config_doc["mqtt_server"] | "null_mqtt_server", sizeof(config.mqtt_server));
  config.mqtt_port = config_doc["mqtt_port"] | 1883;
  strlcpy(config.mqtt_topic, config_doc["mqtt_topic"] | "DEFAULT_dl32s3", sizeof(config.mqtt_topic));
  strlcpy(config.mqtt_stat_topic, config_doc["mqtt_topic"] | "DEFAULT_dl32s3", sizeof(config.mqtt_stat_topic));
  strlcpy(config.mqtt_cmnd_topic, config_doc["mqtt_topic"] | "DEFAULT_dl32s3", sizeof(config.mqtt_cmnd_topic));
  strlcpy(config.mqtt_keys_topic, config_doc["mqtt_topic"] | "DEFAULT_dl32s3", sizeof(config.mqtt_keys_topic));
  strcat(config.mqtt_stat_topic, "/stat");
  strcat(config.mqtt_cmnd_topic, "/cmnd");
  strcat(config.mqtt_keys_topic, "/keys");
  strlcpy(config.mqtt_client_name, config_doc["mqtt_client_name"] | "DEFAULT_dl32s3", sizeof(config.mqtt_client_name));
  config.mqtt_auth = config_doc["mqtt_auth"] | false;
  config.mqtt_tls = config_doc["mqtt_tls"] | false;
  strlcpy(config.mqtt_user, config_doc["mqtt_user"] | "mqtt", sizeof(config.mqtt_user));
  strlcpy(config.mqtt_password, config_doc["mqtt_password"] | "null_mqtt_pass", sizeof(config.mqtt_password));
  config.magsr_present = config_doc["magsr_present"] | false;
  config.ftp_enabled = config_doc["ftp_enabled"] | false;
  config.ftp_port = 21; // current library does not allow argument-defined port
  strlcpy(config.ftp_user, config_doc["ftp_user"] | "null_ftp_user", sizeof(config.ftp_user));
  strlcpy(config.ftp_password, config_doc["ftp_password"] | "null_ftp_pass", sizeof(config.ftp_password));
  config.exitUnlockDur_S = config_doc["exitUnlockDur_S"] | 5;
  config.httpUnlockDur_S = config_doc["httpUnlockDur_S"] | 5;
  config.keyUnlockDur_S = config_doc["keyUnlockDur_S"] | 5;
  config.cmndUnlockDur_S = config_doc["cmndUnlockDur_S"] | 5;
  config.badKeyLockoutDur_S = config_doc["badKeyLockoutDur_S"] | 5;
  config.exitButMinThresh_Ms = config_doc["exitButMinThresh_Ms"] | 50;
  config.keyWaitDur_Ms = config_doc["keyWaitDur_Ms"] | 1500;
  config.garageMomentaryDur_Ms = config_doc["garageMomentaryDur_Ms"] | 500;
  config.addKeyTimeoutDur_S = config_doc["addKeyTimeoutDur_S"] | 10;
  config_file.close();
  return true;
}

// Loads the addressing from a file
bool loadFSJSON_addressing(const char* addressing_filename, Addressing& addressing) {
  if (FFat.exists(addressing_filename) == false) {
    Serial.println("FAILED: Using dynamic addressing");
    return false;
  }
  // Open file for reading
  File addressing_file = FFat.open(addressing_filename);

  // Allocate a temporary JsonDocument
  JsonDocument addressing_doc;

  //Serial.print((char)addressing_file.read());

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(addressing_doc, addressing_file);
  if (error) {
    Serial.print("FAILED: '");
      switch (error.code()) {
    case DeserializationError::Ok:
        Serial.println("Deserialization succeeded");
        break;
    case DeserializationError::InvalidInput:
        Serial.println("Invalid input!");
        break;
    case DeserializationError::NoMemory:
        Serial.println("Not enough memory");
        break;
    default:
        Serial.println("Deserialization failed");
        break;
    }
    Serial.println("' - Using dynamic addressing");
    return false;
  }
  strlcpy(addressing.localIP, addressing_doc["localIP"] | "false", sizeof(addressing.localIP));
  strlcpy(addressing.subnetMask, addressing_doc["subnetMask"] | "null_wifi_ssid", sizeof(addressing.subnetMask));
  strlcpy(addressing.gatewayIP, addressing_doc["gatewayIP"] | "null_wifi_pass", sizeof(addressing.gatewayIP));
  addressing_file.close();
  staticIP = true;
  Serial.println("Static addressing loaded");
  return true;
}

bool configSDtoFFat() {
  if ((SD_present == true) && (SD.exists(config_filename))) {
    File sourceFile = SD.open(config_filename);
    File destFile = FFat.open(config_filename, FILE_WRITE);
    static uint8_t buf[1];
    while ( sourceFile.read( buf, 1) ) {
      destFile.write( buf, 1 );
    }
    destFile.close();
    sourceFile.close();
  } else {
    playUnauthorizedTone();
    Serial.println("No SD Card Mounted or no such file");
    return false;
  }
  playTwinkleUpTone();
  Serial.println("Config file successfuly copied from SD to FFat");
  return true;
}

bool configFFattoSD() {
  if ((SD_present == true) && (FFat.exists(config_filename))){
    File sourceFile = FFat.open(config_filename);
    File destFile = SD.open(config_filename, FILE_WRITE);
    static uint8_t buf[1];
    while ( sourceFile.read( buf, 1) ) {
      destFile.write( buf, 1 );
    }
    destFile.close();
    sourceFile.close();
    playTwinkleDownTone();
    Serial.println("Config file successfuly copied from FFat to SD");
  } else {
    playUnauthorizedTone();
    Serial.println("");
    Serial.println("No SD Card Mounted or no such file");
    return false;
  }
  return true;
}

bool addressingSDtoFFat() {
  if ((SD_present == true) && (SD.exists(addressing_filename))) {
    File sourceFile = SD.open(addressing_filename);
    File destFile = FFat.open(addressing_filename, FILE_WRITE);
    static uint8_t buf[1];
    while ( sourceFile.read( buf, 1) ) {
      destFile.write( buf, 1 );
    }
    destFile.close();
    sourceFile.close();
  } else {
    playUnauthorizedTone();
    Serial.println("");
    Serial.println("No SD Card Mounted or no such file");
    return false;
  }
  Serial.println("Addressing file successfuly copied from SD to FFat");
  restart();
  return true;
}

bool allSDtoFFat() {
  int copiedFileCount = 0;

  // List of file names to loop through
  String files[] = {config_filename, addressing_filename, keys_filename, bell_filename, caCrt_filename, clCrt_filename, clKey_filename};
  
  // Get the number of elements in the files array
  int numFiles = sizeof(files) / sizeof(files[0]);

  // Loop through the files based on the number of elements in the array
  for (int i=0; i<numFiles; i++) {
    if ((SD_present == true) && (SD.exists(files[i]))) {
      File sourceFile = SD.open(files[i]);
      File destFile = FFat.open(files[i], FILE_WRITE);
      static uint8_t buf[1];
      while ( sourceFile.read( buf, 1) ) {
        destFile.write( buf, 1 );
      }
      destFile.close();
      sourceFile.close();
      copiedFileCount++;
    } else {
      Serial.print("No SD Card Mounted or no such file: ");
      Serial.println(files[i]);
    }
  }
  if (copiedFileCount > 0) {
    playTwinkleUpTone();
    Serial.print(copiedFileCount);
    Serial.println(" files successfuly copied from SD to FFat");
    restart();
  } else {
    playUnauthorizedTone();
    Serial.println("No SD Card Mounted or no valid files");
    return false;
  }
  return true;
}

bool keysSDtoFFat() {
  if ((SD_present == true) && (SD.exists(keys_filename))) {
    File sourceFile = SD.open(keys_filename);
    File destFile = FFat.open(keys_filename, FILE_WRITE);
    static uint8_t buf[1];
    while ( sourceFile.read( buf, 1) ) {
      destFile.write( buf, 1 );
    }
    destFile.close();
    sourceFile.close();
    Serial.println("Keys file successfuly copied from SD to FFat");
  } else {
    Serial.println("");
    playUnauthorizedTone();
    Serial.println("No SD Card Mounted or no such file");
    return false;
  }
  playTwinkleUpTone();
  return true;
}

bool keysFFattoSD() {
  if ((SD_present == true) && (FFat.exists(keys_filename))){
    File sourceFile = FFat.open(keys_filename);
    File destFile = SD.open(keys_filename, FILE_WRITE);
    static uint8_t buf[1];
    while ( sourceFile.read( buf, 1) ) {
      destFile.write( buf, 1 );
    }
    destFile.close();
    sourceFile.close();
    playTwinkleDownTone();
    Serial.println("Keys file successfuly copied from FFat to SD");
  } else {
    Serial.println("");
    playUnauthorizedTone();
    Serial.println("No SD Card Mounted or no such file");
    return false;
  }
  return true;
}

void addKeyMode() {
  setPixPurple();
  playAddModeTone();
  Serial.println("Add Key mode - Waiting for key");
  mqttPublish(config.mqtt_stat_topic, "Add Key mode - Waiting for key");
  add_count = 0;
  add_mode = true;
  while (add_count < (config.addKeyTimeoutDur_S * 100) && add_mode) {
    noInterrupts();
    wiegand.flush();
    interrupts();
    if (add_count % 100 == 0 && (digitalRead(DS03) == HIGH)) {
      playGeigerTone();
    }
    if (Serial.available()) {
      serialCmd = Serial.readStringUntil('\n');
      serialCmd.trim();
      Serial.print("Serial key input: ");
      Serial.println(serialCmd);
      scannedKey = serialCmd;
      writeKey(scannedKey);
    }
    delay(10);
    add_count++;
  }
  if (scannedKey == "") {
    add_mode = false;
    Serial.println("No new key added");
    mqttPublish(config.mqtt_stat_topic, "No new key added");
  } else {
    Serial.print("scannedKey: ");
  }
  setPixBlue();
  return;
}

String urlDecode(const String& input) {
  String decoded = "";
  char temp[] = "0x00";
  unsigned int len = input.length();
  unsigned int i=0;

  while (i < len) {
    char c = input.charAt(i);
    if (c == '+') {
      decoded += ' ';
    } else if (c == '%' && i + 2 < len) {
      temp[2] = input.charAt(i + 1);
      temp[3] = input.charAt(i + 2);
      decoded += (char)strtol(temp, NULL, 16);
      i += 2;
    } else {
      decoded += c;
    }
    i++;
  }
  return decoded;
}

// --- Neopixel Functions --- Neopixel Functions --- Neopixel Functions --- Neopixel Functions --- Neopixel Functions --- Neopixel Functions ---

// NOTE: Onboard pixel uses G,R,B

void setPixRed() {
  pixel.setPixelColor(0, pixel.Color(0,25,0));
  pixel.show();
}

void setPixAmber() {
  pixel.setPixelColor(0, pixel.Color(10,25,0));
  pixel.show();
}

void setPixGreen() {
  pixel.setPixelColor(0, pixel.Color(25,0,0));
  pixel.show();
}

void setPixPurple() {
  pixel.setPixelColor(0, pixel.Color(0,10,25));
  pixel.show();
}

void setPixBlue() {
  pixel.setPixelColor(0, pixel.Color(0,0,25));
  pixel.show();
}

// --- Wifi Functions --- Wifi Functions --- Wifi Functions --- Wifi Functions --- Wifi Functions --- Wifi Functions --- Wifi Functions ---

// Function to convert sgtring-format IP address into ip address object
IPAddress stringToIPAddress(const char* ipStr) {
    // Split the input string by periods '.'
    uint8_t bytes[4];
    int byteIndex = 0;
    // Convert each part of the string to a byte
    char* token = strtok((char*)ipStr, ".");
    while (token != NULL && byteIndex < 4) {
        bytes[byteIndex] = atoi(token);
        token = strtok(NULL, ".");
        byteIndex++;
    }
    // Return the IPAddress object
    return IPAddress(bytes[0], bytes[1], bytes[2], bytes[3]);
}

int connectWifi() {
  // Configures static IP address
  if (staticIP == true) {
    if (!WiFi.config(stringToIPAddress(addressing.localIP), stringToIPAddress(addressing.gatewayIP), stringToIPAddress(addressing.subnetMask))) {
      Serial.println("STA Failed to configure");
      (staticIP == false);
    }
  }
  WiFi.mode(WIFI_STA); //Optional
  Serial.print("Connecting to SSID " + (String)config.wifi_ssid);
  //Serial.print(" with password ");
  //Serial.print(config.wifi_password);
  int count = 0;
  lastWifiConnectAttempt = millis();
  WiFi.begin(config.wifi_ssid, config.wifi_password);
  WiFi.mode(WIFI_MODE_STA);
  
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(1000);
    count++;
    if (count > 10) {
      Serial.print("Unable to connect to SSID ");
      Serial.println(config.wifi_ssid);
      WiFi.disconnect();
      return 1;
    }
  }
  Serial.print("\nSuccessfully connected to SSID ");
  Serial.println(config.wifi_ssid);
  if (config.mqtt_enabled) {
    if (config.mqtt_tls) {
      startMQTTConnection_tls();
    } else {
      startMQTTConnection();
    }
  }
  return 0;
}

// --- Action Functions --- Action Functions --- Action Functions --- Action Functions --- Action Functions --- Action Functions ---

void unlock(int secs) {
  int loops = 0;
  if (garage_mode) {
    Serial.println("Toggle garage door");
    if (mqttConnected()) {
      mqttPublish(config.mqtt_stat_topic, "Toggle garage door");
    }
    setPixGreen();
    digitalWrite(lockRelay_pin, HIGH);
    playUnlockTone();
    delay(config.garageMomentaryDur_Ms);
    digitalWrite(lockRelay_pin, LOW);
    setPixBlue();
  return;
  }
  if (failSecure) {
    digitalWrite(lockRelay_pin, HIGH);
  } else {
    digitalWrite(lockRelay_pin, LOW);
  }
  playUnlockTone();
  Serial.print("Unlock - ");
  Serial.print(secs);
  Serial.println(" Seconds");
  setPixGreen();
  if (mqttConnected()) {
    mqttPublish(config.mqtt_stat_topic, "unlocked");
  }
  while (loops < secs) {
    loops++;
    delay(1000);
  }
  Serial.println("Lock");
  setPixBlue();
    if (failSecure) {
    digitalWrite(lockRelay_pin, LOW);
  } else {
    digitalWrite(lockRelay_pin, HIGH);
  }
  if (mqttConnected()) {
    mqttPublish(config.mqtt_stat_topic, "locked");
  }
}

void garage_toggle() {
  if (garage_mode) {
    Serial.println("Toggle garage door");
    if (mqttConnected()) {
      mqttPublish(config.mqtt_stat_topic, "Toggle garage door");
    }
    setPixGreen();
    digitalWrite(lockRelay_pin, HIGH);
    playUnlockTone();
    delay(config.garageMomentaryDur_Ms);
    digitalWrite(lockRelay_pin, LOW);
    setPixBlue();
  } else if (garage_mode == false) {
    Serial.println("Unit is not in garage mode.");
    if (mqttConnected()) {
      mqttPublish(config.mqtt_stat_topic, "Unit is not in garage mode.");
    }
  }
  return;
}

void garage_open() {
  if (garage_mode && (doorOpen == false)) {
    Serial.println("Opening garage door");
    if (mqttConnected()) {
      mqttPublish(config.mqtt_stat_topic, "Opening garage door");
    }
    setPixGreen();
    digitalWrite(lockRelay_pin, HIGH);
    playUnlockTone();
    delay(config.garageMomentaryDur_Ms);
    digitalWrite(lockRelay_pin, LOW);
    setPixBlue();
  } else if  (garage_mode && (doorOpen)) {
    Serial.println("Door is already open!");
    if (mqttConnected()) {
      mqttPublish(config.mqtt_stat_topic, "Door is already open!");
    }
  } else if (garage_mode == false) {
    Serial.println("Unit is not in garage mode.");
    if (mqttConnected()) {
      mqttPublish(config.mqtt_stat_topic, "Unit is not in garage mode.");
    }
  }
  return;
}

void garage_close() {
  if ((garage_mode) && (doorOpen)) {
    Serial.println("Closing garage door");
    if (mqttConnected()) {
      mqttPublish(config.mqtt_stat_topic, "Closing garage door");
    }
    setPixGreen();
    digitalWrite(lockRelay_pin, HIGH);
    playUnlockTone();
    delay(config.garageMomentaryDur_Ms);
    digitalWrite(lockRelay_pin, LOW);
    setPixBlue();
  } else if  (garage_mode && (doorOpen == false)) {
    Serial.println("Door is already closed!");
    if (mqttConnected()) {
      mqttPublish(config.mqtt_stat_topic, "Door is already closed!");
    }
  } else if (garage_mode == false) {
    Serial.println("Unit is not in garage mode.");
    if (mqttConnected()) {
      mqttPublish(config.mqtt_stat_topic, "Unit is not in garage mode.");
    }
  }
  return;
}

// --- Button Functions --- Button Functions --- Button Functions --- Button Functions --- Button Functions --- Button Functions ---

int checkExit() {
  long count = 0;
  if (digitalRead(exitButton_pin) == LOW) {
    while (digitalRead(exitButton_pin) == LOW) {
      count = count + 10;
      delay(10);
      if (count > 3000) {
        addKeyMode();
        return 0;
      }
    }
    if (count > config.exitButMinThresh_Ms && count < 3001){
      Serial.print("Exit Button pressed");
      Serial.print(" for ");
      Serial.print(count);
      Serial.print("ms");
      Serial.println("");
      if (mqttConnected()) {
        String msg_str = ("Exit Button pressed for " + String(count) + "ms");
        char* msg_char = new char[msg_str.length() + 1];
        msg_str.toCharArray(msg_char, msg_str.length() + 1);
        mqttPublish(config.mqtt_stat_topic, msg_char);
      }
      unlock(config.exitUnlockDur_S);
      return 0;
    }
  }
  return 0;
}

int check0() {
  long count = 0;
  if (digitalRead(IO0) == LOW) {
    while (digitalRead(IO0) == LOW) {
      count = count + 10;
      delay(10);
      if (count > 3000) {
        addKeyMode();
        return 0;
      }
    }
    if (count > config.exitButMinThresh_Ms && count < 3001){
      Serial.print("IO0 pressed");
      Serial.print(" for ");
      Serial.print(count);
      Serial.print("ms");
      Serial.println("");
      if (mqttConnected()) {
        String msg_str = ("IO0 pressed for " + String(count) + "ms");
        char* msg_char = new char[msg_str.length() + 1];
        msg_str.toCharArray(msg_char, msg_str.length() + 1);
        mqttPublish(config.mqtt_stat_topic, msg_char);
      }
      unlock(config.exitUnlockDur_S);
      return 0;
    }
  }
  return 0;
}

void checkAUX() {
  if (digitalRead(AUXButton_pin) == LOW) {
    playBipTone();
    long count = 0;
    Serial.println("AUX Button Pressed");
    if (mqttConnected()) {
      mqttPublish(config.mqtt_stat_topic, "AUX button pressed");
    }
    while (digitalRead(AUXButton_pin) == LOW && (count < 500)) {
      count++;
      delay(10);
    }
    if (count < 499) {
      printStats();
      publishStats();
      publishStorage();
    }
    if (count > 499) {
      setPixPurple();
      Serial.println("Release button now to upload files (SD->FFAT)");
      playUploadTone();
    }
    while ((digitalRead(AUXButton_pin) == LOW) && (count < 1000)) {
      count++;
      delay(10);
    }
    if (count > 999) {
      setPixAmber();
      Serial.println("Release button now to wipe authorized keys list.");
      playPurgeTone();
    }
    while (digitalRead(AUXButton_pin) == LOW && (count < 1500)) {
      count++;
      delay(10);
    }
    if (count > 1499) {
      setPixRed();
      Serial.println("Release button now to perform factory reset.");
      playFactoryTone();
    }
    while (digitalRead(AUXButton_pin) == LOW) {
      delay(10);
    }
    if ((count > 499)&&(count < 1000)) {
      Serial.println("Uploading config, addressing and key files from SD card to FFat...");
      mqttPublish(config.mqtt_stat_topic, "Uploading config, addressing and key files from SD card to FFat...");
      delay(1000);
      allSDtoFFat();
      delay(1000);
    } else if ((count > 999)&&(count < 1500)) {
      Serial.print("Purging stored keys... ");
      mqttPublish(config.mqtt_stat_topic, "Purging stored keys...");
      deleteFile(FFat, keys_filename);
      playTwinkleDownTone();
      Serial.println("Done");
      mqttPublish(config.mqtt_stat_topic, "Done");
      delay(1000);
    } else if (count > 1499) {
      Serial.println("Factory resetting device... ");
      mqttPublish(config.mqtt_stat_topic, "Factory resetting device...");
      deleteFile(FFat, keys_filename);
      deleteFile(FFat, config_filename);
      deleteFile(FFat, addressing_filename);
      FFat.format();
      playTwinkleDownTone();
      Serial.println("Factory reset complete.");
      mqttPublish(config.mqtt_stat_topic, "Factory reset complete.");
      restart();
      delay(3000);
    }
    setPixBlue();
    return;
  }
}

void checkBell() {
  if (digitalRead(bellButton_pin) == LOW) {
    Serial.println("");
    Serial.print("Bell Pressed - ");
    if (mqttConnected()) {
      mqttPublish(config.mqtt_stat_topic, "bell pressed");
    }
    ringBell();
  }
}

//Check magnetic sensor for open/closed door state (Optional)
void checkMagSensor() {
  if (doorOpen == true && digitalRead(magSensor_pin) == LOW && magsr_present) {
    //send not that door has closed
    doorOpen = false;
    Serial.println("Door Closed");
    if (mqttClient.connected()) {
      mqttPublish(config.mqtt_stat_topic, "door_closed");
    }
    //Serial.print("doorOpen == ");
    //Serial.println(doorOpen);
  } else if (doorOpen == false && digitalRead(magSensor_pin) == HIGH && magsr_present) {
    doorOpen = true;
    Serial.println("Door Opened");
    if (mqttClient.connected()) {
      mqttPublish(config.mqtt_stat_topic, "door_opened");
    }
    //Serial.print("doorOpen == ");
    //Serial.println(doorOpen);
  }
}

// --- Buzzer Functions --- Buzzer Functions --- Buzzer Functions --- Buzzer Functions --- Buzzer Functions --- Buzzer Functions ---

void loadBellFile() {
  File file = FFat.open(bell_filename);
  bellFile = "";
  if (!file) {
    Serial.println("FAILED: Using default");
    bellFile = "Default Tone:1:0 C#4 1 43;1 D4 1 43;2 D#4 1 43;3 E4 1 43;4 C#4 1 43;5 D4 1 43;6 D#4 1 43;7 E4 1 43;8 C#4 1 43;9 D4 1 43;10 D#4 1 43;11 E4 1 43;12 C#4 1 43;13 D4 1 43;14 D#4 1 43;15 E4 1 43;:";
    return;
  }
  while (file.available()) {
    bellFile += (char)file.read();
  }
  Serial.println("OK");
  file.close();
}

boolean setCurrentBell(String selectedBell) {
  char fullPath[(selectedBell.length()) + 2]; // +1 for '/', +1 for '\0'
  fullPath[0] = '/';
  strcpy(fullPath + 1, selectedBell.c_str());  // Copy filename after the '/'
  if ((FFat.exists(bell_filename))&&(FFat.exists(fullPath))){
    deleteFile(FFat, bell_filename);
  }
  copyFileFFat(fullPath, bell_filename);
  loadBellFile();
  return true;
}

note_t parseNote(const String& noteStr) {
  if (noteStr == "C")  return NOTE_C;
  if (noteStr == "C#") return NOTE_Cs;
  if (noteStr == "D")  return NOTE_D;
  if (noteStr == "D#") return NOTE_Eb;
  if (noteStr == "Eb") return NOTE_Eb;
  if (noteStr == "E")  return NOTE_E;
  if (noteStr == "F")  return NOTE_F;
  if (noteStr == "F#") return NOTE_Fs;
  if (noteStr == "G")  return NOTE_G;
  if (noteStr == "G#") return NOTE_Gs;
  if (noteStr == "A")  return NOTE_A;
  if (noteStr == "A#") return NOTE_Bb;
  if (noteStr == "Bb") return NOTE_Bb;
  if (noteStr == "B")  return NOTE_B;
  return NOTE_C; // default fallback
}

boolean checkStopBell() {
  if ((digitalRead(exitButton_pin) == LOW) || (digitalRead(IO0) == LOW) || (digitalRead(AUXButton_pin) == LOW) || (digitalRead(bellButton_pin) == LOW)) {
    return true;
  }
  return false;
}

// Sorting helper
bool compareNoteEvents(const NoteEvent& a, const NoteEvent& b) {
  return a.tick < b.tick;
}

// Function to translate sequence file to playNote() instances
void parseAndPlaySequence(const String& sequence) {
  int firstColon = sequence.indexOf(':');
  int secondColon = sequence.indexOf(':', firstColon + 1);
  if (secondColon == -1) return;

  // Extract track name from before the first colon
  trackName = sequence.substring(0, firstColon);

  Serial.print("Playing track: ");
  Serial.println(trackName);

  // Extract tickMs value from between the first and second colons
  String tickStr = sequence.substring(firstColon + 1, secondColon);
  tickMs = tickStr.toInt();  // Save to existing tickMs variable

  String notesPart = sequence.substring(secondColon + 1);
  std::vector<NoteEvent> noteEvents;
  int start = 0;
  while (start < notesPart.length()) {
    int end = notesPart.indexOf(';', start);
    if (end == -1) end = notesPart.length();
    String noteEntry = notesPart.substring(start, end);
    noteEntry.trim();
    if (noteEntry.length() > 0) {
      int i1 = noteEntry.indexOf(' ');
      int i2 = noteEntry.indexOf(' ', i1 + 1);
      int i3 = noteEntry.indexOf(' ', i2 + 1);
      if (i1 != -1 && i2 != -1 && i3 != -1) {
        String tickStr = noteEntry.substring(0, i1);
        String fullNote = noteEntry.substring(i1 + 1, i2);
        String durStr = noteEntry.substring(i2 + 1, i3);
        // 4th value (instrument code) is ignored
        int tick = tickStr.toInt();
        int dur = durStr.toInt();
        // Parse pitch and octave
        String pitch = fullNote;
        int octave = 0;
        if (isDigit(fullNote.charAt(fullNote.length() - 1))) {
          octave = fullNote.charAt(fullNote.length() - 1) - '0';
          pitch = fullNote.substring(0, fullNote.length() - 1);
        }
        note_t note = parseNote(pitch);
        noteEvents.push_back({tick, note, octave, dur});
      }
    }
    start = end + 1;
  }
  std::sort(noteEvents.begin(), noteEvents.end(), compareNoteEvents);
  int currentTick = -1;
  for (const auto& evt : noteEvents) {
    if (currentTick != -1 && evt.tick > currentTick) {
      int tickDelta = evt.tick - currentTick;
      for(int i=0; i<tickMs; i++) {
        delay(tickDelta);
        if (checkStopBell()) {
          Serial.println("Stopping bell");
          mqttPublish(config.mqtt_stat_topic, "Stopping bell");
          return;
        }
      }
    }
    playNote(evt.note, evt.octave, evt.dur);
    currentTick = evt.tick;
  }
}

void playNote(note_t note, int octave, int dur) {
  // Serial.print("Playing Note ");
  // Serial.print(note);
  // Serial.print(" in octave ");
  // Serial.print(octave);
  // Serial.print(" for duration ");
  // Serial.println(dur);
  ledcWriteNote(buzzer_pin, note, octave);
  delay(dur*100);
  ledcWriteTone(buzzer_pin, 0);
}

void playBipTone() {
  if (digitalRead(DS03) == HIGH) {
    ledcWriteTone(buzzer_pin, 100);
    delay(100);
    ledcWriteTone(buzzer_pin, 0);
  }
}

void playUnlockTone() {
  if (digitalRead(DS03) == HIGH) {
    for (int i=0; i <= 1; i++) {
      ledcWriteTone(buzzer_pin, 5000);
      delay(50);
      ledcWriteTone(buzzer_pin, 0);
      delay(100);
    }
  }
}

void playUnauthorizedTone() {
  if (digitalRead(DS03) == HIGH) {
    ledcWriteTone(buzzer_pin, 700);
    delay(200);
    ledcWriteTone(buzzer_pin, 400);
    delay(600);
    ledcWriteTone(buzzer_pin, 0);
  }
}

void playPurgeTone() {
  if (digitalRead(DS03) == HIGH) {
    ledcWriteTone(buzzer_pin, 500);
    delay(1000);
    ledcWriteTone(buzzer_pin, 0);
  }
}

void playFactoryTone() {
  if (digitalRead(DS03) == HIGH) {
    ledcWriteTone(buzzer_pin, 7000);
    delay(1000);
    ledcWriteTone(buzzer_pin, 0);
  }
}

void playAddModeTone() {
  if (digitalRead(DS03) == HIGH) {
    for (int i=0; i<=2; i++) {
      ledcWriteTone(buzzer_pin, 6500);
      delay(80);
      ledcWriteTone(buzzer_pin, 0);
      delay(80);
    }
  }
}

void playUploadTone() {
  if (digitalRead(DS03) == HIGH) {
    for (int i=0; i<=4; i++) {
      ledcWriteTone(buzzer_pin, 6500);
      delay(80);
      ledcWriteTone(buzzer_pin, 0);
      delay(80);
    }
  }
}

void playTwinkleUpTone() {
  if (digitalRead(DS03) == HIGH) {
    ledcWriteTone(buzzer_pin, 2000);
    delay(80);
    ledcWriteTone(buzzer_pin, 4000);
    delay(80);
    ledcWriteTone(buzzer_pin, 6000);
    delay(80);
    ledcWriteTone(buzzer_pin, 8000);
    delay(80);
    ledcWriteTone(buzzer_pin, 0);
  }
}

void playTwinkleDownTone() {
  if (digitalRead(DS03) == HIGH) {
    ledcWriteTone(buzzer_pin, 8000);
    delay(80);
    ledcWriteTone(buzzer_pin, 6000);
    delay(80);
    ledcWriteTone(buzzer_pin, 4000);
    delay(80);
    ledcWriteTone(buzzer_pin, 2000);
    delay(80);
    ledcWriteTone(buzzer_pin, 0);
  }
}

void playBellFile(String filename) {
  File file = FFat.open(filename);
  String tempBellFile = "";
  if (!file) {
    Serial.println("FAILED: Using default");
    bellFile = "Default Tone:1:0 C#4 1 43;:";
    return;
  }
  while (file.available()) {
    tempBellFile += (char)file.read();
  }
  file.close();
  parseAndPlaySequence(tempBellFile);
}

void ringBell() {
  if (digitalRead(DS03) == HIGH) {
    Serial.println("Ringing Bell");
    mqttPublish(config.mqtt_stat_topic, "Ringing Bell");
    parseAndPlaySequence(bellFile);
  } else {
    Serial.println("Cannot ring bell in silent mode");
    mqttPublish(config.mqtt_stat_topic, "Cannot ring bell in silent mode");
  }
}

void playGeigerTone() {
  delay(10); 
  if (digitalRead(DS03) == HIGH) {
    ledcWrite(buzzer_pin, 0);
    for (int i=1; i<20; i++) {
      ledcWriteTone(buzzer_pin, i * 100);
      delay(13);    
    }
    ledcWrite(buzzer_pin, 0);
  }
}

void playRandomTone() {
  if (digitalRead(DS03) == HIGH) {
    Serial.println("Ringing bell");
    for (int i=0; i<=3; i++) {
      for (int i=0; i<=25; i++) {
        ledcWriteTone(buzzer_pin, random(500, 10000));
        delay(100);
        if (digitalRead(exitButton_pin) == LOW) {
          return;
        }
      }
      ledcWriteTone(buzzer_pin, 0);
      delay(1000);
    }
    ledcWriteTone(buzzer_pin, 0);
  }
}

// --- MQTT Functions --- MQTT Functions --- MQTT Functions --- MQTT Functions --- MQTT Functions --- MQTT Functions ---

// Load credentials and apply to WiFiClientSecure instance wifiClient_tls
bool loadTLSClient() {
  Serial.print("Loading TLS client crt and key...");
  client_cert = readFileToBuffer(clCrt_filename); // Client certificate
  client_key = readFileToBuffer(clKey_filename); // Client private key

  if (!client_cert) {
    Serial.print("FAILED: file ");
    Serial.print(clCrt_filename);
    Serial.print("missing or invalid");
    return false;
  } else if (!client_key) {
    Serial.print("FAILED: file ");
    Serial.print(clKey_filename);
    Serial.print("missing or invalid");
    return false;
  }
  wifiClient_tls.setCertificate(client_cert);
  wifiClient_tls.setPrivateKey(client_key);
  Serial.println("OK");
  return true;
}

// Load credentials and apply to WiFiClientSecure instance wifiClient_tls
bool loadTLSCA() {
  Serial.print("Loading TLS CA certificate...");
  ca_cert = readFileToBuffer(caCrt_filename); // Broker root certificate
  if (!ca_cert) {
    Serial.print("FAILED: file ");
    Serial.print(caCrt_filename);
    Serial.print("missing or invalid");
    return false;
  }
  wifiClient_tls.setCACert(ca_cert);
  Serial.println("OK");
  return true;
}

// Helper to read certs into heap-allocated buffer
char* readFileToBuffer(const char* path) {
  File file = FFat.open(path, "r");
  if (!file || file.isDirectory()) {
    Serial.printf("Failed to open %s\n", path);
    return nullptr;
  }

  size_t size = file.size();
  char* buffer = (char*)malloc(size + 1);
  if (!buffer) {
    Serial.printf("Failed to allocate memory for %s\n", path);
    return nullptr;
  }

  file.readBytes(buffer, size);
  buffer[size] = '\0';  // Null-terminate
  file.close();
  return buffer;
}

boolean mqttConnected() {
  if (config.mqtt_tls) {
    if (mqttClient_tls.connected()) {
      return true;
    } else {
      return false;
    }
  } else {
    if (mqttClient.connected()) {
      return true;
    } else {
      return false;
    }
  }
}

boolean mqttPublish(const char* topic, const char* payload) {
  if (config.mqtt_tls) {
    if (mqttClient_tls.connected()) {
      mqttClient_tls.publish(topic, payload);
      return true;
    } else {
      return false;
    }
  } else {
    if (mqttClient.connected()) {
      mqttClient.publish(topic, payload);
      return true;
    } else {
      return false;
    }
  }
}

void startMQTTConnection() {
  mqttClient.setServer(config.mqtt_server, config.mqtt_port);
  mqttClient.setCallback(MQTTcallback);
  delay(100);
  if (mqttClient.connected()) {
    mqttClient.publish(config.mqtt_stat_topic, "locked", true);
  }
  lastMQTTConnectAttempt = millis();
}

void startMQTTConnection_tls() {
  mqttClient_tls.setServer(config.mqtt_server, config.mqtt_port);
  mqttClient_tls.setCallback(MQTTcallback);
  delay(100);
  if (mqttClient_tls.connected()) {
    mqttClient_tls.publish(config.mqtt_stat_topic, "locked", true);
  }
  lastMQTTConnectAttempt = millis();
}

void MQTTcallback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  Serial.print("Message arrived TOPIC:[");
  Serial.print(topic);
  Serial.print("] PAYLOAD:");
  for (int i=0; i<length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  executeCommand((char *)payload);
  return;
}

void maintainConnnectionMQTT() {
  if (!mqttConnected()) {
    if (millis() - lastMQTTConnectAttempt > mqttReconnectInterval) {
      lastMQTTConnectAttempt = millis();
      // Attempt to reconnect
      Serial.print("Attempting reconnection to MQTT broker ");
      Serial.print(config.mqtt_server);
      Serial.println("... ");
      mqttConnect();
    }
  }
}

boolean mqttConnect() {
  if (!(config.mqtt_enabled) || (WiFi.status() != WL_CONNECTED)) {
    return false;
  }
  if (config.mqtt_tls && config.mqtt_auth == false) {
    if (mqttClient_tls.connect(config.mqtt_client_name)) {
      Serial.println("Connected to MQTT broker ");
      mqttPublish(config.mqtt_stat_topic, "Connected to MQTT Broker");
      mqttClient_tls.subscribe(config.mqtt_cmnd_topic);
      publishStats();
      publishStorage();
      return mqttClient_tls.connected();
    }
  } else if (config.mqtt_tls && config.mqtt_auth) {
    if (mqttClient_tls.connect(config.mqtt_client_name, config.mqtt_user, config.mqtt_password)) {
      Serial.println("Connected to MQTT broker ");
      mqttPublish(config.mqtt_stat_topic, "Connected to MQTT Broker");
      mqttClient_tls.subscribe(config.mqtt_cmnd_topic);
      publishStats();
      publishStorage();
      return mqttClient_tls.connected();
    }
  } else if (config.mqtt_tls == false && config.mqtt_auth) {
    if (mqttClient.connect(config.mqtt_client_name, config.mqtt_user, config.mqtt_password)) {
      mqttClient.setBufferSize(512);
      Serial.print("Connected to MQTT broker ");
      Serial.print(config.mqtt_server);
      Serial.print(" on port ");
      Serial.print(config.mqtt_port);
      Serial.print(" as ");
      Serial.println(config.mqtt_client_name);
      mqttPublish(config.mqtt_stat_topic, "Connected to MQTT Broker");
      mqttClient.subscribe(config.mqtt_cmnd_topic);
      publishStats();
      return mqttClient.connected();
    }
  } else {
    if (mqttClient.connect(config.mqtt_client_name)) {
      mqttClient_tls.setBufferSize(512);
      Serial.print("Connected to MQTT broker ");
      Serial.print(config.mqtt_server);
      Serial.print(" as ");
      Serial.println(config.mqtt_client_name);
      mqttPublish(config.mqtt_stat_topic, "Connected to MQTT Broker");
      mqttClient.subscribe(config.mqtt_cmnd_topic);
      publishStats();
      publishStorage();
      return mqttClient.connected();
    }
  }
  Serial.print("Unable to connect to MQTT broker ");
  Serial.println(config.mqtt_server);
  Serial.print("Error: ");
  char lastError[100];
  wifiClient_tls.lastError(lastError,100);  //Get the last error for WiFiClientSecure
  Serial.println(lastError);
  return false;
}

void checkMqtt() {
  if (config.mqtt_tls) {
    if (mqttClient_tls.connected()) {
      mqttClient_tls.loop();
      unsigned long now = millis();
      if (now - lastMsg > 2000) {
        lastMsg = now;
      }
    }
  } else {
    if (mqttClient.connected()) {
      mqttClient.loop();
      unsigned long now = millis();
      if (now - lastMsg > 2000) {
        lastMsg = now;
      }
    }
  }
}

// --- Hardware Revision Functions --- Hardware Revision Functions --- Hardware Revision Functions ---
void detectHardwareRevision() {
  if ((float)(analogRead(attSensor_pin)/4095*3.3) == 0.00) {
    hwRev = 20240812;
    Serial.println("Hardware revision 20240812 detected");
  } else if ((float)(analogRead(attSensor_pin)/4095*3.3) == 0.29) {
    hwRev = 20250000;
    Serial.println("Hardware revision 2025xxxx detected");
  }
}

// --- Serial Commands --- Serial Commands --- Serial Commands --- Serial Commands --- Serial Commands ---

void checkSerialCmd() {
  if (Serial.available()) {
    serialCmd = Serial.readStringUntil('\n');
    serialCmd.trim();
    Serial.print("Command: ");
    Serial.println(serialCmd);
    executeCommand(serialCmd);
  }
}

boolean executeCommand(String command) {
  if (command.equals("list_commands")) {
    listCmnds();
  } else if (command.equals("list_ffat")) {
    listDir(FFat, "/", 0);
  } else if (command.equals("list_sd")) {
    listDir(SD, "/", 0);
  } else if (command.equals("list_keys")) {
    outputKeys();
  } else if (command.equals("purge_addressing")) {
    deleteFile(FFat, addressing_filename);
  } else if (command.equals("purge_keys")) {
    deleteFile(FFat, keys_filename);
    Serial.println("All authorized keys purged");
    mqttPublish(config.mqtt_stat_topic, "All authorized keys purged");
    playTwinkleDownTone();
  } else if (command.equals("purge_config")) {
    deleteFile(FFat, config_filename);
    playTwinkleDownTone();
  } else if (command.equals("add_key_mode")) {
    addKeyMode();
  } else if (command.equals("ring_bell")) {
    ringBell();
  } else if (command.equals("stats")) {
    printStats();
    publishStats();
    publishStorage();
  } else if (command.equals("copy_keys_sd_to_ffat")) {
    keysSDtoFFat();
  } else if (command.equals("copy_config_sd_to_ffat")) {
    configSDtoFFat();
  } else if (command.equals("show_config")) {
    readFile(FFat, config_filename);
  } else if (command.equals("show_version")) {
    Serial.println(codeVersion);
  } else if (command.equals("restart")) {
    restart();
  } else if (command.equals("reboot")) {
    restart();
  } else if (command.equals("unlock")) {
    unlock(config.cmndUnlockDur_S);
  } else if ((command.equals("garage_toggle")&& garage_mode)) {
    Serial.println("Toggling garage door");
    mqttPublish(config.mqtt_stat_topic, "Toggling garage door");
    garage_toggle();
  } else if ((command.equals("garage_open")&& garage_mode)) {
    Serial.println("Toggling garage door");
    mqttPublish(config.mqtt_stat_topic, "Opening garage door");
    garage_open();
  } else if ((command.equals("garage_close")&& garage_mode)) {
    Serial.println("Closing garage door");
    mqttPublish(config.mqtt_stat_topic, "Closing garage door");
    garage_close();
  } else {
    Serial.println("bad command");
    return false;
  }
  return true;
}

void listCmnds() {
  if (mqttConnected()) {
    mqttPublish(config.mqtt_stat_topic, "add_key_mode\ncopy_config_sd_to_ffat\ncopy_keys_sd_to_ffat\ngarage_close\ngarage_open\ngarage_toggle\nlist_commands\nlist_ffat\nlist_keys\nlist_sd\npurge_addressing\npurge_config\npurge_keys\nrestart\nring_bell\nshow_config\nshow_version\nstats\nunlock");
  }
  Serial.printf("add_key_mode\ncopy_config_sd_to_ffat\ncopy_keys_sd_to_ffat\ngarage_close\ngarage_open\ngarage_toggle\nlist_commands\nlist_ffat\nlist_keys\nlist_sd\npurge_addressing\npurge_config\npurge_keys\nrestart\nring_bell\nshow_config\nshow_version\nstats\nunlock");
}

// --- FTP Functions --- FTP Functions --- FTP Functions --- FTP Functions --- FTP Functions --- FTP Functions ---

void _callback(FtpOperation ftpOperation, unsigned int freeSpace, unsigned int totalSpace){
  switch (ftpOperation) {
    case FTP_CONNECT:
      Serial.println(F("FTP client connected"));
      break;
    case FTP_DISCONNECT:
      Serial.println(F("FTP client disconnected"));
      break;
    case FTP_FREE_SPACE_CHANGE:
      Serial.printf("FTP change in free space, free %u of %u\n", freeSpace, totalSpace);
      break;
    default:
      break;
  }
};

void _transferCallback(FtpTransferOperation ftpOperation, const char* name, unsigned int transferredSize){
  switch (ftpOperation) {
    case FTP_UPLOAD_START:
      Serial.println(F("FTP upload started"));
      break;
    case FTP_UPLOAD:
      Serial.printf("FTP upload of file %s bytes %u\n", name, transferredSize);
      break;
    case FTP_TRANSFER_STOP:
      Serial.println(F("FTP transfer completed"));
      break;
    case FTP_TRANSFER_ERROR:
      Serial.println(F("FTP transfer error"));
      break;
    default:
      break;
  }
};

// --- Web Functions --- Web Functions --- Web Functions --- Web Functions --- Web Functions --- Web Functions ---

void removeNewlines(char* str) {
  int readIndex = 0;
  int writeIndex = 0;

  while (str[readIndex]) {
    if (str[readIndex] != '\n' && str[readIndex] != '\r') {
      str[writeIndex++] = str[readIndex];
    }
    readIndex++;
  }
  str[writeIndex] = '\0'; // Null-terminate the cleaned string
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += webServer.uri();
  message += "\nMethod: ";
  message += (webServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webServer.args();
  message += "\n";
  for (uint8_t i = 0; i < webServer.args(); i++) {
    message += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
  }
  webServer.send(404, "text/plain", message);
}

// Output a list of allowed keys to serial
void outputKeys() {
  Serial.print("Reading file: ");
  Serial.println(keys_filename);
  File outFile = FFat.open(keys_filename);
  while (outFile.available()) {
    Serial.write(outFile.read());
  }
}

// Restart unit
void restartESPHTTP() {
  webServer.sendHeader("Location", "/",true);  
  webServer.send(302, "text/plain", "");
  delay(1000);
  restart();
}

// Display allowed keys in webUI
void displayKeys() {
  //pageContent += "      <br/>\n";
  pageContent += "      <table class='keyTable'>\n";
  char buffer[64];
  int count = 0;
  File keyFile = FFat.open(keys_filename);
  while (keyFile.available()) {
    int l = keyFile.readBytesUntil('\n', buffer, sizeof(buffer));
    if (l > 0 && buffer[l - 1] == '\r') {
      buffer[l - 1] = 0; // if it's using Windows line endings, remove that too
    } else {
      buffer[l] = 0;
    }
    pageContent += "        <tr><td class='keyCell'>";
    pageContent += buffer;
    pageContent += "</td><td class='keyDelCell'>";
    pageContent += "<a class='keyDelLink' href='/remKey/";
    pageContent += buffer;
    pageContent += "'>DELETE</a>";
    pageContent += "</td></tr>\n";
    count++;
  }
  if (count < 1) {
    pageContent += "        <tr><td class='keyCell'>[NO SAVED KEYS]</td></tr>\n";
  }
  pageContent += "      </table>\n";
  keyFile.close();
  pageContent += "      <form action='/addFormKey/' method='get'>\n";
  pageContent += "        <input type='text' id='key' name='key' class='addKeyInput'></input>\n";
  pageContent += "        <input type='submit' value='ADD' class='addKeyButton' required>\n";
  pageContent += "      </form>\n";
  pageContent += "      <br/>\n";
  
}

void bellSelect() {
  pageContent += "      <a class='header'>Bell Melody</a>\n";
  pageContent += "      <div class='inputRow'>\n";
  pageContent += "        <form action='/setFormBell/' method='get'>\n";
  pageContent += "          <select class='targetBell' name='targetBell'>\n";
  pageContent += "            <option value='";
  pageContent += bell_filename;
  pageContent += "'>[Select bell melody]</option>\n";
  File root = FFat.open("/");
  File file = root.openNextFile();
  while (file) {
    String fileName = file.name();
    if (fileName.endsWith(".bell")) {
      pageContent += "          <option value='";
      pageContent += fileName;
      pageContent += "'>";
      pageContent += fileName;
      pageContent += "</option>\n";
    }
    file = root.openNextFile();
  }
  pageContent += "          </select>\n";
  pageContent += "          <input class='targetBellButton' type='submit' name='play' value='PLAY'>\n";
  pageContent += "          <input class='targetBellButton' type='submit' name='select' value='SELECT'>\n";
  pageContent += "        </form>\n";
  pageContent += "      </div>\n";
  pageContent += "      <br/>\n";
}

const char* updateScript = R"rawliteral(
      <script>
        function uploadFirmware() {
          var file = document.getElementById("firmware").files[0];
          if (!file) {
            alert("Please choose a file first!");
            return;
          }
          document.getElementById("progress").style.visibility='visible';
          var xhr = new XMLHttpRequest();
          xhr.upload.addEventListener("progress", function(evt) {
            if (evt.lengthComputable) {
              var percent = (evt.loaded / evt.total) * 100;
              document.getElementById("progress").value = percent;
            }
          }, false);
          xhr.open("POST", "/update_post", true);
          xhr.onload = function () {
            if (xhr.status == 200) {
              alert(xhr.responseText);
              setTimeout(function(){ location.reload(); }, 3000);
            } else {
              alert("Error: " + xhr.responseText);
            }
          };
          var formData = new FormData();
          formData.append("firmware", file);
          xhr.send(formData);
        }
      </script>
      <script>
        function startGithubUpdate() {
          if (!confirm("Start GitHub OTA update?\nDevice will restart after update.")) return;
          const progress = document.getElementById("progress");
          progress.style.visibility = 'visible';
          progress.value = 5;
          const interval = setInterval(() => {
            if (progress.value < 90) {
              progress.value += 5;
            }
          }, 500); // fake smooth progress
          fetch('/githubOta')
          .then(res => {
            clearInterval(interval);
            progress.value = 100;
            alert("Update started.\nIf successful, device will reboot shortly.");
          })
          .catch(err => {
            clearInterval(interval);
            progress.style.visibility = 'hidden';
            alert("Error starting update: " + err.message);
          });
        }
      </script>
)rawliteral";

const char* updateForm = R"rawliteral(
      <a class="header">Firmware Update</a>
      <br/>
      <button onclick="startGithubUpdate()">Update from latest GitHub Release</button>
      <br/>
      <div class="inputRow">
        <input type="file" name="file" class="fileMgInputFile" id="firmware" required>
        <button type="button" class="fileMgInputButton" onclick="uploadFirmware()">UPDATE</button>
      </div>
      <progress id="progress" class="uploadBar" value="0" max="100" style="visibility: hidden;"></progress><br/>
)rawliteral";

void updateControls() {
  pageContent += updateScript;
  pageContent += updateForm;
}

// Display file management pane
void displayFiles() {
  pageContent += "      <div id='previewModal' class='previewModal' style='display:none; position:fixed; border:2px solid #000; z-index:1000; overflow:auto;'>\n";
  pageContent += "        <pre id='previewContent' class='previewContent' style='max-height:400px; overflow:auto; text-align:left;'></pre>\n";
  pageContent += "        <button onclick='closePreview()' style='width:100%; height:40px; background-color:#ff3200; color:black; border:none; font-size:16px;'>Close</button>\n";
  pageContent += "      </div>\n";
  pageContent += "      <script>\n";
  pageContent += "        function closePreview() {\n";
  pageContent += "          document.getElementById('previewModal').style.display = 'none';\n";
  pageContent += "        }\n";
  pageContent += "        async function previewFile(encodedName) {\n";
  pageContent += "          const res = await fetch('/previewFile/' + encodeURIComponent(encodedName));\n";
  pageContent += "          const text = await res.text();\n";
  pageContent += "          document.getElementById('previewContent').innerText = text;\n";
  pageContent += "          document.getElementById('previewModal').style.display = 'block';\n";
  pageContent += "        }\n";
  pageContent += "        function showMessage(message) {\n";
  pageContent += "          document.getElementById('previewContent').innerText = message;\n";
  pageContent += "          document.getElementById('previewModal').style.display = 'block';\n";
  pageContent += "        }\n";
  pageContent += "      </script>\n";
  pageContent += "      <a class='header'>File Management</a>\n";
  pageContent += "      <br/>\n";
  pageContent += "      <table class='fileTable'>\n";
  int count = 0;
  File root = FFat.open("/");
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      String name = String(file.name());
      if (name.startsWith("/")) {
        name = name.substring(1);
      }
      pageContent += "        <tr><td class='fileCell'>";
      pageContent += (name + " (" + String(file.size()) + " bytes)");
      pageContent += "</td><td class='fileMgCell'>";
      pageContent += "<a class='fileMgLink' href='#' onclick='previewFile(\"";
      pageContent += name;
      pageContent += "\")'>VIEW</a>";
      pageContent += "</td><td class='fileMgCell'>";
      pageContent += "<a class='fileMgLink' href='/downloadFile/";
      pageContent += (name + " ");
      pageContent += "'>DLOAD</a>";
      pageContent += "</td><td class='fileMgCell'>";
      pageContent += "<a class='fileMgLink' href='/deleteFile/";
      pageContent += (name + " ");
      pageContent += "'>DEL</a>";
      pageContent += "</td>";
      pageContent += "</tr>\n";
      count++;
    }
    file = root.openNextFile();
  }
  if (count < 1) {
    pageContent += "        <tr><td class='fileCell'>";
    pageContent += "[NO FILES]";
    pageContent += "</td></tr>\n";
  }
  pageContent += "      </table>\n";
  pageContent += "      <form method='POST' action='/upload' enctype='multipart/form-data' style='margin-top:5px;'>\n";
  pageContent += "        <div class='inputRow'>\n";
  pageContent += "          <input type='file' name='file' class='fileMgInputFile' required>\n";
  pageContent += "          <input type='submit' class='fileMgInputButton' value='UPLOAD'>\n";
  pageContent += "        </div>\n";
  pageContent += "      </form>\n";
  pageContent += "      <br/>";
}

// Output config to serial
void outputConfig() {
  Serial.print("Reading file: ");
  Serial.println(config_filename);
  File outFile = FFat.open(config_filename);
  while (outFile.available()) {
    Serial.write(outFile.read());
  }
  webServer.sendHeader("Location", "/",true);
  webServer.send(302, "text/plain", "");
}

void MainPage() {
  sendHTMLHeader();
  siteButtons();
  displayKeys();
  bellSelect();
  displayFiles();
  updateControls();
  statsPanel();
  siteModes();
  siteFooter();
  sendHTMLContent();
  sendHTMLStop();
}

void unlockHTTP() {
  webServer.sendHeader("Location", "/",true);  
  webServer.send(302, "text/plain", "");
  Serial.println("Unlocked via HTTP");
  if (mqttConnected()) {
    mqttPublish(config.mqtt_stat_topic, "HTTP Unlock");
  }  
  unlock(config.httpUnlockDur_S);
}

void garageToggleHTTP() {
  webServer.sendHeader("Location", "/",true);  
  webServer.send(302, "text/plain", "");
  Serial.println("Garage Door toggled HTTP");
  if (mqttConnected()) {
    mqttPublish(config.mqtt_stat_topic, "Garage Door toggled HTTP");
  }  
  garage_toggle();
}

void garageOpenHTTP() {
  webServer.sendHeader("Location", "/",true);  
  webServer.send(302, "text/plain", "");
  if (doorOpen == false) {
    Serial.println("Garage Door opened HTTP");
    if (mqttConnected()) {
      mqttPublish(config.mqtt_stat_topic, "Garage Door opened HTTP");
    }  
    garage_open();
  }
}

void garageCloseHTTP() {
  webServer.sendHeader("Location", "/",true);  
  webServer.send(302, "text/plain", "");
  if (doorOpen) {
    Serial.println("Garage Door closed via HTTP");
    if (mqttConnected()) {
      mqttPublish(config.mqtt_stat_topic, "Garage Door closed via HTTP");
    }
    garage_close();
  }
}

void configSDtoFFatHTTP() {
  configSDtoFFat();
  webServer.sendHeader("Location", "/",true);
  webServer.send(302, "text/plain", "");
}

void configFFattoSDHTTP() {
  webServer.sendHeader("Location", "/",true);
  webServer.send(302, "text/plain", "");
  configFFattoSD();
}

void keysSDtoFFatHTTP() {
  webServer.sendHeader("Location", "/",true);  
  webServer.send(302, "text/plain", "");
  keysSDtoFFat();
}

void keysFFattoSDHTTP() {
  webServer.sendHeader("Location", "/",true);  
  webServer.send(302, "text/plain", "");
  keysFFattoSD();
}

void purgeKeysHTTP() {
  deleteFile(FFat, keys_filename);
  playTwinkleDownTone();
  webServer.sendHeader("Location", "/",true);  
  webServer.send(302, "text/plain", "");
}

void addKeyModeHTTP() {
  webServer.sendHeader("Location", "/",true);  
  webServer.send(302, "text/plain", "");
  addKeyMode();
}

void purgeConfigHTTP() {
  webServer.sendHeader("Location", "/",true);  
  webServer.send(302, "text/plain", "");
  deleteFile(FFat, config_filename);
}

int FFat_file_download(String filename) {
  if (FFat_present) {
    File download = FFat.open(filename);
    if (download) {
      webServer.sendHeader("Content-Type", "text/text");
      webServer.sendHeader("Content-Disposition", "attachment; filename=" + filename);
      webServer.sendHeader("Connection", "close");
      webServer.streamFile(download, "application/octet-stream");
      download.close();
      Serial.print("Downloading file ");
      Serial.println(filename);
      return 0;
    }
  }
  return 1;
}

void ringBellHTTP() {
  webServer.sendHeader("Location", "/",true);
  webServer.send(302, "text/plain", "");
  ringBell();
}

int parseSDAddressingFile () {
  //TODO
}

void saveAddressingStaticHTTP() {
  webServer.sendHeader("Location", "/",true);  
  webServer.send(302, "text/plain", "");
  Serial.println("Saving current addressing to static file");
  if (mqttConnected()) {
    mqttPublish(config.mqtt_stat_topic, "Saving current addressing to static file");
  } 
  JsonDocument addressing_doc;
  addressing_doc["localIP"] = WiFi.localIP().toString();
  addressing_doc["subnetMask"] = WiFi.subnetMask().toString();
  addressing_doc["gatewayIP"] = WiFi.gatewayIP().toString();
  if (FFat.exists(addressing_filename)){
    deleteFile(FFat, addressing_filename);
  }
  File addressing_file = FFat.open(addressing_filename, FILE_WRITE);
  serializeJsonPretty(addressing_doc, addressing_file);
  addressing_file.close();
  restart();
}

void addressingStaticSDtoFFatHTTP() {
  webServer.sendHeader("Location", "/",true);
  webServer.send(302, "text/plain", "");
  addressingSDtoFFat();
}

void githubOta() {
  webServer.sendHeader("Location", "/",true);  
  webServer.send(302, "text/plain", "");
  delay(2000);
  Serial.println("Performing OTA update from github");
  handleUrlOTA(releaseRepoUrl, releaseRepoPort, releaseRepoPath);
}

void purgeAddressingStaticHTTP() {
  webServer.sendHeader("Location", "/",true);  
  webServer.send(302, "text/plain", "");
  deleteFile(FFat, addressing_filename);
  if (mqttConnected()) {
    mqttPublish(config.mqtt_stat_topic, "Static addressing file purged");
  }
  playTwinkleDownTone();
  Serial.println("Static addressing file purged");
}

void sendHTMLHeader() {
  webServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  webServer.sendHeader("Pragma", "no-cache");
  webServer.sendHeader("Expires", "-1");
  webServer.setContentLength(CONTENT_LENGTH_UNKNOWN);
  webServer.send(200, "text/html", "");
  siteHeader();
  webServer.sendContent(pageContent);
  pageContent = "";
}

void sendHTMLContent() {
  webServer.sendContent(pageContent);
  pageContent = "";
}

void sendHTMLStop() {
  webServer.sendContent("");
  webServer.client().stop();
}

const char* html_head = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <style>
      .mainDiv {width: 350px; margin: 20px auto; text-align: center; border: 3px solid #ff3200; background-color: #555555; left: auto; right: auto;}
      .header {font-family: Arial, Helvetica, sans-serif; font-size: 20px; color: #ff3200;}
      .smalltext {font-family: Arial, Helvetica, sans-serif; font-size: 13px; color: #ff3200;}
      .previewModal {display:none; position:fixed; top:50%; left:50%; transform:translate(-50%,-50%); width:400px; background:white; border:2px solid black; padding:10px; z-index:1000;}
      .previewModal button {margin-top: 5px; margin-right: 10px;}
      .previewContent {max-height: 400px; overflow: auto; text-align: left;}
      .keyTable {background-color: #303030; font-size: 11px; width: 300px; resize: vertical; margin-left: auto; margin-right: auto; margin-top: 3px; border: 1px solid #ff3200; border-collapse: collapse;}
      .keyCell {height: 15px; color: #ff3200;}
      .keyDelCell {height: 15px; width: 45px; background-color: #ff3200; color: black;}
      .keyDelCell:hover {height: 15px; width: 45px; background-color: #ef2200; color: black; border: 1px solid #000000}
      .keyDelLink {font-family: Arial, Helvetica, sans-serif; font-size: 10px; color: black; text-decoration: none;}
      .fileTable {background-color: #303030; font-size: 11px; width: 300px; resize: vertical; margin-left: auto; margin-right: auto; border: 1px solid #ff3200; border-collapse: collapse;}
      .fileCell {height: 15px; color: #ff3200;}
      .fileMgCell {height: 15px; width: 45px; background-color: #ff3200; color: black; border: 1px solid #000000}
      .fileMgCell:hover {height: 15px; width: 45px; background-color: #ef2200; color: black; border: 1px solid #000000}
      .fileMgLink {font-family: Arial, Helvetica, sans-serif; font-size: 10px; color: black; text-decoration: none;}
      .fileMgInputFile {height: 18px; width: 235px; margin-left:0px; background-color: #ff3200; color: black; font-family:Arial, Helvetica, sans-serif; font-size: 10px; border: 1px solid #000000; vertical-align:middle}
      .fileMgInputFile:hover {height: 18px; width: 235px; margin-left:0px; background-color: #ef2200; color: font-family:Arial, Helvetica, sans-serif; font-size: 10px; black; border: 1px solid #000000}
      .fileMgInputButton {height: 18px; width: 60px; margin-left:0px; background-color: #ff3200; color: black; font-family:Arial, Helvetica, sans-serif; font-size: 10px; border: 1px solid #000000; vertical-align:middle}
      .fileMgInputButton:hover {height: 18px; width: 60px; margin-left:0px; background-color: #ef2200; color: font-family:Arial, Helvetica, sans-serif; font-size: 10px; black; border: 1px solid #000000}
      .targetBell {height: 18px; width: 175px; margin-left:0px; background-color: #ff3200; color: black; font-family:Arial, Helvetica, sans-serif; font-size: 10px; border: 1px solid #000000; vertical-align:middle}
      .targetBell:hover {height: 18px; width: 175px; margin-left:0px; background-color: #ef2200; color: font-family:Arial, Helvetica, sans-serif; font-size: 10px; black; border: 1px solid #000000}
      .targetBellButton {height: 18px; width: 60px; margin-left:0px; background-color: #ff3200; color: black; font-family:Arial, Helvetica, sans-serif; font-size: 10px; border: 1px solid #000000; vertical-align:middle}
      .targetBellButton:hover {height: 18px; width: 60px; margin-left:0px; background-color: #ef2200; color: font-family:Arial, Helvetica, sans-serif; font-size: 10px; black; border: 1px solid #000000}
      .inputRow {display: flex; flex-direction: row; gap: 2px; justify-content: center; align-items: center; border: 0;}
      .updateInput {height: 18px; margin-left:0px; background-color: #ff3200; color: black; font-family:Arial, Helvetica, sans-serif; font-size: 10px; border: 1px solid #000000; vertical-align:middle}
      .updateInput:hover {height: 18px; margin-left:0px; background-color: #ef2200; color: font-family:Arial, Helvetica, sans-serif; font-size: 10px; black; border: 1px solid #000000}
      .uploadBar {width:300px;}
      .statsBox {background-color: #303030; font-size: 11px; width: 300px; height: 130px; resize: vertical; color: #ff3200;}
      .orangeButton {height 30px; width: 49px; auto;background-color: #ff3200; border: none; font-family:Arial, Helvetica, sans-serif; font-size: 10px; color: black; border: 1px solid #ff3200;}
      .orangeButton:hover {height 30px; width: 49px; background-color: #ef2200; border: none; font-family: Arial, Helvetica, sans-serif; font-size: 10px; color: black; border: 1px solid #ff3200;}
      .addKeyInput {height 17px; width: 245px;border: 1px solid #ff3200; font-family:Arial, Helvetica, sans-serif; font-size: 10px; color: black;}
      .addKeyButton {height 30px; width: 49px; background-color: #ff3200; border: none; font-family:Arial, Helvetica, sans-serif; font-size: 10px; color: black; border: 1px solid #ff3200;}
      .addKeyButton:hover {height 30px; width: 49px; background-color: #ef2200; border: none; font-family: Arial, Helvetica, sans-serif; font-size: 10px; color: black; border: 1px solid #ff3200;}
      tr, td {border: 1px solid #ff3200;}
      button {width: 300px; background-color: #ff3200; border: none; text-decoration: none;}
      button:hover {width: 300px; background-color: #ef2200; border: none; text-decoration: none;}
      h1 {font-family: Arial, Helvetica, sans-serif; color: #ff3200;}
      h3 {font-family: Arial, Helvetica, sans-serif; color: #ff3200;}
      a {font-family: Arial, Helvetica, sans-serif; font-size: 10px; color: #ff3200;}
      body {background-color: #303030; text-align: center;}
    </style>
  </head>
)rawliteral";

void siteHeader() {
  pageContent = html_head;
  pageContent += "  <body>\n";
  pageContent += "    <div class='mainDiv'>\n";
  pageContent += "      <h1>DL32 MENU</h1>\n";
  pageContent += "      <h3>";
  pageContent += config.mqtt_client_name;
  pageContent += "      </h3>\n";
}

void siteModes() {
  int modeCount = 0;
  pageContent += "      <a class='smalltext'>";
  if (forceOffline) {
    pageContent += " [Forced Offline Mode] ";
    modeCount++;
  }
  if (failSecure == false) {
    pageContent += " [Failsafe Mode] ";
    modeCount++;
  }
  if (digitalRead(DS03) == LOW) {
    pageContent += " [Silent Mode] ";
    modeCount++;
  }
  if (garage_mode) {
    pageContent += " [Garage Mode] ";
    modeCount++;
  }
  pageContent += "</a>\n";
  if (modeCount > 0) {
    pageContent += "      <br/>\n";
  }
}

void siteButtons() {
  pageContent += "      <a class='header'>Device Control</a>\n";
  if (garage_mode) {
    pageContent += "      <a href='/garageToggleHTTP'><button>Toggle</button></a>\n";
    pageContent += "      <br/>\n";
    if (doorOpen == false) {
      pageContent += "      <a href='/garageOpenHTTP'><button>Open</button></a>\n";
    }
    if (doorOpen) {
      pageContent += "      <a href='/garageCloseHTTP'><button>Close</button></a>\n";
    }
  } else {
    pageContent += "      <a href='/unlockHTTP'><button>Unlock</button></a>\n";
  }
  pageContent += "      <br/>\n";
  pageContent += "      <a href='/ringBellHTTP'><button>Ring Bell</button></a>\n";
  pageContent += "      <br/>\n";
  pageContent += "      <a href='/restartESPHTTP'><button>Restart DL32</button></a>\n";
  pageContent += "      <br/>\n";
  pageContent += "      <br/>\n";
  pageContent += "      <a class='header'>System</a>\n";
  pageContent += "      <a href='/downloadFile";
  pageContent += config_filename;
  pageContent += "      '><button>Download config file</button></a>\n";
  pageContent += "      <br/>\n";
  pageContent += "      <a href='/configSDtoFFatHTTP'><button>Upload config SD to DL32</button></a>\n";
  pageContent += "      <br/>\n";
  pageContent += "      <a href='/configFFattoSDHTTP'><button>Download config DL32 to SD</button></a>\n";
  pageContent += "      <br/>\n";
  pageContent += "      <a href='/purgeConfigHTTP'><button>Purge configuration</button></a>\n";
  pageContent += "      <br/><br/>\n";
  pageContent += "      <a class='header'>IP Addressing</a>\n";
  if (staticIP) {
    pageContent += "      <a href='/downloadFile";
    pageContent += addressing_filename;
    pageContent += "      '><button>Download static addressing file</button></a>\n";
    pageContent += "      <br/>\n";
  }
  pageContent += "      <a href='/addressingStaticSDtoFFatHTTP'><button>Upload static addressing SD to DL32</button></a>\n";
  pageContent += "      <br/>\n";
  if (staticIP == false) {
    pageContent += "      <a href='/saveAddressingStaticHTTP'><button>Save current addressing as static</button></a>\n";
    pageContent += "      <br/>\n";
  }
  if (staticIP) {
    pageContent += "      <a href='/purgeAddressingStaticHTTP'><button>Purge static addressing</button></a>\n";
    pageContent += "      <br/>\n";
  }
  pageContent += "      <br/>\n";
  pageContent += "      <a class='header'>Key Management</a>\n";
  pageContent += "      <a href='/downloadFile";
  pageContent += keys_filename;
  pageContent += "      ''><button>Download key file</button></a>\n";
  pageContent += "      <br/>\n";
  pageContent += "      <a href='/keysSDtoFFatHTTP'><button>Upload keys SD to DL32</button></a>\n";
  pageContent += "      <br/>\n";
  pageContent += "      <a href='/keysFFattoSDHTTP'><button>Download keys DL32 to SD</button></a>\n";
  pageContent += "      <br/>\n";
  pageContent += "      <a href='/addKeyModeHTTP'><button>Enter add key mode</button></a>\n";
  pageContent += "      <br/>\n";
  if (FFat.exists(keys_filename)) {
    pageContent += "      <a href='/purgeKeysHTTP'><button>Purge stored keys</button></a>\n";
    pageContent += "      <br/>\n";
  } 
}

void siteFooter() {
  IPAddress ip_addr = WiFi.localIP();
  pageContent += "      <a class='smalltext'>";
  pageContent += "IP: ";
  pageContent += (String(ip_addr[0]) + "." + String(ip_addr[1]) + "." + String(ip_addr[2]) + "." + String(ip_addr[3]));
  if (staticIP) {
    pageContent += "<sup>(S)</sup>";
  } else {
    pageContent += "<sup>(D)</sup>";
  }
  pageContent += "</a>";
  pageContent += "&nbsp;&nbsp;&nbsp;&nbsp;";
  pageContent += "<a class='smalltext'>";
  pageContent += "ver: ";
  pageContent += (String(codeVersion));
  pageContent += "&nbsp;&nbsp;&nbsp;&nbsp;";
  pageContent += "</a>";
  pageContent += "<a class='smalltext' href='https://github.com/Mark-Roly/dl32-arduino' target='_blank' rel='noopener noreferrer'>";
  pageContent += "github";
  pageContent += "</a>\n";
  pageContent += "      <br/><br/>\n";
  pageContent += "    </div>\n";
  pageContent += "  </body>\n";
  pageContent += "</html>\n";
}

void echoUri() {
  webServer.send(200, "text//plain", webServer.uri() );
}

void startFTPServer() {
  ftpSrv.begin(config.ftp_user, config.ftp_password);
  Serial.print("FTP Server started at ");
  Serial.print(WiFi.localIP());
  Serial.print(" on port ");
  Serial.println(config.ftp_port);
}

void startWebServer() {
  webServer.on("/unlockHTTP", unlockHTTP);
  webServer.on("/garageToggleHTTP", garageToggleHTTP);
  webServer.on("/garageOpenHTTP", garageOpenHTTP);
  webServer.on("/garageCloseHTTP", garageCloseHTTP);
  webServer.on("/ringBellHTTP", ringBellHTTP);
  webServer.on("/addKeyModeHTTP", addKeyModeHTTP);
  webServer.on("/purgeKeysHTTP", purgeKeysHTTP);
  webServer.on("/purgeConfigHTTP", purgeConfigHTTP);
  webServer.on("/restartESPHTTP", restartESPHTTP);
  webServer.on("/configSDtoFFatHTTP", configSDtoFFatHTTP);
  webServer.on("/configFFattoSDHTTP", configFFattoSDHTTP);
  webServer.on("/keysSDtoFFatHTTP", keysSDtoFFatHTTP);
  webServer.on("/keysFFattoSDHTTP", keysFFattoSDHTTP);
  webServer.on("/saveAddressingStaticHTTP", saveAddressingStaticHTTP);
  webServer.on("/addressingStaticSDtoFFatHTTP", addressingStaticSDtoFFatHTTP);
  webServer.on("/purgeAddressingStaticHTTP", purgeAddressingStaticHTTP);
  webServer.on("/githubOta", githubOta);
  webServer.on(UriRegex("/setBell/([\\w\\.\\-]{1,32})"), HTTP_GET, [&]() {
    Serial.print("Setting bell tone from url ");
    Serial.println(webServer.pathArg(0));
    setCurrentBell(webServer.pathArg(0));
    webServer.sendHeader("Location", "/",true);
    webServer.send(302, "text/plain", "");
  });
  webServer.on("/setFormBell/", HTTP_GET, [&]() {
    if (webServer.hasArg("play")) {
      Serial.print("Playing bell: ");
      Serial.println(webServer.arg("targetBell"));
      playBellFile("/" + webServer.arg("targetBell"));
      webServer.sendHeader("Location", "/",true);
      webServer.send(302, "text/plain", "");
    } else if (webServer.hasArg("select")) {
      Serial.print("Setting bell tone from WebUI: ");
      Serial.println(webServer.arg("targetBell"));
      setCurrentBell(webServer.arg("targetBell"));
      webServer.sendHeader("Location", "/",true);
      webServer.send(302, "text/plain", "");
    }
  });
  webServer.on(UriRegex("/addKey/([0-9a-zA-Z]{4,8})"), HTTP_GET, [&]() {
    writeKey(webServer.pathArg(0));
    webServer.sendHeader("Location", "/",true);
    webServer.send(302, "text/plain", "");
  });
  webServer.on("/update_post", HTTP_POST, []() {
    webServer.sendHeader("Connection", "close");
    if (Update.hasError()) {
      webServer.send(200, "text/plain", "Update Failed!");
    } else {
      webServer.send(200, "text/plain", "Update Successful. Restarting...");
      delay(100);
      restart();
    }
  }, handleUpdateUpload);
  webServer.on(UriRegex("/remKey/([0-9a-zA-Z]{3,16})"), HTTP_GET, [&]() {
    removeKey(webServer.pathArg(0));
    webServer.sendHeader("Location", "/",true);
    webServer.send(302, "text/plain", "");
  });
  webServer.on(UriRegex("/previewFile/([\\w\\.\\-]{1,32})"), HTTP_GET, [&]() {
    previewFile(webServer.pathArg(0));
  });
  webServer.on(UriRegex("/downloadFile/([0-9a-zA-Z.\\-_]{3,32})"), HTTP_GET, []() {
    String filename = "/" + webServer.pathArg(0);
    if (FFat.exists(filename)) {
      File file = FFat.open(filename, FILE_READ);
      webServer.sendHeader("Content-Type", "application/octet-stream");
      webServer.sendHeader("Content-Disposition", "attachment; filename=\"" + String(webServer.pathArg(0)) + "\"");
      webServer.streamFile(file, "application/octet-stream");
      file.close();
    } else {
      webServer.send(404, "text/plain", "File not found");
    }
  });
  webServer.on(UriRegex("/deleteFile/(.+)"), HTTP_GET, [&]() {
    String encoded = webServer.pathArg(0);
    String path = "/" + urlDecode(encoded);
    deleteFile(FFat, path.c_str());
    playTwinkleDownTone();
    webServer.sendHeader("Location", "/",true);
    webServer.send(302, "text/plain", "");
  });
  webServer.on(UriRegex("/serial/([0-9a-zA-Z_-]{3,10})"), HTTP_GET, [&]() {
    Serial.print("URL Command entered: ");
    Serial.print(webServer.pathArg(0));
    executeCommand(webServer.pathArg(0));
    webServer.sendHeader("Location", "/",true);
    webServer.send(302, "text/plain", "");
  });
  webServer.on("/upload", HTTP_POST, []() {
    webServer.sendHeader("Location", "/",true);
    webServer.send(302, "text/plain", "");
    playTwinkleUpTone();
  }, handleFileUpload);
  webServer.on("/", MainPage);
  webServer.on(UriRegex("/addFormKey/.{0,20}"), HTTP_GET, [&]() {
    Serial.print("Writing key ");
    Serial.print(webServer.arg("key"));
    Serial.println(" from webUI input.");
    writeKey(webServer.arg("key"));
    webServer.sendHeader("Location", "/",true);
    webServer.send(302, "text/plain", "");
  });
  webServer.begin();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Web Server started at http://");
    Serial.println(WiFi.localIP());
  }
}

// --- File Upload Functions --- File Upload Functions --- File Upload Functions --- File Upload Functions --- File Upload Functions ---

void handleFileUpload() {
  HTTPUpload& upload = webServer.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = "/" + upload.filename;
    uploadFile = FFat.open(filename, FILE_WRITE);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
    }
  }
}

// --- OTA Functions --- OTA Functions --- OTA Functions --- OTA Functions --- OTA Functions --- OTA Functions --- OTA Functions ---

void handleUpdateUpload() {
  HTTPUpload& upload = webServer.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Starting OTA: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) { // Explicitly target FLASH
      Update.printError(Serial);
    }
  } 
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } 
  else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) { // true to set the size automatically
      Serial.printf("Update Success: %u bytes\nRebooting...\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

bool handleUrlOTA(const char* host, uint16_t port, const char* path) {
    Serial.printf("[OTA] Connecting to %s:%u...\n", host, port);
    if (!wifiClient.connect(host, port)) {
        Serial.println("[OTA] Connection failed!");
        return false;
    }

    // Use wifiClient throughout the function
    Serial.printf("[OTA] Downloading file %s...\n", path);
    wifiClient.printf("GET %s HTTP/1.1\r\n", path);
    wifiClient.printf("Host: %s\r\n", host);
    wifiClient.println("Connection: close\r\n");

    // Wait for and skip headers
    while (wifiClient.connected()) {
        String line = wifiClient.readStringUntil('\n');
        if (line == "\r") break;
    }

    // Start OTA with unknown size
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Serial.println("[OTA] Not enough space for OTA");
        return false;
    }

    Serial.println("[OTA] Starting OTA...");

    uint8_t buff[1024];
    size_t written = 0;

    // Read until connection closes
    while (wifiClient.connected() || wifiClient.available()) {
        size_t len = wifiClient.available();
        if (len) {
            int readBytes = wifiClient.read(buff, std::min(len, sizeof(buff)));
            if (readBytes > 0) {
                size_t writtenNow = Update.write(buff, readBytes);
                if (writtenNow != readBytes) {
                    Serial.println("[OTA] Write error");
                    return false;
                }
                written += writtenNow;
            }
        }
        delay(1);
    }

    Serial.printf("[OTA] Written: %u bytes\n", written);

    if (!Update.end(true)) {
      Serial.printf("[OTA] Update failed: %s\n", Update.errorString());
      return false;
    }

    if (!Update.isFinished()) {
        Serial.println("[OTA] Update incomplete");
        return false;
    }

    Serial.println("[OTA] Success! Rebooting...");
    delay(1000);
    ESP.restart();
    return true;
}


// --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP --- SETUP ---

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("");
  Serial.println("======================");
  Serial.println("|  STARTUP SEQUENCE  |");
  Serial.println("======================");
  detectHardwareRevision();
  Serial.print("Firmware version ");
  Serial.println(codeVersion);
  Serial.print("Initializing WDT...");
  secondTick.attach(1, ISRwatchdog);
  Serial.println("OK");
  fatfs_setup();
  checkSDPresent(1);
  Serial.print("Configuring GPIO...");
  Serial.flush();
  pinMode(buzzer_pin, OUTPUT);
  pinMode(lockRelay_pin, OUTPUT);
  pinMode(exitButton_pin, INPUT_PULLUP);
  pinMode(wiegand_0_pin, INPUT);
  pinMode(wiegand_1_pin, INPUT);
  pinMode(AUXButton_pin, INPUT_PULLUP);
  pinMode(bellButton_pin, INPUT_PULLUP);
  pinMode(magSensor_pin, INPUT_PULLUP);
  pinMode(SD_CS_PIN, OUTPUT);
  pinMode(SD_CD_PIN, INPUT_PULLUP);
  pinMode(DS01, INPUT_PULLUP);
  pinMode(DS02, INPUT_PULLUP);
  pinMode(DS03, INPUT_PULLUP);
  pinMode(DS04, INPUT_PULLUP);
  pinMode(IO0, INPUT_PULLUP);
  digitalWrite(buzzer_pin, LOW);
  Serial.println("OK");

  // Should load default config if run for the first time
  Serial.print("Loading configuration...");
  loadFSJSON_config(config_filename, config);
  if (config.mqtt_tls && config.mqtt_auth == false) {
    if (!loadTLSClient()) {
      Serial.println("Could not load TLS client cert/key from FFat.");
    }
    if (!loadTLSCA()) {
      Serial.println("Could not load CA cert from FFat.");
    }
  } else if (config.mqtt_tls && config.mqtt_auth) {
    if (!loadTLSCA()) {
      Serial.println("Could not load CA cert from FFat.");
    }
  }
  Serial.print("Loading bell file...");
  loadBellFile();
  Serial.print("Loading addressing...");
  loadFSJSON_addressing(addressing_filename, addressing);

  // Check Dip Switch states
  if (digitalRead(DS01) == LOW) {
    Serial.print("DIP Switch #1 ON");
    Serial.println(" - Forced offline mode");
    forceOffline = true;
  }
  if (digitalRead(DS02) == LOW) {
    failSecure = false;
    Serial.print("DIP Switch #2 ON");
    Serial.println(" - FailSafe Strike mode");
  }
  if (digitalRead(DS03) == LOW) {
    Serial.print("DIP Switch #3 ON");
    Serial.println(" - Silent mode");
  }
  if (digitalRead(DS04) == LOW) {
    Serial.print("DIP Switch #4 ON");
    Serial.println(" - Garage mode");
    garage_mode = true;
  }
  if (!(config.wifi_enabled)) {
    Serial.print("Wifi disabled");
    Serial.println(" - Forced offline mode");
    forceOffline = true;
  }
  if (config.magsr_present) {
    Serial.println("Door state sensor enabled");
    magsr_present = true;
  } else {
    Serial.println("Door state sensor disabled");
  }

  if (failSecure) {
    digitalWrite(lockRelay_pin, LOW);
  } else {
    digitalWrite(lockRelay_pin, HIGH);
  }
  if (forceOffline == false) {
    connectWifi();
    startWebServer();
    ftpSrv.setCallback(_callback);
    ftpSrv.setTransferCallback(_transferCallback);
    if (config.ftp_enabled) {
      startFTPServer();
    } else {
      Serial.println("FTP server disabled.");
    }
    if (config.mqtt_enabled) {
      Serial.print("Attempting connection to MQTT broker ");
      Serial.print(config.mqtt_server);
      Serial.print(":");
      Serial.println(config.mqtt_port);
      mqttConnect();
    }
  } else {
    Serial.println("Running in offline mode.");
  }
  pixel.begin();
  // instantiate listeners and initialize Wiegand reader, configure pins
  wiegand.onReceive(receivedData, "Key read: ");
  wiegand.onReceiveError(receivedDataError, "Key read error: ");
  wiegand.begin(Wiegand::LENGTH_ANY, true);
  attachInterrupt(digitalPinToInterrupt(wiegand_0_pin), pinStateChanged, CHANGE);
  attachInterrupt(digitalPinToInterrupt(wiegand_1_pin), pinStateChanged, CHANGE);
  pinStateChanged();
  ledcAttachChannel(buzzer_pin, freq, resolution, channel);
  setPixBlue();
  Serial.println("Ready.");
  Serial.println("");
}

// --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP --- LOOP ---

void loop() {
  if (forceOffline == false) {
    if (WiFi.status() == WL_CONNECTED) {
      if (disconCount > 0) {
        restart();
      }
      //Online Functions
      webServer.handleClient();
      ftpSrv.handleFTP();
      if (config.mqtt_enabled) {
        maintainConnnectionMQTT();
        checkMqtt();
      }
      disconCount = 0;
    } else {
      if (millis() > (lastWifiConnectAttempt + wifiReconnectInterval)) {
        disconCount = 0;
        // Serial.print("millis: ");
        // Serial.println(millis());
        // Serial.print("lastWifiConnectAttempt: ");
        // Serial.println(lastWifiConnectAttempt);
        // Serial.print("wifiReconnectInterval: ");
        // Serial.println(wifiReconnectInterval);
        Serial.println("WiFi reconnection attempt...");
        connectWifi();
      }
      else if (disconCount == 0) {
        Serial.println("Disconnected from WiFi");
      }
      disconCount++;
    }
    checkMqtt();
  }
  
  // Offline Functions
  checkKey();
  checkMagSensor();
  checkSerialCmd();
  checkExit();
  check0();
  checkAUX();
  checkBell();
  checkSDPresent(1);
  watchdogCount = 0;
  delay(10);
}