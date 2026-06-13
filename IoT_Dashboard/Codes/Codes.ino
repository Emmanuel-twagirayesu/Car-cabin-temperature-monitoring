#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
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
String currentACState = "OFF";
String currentHumidifierState = "OFF";
int wifiRetryCount = 0;

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
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("📡 IP Address: ");
    Serial.println(WiFi.localIP());
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    delay(1000);
  } else {
    Serial.println("\n❌ WiFi Failed!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Check password");
    delay(2000);
  }
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi disconnected! Reconnecting...");
    connectToWiFi();
    delay(3000);
  }
  
  // Read sensor and update LCD every 2 seconds
  if (millis() - lastCheck > 2000) {
    lastCheck = millis();
    
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    
    if (!isnan(temperature) && !isnan(humidity)) {
      // Update LCD display
      lcd.setCursor(0, 0);
      lcd.print("T:");
      lcd.print(temperature, 1);
      lcd.print("C ");
      lcd.print(currentACState == "ON" ? "AC:ON " : "AC:OFF");
      
      lcd.setCursor(0, 1);
      lcd.print("H:");
      lcd.print(humidity, 1);
      lcd.print("% ");
      lcd.print(currentHumidifierState == "ON" ? "HUM:ON" : "HUM:OFF");
      
      Serial.print("📊 Sensor Reading: ");
      Serial.print(temperature);
      Serial.print("°C, ");
      Serial.print(humidity);
      Serial.println("%");
    } else {
      lcd.setCursor(0, 0);
      lcd.print("Sensor Error!   ");
      lcd.setCursor(0, 1);
      lcd.print("Check DHT22     ");
      Serial.println("❌ DHT Sensor Error!");
    }
  }
  
  // Send data to Firebase every 15 seconds
  if (millis() - lastSend > 15000) {
    lastSend = millis();
    
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    
    if (!isnan(temperature) && !isnan(humidity)) {
      sendToFirebase(temperature, humidity);
    }
  }
  
  // Check for remote control commands
  checkRemoteCommands();
  
  delay(100);
}

void sendToFirebase(float temperature, float humidity) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Cannot send: WiFi not connected");
    return;
  }
  
  HTTPClient http;
  unsigned long timestamp = millis() / 1000;
  
  // Send sensor data
  String logPath = String(FIREBASE_URL) + "/logs/" + device_id + "/" + timestamp + ".json";
  String logData = "{\"temperature\":" + String(temperature) + 
                   ",\"humidity\":" + String(humidity) + 
                   ",\"timestamp\":" + String(timestamp) + "}";
  
  http.begin(logPath);
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = http.PUT(logData);
  
  if (httpCode == 200) {
    Serial.println("✅ Sensor data sent to Firebase");
  } else {
    Serial.print("❌ Firebase Error: ");
    Serial.println(httpCode);
  }
  
  http.end();
  
  // Update device status
  delay(100);
  String statusPath = String(FIREBASE_URL) + "/devices/" + device_id + "/status.json";
  http.begin(statusPath);
  http.PUT("online");
  http.end();
  
  delay(100);
  String timePath = String(FIREBASE_URL) + "/devices/" + device_id + "/lastSeen.json";
  http.begin(timePath);
  http.PUT(String(timestamp));
  http.end();
  
  Serial.println("✅ Device status updated");
}

void checkRemoteCommands() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  
  // Check AC command
  String acPath = String(FIREBASE_URL) + "/controls/" + device_id + "/ac.json";
  http.begin(acPath);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String newState = http.getString();
    newState.replace("\"", "");
    newState.trim();
    
    if (newState != currentACState && (newState == "ON" || newState == "OFF")) {
      currentACState = newState;
      digitalWrite(AC_PIN, currentACState == "ON" ? HIGH : LOW);
      Serial.print("✅ AC turned ");
      Serial.println(currentACState);
      
      // Update LCD immediately
      lcd.setCursor(9, 0);
      lcd.print(currentACState == "ON" ? "AC:ON " : "AC:OFF");
    }
  }
  http.end();
  
  // Check Humidifier command
  delay(50);
  String humPath = String(FIREBASE_URL) + "/controls/" + device_id + "/humidifier.json";
  http.begin(humPath);
  httpCode = http.GET();
  
  if (httpCode == 200) {
    String newState = http.getString();
    newState.replace("\"", "");
    newState.trim();
    
    if (newState != currentHumidifierState && (newState == "ON" || newState == "OFF")) {
      currentHumidifierState = newState;
      digitalWrite(HUMIDIFIER_PIN, currentHumidifierState == "ON" ? HIGH : LOW);
      Serial.print("✅ Humidifier turned ");
      Serial.println(currentHumidifierState);
      
      // Update LCD immediately
      lcd.setCursor(9, 1);
      lcd.print(currentHumidifierState == "ON" ? "HUM:ON" : "HUM:OFF");
    }
  }
  http.end();
}
