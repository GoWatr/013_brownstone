#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <SensorModbusMaster.h>
#include <SoftwareSerial.h>
#define current_fw_version 1.0
#define LED_BUILTIN 2
#define SUMP1_MOTOR 32
#define A_block_OHT_MOTOR 33
WiFiMulti WiFiMulti;

const byte requestframe[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD};
const byte requestframe2[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD};

SoftwareSerial Sump1_Serial_port(16,17); // rx,tx
SoftwareSerial A_block_Serial_port(26,27); // rx,tx

float sump_maximum_level = 100;
float Ablock_maximum_level = 100;
int16_t hydro_result_sump1, hydro_result_Ablock = 0;
float sump1_level_percentage, Ablock_level_percentage = 0.0;
int16_t sump1_level, Ablock_level = 0;
float Sump1_cm , Ablock_cm = 0;
int SM1S = 0;
int OM1S = 0;
float valueFloat1;
float valueFloat2;

byte values[10], values1[10];
String ID1_scm = "sump1cm:";
String ID2_sper = ",sump1per:";
String ID3_sms = ",sump1mtrsts:";
String ID4_oms = ",ohtamtrsts:";
String ID5_ocm = ",aohtcm:";
String ID6_oper = ",aohtper:";
String datasend = "";
int counter = 0;
const char *ssid = "V123";
const char *password = "112345678";
const char *serverUrl = "https://www.gowatr.com/wms/pushdata/brownstone_data.php?project_id=browna&data=";
float userFloat;
void setup()
{
  Serial.begin(9600);
  Sump1_Serial_port.begin(9600);
  A_block_Serial_port.begin(9600); 
  pinMode(SUMP1_MOTOR, OUTPUT);
  pinMode(A_block_OHT_MOTOR, OUTPUT);
  digitalWrite(SUMP1_MOTOR, LOW);
  digitalWrite(A_block_OHT_MOTOR, LOW);
  WiFi.begin(ssid, password);
  delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi is connected");
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void loop()
{
  if (Serial.available() > 0) {
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
  }
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
  //sump
  memset(values, 0, sizeof(values));
  memset(values1, 0, sizeof(values1));
  if (Sump1_Serial_port.write(requestframe, sizeof(requestframe)) == 8) 
  {
    for (byte i = 0; i <= 10; i++)
    {
      values[i] = Sump1_Serial_port.read();
      delay(10);
    }
    Serial.print("Sump1 Received BYTES: ");
    for (byte i = 0; i <= 10; i++)
    {
      Serial.print(values[i], HEX);
      Serial.print(' ');
    }
    Serial.println(" ");
    hydro_result_sump1 = (values[5] << 8) | values[6];
    Serial.print("The SOLVED RESULT is ");
    Serial.println(hydro_result_sump1);
    sump1_level = valueFloat1;// hydro_result_sump1; //userFloat;
    sump1_level_percentage = ((sump1_level / sump_maximum_level) * 100);
    } else{
    Serial.println("sump1 data not received");
  }
  
  //OHT
  if (A_block_Serial_port.write(requestframe2, sizeof(requestframe2)) == 8)
  {
    for (byte i = 0; i <= 10; i++)
    {
      values1[i] = A_block_Serial_port.read();
      delay(10);
    }
    Serial.print("Ablock Received BYTES: ");
    for (byte i = 0; i <= 10; i++)
    {
      Serial.print(values1[i], HEX);
      Serial.print(' ');
    }
    Serial.println(" ");
    hydro_result_Ablock = (values1[4] << 8) | values1[5];
    Serial.print("The SOLVED RESULT is ");
    Serial.println(hydro_result_Ablock);
    Ablock_level =valueFloat2;// hydro_result_Ablock;
    Ablock_level_percentage = ((Ablock_level / Ablock_maximum_level) * 100);
    }else{
    Serial.println("Ablock data not received");
    }
    
  if ((sump1_level_percentage >= 0) && (sump1_level_percentage <= 45))
      controlMotor(1, true);
    else if ((hydro_result_sump1 == -1) || (sump1_level_percentage >= 80))
      controlMotor(1, false);

    if ((Ablock_level_percentage >= 0) && (Ablock_level_percentage <= 50) && (sump1_level_percentage >= 40))
      controlMotor(2, true);
    else if ((hydro_result_Ablock == -1) || (Ablock_level_percentage >= 95))
      controlMotor(2, false);

    datasend = (ID1_scm + String(hydro_result_sump1) + ID2_sper + String(sump1_level_percentage) + ID3_sms + String(SM1S) + ID4_oms + String(OM1S) + ID5_ocm + String(hydro_result_Ablock) + ID6_oper + String(Ablock_level_percentage));
    Serial.println("Sump1 Sensor RAW Value: " + String(hydro_result_sump1));
    Serial.println("Sump1 tank waterlevel is " + String(sump1_level_percentage) + "%");

    Serial.println("Ablock OHT Sensor RAW Value: " + String(hydro_result_Ablock));
    Serial.println("Ablock OHT tank waterlevel is " + String(Ablock_level_percentage) + "%");

    Serial.print("SUMP1_MOTOR:"); Serial.print(SM1S);
    Serial.print("\t Ablock_OHT_MOTOR:"); Serial.println(OM1S);
    Serial.println(datasend); Serial.println("");
    delay(1000);
    
  if (counter >= 7){
    counter = 0;
    Send_Data_WIFI(datasend);
    Serial.println("");
  }
  Sump1_Serial_port.flush();
  A_block_Serial_port.flush();
  delay(100);
}

void controlMotor(int motor, bool state)
{
  switch (motor)
  {
    case 1: // SUMP1_MOTOR
    digitalWrite(SUMP1_MOTOR, state ? HIGH : LOW);
    SM1S = state ? 1 : 0;
    break;
        
    case 2: // A_block_OHT_MOTOR
    digitalWrite(A_block_OHT_MOTOR, state ? HIGH : LOW);
    OM1S = state ? 1 : 0;
    break;
    default:
    break;
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
