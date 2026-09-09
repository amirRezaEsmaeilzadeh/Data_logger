#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <DHT.h>
#include <LittleFS.h>
#include <Preferences.h>

// WiFi config
const char* ssid = "WiFi name";
const char* password = "WiFi password?";
WiFiServer server(8080);
WiFiClient activeClient;

// Sensors config
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

unsigned long previousMillisDHT = 0;
unsigned long previousMillisADXL = 0;
const long intervalDHT = 2000;
const long intervalADXL = 100;

// RAM config
String dataBuffer = "";
unsigned long lastFlashWriteTime = 0;
const long flashWriteInterval = 10000;
const int maxBufferCapacity = 3000;

// Storage config
Preferences preferences;
unsigned long dayStartTime = 0;
const long dayDuration = 86400000;
unsigned int currentDay = 0;

// Save line to flash
void logDataToFlash(String dataLine) {
  String filename = "/log_" + String(currentDay) + ".csv";
  File file = LittleFS.open(filename, FILE_APPEND);
  if (file) {
    file.print(dataLine);
    file.close();
  }
}

// 7 day history
void checkDayRotation(unsigned long currentMillis) {
  if (currentMillis - dayStartTime >= dayDuration) {
    currentDay = (currentDay + 1) % 7;
    preferences.putUInt("day", currentDay);
    String newFilename = "/log_" + String(currentDay) + ".csv";
    LittleFS.remove(newFilename);
    File f = LittleFS.open(newFilename, FILE_APPEND);
    if (f) {
      f.close()
    };
    Serial.print("Rotated to new day slot: ");
    Serial.println(newFilename);
    dayStartTime = currentMillis;
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();

  if (!accel.begin()) {
    Serial.println("Error: No ADXL345 detected!");
    while (1)
      ;
  }
  accel.setRange(ADXL345_RANGE_16_G);

  if (!LittleFS.begin(true)) {
    Serial.println("Storage Error!");
  }

  preferences.begin("logger", false);
  currentDay = preferences.getUInt("day", 0);

  File f = LittleFS.open("/log_" + String(currentDay) + ".csv", FILE_APPEND);
  if (f) f.close();

  Serial.print("Booting up. Resuming logging in slot: log_");
  Serial.println(currentDay);

  // Start WiFi
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected successfully!");
  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());
  dataBuffer.reserve(4096);
  server.begin();

  dayStartTime = millis();
}

void loop() {

  // Reconnect to WiFi if disconnected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi lost. Reconnecting...");
    WiFi.disconnect();
    WiFi.reconnect();
    delay(5000);
    return;
  }

  unsigned long currentMillis = millis();
  checkDayRotation(currentMillis);

  // Stops the connection of multiple clients for privacy
  WiFiClient newClient = server.available();
  if (newClient) {
    if (!activeClient || !activeClient.connected()) {
      activeClient = newClient;
      Serial.println("New Client Connected!");
    } else {
      newClient.stop();
    }
  }

  bool isConnected = activeClient && activeClient.connected();

  if (isConnected && activeClient.available()) {
    String command = activeClient.readStringUntil('\n');
    command.trim();

    // Give history data to QT
    if (command.startsWith("GET_LOG_")) {
      String dayStr = command.substring(8);
      String filename = "/log_" + dayStr + ".csv";
      File file = LittleFS.open(filename, FILE_READ);

      if (file) {
        Serial.println("Sending file: " + filename);
        activeClient.print("START_FILE," + filename + "\n");
        byte fileBuffer[256];
        while (file.available()) {
          size_t bytesRead = file.read(fileBuffer, sizeof(fileBuffer));
          activeClient.write(fileBuffer, bytesRead);
        }
        file.close();
        activeClient.print("END_FILE\n");
        Serial.println("Transfer complete.");
      } else {
        activeClient.print("ERROR,File not found\n");
      }
    }

    else if (command.indexOf("GET_LIST") >= 0) {
      String response = "LIST";
      for (int i = 0; i < 7; i++) {
        int targetFile = (currentDay - i + 7) % 7;
        String filename = "/log_" + String(targetFile) + ".csv";

        if (LittleFS.exists(filename)) {
          response += "," + String(i) + ":" + String(targetFile);
        }
      }
      activeClient.print(response + "\n");
      Serial.println("Sent file list to Qt: " + response);
    }
  }

  // DHT11
  if (currentMillis - previousMillisDHT >= intervalDHT) {
    previousMillisDHT = currentMillis;
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      String dhtData = "DHT," + String(t, 2) + "," + String(h, 2) + "\n";
      dataBuffer += dhtData; // Save to RAM
      if (isConnected) {
        activeClient.print(dhtData); // Send data to QT
      }
    }
  }

  // ADXL345
  if (currentMillis - previousMillisADXL >= intervalADXL) {
    previousMillisADXL = currentMillis;
    sensors_event_t event;
    accel.getEvent(&event);

    String adxlData = "ADXL," + String(event.acceleration.x, 2) + "," + String(event.acceleration.y, 2) + "," + String(event.acceleration.z, 2) + "\n";
    dataBuffer += adxlData; // Save to RAM
    if (isConnected) {
      activeClient.print(adxlData); // Send data to QT
    }
  }

  // Send data from RAM to flash
  if (currentMillis - lastFlashWriteTime >= flashWriteInterval || dataBuffer.length() > maxBufferCapacity) {
    if (dataBuffer.length() > 0) {
      logDataToFlash(dataBuffer);
      dataBuffer = "";
      Serial.println("Flushed RAM buffer to Flash.");
    }
    lastFlashWriteTime = currentMillis;
  }
}