//#include <ESP8266WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Esp32WifiManager.h>
#include <NTPClient.h>
#include <FirebaseESP32.h>
#include <WiFiUdp.h>

#define buzzerPin 18
bool buzzerOn = false;

const int servoPin = 4;
int sudut = 6;
Servo servoBawah;

LiquidCrystal_I2C lcd(0x27,16,2);

#define NTP_OFFSET 60 * 60     // In seconds
#define NTP_INTERVAL 60 * 1000 // In miliseconds
#define NTP_ADDRESS "id.pool.ntp.org"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

// Wifi config
const char *ssid = "INTERNET";
const char *password = "internet123";

// Define firebase config
#define FIREBASE_HOST "intara-47-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "2sSYk7KAz2HSdY5eZUemYVcNBhQWSlyfkKzM3lRN"
FirebaseData fbdo;

// Firebase initial variables
String deviceid = "device-1";
String patientid = "";

const int photodiodePin = 15;  // Pin sensor fotodioda
int maxTetes = 0;             // Jumlah tetes maksimum yang akan dihitung
int tetes = 0;                // Variabel untuk menghitung banyak tetes

unsigned long startTime;       // Waktu awal
unsigned long currentTime;     // Waktu sekarang
unsigned long elapsedTime;     // Waktu yang telah berlalu
unsigned long dropsPerMinute;  // Tetes per menit
int tetesSebelumnya = 0;       // Jumlah tetes pada pembacaan sebelumnya

void setup() {
  Serial.begin(9600);          // Mulai komunikasi serial dengan baud rate 9600
  pinMode(photodiodePin, INPUT);  // Set pin sensor fotodioda sebagai input
  pinMode(buzzerPin, OUTPUT);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  FirebaseData fbdata;
  Firebase.get(fbdata, "/devicestore/" + deviceid + "/patientid");
  patientid = fbdata.stringData();
  Firebase.get(fbdata, "/patientstore/" + patientid + "/targetinfusion");
  maxTetes = fbdata.intData();
  // time utils setup
  timeClient.begin();
  // END OF NEW ADDITIONS

  lcd.begin();
  lcd.backlight();
  lcd.clear();
  

  servoBawah.attach(servoPin);
  servoBawah.write(sudut);
  delay(500);

  lcd.setCursor(5, 0);
  lcd.print("INTARA");

  lcd.setCursor(0, 1);
  lcd.print("JUMLAH TETES: ");
  // Serial.println("Masukkan jumlah tetes: ");  // Tampilkan pesan di Serial Monitor

  // while (!Serial.available()) {}  // Tunggu hingga pengguna memasukkan nilai
  // maxTetes = Serial.parseInt();  // Baca nilai yang dimasukkan oleh pengguna
  lcd.setCursor(13, 1);
  lcd.print(maxTetes);
  delay(2000);
  Serial.print("Jumlah tetes yang akan dihitung: ");
  Serial.println(maxTetes);  // Tampilkan nilai yang dimasukkan di Serial Monitor
}

void loop() {
  int sensorValue = digitalRead(photodiodePin);  // Baca nilai dari pin sensor fotodioda

  if (tetes != tetesSebelumnya) {
    currentTime = millis();              // Mendapatkan waktu sekarang
    elapsedTime = currentTime - startTime;  // Menghitung waktu yang telah berlalu
    dropsPerMinute = ((tetes - tetesSebelumnya) * 60000) / elapsedTime;  // Menghitung tetes per menit
  
    //lcd.setCursor(0, 2);
    //lcd.print("Tetes/menit: ");
    //lcd.println(dropsPerMinute);
    Serial.print("TPM : ");
    Serial.println(dropsPerMinute);
  
    tetesSebelumnya = tetes;  // Perbarui jumlah tetes pada pembacaan sebelumnya
    startTime = currentTime;  // Perbarui waktu awal untuk perhitungan selanjutnya
  }

  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print("INTARA");
  lcd.setCursor(0, 1);
  lcd.print("TETES: ");
  lcd.setCursor(6, 1);
  lcd.print(tetes);
  lcd.setCursor(9, 1);
  lcd.print("TPM: ");
  lcd.setCursor(13, 1);
  lcd.print(dropsPerMinute);
  
  if (tetes == 1) {
    startTime = millis();  // Menyimpan waktu awal
  }
  
  if (sensorValue == HIGH && tetes < maxTetes) {  // Jika sensor mendeteksi cairan dan jumlah tetes masih kurang dari batas maksimum
    tetes++;  // Tambahkan 1 pada variabel tetes
    Serial.print("Tetes ke-");  // Tampilkan pesan pada Serial Monitor
    Serial.println(tetes);
    delay(200);  // Jeda 200ms untuk mencegah penghitungan tetes berlebihan
  }

  else if(tetes < maxTetes){
    if(buzzerOn == false){
      tone(buzzerPin, 750);
      delay(1000);
      noTone(buzzerPin);
      buzzerOn = true;
    }
    servoBawah.write(10);
    delay(250);
  }
  
  else if (tetes >= maxTetes) {  // Jika jumlah tetes sudah mencapai batas maksimum
    Serial.println("Jumlah tetes sudah mencapai batas maksimum.");  // Tampilkan pesan pada Serial Monitor
    tetes = 0;
    dropsPerMinute = 0;
    
    if(buzzerOn == true){
      tone(buzzerPin, 750);
      delay(1000);
      noTone(buzzerPin);
      buzzerOn = false;
    }

    else{
    buzzerOn = false;
    }
    
    servoBawah.write(0);
    delay(250);

    inputBaru();
  }
  updateToFirebase(tetes, dropsPerMinute);
}
    
void inputBaru(){
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("LIMIT  TETES");
    delay(2000);
    lcd.clear();
    lcd.print("JUMLAH TETES: ");
    
    //Serial.println("Masukkan jumlah tetes baru: ");  // Tampilkan pesan di Serial Monitor
    Serial.println("Masukkan jumlah tetes baru: ");  // Tampilkan pesan di Serial Monitor
    while (!Serial.available()) {}  // Tunggu hingga pengguna memasukkan nilai
    maxTetes = Serial.parseInt();  // Baca nilai yang dimasukkan oleh pengguna
    lcd.setCursor(13, 0);
    lcd.println(maxTetes);
    Serial.print("Jumlah tetes yang akan dihitung: ");
    Serial.println(maxTetes);  // Tampilkan nilai yang dimasukkan di Serial Monitor
}


void updateToFirebase(unsigned long dropCount, unsigned long dropPerMin) {
	// get cur time 
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();

	// define usage id
  String strtime = String(epochTime);
  String usageid = deviceid + "_" + strtime;

	// updating data to firebase
  FirebaseJson jsonData;
  jsonData.set("usageid", usageid);
  jsonData.set("deviceid", deviceid);
  jsonData.set("patientid", patientid);
  jsonData.set("weightA", 0);
  jsonData.set("weightB", 0);
  jsonData.set("time", epochTime);
  jsonData.set("dropCount", dropCount);
  jsonData.set("dropPerMin", dropPerMin);
  Firebase.set(fbdo, "/usagestore/" + usageid, jsonData);
}

