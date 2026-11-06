#define CurrentVersion 1.0
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <SensorModbusMaster.h>
#include <SoftwareSerial.h>
#include "OTAupdate.h"

#define LED_BUILTIN 2
#define SUMP2_MOTOR 32
#define C_block_OHT_MOTOR 33
WiFiMulti WiFiMulti;

const byte requestframe[] = { 0x01, 0x03, 0x00, 0x04, 0x00, 0x01, 0xC5, 0xCB };
const byte requestframe2[] = { 0x02, 0x03, 0x00, 0x04, 0x00, 0x01, 0xC5, 0xF8 };

SoftwareSerial Sump2_Serial_port(16, 17);    // rx,tx
SoftwareSerial C_block_Serial_port(26, 27);  // rx,tx

float sump_maximum_level = 1350;
float Cblock_maximum_level = 1300;
int16_t hydro_result_sump2, hydro_result_Cblock = 0;
float sump2_level_percentage, Cblock_level_percentage = 0.0;
int sump2_level, Cblock_level;
int SM1S, OM1S;
int Sump_Relay_State, Cblock_Relay_State = 0;
const unsigned long checkverupdate = 300000;  // 5 minutes in milliseconds
static unsigned long lastUpdateCheck = 0;

byte values[7], values1[7];
String ID1_srv = "Sump2rawvalue:";
String ID2_sper = ",Sump2percentage:";
String ID3_ohtms = ",Cblockmotorstatus:";
String ID4_ohtrv = ",Cblockohtrawvalue:";
String ID5_ohtp = ",Cblockohtpercentage:";
String datasend = "";
int counter, counter1, counter2 = 0;
const char *ssid = "Gowatr";
const char *password = "Gowatr@brownstone";
const char *serverUrl = "https://www.gowatr.com/wms/pushdata/brownstone_data.php?project_id=brownc&data=";

float valueFloat1;
float valueFloat2;
void setup() {
  Serial.begin(9600);
  Sump2_Serial_port.begin(9600);
  C_block_Serial_port.begin(9600);
  pinMode(SUMP2_MOTOR, OUTPUT);
  pinMode(C_block_OHT_MOTOR, OUTPUT);
  digitalWrite(SUMP2_MOTOR, LOW);
  digitalWrite(C_block_OHT_MOTOR, LOW);
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
  delay(30);
  Sump2_Serial_port.flush();
  delay(30);
  C_block_Serial_port.flush();
  delay(30);
  //OHT
  if (C_block_Serial_port.write(requestframe2, sizeof(requestframe2)) == 8) {
    delay(1000);
    for (byte i = 0; i <= 7; i++) {
      values1[i] = C_block_Serial_port.read();
      delay(50);
    }
    Serial.print("Cblock Received BYTES: ");
    for (byte i = 0; i <= 7; i++) {
      Serial.print(values1[i], HEX);
      Serial.print(' ');
    }
    Serial.println(" ");
    if ((values1[0] == 3) && (values1[1] == 2)) {

    } else if ((values1[0] == 2) && (values1[1] == 3)) {
      hydro_result_Cblock = (values1[3] << 8) | values1[4];
      Cblock_level = hydro_result_Cblock;
      Cblock_level_percentage = ((Cblock_level / Cblock_maximum_level) * 100);
      counter2 = 0;
    } else if ((values1[1] == 2) && (values1[2] == 3)) {
      hydro_result_Cblock = (values1[4] << 8) | values1[5];
      Cblock_level = hydro_result_Cblock;
      Cblock_level_percentage = ((Cblock_level / Cblock_maximum_level) * 100);
      counter2 = 0;
    }
    //else if ((values1[2] == 2) && (values1[3] == 3)) {
    //   hydro_result_Cblock = (values1[5] << 8) | values1[6];
    //   Cblock_level = hydro_result_Cblock;
    //   Cblock_level_percentage = ((Cblock_level / Cblock_maximum_level) * 100);
    //   counter2 = 0;
    // }
    else if (counter2 >= 20) {
      Cblock_level = -1;
      Cblock_level_percentage = -1;
      counter2 = 0;
      Serial.println("counter2 " + String(counter2));
    } else if ((values1[0] != 2) && (values1[1] != 3)) {
      counter2++;
      Serial.println("counter2 " + String(counter2));
      Cblock_level = sump2_level;
      Cblock_level_percentage = sump2_level_percentage;
    }
  }
  delay(1000);
  delay(30);
  C_block_Serial_port.flush();
  delay(30);

  //sump
  if (C_block_Serial_port.write(requestframe, sizeof(requestframe)) == 8) {
    delay(1000);
    for (byte i = 0; i <= 7; i++) {
      values[i] = C_block_Serial_port.read();
      delay(50);
    }
    Serial.print("Sump2 Received BYTES: ");
    for (byte i = 0; i <= 7; i++) {
      Serial.print(values[i], HEX);
      Serial.print(' ');
    }
    Serial.println(" ");

    if ((values[0] == 1) && (values[1] == 3)) {
      hydro_result_sump2 = (values[3] << 8) | values[4];
      sump2_level = hydro_result_sump2;  //userFloat;
      sump2_level_percentage = ((sump2_level / sump_maximum_level) * 100);
      counter1 = 0;
    } else if ((values[1] == 1) && (values[2] == 3)) {
      hydro_result_sump2 = (values[4] << 8) | values[5];
      sump2_level = hydro_result_sump2;  //userFloat;
      sump2_level_percentage = ((sump2_level / sump_maximum_level) * 100);
      counter1 = 0;
    } else if ((values[2] == 1) && (values[3] == 3)) {
      hydro_result_sump2 = (values[5] << 8) | values[6];
      sump2_level = hydro_result_sump2;  //userFloat;
      sump2_level_percentage = ((sump2_level / sump_maximum_level) * 100);
      counter1 = 0;
    } else if (counter1 >= 20) {
      sump2_level = -1;
      sump2_level_percentage = -1;
      counter1 = 0;
      Serial.println("counter1 " + String(counter1));
    } else if ((values[0] == 0xFF) && (values[1] == 0xFF)) {
      counter1++;
      Serial.println("counter1 " + String(counter1));
      sump2_level = sump2_level;
      sump2_level_percentage = sump2_level_percentage;
    }
    delay(100);
  }

  if ((Cblock_level_percentage <= 80) && (sump2_level_percentage >= 40) && (hydro_result_Cblock != -1) && (hydro_result_sump2 != -1))
    Cblock_Relay_ON();
  else if ((Cblock_level_percentage >= 96) || (sump2_level_percentage <= 10) || ((hydro_result_Cblock == -1) && (hydro_result_sump2 == -1)))
    Cblock_Relay_OFF();

  datasend = (ID1_srv + String(hydro_result_sump2) + ID2_sper + String(sump2_level_percentage) + ID3_ohtms + String(Cblock_Relay_State) + ID4_ohtrv + String(hydro_result_Cblock) + ID5_ohtp + String(Cblock_level_percentage));

  Serial.println("Cblock OHT Sensor RAW Value: " + String(hydro_result_Cblock));
  Serial.println("Sump2 Sensor RAW Value     : " + String(hydro_result_sump2));

  Serial.println("Cblock OHT tank waterlevel is " + String(Cblock_level_percentage) + "%");
  Serial.println("Sump2 tank waterlevel is      " + String(sump2_level_percentage) + "%");

  Serial.print("\t Cblock_OHT_MOTOR:");
  Serial.println(Cblock_Relay_State);
  Serial.println(datasend);
  Serial.println("");
  delay(1000);

  if (counter >= 23) {
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

void Cblock_Relay_ON() {
  digitalWrite(C_block_OHT_MOTOR, HIGH);
  Cblock_Relay_State = 1;
}

void Cblock_Relay_OFF() {
  digitalWrite(C_block_OHT_MOTOR, LOW);
  Cblock_Relay_State = 0;
}