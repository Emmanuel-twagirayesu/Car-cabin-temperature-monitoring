#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>  // IMPORTANT: Add this for HTTPS
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ====================== WIFI ======================
const char* ssid = "MMII_PC";
const char* password = "1234509876";

// ====================== FIREBASE REST API ======================
#define FIREBASE_URL "https://temp-hum-c4be4-default-rtdb.europe-west1.firebasedatabase.app"

// ====================== DEVICE ======================
const char* device_id = "DEVICE_001";

// ====================== PINS ======================
#define DHTPIN D4
#define DHTTYPE DHT22
#define AC_PIN D5
#define HUMIDIFIER_PIN D6

// ====================== INITIALIZE ======================
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ====================== VARIABLES ======================
unsigned long lastSend = 0;
unsigned long lastCheck = 0;
unsigned long lastWiFiCheck = 0;
String currentACState = "OFF";
String currentHumidifierState = "OFF";
int wifiRetryCount = 0;
int sensorErrorCount = 0;
int firebaseErrorCount = 0;

// ====================== SETUP ======================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n==========================================");
  Serial.println("ESP8266 IoT System Starting");
  Serial.println("==========================================");
  
  // Initialize pins
  pinMode(AC_PIN, OUTPUT);
  pinMode(HUMIDIFIER_PIN, OUTPUT);
  digitalWrite(AC_PIN, LOW);
  digitalWrite(HUMIDIFIER_PIN, LOW);
  
  // Initialize LCD
  Wire.begin(D1, D2);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("ESP8266 Starting");
  lcd.setCursor(0, 1);
  lcd.print("Please wait...");
  
  // Initialize DHT sensor
  dht.begin();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Ready screen
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready!");
  lcd.setCursor(0, 1);
  lcd.print("T:--C H:--%");
  
  Serial.println("==========================================");
  Serial.println("System Ready! Sending data every 15 seconds");
  Serial.println("==========================================\n");
}

// ====================== WiFi CONNECTION ======================
void connectToWiFi() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
    lcd.setCursor(0, 1);
    lcd.print("Attempt: ");
    lcd.print(attempts);
    lcd.print(" ");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
    wifiRetryCount = 0;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    delay(1000);
  } else {
    Serial.println("\n❌ WiFi Failed!");
    wifiRetryCount++;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Check connection");
    delay(2000);
  }
}

// ====================== CHECK WiFi CONNECTION ======================
bool isWiFiConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastWiFiCheck > 10000) {
      lastWiFiCheck = millis();
      Serial.println("⚠️ WiFi disconnected! Reconnecting...");
      connectToWiFi();
    }
    return false;
  }
  return true;
}

// ====================== SEND TO FIREBASE USING POST ======================
void sendToFirebase(float temperature, float humidity) {
  if (!isWiFiConnected()) {
    Serial.println("❌ Cannot send: WiFi not connected");
    firebaseErrorCount++;
    return;
  }
  
  // IMPORTANT: Use WiFiClientSecure for HTTPS
  WiFiClientSecure client;
  client.setInsecure();  // Skip certificate verification (for testing)
  // For production, you should use setCertificate() or setFingerprint()
  
  HTTPClient http;
  unsigned long timestamp = millis() / 1000;
  
  // Create JSON data for sensor reading
  String logData = "{";
  logData += "\"temperature\":" + String(temperature, 2) + ",";
  logData += "\"humidity\":" + String(humidity, 2) + ",";
  logData += "\"timestamp\":" + String(timestamp);
  logData += "}";
  
  // Use POST to add a new entry with auto-generated ID
  String logPath = String(FIREBASE_URL) + "/logs/" + device_id + ".json";
  
  Serial.println("📤 Sending to: " + logPath);
  Serial.println("📦 Data: " + logData);
  
  http.begin(client, logPath);
  http.addHeader("Content-Type", "application/json");
  
  // Using POST to add new entry
  int httpCode = http.POST(logData);
  
  if (httpCode == 200) {
    Serial.println("✅ Sensor data sent to Firebase");
    firebaseErrorCount = 0;
  } else {
    Serial.print("❌ Firebase Error (Sensor Data): ");
    Serial.println(httpCode);
    String response = http.getString();
    Serial.println("Response: " + response);
    firebaseErrorCount++;
  }
  
  http.end();
  
  // Update device status using PATCH
  delay(200);
  String statusPath = String(FIREBASE_URL) + "/devices/" + device_id + ".json";
  String statusData = "{\"status\":\"online\",\"lastSeen\":" + String(timestamp) + "}";
  
  http.begin(client, statusPath);
  http.addHeader("Content-Type", "application/json");
  httpCode = http.PATCH(statusData);
  
  if (httpCode == 200) {
    Serial.println("✅ Device status updated");
  } else {
    Serial.print("⚠️ Status update error: ");
    Serial.println(httpCode);
  }
  http.end();
}

// ====================== CHECK REMOTE COMMANDS ======================
void checkRemoteCommands() {
  if (!isWiFiConnected()) return;
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  // Check AC command
  String acPath = String(FIREBASE_URL) + "/controls/" + device_id + "/ac.json";
  
  http.begin(client, acPath);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String newState = http.getString();
    newState.replace("\"", "");
    newState.trim();
    
    Serial.println("📥 AC Command: " + newState);
    
    if (newState != currentACState && (newState == "ON" || newState == "OFF")) {
      currentACState = newState;
      digitalWrite(AC_PIN, currentACState == "ON" ? HIGH : LOW);
      Serial.print("✅ AC turned ");
      Serial.println(currentACState);
      
      // Update LCD immediately
      lcd.setCursor(9, 0);
      lcd.print(currentACState == "ON" ? "AC:ON " : "AC:OFF");
    }
  } else if (httpCode == 404) {
    // Path doesn't exist yet - this is normal for first run
    Serial.println("ℹ️ No AC command found");
  } else if (httpCode > 0) {
    Serial.print("⚠️ AC command check failed: ");
    Serial.println(httpCode);
    String response = http.getString();
    Serial.println("Response: " + response);
  } else {
    Serial.print("⚠️ AC command connection failed: ");
    Serial.println(httpCode);
  }
  http.end();
  
  // Check Humidifier command
  delay(200);
  String humPath = String(FIREBASE_URL) + "/controls/" + device_id + "/humidifier.json";
  
  http.begin(client, humPath);
  httpCode = http.GET();
  
  if (httpCode == 200) {
    String newState = http.getString();
    newState.replace("\"", "");
    newState.trim();
    
    Serial.println("📥 Humidifier Command: " + newState);
    
    if (newState != currentHumidifierState && (newState == "ON" || newState == "OFF")) {
      currentHumidifierState = newState;
      digitalWrite(HUMIDIFIER_PIN, currentHumidifierState == "ON" ? HIGH : LOW);
      Serial.print("✅ Humidifier turned ");
      Serial.println(currentHumidifierState);
      
      // Update LCD immediately
      lcd.setCursor(9, 1);
      lcd.print(currentHumidifierState == "ON" ? "HUM:ON" : "HUM:OFF");
    }
  } else if (httpCode == 404) {
    Serial.println("ℹ️ No humidifier command found");
  } else if (httpCode > 0) {
    Serial.print("⚠️ Humidifier command check failed: ");
    Serial.println(httpCode);
    String response = http.getString();
    Serial.println("Response: " + response);
  } else {
    Serial.print("⚠️ Humidifier command connection failed: ");
    Serial.println(httpCode);
  }
  http.end();
}

// ====================== UPDATE LCD DISPLAY ======================
void updateLCD(float temperature, float humidity) {
  lcd.setCursor(0, 0);
  lcd.print("T:");
  if (!isnan(temperature)) {
    lcd.print(temperature, 1);
    lcd.print("C ");
  } else {
    lcd.print("ERR ");
  }
  lcd.print(currentACState == "ON" ? "AC:ON " : "AC:OFF");
  
  lcd.setCursor(0, 1);
  lcd.print("H:");
  if (!isnan(humidity)) {
    lcd.print(humidity, 1);
    lcd.print("% ");
  } else {
    lcd.print("ERR ");
  }
  lcd.print(currentHumidifierState == "ON" ? "HUM:ON" : "HUM:OFF");
}

// ====================== READ SENSOR WITH RETRY ======================
bool readSensor(float &temperature, float &humidity) {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  
  if (isnan(temperature) || isnan(humidity)) {
    sensorErrorCount++;
    Serial.print("❌ DHT Sensor Error! Count: ");
    Serial.println(sensorErrorCount);
    
    if (sensorErrorCount >= 5) {
      lcd.setCursor(0, 0);
      lcd.print("Sensor Failed!  ");
      lcd.setCursor(0, 1);
      lcd.print("Restarting...   ");
      delay(2000);
      ESP.restart();
    }
    return false;
  }
  
  sensorErrorCount = 0;
  return true;
}

// ====================== INITIALIZE FIREBASE NODES ======================
void initFirebaseNodes() {
  if (!isWiFiConnected()) return;
  
  Serial.println("🔧 Initializing Firebase structure...");
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  // Create initial control nodes with default OFF state using PUT
  String acInitPath = String(FIREBASE_URL) + "/controls/" + device_id + "/ac.json";
  http.begin(client, acInitPath);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.PUT("\"OFF\"");
  
  if (httpCode == 200) {
    Serial.println("✅ AC control node created");
  } else {
    Serial.print("⚠️ AC node creation: ");
    Serial.println(httpCode);
  }
  http.end();
  
  delay(200);
  
  String humInitPath = String(FIREBASE_URL) + "/controls/" + device_id + "/humidifier.json";
  http.begin(client, humInitPath);
  http.addHeader("Content-Type", "application/json");
  httpCode = http.PUT("\"OFF\"");
  
  if (httpCode == 200) {
    Serial.println("✅ Humidifier control node created");
  } else {
    Serial.print("⚠️ Humidifier node creation: ");
    Serial.println(httpCode);
  }
  http.end();
  
  Serial.println("✅ Firebase initialization complete");
}

// ====================== MAIN LOOP ======================
void loop() {
  // Initialize Firebase nodes once at start
  static bool firebaseInitialized = false;
  if (!firebaseInitialized && isWiFiConnected()) {
    initFirebaseNodes();
    firebaseInitialized = true;
  }
  
  // Read sensor and update LCD every 2 seconds
  if (millis() - lastCheck > 2000) {
    lastCheck = millis();
    
    float temperature, humidity;
    if (readSensor(temperature, humidity)) {
      updateLCD(temperature, humidity);
      
      Serial.print("📊 Sensor Reading: ");
      Serial.print(temperature, 1);
      Serial.print("°C, ");
      Serial.print(humidity, 1);
      Serial.println("%");
    } else {
      lcd.setCursor(0, 0);
      lcd.print("Sensor Error!   ");
      lcd.setCursor(0, 1);
      lcd.print("Check DHT22     ");
    }
  }
  
  // Send data to Firebase every 15 seconds
  if (millis() - lastSend > 15000) {
    lastSend = millis();
    
    float temperature, humidity;
    if (readSensor(temperature, humidity)) {
      sendToFirebase(temperature, humidity);
    } else {
      Serial.println("❌ Skipping Firebase send due to sensor error");
    }
  }
  
  // Check for remote control commands every 5 seconds
  static unsigned long lastCommandCheck = 0;
  if (millis() - lastCommandCheck > 5000) {
    lastCommandCheck = millis();
    checkRemoteCommands();
  }
  
  // Display error counts on LCD if there are issues
  static unsigned long lastErrorDisplay = 0;
  if (millis() - lastErrorDisplay > 30000) {
    lastErrorDisplay = millis();
    if (firebaseErrorCount > 0 || sensorErrorCount > 0) {
      Serial.print("⚠️ Error Stats - Sensor: ");
      Serial.print(sensorErrorCount);
      Serial.print(", Firebase: ");
      Serial.println(firebaseErrorCount);
    }
  }
  
  delay(100);
}
