#define CurrentVersion 2.0
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <SensorModbusMaster.h>
#include <SoftwareSerial.h>
#include "OTAupdate.h"

#define LED_BUILTIN 2
#define SUMP1_MOTOR 32
#define A_block_OHT_MOTOR 33
WiFiMulti WiFiMulti;

const byte requestframe[] = { 0x01, 0x03, 0x00, 0x04, 0x00, 0x01, 0xC5, 0xCB };
const byte requestframe2[] = { 0x01, 0x03, 0x00, 0x04, 0x00, 0x02, 0x85, 0xCA };

SoftwareSerial Sump1_Serial_port(16, 17);    // rx,tx
SoftwareSerial A_block_Serial_port(26, 27);  // rx,tx

float sump_maximum_level = 1900;
float Ablock_maximum_level = 1300;
int16_t hydro_result_sump1, hydro_result_Ablock = 0;
float sump1_level_percentage, Ablock_level_percentage = 0.0;
int16_t sump1_level, Ablock_level;
int SM1S, OM1S = 0;
int Sump1_Relay_State, Ablock_Relay_State = 0;
const unsigned long checkverupdate = 15 * 60 * 1000;  // 5 minutes in milliseconds
static unsigned long lastUpdateCheck = 0;
int counter1 = 0;
byte values[7], values1[7];
String ID1_srv = "Sump1rawvalue:";
String ID2_sper = ",Sump1percentage:";
String ID3_ohtms = ",Ablockmotorstatus:";
String ID4_ohtrv = ",Ablockohtrawvalue:";
String ID5_ohtp = ",Ablockohtpercentage:";
String datasend = "";
int counter = 0;
const char *ssid = "Gowatr_R1";
const char *password = "Gowatr@brownstone";
const char *serverUrl = "https://www.gowatr.com/wms/pushdata/brownstone_data.php?project_id=browna&data=";

float valueFloat1;
float valueFloat2;
void setup() {
  Serial.begin(9600);
  Sump1_Serial_port.begin(9600);
  A_block_Serial_port.begin(9600);
  pinMode(SUMP1_MOTOR, OUTPUT);
  pinMode(A_block_OHT_MOTOR, OUTPUT);
  digitalWrite(SUMP1_MOTOR, LOW);
  digitalWrite(A_block_OHT_MOTOR, LOW);
  pinMode(LED_BUILTIN, OUTPUT);
  WiFi.begin(ssid, password);
  delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi is connected");
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void loop() {
  /*if (Serial.available() > 0) {
    String userInput = Serial.readStringUntil('\n');
    int commaIndex = userInput.indexOf(',');
    if(commaIndex != -1){
      String value1Str=userInput.substring(0, commaIndex);
      String value2Str=userInput.substring(commaIndex + 1);
    valueFloat1 = value1Str.toFloat();
    valueFloat2 = value2Str.toFloat();
    if (!isnan(valueFloat1) && !isnan(valueFloat2)) {
        Serial.print("You entered value1: ");
        Serial.println(valueFloat1);
        Serial.print("You entered value2: ");
        Serial.println(valueFloat2);
      }
    }
  }*/
  Serial.println("current version is " + String(CurrentVersion));
  counter++;
  delay(1000);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("**************connected to WiFi**************");
    digitalWrite(LED_BUILTIN, HIGH);
  } else if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi is not connected, attempt to reconnect");
    digitalWrite(LED_BUILTIN, LOW);
    WiFi.begin(ssid, password);
  }
  delay(3000);
  //OHT
  if (A_block_Serial_port.write(requestframe2, sizeof(requestframe2)) == 8) {
    A_block_Serial_port.flush();
    delay(1000);
    for (byte i = 0; i <= 9; i++) {
      values1[i] = A_block_Serial_port.read();
      delay(50);
    }
    Serial.print("Ablock Received BYTES: ");
    for (byte i = 0; i <= 9; i++) {
      Serial.print(values1[i], HEX);
      Serial.print(' ');
    }
    Serial.println(" ");
    if ((values1[0] == 1) && (values1[1] == 3)) {
      hydro_result_Ablock = (values1[3] << 8) | values1[4];
      Ablock_level = hydro_result_Ablock;
      Ablock_level_percentage = ((Ablock_level / Ablock_maximum_level) * 100);
    } else if ((values1[1] == 1) && (values1[2] == 3)) {
      hydro_result_Ablock = (values1[4] << 8) | values1[5];
      Ablock_level = hydro_result_Ablock;
      Ablock_level_percentage = ((Ablock_level / Ablock_maximum_level) * 100);
    } else if ((values1[2] == 1) && (values1[3] == 3)) {
      hydro_result_Ablock = (values1[5] << 8) | values1[6];
      Ablock_level = hydro_result_Ablock;
      Ablock_level_percentage = ((Ablock_level / Ablock_maximum_level) * 100);
    } else if ((values1[3] == 1) && (values1[4] == 3)) {
      hydro_result_Ablock = (values1[6] << 8) | values1[7];
      Ablock_level = hydro_result_Ablock;
      Ablock_level_percentage = ((Ablock_level / Ablock_maximum_level) * 100);
    } else if (counter1 >= 20) {
      hydro_result_Ablock = -1;
      Ablock_level = -1;
      Ablock_level_percentage = -1;
      counter1 = 0;
      Serial.println("counter1 " + String(counter1));
    } else if ((values1[0] != 1) && (values1[1] != 3)) {
      counter1++;
      Serial.println("counter1 " + String(counter1));
      Ablock_level = Ablock_level;
      Ablock_level_percentage = Ablock_level_percentage;
    }
  }
  delay(3000);

  //sump
  if (Sump1_Serial_port.write(requestframe, sizeof(requestframe)) == 8) {
    Sump1_Serial_port.flush();
    delay(1000);
    for (byte i = 0; i <= 7; i++) {
      values[i] = Sump1_Serial_port.read();
      delay(50);
    }
    Serial.print("Sump1 Received BYTES: ");
    for (byte i = 0; i <= 7; i++) {
      Serial.print(values[i], HEX);
      Serial.print(' ');
    }
    Serial.println(" ");
    hydro_result_sump1 = (values[3] << 8) | values[4];
    sump1_level = hydro_result_sump1;
    sump1_level_percentage = ((sump1_level / sump_maximum_level) * 100);
  }
  delay(100);

  if ((Ablock_level_percentage <= 80) && (sump1_level_percentage >= 40) && (hydro_result_Ablock != -1))
    Ablock_Relay_ON();
  else if ((Ablock_level_percentage >= 95) || (sump1_level_percentage <= 10) || (hydro_result_Ablock == -1))
    Ablock_Relay_OFF();

  datasend = (ID1_srv + String(hydro_result_sump1) + ID2_sper + String(sump1_level_percentage) + ID3_ohtms + String(Ablock_Relay_State) + ID4_ohtrv + String(hydro_result_Ablock) + ID5_ohtp + String(Ablock_level_percentage));

  Serial.println("Ablock OHT Sensor RAW Value:  " + String(hydro_result_Ablock));
  Serial.println("Sump1 Sensor RAW Value:       " + String(hydro_result_sump1));

  Serial.println("Ablock OHT tank waterlevel is " + String(Ablock_level_percentage) + "%");
  Serial.println("Sump1 tank waterlevel is      " + String(sump1_level_percentage) + "%");

  Serial.print("\t Ablock_OHT_MOTOR: ");
  Serial.println(Ablock_Relay_State);
  Serial.println(datasend);
  Serial.println("");
  delay(1000);

  if (counter >= 21) {
    counter = 0;
    Send_Data_WIFI(datasend);
    Serial.println("");
  }

  unsigned long currentMillis_update = millis();
  if (currentMillis_update - lastUpdateCheck >= checkverupdate) {
    lastUpdateCheck = currentMillis_update;
    check_for_updates();  // Check for updates every hour
  }
}

void Send_Data_WIFI(String datasend) {
  HTTPClient http;
  String url = String(serverUrl) + datasend;
  Serial.println(url);
  http.begin(url);

  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
  } else {
    Serial.println("HTTP Request failed");
  }
  http.end();
}

void Ablock_Relay_ON() {
  digitalWrite(A_block_OHT_MOTOR, HIGH);
  Ablock_Relay_State = 1;
}

void Ablock_Relay_OFF() {
  digitalWrite(A_block_OHT_MOTOR, LOW);
  Ablock_Relay_State = 0;
}