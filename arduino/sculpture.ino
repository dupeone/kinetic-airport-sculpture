#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <FastLED.h>
#include <ArduinoJson.h>

// =========================
// WIFI
// =========================
const char* WIFI_SSID = "*******";
const char* WIFI_PASS = "*******";

// =========================
// RAPIDAPI / AERODATABOX
// =========================
const char* RAPIDAPI_HOST = "aerodatabox.p.rapidapi.com";
const char* RAPIDAPI_KEY  = "**************************";


// =========================
// SERVO DRIVER
// =========================
Adafruit_PWMServoDriver pca9685 = Adafruit_PWMServoDriver();

#define SERVO_MIN  280
#define SERVO_MAX  320
#define CENTER     300

// Rod 1 = JFK arrivals
// Rod 2 = LGA arrivals
// Rod 3 = JFK departures
// Rod 4 = LGA departures
#define SERVO_1 0
#define SERVO_2 4
#define SERVO_3 8
#define SERVO_4 12

const int STEP_SIZE  = 1;
const int STEP_DELAY = 60;

int pos1 = SERVO_MIN;  // arrivals start at extreme
int pos2 = SERVO_MIN;  // arrivals start at extreme
int pos3 = SERVO_MAX;  // departures start at extreme
int pos4 = SERVO_MAX;  // departures start at extreme

// =========================
// LED STRIP
// =========================
#define LED_PIN   5
#define NUM_LEDS  18

CRGB leds[NUM_LEDS];

uint8_t currentB1 = 20;
uint8_t currentB2 = 20;
uint8_t currentB3 = 20;
uint8_t currentB4 = 20;

// =========================
// TIMING
// =========================
unsigned long lastUpdateMs = 0;
const unsigned long UPDATE_INTERVAL_MS = 60000;  // 1 minute

// =========================
// DATA
// =========================
int jfkArrivals   = 0;
int lgaArrivals   = 0;
int jfkDepartures = 0;
int lgaDepartures = 0;

// previous counts, so rods still move if counts differ
int prevJfkArrivals   = -1;
int prevLgaArrivals   = -1;
int prevJfkDepartures = -1;
int prevLgaDepartures = -1;

// shorter window = more sensitivity
const int OFFSET_MINUTES   = 0;
const int DURATION_MINUTES = 15;

// =========================
// SERVO HELPERS
// =========================
void writeAll() {
  pca9685.setPWM(SERVO_1, 0, pos1);
  pca9685.setPWM(SERVO_2, 0, pos2);
  pca9685.setPWM(SERVO_3, 0, pos3);
  pca9685.setPWM(SERVO_4, 0, pos4);
}

void moveAllSmooth(int t1, int t2, int t3, int t4) {
  while (pos1 != t1 || pos2 != t2 || pos3 != t3 || pos4 != t4) {
    if (pos1 < t1) pos1 += STEP_SIZE;
    else if (pos1 > t1) pos1 -= STEP_SIZE;

    if (pos2 < t2) pos2 += STEP_SIZE;
    else if (pos2 > t2) pos2 -= STEP_SIZE;

    if (pos3 < t3) pos3 += STEP_SIZE;
    else if (pos3 > t3) pos3 -= STEP_SIZE;

    if (pos4 < t4) pos4 += STEP_SIZE;
    else if (pos4 > t4) pos4 -= STEP_SIZE;

    if (abs(pos1 - t1) <= STEP_SIZE) pos1 = t1;
    if (abs(pos2 - t2) <= STEP_SIZE) pos2 = t2;
    if (abs(pos3 - t3) <= STEP_SIZE) pos3 = t3;
    if (abs(pos4 - t4) <= STEP_SIZE) pos4 = t4;

    writeAll();
    delay(STEP_DELAY);
  }

  writeAll();
}

// =========================
// LED HELPERS
// left = green
// right = blue
// brightness = amount
// =========================
void setSegmentBrightness(int startIdx, int endIdx, uint8_t brightness, bool isArrival) {
  for (int i = startIdx; i < endIdx; i++) {
    if (isArrival) {
      leds[i] = CRGB(0, brightness, 0);   // green
    } else {
      leds[i] = CRGB(0, 0, brightness);   // blue
    }
  }
}

void fadeLEDsTo(uint8_t targetB1, uint8_t targetB2, uint8_t targetB3, uint8_t targetB4) {
  bool done = false;

  while (!done) {
    done = true;

    if (currentB1 < targetB1) { currentB1++; done = false; }
    else if (currentB1 > targetB1) { currentB1--; done = false; }

    if (currentB2 < targetB2) { currentB2++; done = false; }
    else if (currentB2 > targetB2) { currentB2--; done = false; }

    if (currentB3 < targetB3) { currentB3++; done = false; }
    else if (currentB3 > targetB3) { currentB3--; done = false; }

    if (currentB4 < targetB4) { currentB4++; done = false; }
    else if (currentB4 > targetB4) { currentB4--; done = false; }

    setSegmentBrightness(0, 4,   currentB1, true);   // JFK arrivals
    setSegmentBrightness(4, 8,   currentB2, true);   // LGA arrivals
    setSegmentBrightness(8, 12,  currentB3, false);  // JFK departures
    setSegmentBrightness(12, 16, currentB4, false);  // LGA departures

    leds[16] = CRGB::Black;
    leds[17] = CRGB::Black;

    FastLED.show();
    delay(15);
  }
}

void flashLEDs() {
  for (int b = 0; b <= 120; b += 10) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB(b, b, b);
    }
    FastLED.show();
    delay(10);
  }

  for (int b = 120; b >= 0; b -= 10) {
    for (int i = 0; i < NUM_LEDS; i++) {
      leds[i] = CRGB(b, b, b);
    }
    FastLED.show();
    delay(10);
  }
}

// =========================
// MAPPING
// low count = extremes
// high count = near center, but never center
// arrivals:   280 -> 299
// departures: 320 -> 301
// =========================
int mapArrivalToServo(int count) {
  int c = constrain(count, 0, 20);
  return map(c, 0, 20, SERVO_MIN, 299);
}

int mapDepartureToServo(int count) {
  int c = constrain(count, 0, 20);
  return map(c, 0, 20, SERVO_MAX, 301);
}

uint8_t mapCountToBrightness(int count) {
  int c = constrain(count, 0, 20);
  return (uint8_t)map(c, 0, 20, 10, 160);
}

// force visible movement when counts change
int forceArrivalMovement(int mappedTarget, int newCount, int oldCount) {
  if (oldCount == -1) return mappedTarget;
  if (newCount == oldCount) return mappedTarget;

  int diff = newCount - oldCount;
  mappedTarget += diff;   // more arrivals = move inward on left side

  if (mappedTarget < SERVO_MIN) mappedTarget = SERVO_MIN;
  if (mappedTarget > 299) mappedTarget = 299;

  return mappedTarget;
}

int forceDepartureMovement(int mappedTarget, int newCount, int oldCount) {
  if (oldCount == -1) return mappedTarget;
  if (newCount == oldCount) return mappedTarget;

  int diff = newCount - oldCount;
  mappedTarget -= diff;   // more departures = move inward on right side

  if (mappedTarget < 301) mappedTarget = 301;
  if (mappedTarget > SERVO_MAX) mappedTarget = SERVO_MAX;

  return mappedTarget;
}

// =========================
// WIFI
// =========================
bool connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.println("WiFi failed");
  return false;
}

// =========================
// API
// =========================
String makeAirportURL(const String& code) {
  String url = "https://";
  url += RAPIDAPI_HOST;
  url += "/flights/airports/iata/";
  url += code;
  url += "?offsetMinutes=" + String(OFFSET_MINUTES);
  url += "&durationMinutes=" + String(DURATION_MINUTES);
  url += "&withLeg=false";
  url += "&direction=Both";
  url += "&withCancelled=false";
  url += "&withCodeshared=true";
  url += "&withCargo=false";
  url += "&withPrivate=false";
  url += "&withLocation=false";
  return url;
}

bool fetchAirportBoth(const String& airportCode, int &arrivalsCount, int &departuresCount) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = makeAirportURL(airportCode);

  Serial.println("Requesting:");
  Serial.println(url);

  if (!https.begin(client, url)) {
    Serial.println("HTTPS begin failed");
    return false;
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("x-rapidapi-host", RAPIDAPI_HOST);
  https.addHeader("x-rapidapi-key", RAPIDAPI_KEY);

  int httpCode = https.GET();
  Serial.print("HTTP code: ");
  Serial.println(httpCode);

  if (httpCode != 200) {
    String payload = https.getString();
    Serial.println("Error payload:");
    Serial.println(payload);
    https.end();
    return false;
  }

  String payload = https.getString();
  https.end();

  Serial.print("Payload length: ");
  Serial.println(payload.length());

  DynamicJsonDocument doc(90000);
  DeserializationError err = deserializeJson(doc, payload);

  if (err) {
    Serial.print("JSON parse error: ");
    Serial.println(err.c_str());
    return false;
  }

  if (!doc.is<JsonObject>()) {
    Serial.println("Unexpected JSON shape");
    Serial.println(payload.substring(0, 500));
    return false;
  }

  JsonObject root = doc.as<JsonObject>();

  arrivalsCount = 0;
  departuresCount = 0;

  if (root["arrivals"].is<JsonArray>()) {
    arrivalsCount = root["arrivals"].as<JsonArray>().size();
  } else if (root["Arrivals"].is<JsonArray>()) {
    arrivalsCount = root["Arrivals"].as<JsonArray>().size();
  }

  if (root["departures"].is<JsonArray>()) {
    departuresCount = root["departures"].as<JsonArray>().size();
  } else if (root["Departures"].is<JsonArray>()) {
    departuresCount = root["Departures"].as<JsonArray>().size();
  }

  Serial.print(airportCode);
  Serial.print(" arrivals: ");
  Serial.println(arrivalsCount);

  Serial.print(airportCode);
  Serial.print(" departures: ");
  Serial.println(departuresCount);

  return true;
}

void fetchAllAirportData() {
  int a = 0, d = 0;

  if (fetchAirportBoth("JFK", a, d)) {
    jfkArrivals = a;
    jfkDepartures = d;
  }

  delay(2000);  // helps with rate limiting

  if (fetchAirportBoth("LGA", a, d)) {
    lgaArrivals = a;
    lgaDepartures = d;
  }

  Serial.println("Current values:");
  Serial.print("JFK arrivals: ");   Serial.println(jfkArrivals);
  Serial.print("LGA arrivals: ");   Serial.println(lgaArrivals);
  Serial.print("JFK departures: "); Serial.println(jfkDepartures);
  Serial.print("LGA departures: "); Serial.println(lgaDepartures);
}

// =========================
// APPLY DATA
// =========================
void applyData() {
  int target1 = mapArrivalToServo(jfkArrivals);
  int target2 = mapArrivalToServo(lgaArrivals);
  int target3 = mapDepartureToServo(jfkDepartures);
  int target4 = mapDepartureToServo(lgaDepartures);

  // force visible motion when count changes
  target1 = forceArrivalMovement(target1, jfkArrivals, prevJfkArrivals);
  target2 = forceArrivalMovement(target2, lgaArrivals, prevLgaArrivals);
  target3 = forceDepartureMovement(target3, jfkDepartures, prevJfkDepartures);
  target4 = forceDepartureMovement(target4, lgaDepartures, prevLgaDepartures);

  uint8_t b1 = mapCountToBrightness(jfkArrivals);
  uint8_t b2 = mapCountToBrightness(lgaArrivals);
  uint8_t b3 = mapCountToBrightness(jfkDepartures);
  uint8_t b4 = mapCountToBrightness(lgaDepartures);

  Serial.println("Servo targets:");
  Serial.print("Rod 1: "); Serial.println(target1);
  Serial.print("Rod 2: "); Serial.println(target2);
  Serial.print("Rod 3: "); Serial.println(target3);
  Serial.print("Rod 4: "); Serial.println(target4);

  flashLEDs();
  moveAllSmooth(target1, target2, target3, target4);
  fadeLEDsTo(b1, b2, b3, b4);

  prevJfkArrivals   = jfkArrivals;
  prevLgaArrivals   = lgaArrivals;
  prevJfkDepartures = jfkDepartures;
  prevLgaDepartures = lgaDepartures;
}

// =========================
// SETUP / LOOP
// =========================
void setup() {
  Serial.begin(115200);
  delay(500);

  pca9685.begin();
  pca9685.setPWMFreq(50);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(80);

  writeAll();
  fadeLEDsTo(20, 20, 20, 20);

  connectWiFi();

  fetchAllAirportData();
  applyData();

  lastUpdateMs = millis();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (millis() - lastUpdateMs >= UPDATE_INTERVAL_MS) {
    fetchAllAirportData();
    applyData();
    lastUpdateMs = millis();
  }

  delay(50);
}
