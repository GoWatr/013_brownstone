#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoJson.h>

String VER = "";
WiFiClient wifiClient1;

void update_started() {
  Serial.println("CALLBACK: HTTP update process started");
}

void update_finished() {
  Serial.println("CALLBACK: HTTP update process finished");
}

void update_progress(int cur, int total) {
  Serial.printf("CALLBACK: HTTP update process at %d of %d bytes...\n", cur, total);
}

void update_error(int err) {
  Serial.printf("CALLBACK: HTTP update fatal error code %d\n", err);
}

void otaUpdate(String updateURL) {
  WiFiClient client1;
  httpUpdate.onStart(update_started);
  httpUpdate.onEnd(update_finished);
  httpUpdate.onProgress(update_progress);
  httpUpdate.onError(update_error);

  t_httpUpdate_return ret = httpUpdate.update(client1, updateURL);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      ESP.restart();
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }
}

void check_for_updates() {
  Serial.print("Current Firmware - ");
  Serial.print(CurrentVersion);
  Serial.println(" Checking for Updates ...");
  VER = "Current Firmware: " + String(CurrentVersion);

  HTTPClient http;
  http.begin(wifiClient1,"http://gowatr.com/wms/ota-gowatr/ota_brownstone/A_block/firmware_Ablock.json");

  int httpCode = http.GET();
  String payload = "";
  if (httpCode > 0) {
    payload = http.getString();
    Serial.println("Received payload: " + payload); // Print the received payload
  } else {
    Serial.printf("HTTP GET failed with error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return;
  }
  http.end(); // Close connection

  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("Deserialization failed: ");
    Serial.println(error.c_str()); // Print the error
    ESP.restart();
  } else {
    float newVersion = doc["ver"];
    if (newVersion > CurrentVersion) {
      Serial.println("New Firmware Available. Downloading new Firmware ... ");
      otaUpdate(doc["link"].as<String>());
    } else {
      Serial.println("Already Running Latest Firmware.");
    }
  }
}
