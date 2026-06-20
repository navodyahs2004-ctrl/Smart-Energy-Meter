// ESP32 Smart Energy Meter - Firebase + Telegram version
// Blynk removed. Sensor logic kept from your working code as much as possible.
// Startup relay fixed: relay turns ON when ESP32 powers up.

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Firebase_ESP_Client.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include "addons/TokenHelper.h"
#include <WebServer.h>
#include <Preferences.h>

// ================= USER SETTINGS =================
#define WIFI_SSID "Navodya's A15"
#define WIFI_PASSWORD "Heshan@2004"

#define FIREBASE_API_KEY "AIzaSyAsHWEKq2WTiIDzQ0t-K6l6Lhvvp1kQF_k"
#define FIREBASE_DATABASE_URL "https://smart-energy-meter-7b5a0-default-rtdb.firebaseio.com/"
#define DEVICE_ID "device001"

#define TELEGRAM_BOT_TOKEN "8374484799:AAFT9EXzzBGkfv2lhFklXwB4fhe5x4jHeEo"
#define TELEGRAM_CHAT_ID "7529368115"
#define TELEGRAM_ENABLED true

// Sri Lanka time = UTC + 5:30
#define GMT_OFFSET_SECONDS 19800
#define DAYLIGHT_OFFSET_SECONDS 0

// ================= FIREBASE OBJECTS =================
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool firebaseStarted = false;

// ================= LOCAL WIFI SETUP SERVER =================
WebServer setupServer(80);
Preferences wifiPrefs;

String activeWiFiSSID = "";
String activeWiFiPassword = "";

bool hasSavedWiFiCredentials = false;
bool setupServerRunning = false;
bool setupServerConfigured = false;

// ================= OLED SETTINGS =================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define OLED_SDA 21
#define OLED_SCL 22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ================= PIN SETTINGS =================
#define CURRENT_SENSOR_PIN 34
#define VOLTAGE_SENSOR_PIN 35
#define RELAY_PIN 18
#define BUTTON_PIN 19
#define WIFI_BUTTON_PIN 27
#define BUZZER_PIN 23

// ================= WIFI BUTTON / SETUP MODE SETTINGS =================
#define WIFI_SETUP_AP_PASSWORD "12345678"
#define WIFI_BUTTON_QUICK_MS 1000UL
#define WIFI_BUTTON_LONG_MS 2000UL
#define WIFI_SETUP_DISPLAY_MS 15000UL
#define WIFI_SETUP_MODE_MS 300000UL

// ================= WIFI RECONNECT SETTINGS =================
#define WIFI_STARTUP_TRIES 20
#define WIFI_STARTUP_DELAY_MS 250UL
#define WIFI_RECONNECT_INTERVAL_MS 5000UL
#define WIFI_RECONNECT_CURRENT_BLANK_MS 2000UL
#define WIFI_BLANK_REAL_LOAD_BYPASS_CURRENT 1.00
#define WIFI_DISCONNECTED_NO_LOAD_DISPLAY_CUTOFF 0.30

// Permanent usage log for date/time range analysis in the app.
#define USAGE_LOG_INTERVAL_MS 60000UL

unsigned long lastWiFiReconnectAttempt = 0;
unsigned long currentIgnoreUntilMillis = 0;
unsigned long lastUsageLogUpload = 0;

// ================= POWER CUT AUTO RELAY SETTINGS =================
#define POWER_CUT_DELAY_MS 15000UL
#define POWER_RESTORE_DELAY_MS 2000UL

// ================= RELAY SETTINGS =================
#define RELAY_ACTIVE_LOW true

bool relayState = false;
bool lastButtonState = HIGH;
unsigned long lastButtonTime = 0;

// Fast physical relay button support.
// This only improves button detection; relay logic is not changed.
volatile bool relayButtonQuickRequest = false;
volatile bool relayButtonIsDown = false;
volatile unsigned long relayButtonDownTime = 0;
volatile unsigned long relayButtonLastInterrupt = 0;

#define RELAY_BUTTON_MIN_PRESS_MS 50UL
#define RELAY_BUTTON_MAX_PRESS_MS 1200UL
#define RELAY_BUTTON_LOCKOUT_MS 800UL

unsigned long relayButtonLastActionMillis = 0;

void IRAM_ATTR relayButtonISR() {
  unsigned long now = millis();

  if (now - relayButtonLastInterrupt < 30) {
    return;
  }

  relayButtonLastInterrupt = now;

  bool state = digitalRead(BUTTON_PIN);

  if (state == LOW) {
    relayButtonIsDown = true;
    relayButtonDownTime = now;
  } else {
    if (relayButtonIsDown) {
      unsigned long pressTime = now - relayButtonDownTime;

      if (pressTime >= RELAY_BUTTON_MIN_PRESS_MS &&
          pressTime <= RELAY_BUTTON_MAX_PRESS_MS) {
        relayButtonQuickRequest = true;
      }
    }

    relayButtonIsDown = false;
  }
}

// ================= ACS712 CURRENT SENSOR SETTINGS =================
#define ACS_SENSITIVITY 0.066
#define ACS_DIVIDER_FACTOR 1.5
#define CALIBRATION_FACTOR 1.00
#define CURRENT_OFFSET_CORRECTION 0.1

// ================= ZMPT101B VOLTAGE SENSOR SETTINGS =================
#define ZMPT_CALIBRATION_FACTOR 750.0
#define ZMPT_DIVIDER_FACTOR 1.0
#define VOLTAGE_NOISE_CUTOFF 10.0

float voltageRMS = 0.0;
float voltageFiltered = 0.0;

// ================= CURRENT FILTER SETTINGS =================
#define NOISE_CUTOFF 0.05
#define BASELINE_MARGIN 0.07

#define LOAD_ON_MARGIN 0.14
#define LOAD_CONFIRM_COUNT 3

#define LOAD_OFF_MARGIN 0.12
#define LOAD_OFF_CONFIRM_COUNT 4

#define SAMPLE_COUNT 2000
#define SAMPLE_DELAY_US 200

// ================= POWER OFF DETECTION =================
#define MAINS_LOST_VOLTAGE 30.0
#define MAINS_RESTORE_VOLTAGE 180.0
#define MAINS_CONFIRM_COUNT 3

bool mainsAvailable = true;
int mainsLostCounter = 0;
int mainsRestoreCounter = 0;

bool powerCutTiming = false;
bool powerCutTelegramSent = false;
unsigned long powerCutStartMillis = 0;

bool powerRestoreTiming = false;
bool powerRestoreTelegramSent = false;
unsigned long powerRestoreStartMillis = 0;

bool wifiButtonLastState = HIGH;
bool wifiButtonPressed = false;
bool wifiButtonLongHandled = false;
unsigned long wifiButtonPressMillis = 0;

bool wifiSetupModeActive = false;
unsigned long wifiSetupModeStartMillis = 0;
String wifiSetupApName = "";
String pairingCode = "";
String lastPairRequestId = "";

// ================= ALERT LIMITS =================
#define HIGH_CURRENT_LIMIT 25.0
#define HIGH_POWER_LIMIT 5000.0
#define HIGH_ENERGY_LIMIT 10.0
#define HIGH_COST_LIMIT 1000.0

bool highCurrentSent = false;
bool highPowerSent = false;
bool highEnergySent = false;
bool highCostSent = false;

// ================= POWER / ENERGY SETTINGS =================
float powerW = 0.0;
float energyWh = 0.0;
float energyKWh = 0.0;
float unitPrice = 26.0;
float cost = 0.0;

unsigned long lastEnergyTime = 0;

float zeroCurrentVoltage = 0.0;
float noLoadCurrentRMS = 0.0;

float currentRMS = 0.0;
float currentFiltered = 0.0;

// Load detection variables
bool loadDetected = false;
int loadDetectCounter = 0;
int loadOffCounter = 0;
float lastValidCurrentRMS = 0.0;

// Session variables
bool sessionActive = false;
String sessionStartTime = "";
unsigned long sessionStartMillis = 0;
float sessionMaxPower = 0.0;
float sessionMaxCurrent = 0.0;

// Timers
unsigned long lastFirebaseUpload = 0;
unsigned long lastFirebaseRead = 0;
unsigned long lastSettingsRead = 0;

// ================= TIMER / EXTRA CONTROL SETTINGS =================
#define TIMER_MAX_MINUTES 1440

bool timerActive = false;
int timerSetMinutes = 0;
unsigned long timerStartMillis = 0;
unsigned long timerDurationMs = 0;
int timerEndEpoch = 0;

unsigned long lastMinuteCostLog = 0;
int lastMinuteIndexLogged = -1;

unsigned long lastTelegramCommandCheck = 0;
long telegramLastUpdateId = 0;

// ================= FIREBASE PATHS =================
String basePath() {
  return String("/devices/") + DEVICE_ID;
}

String livePath() {
  return basePath() + "/live";
}

String settingsPath() {
  return basePath() + "/settings";
}

String controlPath() {
  return basePath() + "/control";
}

String historyPath() {
  return basePath() + "/history";
}

String minuteCostPath() {
  return basePath() + "/currentMinuteCost";
}

String usageLogsPath() {
  return basePath() + "/usageLogs";
}

String pairingPath() {
  return basePath() + "/pairing";
}

bool firebaseIsReady() {
  return firebaseStarted && Firebase.ready();
}

float adcToVoltage(int adcValue) {
  return (adcValue / 4095.0) * 3.3;
}

float correctCurrentOffset(float current) {
  current = current - CURRENT_OFFSET_CORRECTION;

  if (current < 0.0) {
    current = 0.0;
  }

  return current;
}

// ================= TIME FUNCTIONS =================
String getDateTimeString() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return String("time_not_ready_") + String(millis() / 1000);
  }

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

String getDateOnlyString() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return String("unknown_date");
  }

  char buffer[12];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
  return String(buffer);
}

String getTimeOnlyString() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return String("time_not_ready");
  }

  char buffer[10];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

String getUsageLogKey() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return String("t_") + String(millis() / 1000);
  }

  char buffer[12];
  strftime(buffer, sizeof(buffer), "t%H%M%S", &timeinfo);
  return String(buffer);
}

String getSessionId() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return String("session_") + String(millis());
  }

  char buffer[25];
  strftime(buffer, sizeof(buffer), "session_%Y%m%d_%H%M%S", &timeinfo);
  return String(buffer);
}

int getNowEpoch() {
  time_t nowEpoch = time(nullptr);
  if (nowEpoch > 100000) {
    return (int)nowEpoch;
  }
  return 0;
}

String formatTimerRemaining() {
  if (!timerActive || timerDurationMs == 0) return "00:00";

  unsigned long elapsed = millis() - timerStartMillis;
  if (elapsed >= timerDurationMs) return "00:00";

  unsigned long remainSec = (timerDurationMs - elapsed) / 1000;
  unsigned long mm = remainSec / 60;
  unsigned long ss = remainSec % 60;

  char buf[12];
  sprintf(buf, "%02lu:%02lu", mm, ss);
  return String(buf);
}

// ================= TELEGRAM FUNCTIONS =================
String urlEncode(const String &text) {
  String encoded = "";

  for (int i = 0; i < text.length(); i++) {
    char c = text.charAt(i);

    if (isalnum(c)) {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else if (c == '\n') {
      encoded += "%0A";
    } else if (c == '.') {
      encoded += ".";
    } else if (c == '-') {
      encoded += "-";
    } else if (c == '_') {
      encoded += "_";
    } else {
      char buf[4];
      sprintf(buf, "%%%02X", (unsigned char)c);
      encoded += buf;
    }
  }

  return encoded;
}

void sendTelegramMessage(String message) {
  if (!TELEGRAM_ENABLED) return;
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;

  String url = "https://api.telegram.org/bot";
  url += TELEGRAM_BOT_TOKEN;
  url += "/sendMessage?chat_id=";
  url += TELEGRAM_CHAT_ID;
  url += "&text=";
  url += urlEncode(message);

  if (https.begin(client, url)) {
    int httpCode = https.GET();

    Serial.print("Telegram HTTP code: ");
    Serial.println(httpCode);

    https.end();
  }
}

// ================= DISPLAY BASIC =================
void showMessage(String line1, String line2, String line3) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println(line1);

  display.setCursor(0, 20);
  display.println(line2);

  display.setCursor(0, 40);
  display.println(line3);

  display.display();
}

// ================= RELAY BASIC =================
void writeRelayPin(bool state) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(RELAY_PIN, state ? LOW : HIGH);
  } else {
    digitalWrite(RELAY_PIN, state ? HIGH : LOW);
  }
}

void buzzerTickOnRelayOn() {
  for (int i = 0; i < 5; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }
}

void resetCurrentState() {
  currentRMS = 0.0;
  currentFiltered = 0.0;
  powerW = 0.0;

  loadDetected = false;
  loadDetectCounter = 0;
  loadOffCounter = 0;
  lastValidCurrentRMS = 0.0;
}

void ignoreCurrentDuringWiFiChange(String reason) {
  // WiFi connect/reconnect can create a short ACS712 spike.
  // Do not stop the meter permanently. Only ignore small no-load readings briefly.
  currentIgnoreUntilMillis = millis() + WIFI_RECONNECT_CURRENT_BLANK_MS;

  if (!loadDetected) {
    resetCurrentState();
  }

  Serial.print("Short no-load current blank for WiFi event: ");
  Serial.println(reason);
}

bool isCurrentPausedForWiFi() {
  return (!loadDetected && millis() < currentIgnoreUntilMillis);
}

// ================= SAVED WIFI CREDENTIALS =================
void loadSavedWiFiCredentials() {
  wifiPrefs.begin("wifi", true);

  String savedSSID = wifiPrefs.getString("ssid", "");
  String savedPassword = wifiPrefs.getString("pass", "");

  wifiPrefs.end();

  if (savedSSID.length() > 0) {
    activeWiFiSSID = savedSSID;
    activeWiFiPassword = savedPassword;
    hasSavedWiFiCredentials = true;

    Serial.println("Saved WiFi found.");
    Serial.print("SSID: ");
    Serial.println(activeWiFiSSID);
  } else {
    activeWiFiSSID = WIFI_SSID;
    activeWiFiPassword = WIFI_PASSWORD;
    hasSavedWiFiCredentials = false;

    Serial.println("No saved WiFi. Using default code WiFi first.");
  }
}

void saveWiFiCredentials(String ssid, String password) {
  wifiPrefs.begin("wifi", false);

  wifiPrefs.putString("ssid", ssid);
  wifiPrefs.putString("pass", password);

  wifiPrefs.end();

  activeWiFiSSID = ssid;
  activeWiFiPassword = password;
  hasSavedWiFiCredentials = true;

  Serial.println("New WiFi saved.");
  Serial.print("SSID: ");
  Serial.println(activeWiFiSSID);
}

void clearSavedWiFiCredentials() {
  wifiPrefs.begin("wifi", false);
  wifiPrefs.clear();
  wifiPrefs.end();

  activeWiFiSSID = "";
  activeWiFiPassword = "";
  hasSavedWiFiCredentials = false;

  Serial.println("Saved WiFi cleared.");
}

// ================= LOCAL SETUP WEB SERVER =================
void startSetupWebServer() {
  if (!setupServerConfigured) {
    setupServer.on("/", HTTP_GET, []() {
      setupServer.sendHeader("Access-Control-Allow-Origin", "*");
      setupServer.send(
        200,
        "text/plain",
        "Smart Plug WiFi Setup Server\nUse /save?ssid=YOUR_WIFI&password=YOUR_PASSWORD"
      );
    });

    setupServer.on("/save", HTTP_GET, []() {
      setupServer.sendHeader("Access-Control-Allow-Origin", "*");

      if (!setupServer.hasArg("ssid")) {
        setupServer.send(400, "application/json", "{\"ok\":false,\"error\":\"ssid_missing\"}");
        return;
      }

      String newSSID = setupServer.arg("ssid");
      String newPassword = "";

      if (setupServer.hasArg("password")) {
        newPassword = setupServer.arg("password");
      }

      newSSID.trim();

      if (newSSID.length() == 0) {
        setupServer.send(400, "application/json", "{\"ok\":false,\"error\":\"empty_ssid\"}");
        return;
      }

      saveWiFiCredentials(newSSID, newPassword);

      setupServer.send(
        200,
        "application/json",
        "{\"ok\":true,\"message\":\"WiFi saved. ESP32 restarting.\"}"
      );

      delay(1000);
      ESP.restart();
    });

    setupServer.onNotFound([]() {
      setupServer.sendHeader("Access-Control-Allow-Origin", "*");
      setupServer.send(404, "application/json", "{\"ok\":false,\"error\":\"not_found\"}");
    });

    setupServerConfigured = true;
  }

  setupServer.begin();
  setupServerRunning = true;

  Serial.println("Setup web server started at http://192.168.4.1");
}

void stopSetupWebServer() {
  if (!setupServerRunning) return;

  setupServerRunning = false;
  Serial.println("Setup web server stopped.");
}

void handleSetupWebServer() {
  if (setupServerRunning) {
    setupServer.handleClient();
  }
}

// ================= WIFI SETUP / PAIRING =================
String makePairingCode() {
  long code = random(100000, 999999);
  return String(code);
}

String makeSetupApName() {
  String suffix = String((uint32_t)ESP.getEfuseMac(), HEX);
  suffix.toUpperCase();

  if (suffix.length() > 4) {
    suffix = suffix.substring(suffix.length() - 4);
  }

  return String("SmartPlug_") + suffix;
}

void showWiFiSetupScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("SETUP MODE");

  display.setCursor(0, 13);
  display.print("WiFi:");
  display.println(wifiSetupApName);

  display.setCursor(0, 27);
  display.print("Pass:");
  display.println(WIFI_SETUP_AP_PASSWORD);

  display.setCursor(0, 41);
  display.print("Code:");
  display.println(pairingCode);

  display.setCursor(0, 54);
  display.println("IP:192.168.4.1");

  display.display();
}

void startWiFiSetupMode(String reason) {
  wifiSetupModeActive = true;
  wifiSetupModeStartMillis = millis();

  if (wifiSetupApName.length() == 0) {
    wifiSetupApName = makeSetupApName();
  }

  pairingCode = makePairingCode();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(wifiSetupApName.c_str(), WIFI_SETUP_AP_PASSWORD);
  ignoreCurrentDuringWiFiChange("setup_mode_started");

  startSetupWebServer();

  if (firebaseIsReady()) {
    FirebaseJson json;
    json.set("setupMode", true);
    json.set("setupApName", wifiSetupApName);
    json.set("pairingCode", pairingCode);
    json.set("reason", reason);
    json.set("updatedAt", getDateTimeString());
    json.set("updatedEpoch", getNowEpoch());
    Firebase.RTDB.updateNode(&fbdo, pairingPath(), &json);
  }

  showWiFiSetupScreen();

  String msg = "📶 WIFI SETUP MODE\n\n";
  msg += "📡 Hotspot: " + wifiSetupApName + "\n";
  msg += "🔐 Password: " + String(WIFI_SETUP_AP_PASSWORD) + "\n";
  msg += "🌐 IP: 192.168.4.1\n";
  msg += "🔢 Pairing Code: " + pairingCode + "\n";
  msg += "📌 Reason: " + reason;
  sendTelegramMessage(msg);
}

void stopWiFiSetupMode() {
  if (!wifiSetupModeActive) return;

  wifiSetupModeActive = false;
  WiFi.softAPdisconnect(true);
  ignoreCurrentDuringWiFiChange("setup_mode_stopped");
  stopSetupWebServer();

  if (firebaseIsReady()) {
    Firebase.RTDB.setBool(&fbdo, pairingPath() + "/setupMode", false);
  }
}

void resetOwnerAndStartSetup(String reason) {
  clearSavedWiFiCredentials();

  if (firebaseIsReady()) {
    Firebase.RTDB.deleteNode(&fbdo, pairingPath() + "/ownerName");
    Firebase.RTDB.deleteNode(&fbdo, pairingPath() + "/ownerId");
    Firebase.RTDB.setBool(&fbdo, pairingPath() + "/ownerExists", false);
    Firebase.RTDB.setBool(&fbdo, pairingPath() + "/pendingRequest/active", false);
    Firebase.RTDB.setString(&fbdo, pairingPath() + "/lastResetReason", reason);
    Firebase.RTDB.setString(&fbdo, pairingPath() + "/lastResetAt", getDateTimeString());
  }

  String msg = "♻️ OWNER RESET\n\n";
  msg += "👤 Old owner removed\n";
  msg += "📱 New owner setup allowed\n";
  msg += "📌 Reason: " + reason;
  sendTelegramMessage(msg);

  startWiFiSetupMode(reason);
}

void handleWiFiButton() {
  bool buttonState = digitalRead(WIFI_BUTTON_PIN);

  if (wifiButtonLastState == HIGH && buttonState == LOW) {
    wifiButtonPressed = true;
    wifiButtonLongHandled = false;
    wifiButtonPressMillis = millis();
  }

  if (wifiButtonPressed && buttonState == LOW && !wifiButtonLongHandled) {
    if (millis() - wifiButtonPressMillis >= WIFI_BUTTON_LONG_MS) {
      wifiButtonLongHandled = true;
      wifiButtonPressed = false;
      resetOwnerAndStartSetup("wifi_button_long_press");
    }
  }

  if (wifiButtonLastState == LOW && buttonState == HIGH) {
    unsigned long pressTime = millis() - wifiButtonPressMillis;

    if (!wifiButtonLongHandled && pressTime < WIFI_BUTTON_QUICK_MS) {
      startWiFiSetupMode("wifi_button_quick_press");
    }

    wifiButtonPressed = false;
    wifiButtonLongHandled = false;
  }

  wifiButtonLastState = buttonState;
}

void checkWiFiSetupTimeout() {
  if (wifiSetupModeActive && millis() - wifiSetupModeStartMillis >= WIFI_SETUP_MODE_MS) {
    stopWiFiSetupMode();
    sendTelegramMessage("📶 WIFI SETUP MODE CLOSED\n\n⏱ Timeout finished.");
  }
}

void checkPairingRequestTelegram() {
  if (!firebaseIsReady()) return;

  if (Firebase.RTDB.getBool(&fbdo, pairingPath() + "/pendingRequest/active")) {
    if (!fbdo.boolData()) return;

    String requestId = "";
    String requesterName = "Unknown phone";
    String requestTime = getDateTimeString();

    if (Firebase.RTDB.getString(&fbdo, pairingPath() + "/pendingRequest/requestId")) {
      requestId = fbdo.stringData();
    }

    if (Firebase.RTDB.getString(&fbdo, pairingPath() + "/pendingRequest/requesterName")) {
      requesterName = fbdo.stringData();
    }

    if (Firebase.RTDB.getString(&fbdo, pairingPath() + "/pendingRequest/requestTime")) {
      requestTime = fbdo.stringData();
    }

    if (requestId.length() > 0 && requestId != lastPairRequestId) {
      lastPairRequestId = requestId;

      String msg = "📲 NEW PAIRING REQUEST\n\n";
      msg += "👤 Phone/User: " + requesterName + "\n";
      msg += "🕒 Time: " + requestTime + "\n";
      msg += "🆔 Device: " + String(DEVICE_ID) + "\n\n";
      msg += "Open app Settings → Allow or Reject.";
      sendTelegramMessage(msg);
    }
  }
}

// ================= FIREBASE FUNCTIONS =================
void uploadLiveData() {
  if (!firebaseIsReady()) return;

  FirebaseJson json;

  json.set("voltage", voltageFiltered);
  json.set("current", currentFiltered);
  json.set("power", powerW);

  json.set("sessionEnergy", energyKWh);
  json.set("sessionCost", cost);
  json.set("unitPrice", unitPrice);

  json.set("relay", relayState ? 1 : 0);
  json.set("mainsAvailable", mainsAvailable);
  json.set("online", true);
  json.set("lastSeen", getDateTimeString());
  json.set("lastSeenEpoch", getNowEpoch());

  if (!mainsAvailable) {
    json.set("plugStatus", "PLUG_OFF_MAINS_LOST");
  } else if (relayState) {
    json.set("plugStatus", "PLUG_ON");
  } else {
    json.set("plugStatus", "PLUG_OFF");
  }

  int timerRemainingSec = 0;
  if (timerActive && timerDurationMs > 0) {
    unsigned long elapsed = millis() - timerStartMillis;
    if (elapsed < timerDurationMs) {
      timerRemainingSec = (int)((timerDurationMs - elapsed) / 1000);
    }
  }

  json.set("timerActive", timerActive);
  json.set("timerSetMinutes", timerSetMinutes);
  json.set("timerRemainingSec", timerRemainingSec);
  json.set("timerEndEpoch", timerEndEpoch);

  json.set("rangeAmpMax", 30);
  json.set("rangePowerMax", 60000);
  json.set("rangeEnergyMax", 100000);
  json.set("rangeVoltageMax", 500);

  if (!Firebase.RTDB.updateNode(&fbdo, livePath(), &json)) {
    Serial.print("Live upload failed: ");
    Serial.println(fbdo.errorReason());
  }
}

void readSettingsFromFirebase() {
  if (!firebaseIsReady()) return;

  if (Firebase.RTDB.getFloat(&fbdo, settingsPath() + "/unitPrice")) {
    float newPrice = fbdo.floatData();

    if (newPrice > 0.0 && newPrice < 1000.0) {
      unitPrice = newPrice;
    }
  }
}

void createDefaultFirebaseData() {
  if (!firebaseIsReady()) return;

  FirebaseJson settings;
  settings.set("deviceName", "Smart Plug");
  settings.set("unitPrice", unitPrice);
  settings.set("currency", "Rs");
  settings.set("telegramEnabled", TELEGRAM_ENABLED);
  settings.set("pairingPermissionProtected", true);

  Firebase.RTDB.updateNode(&fbdo, settingsPath(), &settings);

  FirebaseJson control;
  control.set("relayCommand", relayState ? 1 : 0);
  control.set("deleteAllHistory", false);
  control.set("clearSession", false);
  control.set("timerMinutes", 0);
  control.set("timerStart", false);
  control.set("timerCancel", false);

  Firebase.RTDB.updateNode(&fbdo, controlPath(), &control);

  Firebase.RTDB.setBool(&fbdo, pairingPath() + "/setupMode", false);
}

void deleteAllHistoryFromFirebase() {
  if (!firebaseIsReady()) return;

  Firebase.RTDB.deleteNode(&fbdo, historyPath());
  Firebase.RTDB.deleteNode(&fbdo, basePath() + "/analytics");
  Firebase.RTDB.deleteNode(&fbdo, minuteCostPath());
  Firebase.RTDB.setBool(&fbdo, controlPath() + "/deleteAllHistory", false);

  sendTelegramMessage("🗑️ Smart Energy Meter\n\n✅ History data deleted by user.");
}

void clearCurrentSessionFromFirebase() {
  if (!firebaseIsReady()) return;

  resetSessionCounters();
  Firebase.RTDB.deleteNode(&fbdo, minuteCostPath());
  lastMinuteIndexLogged = -1;
  lastMinuteCostLog = 0;

  Firebase.RTDB.setBool(&fbdo, controlPath() + "/clearSession", false);
  uploadLiveData();

  sendTelegramMessage("🧹 Smart Energy Meter\n\n✅ Home/Cost values cleared.\n📚 History kept safely.");
}

// ================= ENERGY / SESSION FUNCTIONS =================
void updateEnergyNow() {
  unsigned long now = millis();
  float timeHours = (now - lastEnergyTime) / 3600000.0;
  lastEnergyTime = now;

  if (relayState && mainsAvailable && currentFiltered > 0.0 && voltageFiltered > 0.0) {
    energyWh += powerW * timeHours;
  }

  energyKWh = energyWh / 1000.0;
  cost = energyKWh * unitPrice;

  if (powerW > sessionMaxPower) {
    sessionMaxPower = powerW;
  }

  if (currentFiltered > sessionMaxCurrent) {
    sessionMaxCurrent = currentFiltered;
  }
}

void resetSessionCounters() {
  energyWh = 0.0;
  energyKWh = 0.0;
  cost = 0.0;
  powerW = 0.0;

  sessionMaxPower = 0.0;
  sessionMaxCurrent = 0.0;

  highCurrentSent = false;
  highPowerSent = false;
  highEnergySent = false;
  highCostSent = false;

  lastEnergyTime = millis();
  lastMinuteCostLog = 0;
  lastMinuteIndexLogged = -1;
}

void startNewSession(String reason) {
  sessionActive = true;
  sessionStartTime = getDateTimeString();
  sessionStartMillis = millis();

  resetSessionCounters();

  if (firebaseIsReady()) {
    Firebase.RTDB.deleteNode(&fbdo, minuteCostPath());
  }

  uploadLiveData();

  Serial.print("Session started: ");
  Serial.println(reason);
}

void saveSessionToFirebase(String reason) {
  if (!firebaseIsReady()) return;
  if (!sessionActive) return;

  updateEnergyNow();

  String endTime = getDateTimeString();
  unsigned long durationSec = (millis() - sessionStartMillis) / 1000;

  FirebaseJson json;

  json.set("startTime", sessionStartTime);
  json.set("endTime", endTime);
  json.set("durationSec", (int)durationSec);
  json.set("energy", energyKWh);
  json.set("cost", cost);
  json.set("unitPrice", unitPrice);
  json.set("maxPower", sessionMaxPower);
  json.set("maxCurrent", sessionMaxCurrent);
  json.set("reason", reason);

  String id = getSessionId();

  if (!Firebase.RTDB.setJSON(&fbdo, historyPath() + "/" + id, &json)) {
    Serial.print("History save failed: ");
    Serial.println(fbdo.errorReason());
  }

  String msg = "📊 SMART PLUG SESSION SAVED\n\n";
  msg += "📌 Reason: " + reason + "\n";
  msg += "⚡ Energy: " + String(energyKWh, 5) + " kWh\n";
  msg += "💰 Cost: Rs " + String(cost, 2) + "\n";
  msg += "⏱️ Duration: " + String(durationSec / 60) + " min\n";
  msg += "🔌 Max Current: " + String(sessionMaxCurrent, 2) + " A\n";
  msg += "⚙️ Max Power: " + String(sessionMaxPower, 1) + " W";

  sendTelegramMessage(msg);

  sessionActive = false;
}

void uploadMinuteCostPoint() {
  if (!firebaseIsReady()) return;
  if (!sessionActive) return;
  if (!relayState) return;

  unsigned long elapsedSec = (millis() - sessionStartMillis) / 1000;
  int minuteIndex = elapsedSec / 60;

  if (minuteIndex == lastMinuteIndexLogged) return;
  if (minuteIndex > 0 && millis() - lastMinuteCostLog < 50000) return;

  lastMinuteIndexLogged = minuteIndex;
  lastMinuteCostLog = millis();

  FirebaseJson point;
  point.set("minute", minuteIndex);
  point.set("energy", energyKWh);
  point.set("cost", cost);
  point.set("power", powerW);
  point.set("current", currentFiltered);
  point.set("time", getDateTimeString());

  String key = "m" + String(minuteIndex);
  Firebase.RTDB.setJSON(&fbdo, minuteCostPath() + "/" + key, &point);
}

void uploadUsageLogPoint() {
  if (!firebaseIsReady()) return;
  if (!relayState) return;
  if (!mainsAvailable) return;

  unsigned long nowMillis = millis();

  if (nowMillis - lastUsageLogUpload < USAGE_LOG_INTERVAL_MS) {
    return;
  }

  int epoch = getNowEpoch();

  if (epoch <= 0) {
    return;
  }

  lastUsageLogUpload = nowMillis;

  FirebaseJson point;
  point.set("dateTime", getDateTimeString());
  point.set("date", getDateOnlyString());
  point.set("time", getTimeOnlyString());
  point.set("epoch", epoch);
  point.set("voltage", voltageFiltered);
  point.set("current", currentFiltered);
  point.set("power", powerW);
  point.set("energyKWh", energyKWh);
  point.set("cost", cost);
  point.set("unitPrice", unitPrice);
  point.set("relay", relayState ? 1 : 0);
  point.set("mainsAvailable", mainsAvailable);
  point.set("sessionStartTime", sessionStartTime);

  String dateKey = getDateOnlyString();
  String logKey = getUsageLogKey();

  Firebase.RTDB.setJSON(&fbdo, usageLogsPath() + "/" + dateKey + "/" + logKey, &point);
}

void startAppTimer(int minutes) {
  if (minutes <= 0) return;

  if (minutes > TIMER_MAX_MINUTES) {
    minutes = TIMER_MAX_MINUTES;
  }

  timerSetMinutes = minutes;
  timerDurationMs = (unsigned long)minutes * 60000UL;
  timerStartMillis = millis();
  timerActive = true;

  int nowEpoch = getNowEpoch();
  timerEndEpoch = nowEpoch > 0 ? nowEpoch + (minutes * 60) : 0;

  if (!relayState) {
    turnRelayOn("timer_app");
  } else {
    uploadLiveData();
  }

  if (firebaseIsReady()) {
    Firebase.RTDB.setBool(&fbdo, controlPath() + "/timerStart", false);
    Firebase.RTDB.setInt(&fbdo, controlPath() + "/timerMinutes", minutes);
  }

  sendTelegramMessage("⏱️ TIMER STARTED\n\n🔌 Plug: ON\n⏳ Duration: " + String(minutes) + " min\n💰 Current Cost: Rs " + String(cost, 2) + "\n⚡ Energy: " + String(energyKWh, 5) + " kWh");
}

void cancelAppTimer(String reason) {
  if (!timerActive) {
    if (firebaseIsReady()) Firebase.RTDB.setBool(&fbdo, controlPath() + "/timerCancel", false);
    return;
  }

  timerActive = false;
  timerSetMinutes = 0;
  timerDurationMs = 0;
  timerEndEpoch = 0;

  if (firebaseIsReady()) {
    Firebase.RTDB.setBool(&fbdo, controlPath() + "/timerCancel", false);
  }

  uploadLiveData();
  sendTelegramMessage("⏹️ TIMER CANCELLED\n\nReason: " + reason + "\n💰 Cost so far: Rs " + String(cost, 2) + "\n⚡ Energy: " + String(energyKWh, 5) + " kWh");
}

void checkTimerAutoOff() {
  if (!timerActive) return;
  if (timerDurationMs == 0) return;

  unsigned long elapsed = millis() - timerStartMillis;

  if (elapsed >= timerDurationMs) {
    timerActive = false;
    timerSetMinutes = 0;
    timerDurationMs = 0;
    timerEndEpoch = 0;

    turnRelayOff("timer_finished");
  }
}

// ================= RELAY ON/OFF FUNCTIONS =================
void turnRelayOn(String reason) {
  relayState = true;
  writeRelayPin(true);
  buzzerTickOnRelayOn();
  resetCurrentState();
  startNewSession(reason);

  if (firebaseIsReady()) {
    Firebase.RTDB.setInt(&fbdo, controlPath() + "/relayCommand", 1);
  }

  String msg = "🔌 SMART PLUG ON\n\n";
  msg += "✅ Status: PLUG_ON\n";
  msg += "📌 Reason: " + reason + "\n";
  msg += "📈 Voltage: " + String(voltageFiltered, 1) + " V\n";
  msg += "⚡ Power: " + String(powerW, 1) + " W\n";
  msg += "🔋 Energy: " + String(energyKWh, 5) + " kWh\n";
  msg += "💰 Cost: Rs " + String(cost, 2);
  sendTelegramMessage(msg);
}

void turnRelayOff(String reason) {
  if (relayState || sessionActive) {
    saveSessionToFirebase(reason);
  }

  float finalCost = cost;
  float finalEnergy = energyKWh;

  relayState = false;
  writeRelayPin(false);

  timerActive = false;
  timerSetMinutes = 0;
  timerDurationMs = 0;
  timerEndEpoch = 0;

  resetCurrentState();
  resetSessionCounters();

  if (firebaseIsReady()) {
    Firebase.RTDB.setInt(&fbdo, controlPath() + "/relayCommand", 0);
  }

  uploadLiveData();

  sendTelegramMessage("🔴 SMART PLUG OFF\n\n✅ Status: PLUG_OFF\n📌 Reason: " + reason + "\n💰 Final Cost: Rs " + String(finalCost, 2) + "\n⚡ Final Energy: " + String(finalEnergy, 5) + " kWh");
}

// ================= FIREBASE CONTROL READ =================
void readControlFromFirebase() {
  if (!firebaseIsReady()) return;

  checkPairingRequestTelegram();

  if (Firebase.RTDB.getInt(&fbdo, controlPath() + "/relayCommand")) {
    int command = fbdo.intData();

    if (command == 1 && !relayState) {
      turnRelayOn("mobile_app");
    }

    if (command == 0 && relayState) {
      turnRelayOff("mobile_app");
    }
  }

  if (Firebase.RTDB.getBool(&fbdo, controlPath() + "/deleteAllHistory")) {
    bool del = fbdo.boolData();

    if (del) {
      deleteAllHistoryFromFirebase();
    }
  }

  if (Firebase.RTDB.getBool(&fbdo, controlPath() + "/clearSession")) {
    bool clearNow = fbdo.boolData();

    if (clearNow) {
      clearCurrentSessionFromFirebase();
    }
  }

  if (Firebase.RTDB.getBool(&fbdo, controlPath() + "/timerStart")) {
    bool startTimerNow = fbdo.boolData();

    if (startTimerNow) {
      int minutes = 0;
      if (Firebase.RTDB.getInt(&fbdo, controlPath() + "/timerMinutes")) {
        minutes = fbdo.intData();
      }
      startAppTimer(minutes);
    }
  }

  if (Firebase.RTDB.getBool(&fbdo, controlPath() + "/timerCancel")) {
    bool cancelTimerNow = fbdo.boolData();

    if (cancelTimerNow) {
      cancelAppTimer("mobile_app");
    }
  }
}

// ================= BUTTON FUNCTION =================
void handleButton() {
  if (relayButtonQuickRequest) {
    noInterrupts();
    relayButtonQuickRequest = false;
    interrupts();

    unsigned long now = millis();

    if (now - relayButtonLastActionMillis < RELAY_BUTTON_LOCKOUT_MS) {
      return;
    }

    relayButtonLastActionMillis = now;

    if (relayState) {
      turnRelayOff("physical_button_quick_press");
    } else {
      turnRelayOn("physical_button_quick_press");
    }
  }
}

// ================= CURRENT SENSOR CALIBRATION =================
void calibrateZeroCurrent() {
  long sumADC = 0;
  int samples = 3000;

  for (int i = 0; i < samples; i++) {
    sumADC += analogRead(CURRENT_SENSOR_PIN);
    delayMicroseconds(300);
  }

  float averageADC = sumADC / (float)samples;
  float adcVoltage = adcToVoltage(averageADC);

  zeroCurrentVoltage = adcVoltage * ACS_DIVIDER_FACTOR;
}

float readRawACCurrentRMS() {
  float sumSquares = 0.0;

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    int adcValue = analogRead(CURRENT_SENSOR_PIN);

    float adcVoltage = adcToVoltage(adcValue);
    float sensorVoltage = adcVoltage * ACS_DIVIDER_FACTOR;

    float voltageDifference = sensorVoltage - zeroCurrentVoltage;
    float instantCurrent = voltageDifference / ACS_SENSITIVITY;

    sumSquares += instantCurrent * instantCurrent;

    delayMicroseconds(SAMPLE_DELAY_US);
  }

  float rms = sqrt(sumSquares / SAMPLE_COUNT);
  rms = rms * CALIBRATION_FACTOR;

  return rms;
}

void calibrateNoLoadCurrent() {
  float sum = 0.0;
  int rounds = 10;

  for (int i = 0; i < rounds; i++) {
    sum += readRawACCurrentRMS();
    delay(300);
  }

  noLoadCurrentRMS = sum / rounds;
  resetCurrentState();
}

// ================= VOLTAGE SENSOR READING =================
float readACVoltageRMS() {
  float mean = 0.0;
  float m2 = 0.0;
  int count = 0;

  for (int i = 0; i < SAMPLE_COUNT; i++) {
    int adcValue = analogRead(VOLTAGE_SENSOR_PIN);

    float adcVoltage = adcToVoltage(adcValue);
    float sensorVoltage = adcVoltage * ZMPT_DIVIDER_FACTOR;

    count++;

    float delta = sensorVoltage - mean;
    mean += delta / count;

    float delta2 = sensorVoltage - mean;
    m2 += delta * delta2;

    delayMicroseconds(SAMPLE_DELAY_US);
  }

  float variance = m2 / count;
  float sensorRMS = sqrt(variance);

  float mainsVoltage = sensorRMS * ZMPT_CALIBRATION_FACTOR;

  if (mainsVoltage < VOLTAGE_NOISE_CUTOFF) {
    mainsVoltage = 0.0;
  }

  return mainsVoltage;
}

// ================= CORRECTED AC CURRENT READING =================
float readACCurrentRMS() {
  float rawRMS = readRawACCurrentRMS();

  if (rawRMS <= noLoadCurrentRMS + BASELINE_MARGIN) {
    if (loadDetected) {
      loadOffCounter++;

      if (loadOffCounter >= LOAD_OFF_CONFIRM_COUNT) {
        resetCurrentState();
        return 0.0;
      }

      return lastValidCurrentRMS;
    }

    resetCurrentState();
    return 0.0;
  }

  float correctedSquare = (rawRMS * rawRMS) - (noLoadCurrentRMS * noLoadCurrentRMS);

  if (correctedSquare <= 0.0) {
    if (loadDetected) {
      loadOffCounter++;

      if (loadOffCounter >= LOAD_OFF_CONFIRM_COUNT) {
        resetCurrentState();
        return 0.0;
      }

      return lastValidCurrentRMS;
    }

    resetCurrentState();
    return 0.0;
  }

  float correctedCurrent = sqrt(correctedSquare);

  // WiFi OFF/reconnect can create a short fake current spike when no load is connected.
  // This follows the fast Code_26 current logic, but ignores only small no-load spikes
  // for a short time after WiFi events. Real loads above 1A bypass this immediately.
  if (WiFi.status() != WL_CONNECTED &&
      !loadDetected &&
      isCurrentPausedForWiFi() &&
      correctedCurrent < WIFI_BLANK_REAL_LOAD_BYPASS_CURRENT) {
    resetCurrentState();
    return 0.0;
  }

  // Extra WiFi-disconnected no-load offset removal.
  // Important: this runs ONLY when ESP32 is NOT connected to WiFi.
  // It does NOT change CURRENT_OFFSET_CORRECTION. It only removes the
  // observed no-load display offset around 0.26A while WiFi is disconnected.
  float displayCandidate = correctCurrentOffset(correctedCurrent);

  if (WiFi.status() != WL_CONNECTED &&
      displayCandidate <= WIFI_DISCONNECTED_NO_LOAD_DISPLAY_CUTOFF) {
    resetCurrentState();
    return 0.0;
  }

  if (correctedCurrent < NOISE_CUTOFF) {
    if (loadDetected) {
      loadOffCounter++;

      if (loadOffCounter >= LOAD_OFF_CONFIRM_COUNT) {
        resetCurrentState();
        return 0.0;
      }

      return lastValidCurrentRMS;
    }

    resetCurrentState();
    return 0.0;
  }

  if (loadDetected) {
    if (correctedCurrent < LOAD_OFF_MARGIN) {
      loadOffCounter++;

      if (loadOffCounter >= LOAD_OFF_CONFIRM_COUNT) {
        resetCurrentState();
        return 0.0;
      }

      return lastValidCurrentRMS;
    }

    loadOffCounter = 0;

    float displayCurrent = correctCurrentOffset(correctedCurrent);
    lastValidCurrentRMS = displayCurrent;

    return displayCurrent;
  }

  if (!loadDetected) {
    if (correctedCurrent > LOAD_ON_MARGIN) {
      loadDetectCounter++;
    } else {
      loadDetectCounter = 0;
      currentFiltered = 0.0;
      return 0.0;
    }

    if (loadDetectCounter < LOAD_CONFIRM_COUNT) {
      currentFiltered = 0.0;
      return 0.0;
    }

    loadDetected = true;
    loadOffCounter = 0;

    float displayCurrent = correctCurrentOffset(correctedCurrent);
    lastValidCurrentRMS = displayCurrent;

    return displayCurrent;
  }

  return 0.0;
}

// ================= MAINS STATUS CHECK =================
void checkMainsStatus() {
  unsigned long now = millis();

  if (voltageFiltered < MAINS_LOST_VOLTAGE) {
    powerRestoreTiming = false;
    powerRestoreTelegramSent = false;

    if (mainsAvailable) {
      if (!powerCutTiming) {
        powerCutTiming = true;
        powerCutStartMillis = now;
        powerCutTelegramSent = false;
      }

      if (!powerCutTelegramSent) {
        powerCutTelegramSent = true;

        String msg = "🚨 POWER CUT DETECTED\n\n";
        msg += "📉 Voltage: " + String(voltageFiltered, 1) + " V\n";
        msg += "⏱ Waiting 15 seconds\n";
        msg += "🔌 Relay will turn OFF if voltage stays low\n";
        msg += "💰 Current Cost: Rs " + String(cost, 2) + "\n";
        msg += "🔋 Energy: " + String(energyKWh, 5) + " kWh";
        sendTelegramMessage(msg);
      }

      if (now - powerCutStartMillis >= POWER_CUT_DELAY_MS) {
        mainsAvailable = false;
        powerCutTiming = false;
        powerCutTelegramSent = false;

        float savedCostBeforeOff = cost;
        float savedEnergyBeforeOff = energyKWh;

        turnRelayOff("power_cut_under_30V_15s");

        String msg = "🔴 RELAY OFF AFTER POWER CUT\n\n";
        msg += "📉 Voltage: " + String(voltageFiltered, 1) + " V\n";
        msg += "💰 Saved Cost: Rs " + String(savedCostBeforeOff, 2) + "\n";
        msg += "🔋 Saved Energy: " + String(savedEnergyBeforeOff, 5) + " kWh";
        sendTelegramMessage(msg);
      }
    }
  } else {
    powerCutTiming = false;
    powerCutTelegramSent = false;
  }

  if (voltageFiltered > MAINS_RESTORE_VOLTAGE) {
    if (!mainsAvailable) {
      if (!powerRestoreTiming) {
        powerRestoreTiming = true;
        powerRestoreStartMillis = now;
        powerRestoreTelegramSent = false;
      }

      if (!powerRestoreTelegramSent) {
        powerRestoreTelegramSent = true;

        String msg = "✅ POWER RESTORED\n\n";
        msg += "📈 Voltage: " + String(voltageFiltered, 1) + " V\n";
        msg += "⏱ Waiting 2 seconds\n";
        msg += "🔌 Relay will turn ON again";
        sendTelegramMessage(msg);
      }

      if (now - powerRestoreStartMillis >= POWER_RESTORE_DELAY_MS) {
        mainsAvailable = true;
        powerRestoreTiming = false;
        powerRestoreTelegramSent = false;

        turnRelayOn("power_restored_auto_on");

        String msg = "🟢 RELAY ON AGAIN\n\n";
        msg += "📈 Voltage: " + String(voltageFiltered, 1) + " V\n";
        msg += "🔌 Plug restored automatically";
        sendTelegramMessage(msg);
      }
    }
  } else if (!mainsAvailable) {
    powerRestoreTiming = false;
    powerRestoreTelegramSent = false;
  }
}

// ================= LIMIT ALERTS =================
void checkLimitAlerts() {
  if (!sessionActive) return;

  if (!highCurrentSent && currentFiltered >= HIGH_CURRENT_LIMIT) {
    highCurrentSent = true;
    sendTelegramMessage("High current warning\nCurrent: " + String(currentFiltered, 2) + " A");
  }

  if (!highPowerSent && powerW >= HIGH_POWER_LIMIT) {
    highPowerSent = true;
    sendTelegramMessage("High power warning\nPower: " + String(powerW, 1) + " W");
  }

  if (!highEnergySent && energyKWh >= HIGH_ENERGY_LIMIT) {
    highEnergySent = true;
    sendTelegramMessage("High energy warning\nEnergy: " + String(energyKWh, 3) + " kWh");
  }

  if (!highCostSent && cost >= HIGH_COST_LIMIT) {
    highCostSent = true;
    sendTelegramMessage("High cost warning\nCost: Rs " + String(cost, 2));
  }
}

// ================= OLED DISPLAY =================
void showCurrent(float voltage, float current, float power, float energy) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("SMART ENERGY METER");

  display.setCursor(0, 12);
  display.print("V: ");
  display.print(voltage, 1);
  display.println(" V");

  display.setCursor(0, 24);
  display.print("I: ");
  display.print(current, 2);
  display.println(" A");

  display.setCursor(0, 36);
  display.print("P: ");
  display.print(power, 1);
  display.println(" W");

  display.setCursor(0, 50);
  display.print("E: ");
  display.print(energy, 5);
  display.println(" kWh");

  if (timerActive) {
    display.drawLine(92, 10, 92, 63, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(97, 12);
    display.println("TMR");
    display.setCursor(97, 26);
    display.println(formatTimerRemaining());
    display.setCursor(97, 42);
    display.println("ON");
  }

  display.display();
}

// ================= WIFI SETUP =================
void connectWiFi() {
  if (activeWiFiSSID.length() == 0) {
    Serial.println("No WiFi SSID. Starting setup mode.");
    startWiFiSetupMode("no_wifi_credentials");
    return;
  }

  showMessage("Connecting WiFi", activeWiFiSSID, "Please wait...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(activeWiFiSSID.c_str(), activeWiFiPassword.c_str());
  ignoreCurrentDuringWiFiChange("wifi_connect_start");

  int tries = 0;

  while (WiFi.status() != WL_CONNECTED && tries < WIFI_STARTUP_TRIES) {
    delay(WIFI_STARTUP_DELAY_MS);
    Serial.print(".");
    tries++;
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi failed. Meter continues without WiFi.");
    showMessage("WiFi not found", "Meter still works", "Retrying later");
    delay(500);

    if (!hasSavedWiFiCredentials) {
      startWiFiSetupMode("default_wifi_failed");
    }
  }
}

void maintainWiFiAndFirebase() {
  unsigned long now = millis();

  if (wifiSetupModeActive) {
    return;
  }

  if (activeWiFiSSID.length() == 0) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWiFiReconnectAttempt >= WIFI_RECONNECT_INTERVAL_MS) {
      lastWiFiReconnectAttempt = now;

      Serial.println("Background WiFi reconnect attempt...");
      WiFi.mode(WIFI_STA);
      WiFi.begin(activeWiFiSSID.c_str(), activeWiFiPassword.c_str());
      ignoreCurrentDuringWiFiChange("wifi_reconnect_attempt");
    }

    return;
  }

  if (WiFi.status() == WL_CONNECTED && !firebaseStarted) {
    Serial.println("WiFi available. Starting Firebase now...");

    configTime(GMT_OFFSET_SECONDS, DAYLIGHT_OFFSET_SECONDS, "pool.ntp.org", "time.nist.gov");
    setupFirebase();

    if (firebaseStarted) {
      createDefaultFirebaseData();
      uploadLiveData();
      sendTelegramMessage("✅ Smart Energy Meter connected to WiFi\nDevice: " + String(DEVICE_ID));
    }
  }
}

// ================= FIREBASE SETUP =================
void setupFirebase() {
  if (firebaseStarted) return;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Firebase skipped because WiFi is not connected.");
    return;
  }

  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase anonymous sign-up OK");

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    firebaseStarted = true;
  } else {
    Serial.print("Firebase sign-up failed: ");
    Serial.println(config.signer.signupError.message.c_str());
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  randomSeed(esp_random());

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), relayButtonISR, CHANGE);
  pinMode(WIFI_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  relayState = false;
  writeRelayPin(false);

  analogReadResolution(12);
  analogSetPinAttenuation(CURRENT_SENSOR_PIN, ADC_11db);
  analogSetPinAttenuation(VOLTAGE_SENSOR_PIN, ADC_11db);

  Wire.begin(OLED_SDA, OLED_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED failed");
    while (true);
  }

  showMessage("Smart Meter", "Starting...", "Please wait");
  delay(300);

  loadSavedWiFiCredentials();
  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    configTime(GMT_OFFSET_SECONDS, DAYLIGHT_OFFSET_SECONDS, "pool.ntp.org", "time.nist.gov");
    setupFirebase();
  }

  showMessage("ACS712 + ZMPT101B", "Keep load OFF", "Zero Calib...");
  delay(3000);

  calibrateZeroCurrent();

  Serial.print("Zero Current Voltage 1: ");
  Serial.println(zeroCurrentVoltage, 4);

  showMessage("Final Calib", "Plug/Load OFF", "Do not connect load");
  delay(2000);

  relayState = true;
  writeRelayPin(true);
  delay(1500);

  calibrateZeroCurrent();
  calibrateNoLoadCurrent();

  Serial.print("Zero Current Voltage 2: ");
  Serial.println(zeroCurrentVoltage, 4);

  Serial.print("No Load RMS Base: ");
  Serial.println(noLoadCurrentRMS, 4);

  resetCurrentState();

  mainsAvailable = true;
  startNewSession("startup");

  if (firebaseIsReady()) {
    Firebase.RTDB.setInt(&fbdo, controlPath() + "/relayCommand", 1);
  }

  relayState = true;
  writeRelayPin(true);
  uploadLiveData();
  createDefaultFirebaseData();

  showMessage("Ready", "Relay ON", "Firebase Mode");
  delay(300);

  sendTelegramMessage("Smart Energy Meter Online\nDevice: " + String(DEVICE_ID));
}

// ================= LOOP =================
void loop() {
  handleButton();
  handleWiFiButton();
  checkWiFiSetupTimeout();
  handleSetupWebServer();
  maintainWiFiAndFirebase();

  voltageRMS = readACVoltageRMS();

  // Fast voltage update without changing voltage sensor calibration logic.
  // Big jump 0V -> 230V is shown quickly. Small changes are still smoothed.
  if (voltageRMS < VOLTAGE_NOISE_CUTOFF) {
    voltageFiltered = 0.0;
  } else if (voltageFiltered == 0.0 || fabs(voltageRMS - voltageFiltered) > 40.0) {
    voltageFiltered = voltageRMS;
  } else {
    voltageFiltered = 0.60 * voltageFiltered + 0.40 * voltageRMS;
  }

  if (voltageFiltered < VOLTAGE_NOISE_CUTOFF) {
    voltageFiltered = 0.0;
  }

  checkMainsStatus();

  if (relayState && mainsAvailable) {
    currentRMS = readACCurrentRMS();

    if (currentRMS == 0.0 && !loadDetected) {
      currentFiltered = 0.0;
    } else {
      // Fast current update without changing current sensor/load detection logic.
      // First valid current appears quickly, then small changes are smoothed.
      if (currentFiltered == 0.0 || fabs(currentRMS - currentFiltered) > 0.20) {
        currentFiltered = currentRMS;
      } else {
        currentFiltered = 0.60 * currentFiltered + 0.40 * currentRMS;
      }
    }

    if (currentFiltered < NOISE_CUTOFF && !loadDetected) {
      currentFiltered = 0.0;
    }
  } else {
    resetCurrentState();
  }

  powerW = voltageFiltered * currentFiltered;

  updateEnergyNow();
  checkLimitAlerts();
  checkTimerAutoOff();
  uploadMinuteCostPoint();
  uploadUsageLogPoint();

  unsigned long now = millis();

  if (now - lastSettingsRead >= 5000) {
    lastSettingsRead = now;
    readSettingsFromFirebase();
  }

  if (now - lastFirebaseRead >= 1500) {
    lastFirebaseRead = now;
    readControlFromFirebase();
  }

  if (now - lastFirebaseUpload >= 2000) {
    lastFirebaseUpload = now;
    uploadLiveData();
  }

  if (wifiSetupModeActive && millis() - wifiSetupModeStartMillis < WIFI_SETUP_DISPLAY_MS) {
    showWiFiSetupScreen();
  } else {
    showCurrent(voltageFiltered, currentFiltered, powerW, energyKWh);
  }

  Serial.print("Relay: ");
  Serial.print(relayState ? "ON" : "OFF");

  Serial.print(" | WiFi: ");
  Serial.print(WiFi.status() == WL_CONNECTED ? "ON" : "OFF");

  Serial.print(" | Firebase: ");
  Serial.print(firebaseIsReady() ? "ON" : "OFF");

  Serial.print(" | Mains: ");
  Serial.print(mainsAvailable ? "ON" : "OFF");

  Serial.print(" | V: ");
  Serial.print(voltageFiltered, 2);

  Serial.print(" V | I: ");
  Serial.print(currentFiltered, 3);

  Serial.print(" A | Base: ");
  Serial.print(noLoadCurrentRMS, 3);

  Serial.print(" A | LoadDetected: ");
  Serial.print(loadDetected ? "YES" : "NO");

  Serial.print(" | P: ");
  Serial.print(powerW, 2);

  Serial.print(" W | E: ");
  Serial.print(energyKWh, 6);

  Serial.print(" kWh | Cost: Rs ");
  Serial.print(cost, 2);

  Serial.println();
}