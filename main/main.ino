#include "Base64.h"

#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <Wire.h>
#include <FirebaseESP32.h>
// Provide the token generation process info.
#include <addons/TokenHelper.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>
#include <time.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

//defining wifi
const char* ssid = "Coll";
const char* password = "collhins23";

WiFiServer server(80);

//defining firebase
// Firebase API Key
#define DATABASE_URL "https://fingerprint-d17d7-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define API_KEY "AIzaSyC6fNUDEB0fcg2YSCHfWEw9wRroOpAZpac"

#define USER_EMAIL "fingerprintiot24@gmail.com"
#define USER_PASSWORD "F1n&erprint"

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

String enrollPath = "/enrollMode";
String namePath = "/tempName";
String deletePath = "/deleteMode";
String tempIDPath = "/tempId";
String logIDPath = "/logId";

unsigned long sendDataPrevMillis = 0;

unsigned long count = 0;
const unsigned long timeoutPeriod = 1000;  // Timeout set to 1000ms (1 second)
unsigned long lastFirebaseCallTime = 0;

int loopCount;
bool fingerprintState;
uint8_t id;
String tempName;
String logID;
int lastLogID;
//defining pin
#define BUZZER_PIN 5
#define RELAY_PIN 19
#define BUTTON_PIN 4

#define LOOP_DELAY 10

HardwareSerial mySerial(2);

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

// NTP server and time zone configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 25200;  // Adjust for your time zone (0 for UTC)
const int daylightOffset_sec = 0;  // Adjust for daylight savings if applicable

void initWiFi() {
  lcd.clear();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to Wifi");
    delay(300);
  }
  Serial.println(WiFi.localIP());
  server.begin();
}

void initFingerprint() {
  while (!Serial)
    ;
  mySerial.begin(57600, SERIAL_8N1, 16, 17);
  delay(100);
  Serial.println("\n\nAdafruit finger detect test");

  // set the data rate for the sensor serial port
  finger.begin(57600);
  delay(5);
  if (finger.verifyPassword()) {
    Serial.println("Found fingerprint sensor!");
  } else {
    Serial.println("Did not find fingerprint sensor :(");
    while (1) { delay(1); }
  }

  Serial.println(F("Reading sensor parameters"));
  finger.getParameters();
  Serial.print(F("Status: 0x"));
  Serial.println(finger.status_reg, HEX);
  Serial.print(F("Sys ID: 0x"));
  Serial.println(finger.system_id, HEX);
  Serial.print(F("Capacity: "));
  Serial.println(finger.capacity);
  Serial.print(F("Security level: "));
  Serial.println(finger.security_level);
  Serial.print(F("Device address: "));
  Serial.println(finger.device_addr, HEX);
  Serial.print(F("Packet len: "));
  Serial.println(finger.packet_len);
  Serial.print(F("Baud rate: "));
  Serial.println(finger.baud_rate);

  finger.getTemplateCount();

  if (finger.templateCount == 0) {
    Serial.print("Sensor doesn't contain any fingerprint data. Please run the 'enroll' example.");
  } else {
    Serial.println("Waiting for valid finger...");
    Serial.print("Sensor contains ");
    Serial.print(finger.templateCount);
    Serial.println(" templates");
  }
}

void initLCD() {
  lcd.init();
  lcd.backlight();
  //Setup Awal Ketika Running
  lcd.setCursor(2, 0);
  lcd.print("KELAS IOT");
  lcd.setCursor(0, 1);
  lcd.print("FAKULTAS SOCS");
}

void initFirebase() {
  config.api_key = API_KEY;

  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  config.database_url = DATABASE_URL;

  config.token_status_callback = tokenStatusCallback;  // see addons/TokenHelper.h

  Firebase.reconnectNetwork(true);
  fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
  Firebase.begin(&config, &auth);

  Firebase.setDoubleDigits(5);
  Firebase.setReadTimeout(fbdo, 10);
}

void setup() {
  Serial.begin(9600);
  loopCount = 0;
  fingerprintState = false;
  //Pin Initialization
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  //LCD Initialization
  initLCD();

  //Wifi Initialization
  initWiFi();
  //Fingerprint Initialization
  initFingerprint();
  initFirebase();
  getLogID();
}

void uploadFingerprintToFirebase(uint8_t fingerprintID, const String& name) {
  byte fingerprintData[512];
  if (finger.getModel() != FINGERPRINT_OK) {
    Serial.println("Failed to get model");
    return;
  }

  // Encode fingerprint data to Base64
  String base64Image = base64::encode(fingerprintData, sizeof(fingerprintData));

  // Prepare path for Firebase, with each fingerprint ID as the key
  String path = "/fingerprints/" + String(fingerprintID);

  // Create a JSON object for storing name and Base64 data
  FirebaseJson json;
  json.set("name", name);
  json.set("base64Image", base64Image);

  // Upload JSON to Firebase
  if (Firebase.ready()) {
    if (Firebase.setJSON(fbdo, path, json)) {
      Serial.println("Fingerprint and name uploaded successfully.");
    } else {
      Serial.println("Error uploading fingerprint: " + fbdo.errorReason());
    }
  }
}

void uploadLog(const String& newname, const String& activity, const String& status) {
  // Prepare path for Firebase, with each fingerprint ID as the key
  String path = "/log/" + logID;
  String date = getDate();
  // Create a JSON object for storing name and Base64 data
  FirebaseJson json;
  json.set("activity", activity);
  json.set("date", date);
  json.set("name", newname);
  json.set("status", status);

  // Upload JSON to Firebase
  if (Firebase.ready()) {
    if (Firebase.setJSON(fbdo, path, json)) {
      Serial.println("Fingerprint and name uploaded successfully.");
    } else {
      Serial.println("Error uploading fingerprint: " + fbdo.errorReason());
    }
  }
  int newID = logID.toInt() + 1;
  logID = String(newID);
  Firebase.setString(fbdo, logIDPath, logID);
}


String getDate() {
  // Synchronize time with NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // Wait for time to be set
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo)) {
    Serial.println("Failed to obtain time");
    return " ";
  }

  // Format the time
  char dateTimeStr[30];
  sprintf(dateTimeStr, "%04d-%02d-%02d %02d:%02d:%02d WIB",
          timeInfo.tm_year + 1900,  // Correct year offset
          timeInfo.tm_mon + 1,      // Month is 0-based
          timeInfo.tm_mday,
          timeInfo.tm_hour,
          timeInfo.tm_min,
          timeInfo.tm_sec);

  // Print the formatted time
  Serial.printf("Current Date and Time: %s\n", dateTimeStr);
  return dateTimeStr;
}

uint8_t readnumber(void) {
  uint8_t num = 0;

  while (num == 0) {
    while (!Serial.available())
      ;
    num = Serial.parseInt();
  }
  return num;
}

void getLogID() {
  if (Firebase.getString(fbdo, logIDPath)) {
    logID = fbdo.stringData();
    Serial.println("Received name: " + logID);
  } else {
    Serial.println("Failed to fetch logs");
  }
}

void getTempName() {
  if (Firebase.getString(fbdo, namePath)) {
    tempName = fbdo.stringData();
    Serial.println("Received name: " + tempName);
    Firebase.setString(fbdo, enrollPath, "");
  }
}

String getName(int id) {
  if (Firebase.getString(fbdo, "/fingerprints/" + String(id) + "/name")) {
    return fbdo.stringData();
  }
  return " ";
}

void loop() {

  if (Firebase.ready()) {
    // Check if timeout period has passed since last Firebase request
    if (millis() - lastFirebaseCallTime >= timeoutPeriod) {
      // Attempt to get enroll mode from Firebase
      if (Firebase.getBool(fbdo, enrollPath)) {
        bool enrollMode = fbdo.boolData();
        if (enrollMode && !fingerprintState) {
          fingerprintState = true;
          getTempName();
          enrollFingerprint();
          lcd.clear();
          // Reset flag in Firebase after enrollment completes
          Firebase.setBool(fbdo, enrollPath, false);
          fingerprintState = false;
        }
      }

      // Attempt to get delete mode from Firebase
      if (Firebase.getBool(fbdo, deletePath)) {
        bool deleteMode = fbdo.boolData();
        if (deleteMode) {
          uint8_t deleteID = getDeleteID();
          finger.deleteModel(deleteID);
          String deleteName = getName(deleteID);
          uploadLog(tempName, "Delete", "Success");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Fingerprint");
          lcd.setCursor(0, 1);
          lcd.print("Removed");
          deleteBuzzer();
          // Reset delete flag
          Firebase.setBool(fbdo, deletePath, false);
          lcd.clear();
        }
      }

      // Update the last time Firebase was called
      lastFirebaseCallTime = millis();
    }
  }

  // Run getFingerprintID by default if not in enrollment mode
  if (!fingerprintState) {
    getFingerprintID();
  }
}

uint8_t getDeleteID() {
  if (Firebase.getInt(fbdo, tempIDPath)) {
    uint8_t id = fbdo.intData();
    Firebase.setInt(fbdo, enrollPath, 0);
    return id;
  }
  return 0;
}

void enrollFingerprint() {
  lcd.clear();
  id = findAvailableID();
  if (id == 0) {  // ID #0 not allowed, try again!
    return;
  }
  
  while (true) {
    lcd.setCursor(0, 0);
  lcd.print("Enrolling ID#");
  lcd.setCursor(13, 0);
  lcd.print(id);
  Serial.print("Enrolling ID #");
  Serial.println(id);
    int result = getFingerprintEnroll();  // Store the return value

    if (result == 255) {
      uploadLog(tempName, "Enroll", "Failed");
      failBuzzer();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enrollment failed.");
      lcd.setCursor(0, 1);
      lcd.print("Retrying...");
      delay(2000);
      lcd.clear();
    } else {
        Serial.println("Fingerprint enrollment successful!");
      break;  // Exit the loop
    }

    delay(500);
  }
}

uint8_t getFingerprintID() {
  lcd.setCursor(0, 0);
  lcd.print("Scanning Finger");
  lcd.setCursor(0, 1);
  lcd.print("Fingerprint");
  uint8_t p = finger.getImage();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image taken");
      break;
    case FINGERPRINT_NOFINGER:
      Serial.println("No finger detected");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_IMAGEFAIL:
      Serial.println("Imaging error");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK success!

  p = finger.image2Tz();
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return p;
    default:
      Serial.println("Unknown error");
      return p;
  }

  // OK converted!
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println("Found a print match!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    Serial.println("Communication error");
    return p;
  } else if (p == FINGERPRINT_NOTFOUND) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Did not find");
    lcd.setCursor(0, 1);
    lcd.print("a match");
    Serial.println("Did not find a match");
    uploadLog(" ", "Open Lock", "Failed");
    failBuzzer();
    delay(1000);
    return p;
  } else {
    Serial.println("Unknown error");
    return p;
  }

  // found a match!
  lcd.clear();
  Serial.print("Found ID #");
  Serial.print(finger.fingerID);
  lcd.setCursor(0, 0);
  lcd.print("Found ID #");
  lcd.setCursor(10, 0);
  lcd.print(finger.fingerID);
  Serial.print(" with confidence of ");
  Serial.println(finger.confidence);
  lcd.setCursor(0, 1);
  lcd.print("Confidence: ");
  lcd.setCursor(12, 1);
  lcd.print(finger.confidence);
  digitalWrite(RELAY_PIN, HIGH);
  uploadLog(getName(finger.fingerID), "Open Lock", "Success");
  successBuzzer();
  delay(1500);
  digitalWrite(RELAY_PIN, LOW);
  lcd.clear();
  return finger.fingerID;
}


//Enrolling fingerprint
uint8_t getFingerprintEnroll() {
  lcd.setCursor(0, 1);
  lcd.print("Put your finger...");
  int z = -1;
  int p = -1;
  Serial.print("Waiting for valid finger to enroll as #");
  Serial.println(id);
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        lcd.print("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        lcd.print("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        lcd.print("Imaging error");
        break;
      default:
        lcd.print("Unknown error");
        break;
    }
  }

  // OK success!
  lcd.clear();
  lcd.setCursor(0, 0);
  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      lcd.print("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      lcd.print("Image too messy");
      return z;
    case FINGERPRINT_PACKETRECIEVEERR:
      lcd.print("Communication error");
      return z;
    case FINGERPRINT_FEATUREFAIL:
      lcd.print("Could not find fingerprint features");
      return z;
    case FINGERPRINT_INVALIDIMAGE:
      lcd.print("Could not find fingerprint features");
      return z;
    default:
      lcd.print("Unknown error");
      return z;
  }
  lcd.setCursor(0, 1);
  lcd.print("Remove finger");
  delay(2000);
  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ID #");
  lcd.setCursor(4, 0);
  lcd.print(id);
  p = -1;
  lcd.setCursor(0, 1);
  lcd.print("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        Serial.println("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        Serial.print(".");
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        Serial.println("Communication error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        Serial.println("Imaging error");
        break;
      default:
        Serial.println("Unknown error");
        break;
    }
  }

  // OK success!

  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      Serial.println("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      Serial.println("Image too messy");
      return z;
    case FINGERPRINT_PACKETRECIEVEERR:
      Serial.println("Communication error");
      return z;
    case FINGERPRINT_FEATUREFAIL:
      Serial.println("Could not find fingerprint features");
      return z;
    case FINGERPRINT_INVALIDIMAGE:
      Serial.println("Could not find fingerprint features");
      return z;
    default:
      Serial.println("Unknown error");
      return z;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ID #");
  lcd.setCursor(4, 0);
  lcd.print(id);

  lcd.setCursor(0, 1);
  // OK converted!
  Serial.print("Creating model for #");
  Serial.println(id);

  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    lcd.print("Prints matched!");
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    // lcd.print("Communication error");
    return z;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    lcd.print("Fingerprints did not match");
    return z;
  } else {
    // lcd.print("Unknown error");
    return z;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ID #");
  lcd.setCursor(4, 0);
  lcd.print(id);

  lcd.setCursor(0, 1);
  Serial.print("ID ");
  Serial.println(id);
  p = finger.storeModel(id);
  uploadFingerprintToFirebase(id, tempName);
  uploadLog(tempName, "Enroll", "Success");
  lcd.print("Enrollment complete.");
  addBuzzer();
  lcd.clear();
  lcd.setCursor(0, 0);

  delay(2000);
  return true;
}

uint8_t findAvailableID() {
  for (uint8_t id = 1; id <= finger.capacity; id++) {
    if (!isIDOccupied(id)) {
      return id;  // Return the first unoccupied ID
    }
  }
  return 0;  // Return 0 if no IDs are available
}

bool isIDOccupied(uint8_t id) {
  uint8_t result = finger.loadModel(id);
  if (result == FINGERPRINT_OK) {
    Serial.print("ID ");
    Serial.print(id);
    Serial.println(" is occupied.");
    return true;
  } else if (result == FINGERPRINT_NOTFOUND) {
    Serial.print("ID ");
    Serial.print(id);
    Serial.println(" is free.");
    return false;
  } else {
    Serial.print("Error checking ID ");
    Serial.print(id);
    Serial.println(".");
    return false;  // Or handle errors differently if needed
  }
}

// Function to capture fingerprint image and convert to Base64
String captureFingerprintAsBase64() {
  if (finger.getImage() != FINGERPRINT_OK) return "";
  if (finger.image2Tz() != FINGERPRINT_OK) return "";

  // Convert fingerprint image to Base64 (this example uses simulated data)
  byte fingerData[512];
  finger.getModel();
  String base64Image = base64::encode(fingerData, 512);
  Serial.println(base64Image);
  return base64Image;
}

void clearFingerprintData() {
  Serial.println("Clearing all fingerprint data...");
  for (uint8_t id = 1; id <= finger.capacity; id++) {
    finger.deleteModel(id);
  }
  Serial.println("All fingerprint data cleared.");
}

void successBuzzer() {
  //geda
  tone(BUZZER_PIN, 494);
  delay(125);
  tone(BUZZER_PIN, 659);
  delay(125);
  noTone(BUZZER_PIN);
  delay(125);

  //gedi
  tone(BUZZER_PIN, 555);
  delay(125);
  tone(BUZZER_PIN, 659);
  delay(125);
  noTone(BUZZER_PIN);
  delay(125);

  //geda
  tone(BUZZER_PIN, 555);
  delay(125);
  tone(BUZZER_PIN, 659);
  delay(125);
  noTone(BUZZER_PIN);
  delay(125);
  //geda
  tone(BUZZER_PIN, 555);
  delay(125);
  tone(BUZZER_PIN, 555);
  delay(125);
  noTone(BUZZER_PIN);
  delay(125);
  //oo
  tone(BUZZER_PIN, 555);
  delay(1000);
  noTone(BUZZER_PIN);
}

void deleteBuzzer() {
  //geda
  tone(BUZZER_PIN, 587);
  delay(250);
  tone(BUZZER_PIN, 784);
  delay(250);
  tone(BUZZER_PIN, 880);
  delay(125);
  tone(BUZZER_PIN, 698);
  delay(250);
  tone(BUZZER_PIN, 784);
  delay(1000);
  noTone(BUZZER_PIN);
  delay(250);

  tone(BUZZER_PIN, 924);
  delay(250);
  tone(BUZZER_PIN, 880);
  delay(125);
  tone(BUZZER_PIN, 698);
  delay(250);
  tone(BUZZER_PIN, 784);
  delay(1000);
  noTone(BUZZER_PIN);
}

void addBuzzer() {
  tone(BUZZER_PIN, 659);
  delay(150);
  tone(BUZZER_PIN, 659);
  delay(150);
  tone(BUZZER_PIN, 659);
  delay(150);
  tone(BUZZER_PIN, 659);
  delay(150);
  tone(BUZZER_PIN, 698);
  delay(150);
  tone(BUZZER_PIN, 659);
  delay(300);
  tone(BUZZER_PIN, 831);
  delay(300);
  tone(BUZZER_PIN, 880);
  delay(300);
  tone(BUZZER_PIN, 998);
  delay(1000);
  noTone(BUZZER_PIN);
}
void failBuzzer() {
  tone(BUZZER_PIN, 555);
  delay(250);
  noTone(BUZZER_PIN);
  tone(BUZZER_PIN, 555);
  delay(250);
  noTone(BUZZER_PIN);
  tone(BUZZER_PIN, 555);
  delay(250);
  noTone(BUZZER_PIN);
}