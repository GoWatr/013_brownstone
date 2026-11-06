#define CurrentVersion 4.0
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <SoftwareSerial.h>
#include <esp_task_wdt.h>
#include <WiFi.h>

#define LED_BUILTIN 2
#define SUMP1_MOTOR 32
#define A_block_OHT_MOTOR 33
#define WIFI_TIMEOUT_MS 10000   // 10 seconds timeout for WiFi

#define WDT_TIMEOUT 25  // Timeout in seconds
esp_err_t ESP32_ERROR;

const byte requestframe[] = { 0x01, 0x03, 0x00, 0x04, 0x00, 0x01, 0xC5, 0xCB };
const byte requestframe2[] = { 0x01, 0x03, 0x00, 0x04, 0x00, 0x02, 0x85, 0xCA };

SoftwareSerial Sump1_Serial_port(16, 17);    // rx,tx
SoftwareSerial A_block_Serial_port(26, 27);  // rx,tx

float sump_maximum_level = 1900;
float Ablock_maximum_level = 1300;
int16_t hydro_result_sump1, hydro_result_Ablock = 0;
float sump1_level_percentage = 0.0, Ablock_level_percentage = 0.0;
int Ablock_Relay_State = 0;
unsigned long lastDataSend = 0;

byte values[7], values1[7];
String datasend = "";
const char *ssid = "Gowatr_R1";
const char *password = "Gowatr@brownstone";
const char *serverUrl = "https://www.ggowatr.com/wms/pushdata/brownstone_data.php?project_id=browna&data=";

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

  connectWiFi();
  
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
}

void loop() {
  esp_task_wdt_reset();

  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
  }

  readSensorData();

  controlMotors();

  if (millis() - lastDataSend >= 60000 * 2) {
    lastDataSend = millis();
    // esp_task_wdt_reset();
    sendDataToServer();
  }

  delay(5000);
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  unsigned long startAttemptTime = millis();
  
  if (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS) {
    delay(100);
    esp_task_wdt_reset();
  }
  

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    Serial.println("WiFi connection failed. Will retry in loop.");
  }
}

void reconnectWiFi() {
  WiFi.disconnect();
  connectWiFi();
}

void readSensorData() {
  // Read Ablock OHT sensor data
  if (A_block_Serial_port.write(requestframe2, sizeof(requestframe2)) == 8) {
    readSerialData(A_block_Serial_port, values1, 9);
    hydro_result_Ablock = (values1[3] << 8) | values1[4];
    Ablock_level_percentage = (hydro_result_Ablock / Ablock_maximum_level) * 100;
  } else {
    hydro_result_Ablock = -1;
    Ablock_level_percentage = -1;
  }

  // Read Sump1 sensor data
  if (Sump1_Serial_port.write(requestframe, sizeof(requestframe)) == 8) {
    readSerialData(Sump1_Serial_port, values, 7);
    hydro_result_sump1 = (values[3] << 8) | values[4];
    sump1_level_percentage = (hydro_result_sump1 / sump_maximum_level) * 100;
  } else {
    hydro_result_sump1 = -1;
    sump1_level_percentage = -1;
  }
}

void readSerialData(SoftwareSerial &serialPort, byte *buffer, int length) {
  int i = 0;
  unsigned long startTime = millis();
  
  while (i < length && millis() - startTime < 2000) {
    if (serialPort.available()) {
      buffer[i++] = serialPort.read();
    }
  }
  
  if (i < length) {
    Serial.println("Serial data read timeout");
    // esp_task_wdt_reset();
  }
}

void controlMotors() {
  if ((Ablock_level_percentage <= 80) && (sump1_level_percentage >= 40) && (hydro_result_Ablock != -1)) {
    Ablock_Relay_ON();
  } else if ((Ablock_level_percentage >= 95) || (sump1_level_percentage <= 10) || (hydro_result_Ablock == -1)) {
    Ablock_Relay_OFF();
  }
}

void sendDataToServer() {
  HTTPClient http;
  datasend = "Sump1rawvalue:" + String(hydro_result_sump1) + ",Sump1percentage:" + String(sump1_level_percentage) +
             ",Ablockmotorstatus:" + String(Ablock_Relay_State) + ",Ablockohtrawvalue:" + String(hydro_result_Ablock) +
             ",Ablockohtpercentage:" + String(Ablock_level_percentage);

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
