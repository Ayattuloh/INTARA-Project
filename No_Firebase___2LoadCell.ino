//-------------------------------------------------------------------------------------
// HX711_ADC.h
// Arduino master library for HX711 24-Bit Analog-to-Digital Converter for Weigh Scales
// Olav Kallhovd sept2017
// Tested with      : HX711 asian module on channel A and YZC-133 3kg load cell
// Tested with MCU  : Arduino Nano
//-------------------------------------------------------------------------------------
// This is an example sketch on how to use this library for two ore more HX711 modules
// Settling time (number of samples) and data filtering can be adjusted in the config.h file

#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <HX711_ADC.h>
#include <ESP32Servo.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#endif
#include <NTPClient.h>
#include <FirebaseESP32.h>
#include <WiFiUdp.h>

#define NTP_OFFSET 60 * 60     // In seconds
#define NTP_INTERVAL 60 * 1000 // In miliseconds
#define NTP_ADDRESS "id.pool.ntp.org"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

#define FIREBASE_HOST "intara-47-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "2sSYk7KAz2HSdY5eZUemYVcNBhQWSlyfkKzM3lRN"
FirebaseData fbdo;

// Wifi config
const char *ssid = "INTERNET";
const char *password = "internet123";

String deviceid = "device-1";
String patientid = "";

//pins:
const int HX711_dout_1 = 14; //mcu > HX711 no 1 dout pin+
const int HX711_sck_1 = 27; //mcu > HX711 no 1 sck pin
const int HX711_dout_2 = 13; //mcu > HX711 no 2 dout pin
const int HX711_sck_2 = 12; //mcu > HX711 no 2 sck pin

#define buzzerPin 23
bool buzzerOn = false;

const int servoPin1 = 2;
const int servoPin2 = 4;

//HX711 constructor (dout pin, sck pin)
HX711_ADC LoadCell_1(HX711_dout_1, HX711_sck_1); //HX711 1
HX711_ADC LoadCell_2(HX711_dout_2, HX711_sck_2); //HX711 2

const int calVal_eepromAdress_1 = 0; // eeprom adress for calibration value load cell 1 (4 bytes)
const int calVal_eepromAdress_2 = 4; // eeprom adress for calibration value load cell 2 (4 bytes)
unsigned long t = 0;

LiquidCrystal_I2C lcd(0x27,20,4);

Servo servokiri;
Servo servokanan;

void setup() {
  Serial.begin(57600); delay(10);
  lcd.begin();
  lcd.backlight();
  lcd.setCursor(7, 0);
  lcd.print("INTARA");
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  // START OF NEW ADDITIONS
  // Firebase setup
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  FirebaseData fbdata;
  Firebase.get(fbdata, "/devicestore/" + deviceid + "/patientid");
  patientid = fbdata.stringData();
  // time utils setup
  timeClient.begin();
  // END OF NEW ADDITIONS
  pinMode(buzzerPin, OUTPUT);
  servokiri.attach(servoPin2);
  servokanan.attach(servoPin1);
  servokiri.write(180);
  delay(200);
  servokanan.write(30);
  delay(200);

  float calibrationValue_1; // calibration value load cell 1
  float calibrationValue_2; // calibration value load cell 2

  calibrationValue_1 = 738.16; // uncomment this if you want to set this value in the sketch
  calibrationValue_2 = 1052.56; // uncomment this if you want to set this value in the sketch
#if defined(ESP8266) || defined(ESP32)
  //EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch the value from eeprom
#endif
  //EEPROM.get(calVal_eepromAdress_1, calibrationValue_1); // uncomment this if you want to fetch the value from eeprom
  //EEPROM.get(calVal_eepromAdress_2, calibrationValue_2); // uncomment this if you want to fetch the value from eeprom

  LoadCell_1.begin();
  LoadCell_2.begin();
  //LoadCell_1.setReverseOutput();
  //LoadCell_2.setReverseOutput();
  unsigned long stabilizingtime = 2000; // tare preciscion can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  byte loadcell_1_rdy = 0;
  byte loadcell_2_rdy = 0;
  while ((loadcell_1_rdy + loadcell_2_rdy) < 2) { //run startup, stabilization and tare, both modules simultaniously
    if (!loadcell_1_rdy) loadcell_1_rdy = LoadCell_1.startMultiple(stabilizingtime, _tare);
    if (!loadcell_2_rdy) loadcell_2_rdy = LoadCell_2.startMultiple(stabilizingtime, _tare);
  }
  if (LoadCell_1.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 no.1 wiring and pin designations");
  }
  if (LoadCell_2.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 no.2 wiring and pin designations");
  }
  LoadCell_1.setCalFactor(calibrationValue_1); // user set calibration value (float)
  LoadCell_2.setCalFactor(calibrationValue_2); // user set calibration value (float)
  
  Serial.println("Startup is complete");
}

void loop() {
  static boolean newDataReady = 0;
  const int serialPrintInterval = 0; //increase value to slow down serial print activity
  const int batasInfus = 100;

  float a = LoadCell_1.getData();
  float b = LoadCell_2.getData();

  // check for new data/start next conversion:
  if (LoadCell_1.update()) newDataReady = true;
  LoadCell_2.update();

  //get smoothed value from data set
  if ((newDataReady)) {
    if (millis() > t + serialPrintInterval) {
      lcd.setCursor(3, 1);
      lcd.print("INFUS OTOMATIS");
      lcd.setCursor(3, 2);
      lcd.print("KIRI");
      lcd.setCursor(12, 2);
      lcd.print("KANAN");
      lcd.setCursor(3, 3);
      lcd.print(a);
      lcd.setCursor(12, 3);
      lcd.print(b);
      newDataReady = 0;
      t = millis();
    }
  }

// Memeriksa apakah ada infus yang habis
  if (a < batasInfus && b < batasInfus) {
    // Mengaktifkan buzzer
    tone(buzzerPin, 1500);
    delay(1000);
    noTone(buzzerPin);

    servokiri.write(180);
    delay(200);
    servokanan.write(30);
    delay(200);

  }

  // Menutup salah satu selang infus (benerin lagi)
  if (a <= batasInfus && b > batasInfus) {
    if(buzzerOn == false){
      tone(buzzerPin, 1000);
      delay(1000);
      noTone(buzzerPin);
      buzzerOn = true;
    }

      servokiri.write(180);
      delay(200);
      servokanan.write(90);
      delay(200);
    }
    else if (b <= batasInfus && a > batasInfus) {
    if(buzzerOn == false){
      tone(buzzerPin, 1000);
      delay(1000);
      noTone(buzzerPin);
      buzzerOn = true;
    }

      servokanan.write(30);
      delay(200);
      servokiri.write(90);
      delay(200);

    }
    
    else{
      if (a > batasInfus && b > batasInfus){
        int nilaiRandom = random(1, 3);
        if (nilaiRandom == 3){
          if(buzzerOn == false){
            tone(buzzerPin, 250);
            delay(1000);
            noTone(buzzerPin);
            buzzerOn = true;
          }

          servokiri.write(90);
          delay(200);
          servokanan.write(30);
          delay(200);
        }
        else{
          if(buzzerOn == true){
            tone(buzzerPin, 250);
            delay(1000);
            noTone(buzzerPin);
            buzzerOn = false;
          }

          servokanan.write(90);
          delay(200);
          servokiri.write(180);
          delay(200);
        }
      }   
    }

  // receive command from serial terminal, send 't' to initiate tare operation:
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') {
      LoadCell_1.tareNoDelay();
      LoadCell_2.tareNoDelay();
    }
  }

  //check if last tare operation is complete
  if (LoadCell_1.getTareStatus() == true) {
    Serial.println("Tare load cell 1 complete");
  }
  if (LoadCell_2.getTareStatus() == true) {
    Serial.println("Tare load cell 2 complete");
  }

  updateToFirebase(a, b);
}

void updateToFirebase(float weightA, float weightB) {
	// get cur time 
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();

	// firebase query filter
  QueryFilter query;
  query.orderBy("$key");
  query.startAt(deviceid + "_" + String(epochTime - 10));
  query.endAt(deviceid + "_" + String(epochTime));
  query.limitToLast(5);
 
  // Query data from firebase
  String ref = "/usagestore";
  FirebaseData fbdata;
  bool success = Firebase.getJSON(fbdata, ref, query);
  FirebaseJson json = fbdata.jsonObject();

  if (!success) {
    Serial.println("Failed updating to firebase");
    return;
  }
 
  size_t len = json.iteratorBegin();
  FirebaseJson::IteratorValue value;
  for (size_t i = 0; i < len; i++) {
    value = json.valueAt(i);
 
    if (!value.key.startsWith(deviceid)) continue;

    FirebaseJson docData;
    docData.setJsonData(value.value.c_str());
 
    // validate if docData already has weight value
    FirebaseJsonData weight;
    docData.get(weight, "weightA");
    if (weight.intValue != 0) continue;
    docData.get(weight, "weightB");
    if (weight.intValue != 0) continue; 
 
    // docData.set("weightA", 99);
    // docData.set("weightB", 88);
    String path = "/usagestore/" + String(value.key.c_str());
    Firebase.setFloat(fbdo, path + "/weightA", weightA);
    Firebase.setFloat(fbdo, path + "/weightB", weightB);
 
    Serial.println("sukses update " + path);
  }
 
  // Clear all list to free memory
  json.iteratorEnd();
}

