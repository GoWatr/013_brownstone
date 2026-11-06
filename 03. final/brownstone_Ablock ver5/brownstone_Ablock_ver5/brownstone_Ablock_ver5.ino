#define CurrentVersion 6.0
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <SoftwareSerial.h>
#include <esp_task_wdt.h>

#define LED_BUILTIN 2
#define SUMP1_MOTOR 32
#define A_block_OHT_MOTOR 33
#define WDT_TIMEOUT 30  // Timeout in seconds
esp_err_t ESP32_ERROR;

const byte requestframe[] = { 0x01, 0x03, 0x00, 0x04, 0x00, 0x01, 0xC5, 0xCB };
const byte requestframe2[] = { 0x01, 0x03, 0x00, 0x04, 0x00, 0x02, 0x85, 0xCA };

SoftwareSerial Sump1_Serial_port(16, 17);    // rx,tx
SoftwareSerial A_block_Serial_port(26, 27);  // rx,tx

float sump_maximum_level = 1900;
float Ablock_maximum_level = 1300;
int16_t hydro_result_sump1, hydro_result_Ablock = 0;
float sump1_level_percentage, Ablock_level_percentage = 0.0;
int16_t sump1_level, Ablock_level;
int Ablock_Relay_State = 0;
unsigned long lastDataSend = 0;

byte values[7], values1[7];
String datasend = "";
const char *ssid = "Gowatr_R1";
const char *password = "Gowatr@brownstone";
const char *serverUrl = "https://www.gowatr.com/wms/pushdata/brownstone_data.php?project_id=browna&data=";

void setup() {
  Serial.begin(115200);
  delay(100);

  Sump1_Serial_port.begin(9600);
  A_block_Serial_port.begin(9600);
  pinMode(SUMP1_MOTOR, OUTPUT);
  pinMode(A_block_OHT_MOTOR, OUTPUT);
  digitalWrite(SUMP1_MOTOR, LOW);
  digitalWrite(A_block_OHT_MOTOR, LOW);
  pinMode(LED_BUILTIN, OUTPUT);

  WiFi.begin(ssid, password);
  delay(5000);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi is connected");
    digitalWrite(LED_BUILTIN, HIGH);
  }

  Serial.println("Configuring WDT...");
  Serial.print("Watchdog Timeout (in seconds) set to : ");
  Serial.println(WDT_TIMEOUT);
  esp_task_wdt_deinit();
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT * 1000,                 // Convertin ms
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,  // Bitmask of all cores, https://github.com/espressif/esp-idf/blob/v5.2.2/examples/system/task_watchdog/main/task_watchdog_example_main.c
    .trigger_panic = true                             // Enable panic to restart ESP32
  };
  ESP32_ERROR = esp_task_wdt_init(&wdt_config);
  Serial.println("Last Reset : " + String(esp_err_to_name(ESP32_ERROR)));
  esp_task_wdt_add(NULL);
  Serial.printf("Total Heap Size: %d bytes\n", ESP.getHeapSize());
  Serial.printf("Free Heap Size: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
  esp_task_wdt_reset();
  Serial.println("current version is " + String(CurrentVersion));
  unsigned long currentMillis_update = millis();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("**************connected to WiFi**************");
    digitalWrite(LED_BUILTIN, HIGH);
  } else if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi is not connected, attempt to reconnect");
    digitalWrite(LED_BUILTIN, LOW);
    WiFi.reconnect();
    delay(5000);
  }
  delay(5000);

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
    hydro_result_Ablock = (values1[3] << 8) | values1[4];
    Ablock_level = hydro_result_Ablock;
    Ablock_level_percentage = ((Ablock_level / Ablock_maximum_level) * 100);
  }
  delay(5000);
  esp_task_wdt_reset();
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
  delay(5000);

  if ((Ablock_level_percentage <= 80) && (sump1_level_percentage >= 40) && (hydro_result_Ablock != -1))
    Ablock_Relay_ON();
  else if ((Ablock_level_percentage >= 95) || (sump1_level_percentage <= 10) || (hydro_result_Ablock == -1))
    Ablock_Relay_OFF();

  Serial.println("Ablock OHT Sensor RAW Value:  " + String(hydro_result_Ablock));
  Serial.println("Sump1 Sensor RAW Value:       " + String(hydro_result_sump1));
  Serial.println("Ablock OHT tank waterlevel is " + String(Ablock_level_percentage) + "%");
  Serial.println("Sump1 tank waterlevel is      " + String(sump1_level_percentage) + "%");
  Serial.print("\t Ablock_OHT_MOTOR: ");
  Serial.println(Ablock_Relay_State);
  Serial.printf("Total Heap Size: %d bytes\n", ESP.getHeapSize());
  Serial.printf("Free Heap Size: %d bytes\n", ESP.getFreeHeap());
  Serial.println("");

  if (currentMillis_update - lastDataSend >= 60000 * 2) {
    lastDataSend = currentMillis_update;
    sendDataToServer();
  }
}

void sendDataToServer() {
  esp_task_wdt_reset();
  Serial.println("Sending data to server...");

  HTTPClient http;
  datasend = "Sump1rawvalue:" + String(hydro_result_sump1) + ",Sump1percentage:" + String(sump1_level_percentage) + ",Ablockmotorstatus:" + String(Ablock_Relay_State) + ",Ablockohtrawvalue:" + String(hydro_result_Ablock) + ",Ablockohtpercentage:" + String(Ablock_level_percentage);
  Serial.println(datasend);

  String url = String(serverUrl) + datasend;
  Serial.println(url);

  http.begin(url);
  unsigned long start = millis();
  int httpResponseCode = http.GET();

  unsigned long duration = millis() - start;
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    int dataSize = datasend.length();
    float speedKBps = (dataSize / 1024.0) / (duration / 1000.0);  // Speed in KB/s
    Serial.printf("Upload Speed: %.2f KB/s\n", speedKBps);
  } else {
    Serial.println("HTTP Request failed");
  }
  http.end();
  Serial.println("");
  esp_task_wdt_reset();
}


// void sendDataToServer() {
//   Serial.println("Sending data to server...");

//   HTTPClient http;
//   datasend = "Sump1rawvalue:" + String(hydro_result_sump1) + ",Sump1percentage:" + String(sump1_level_percentage) + ",Ablockmotorstatus:" + String(Ablock_Relay_State) + ",Ablockohtrawvalue:" + String(hydro_result_Ablock) + ",Ablockohtpercentage:" + String(Ablock_level_percentage);
//   Serial.println(datasend);

//   String url = String(serverUrl) + datasend;
//   Serial.println(url);

//   http.begin(url);
//   int httpResponseCode = http.GET();

//   if (httpResponseCode > 0) {
//     Serial.print("HTTP Response code: ");
//     Serial.println(httpResponseCode);
//   } else {
//     Serial.println("HTTP Request failed");
//   }
//   http.end();
//   Serial.println("");
// }

void Ablock_Relay_ON() {
  digitalWrite(A_block_OHT_MOTOR, HIGH);
  Ablock_Relay_State = 1;
}

void Ablock_Relay_OFF() {
  digitalWrite(A_block_OHT_MOTOR, LOW);
  Ablock_Relay_State = 0;
}