#include <WiFi.h>
#include <esp_wifi.h>

void readMacAddress() {
  uint8_t macAddress[6];
  esp_err_t response = esp_wifi_get_mac(WIFI_IF_STA, macAddress);
  if (response == ESP_OK) {
    Serial.printf("MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  macAddress[0], macAddress[1], macAddress[2],
                  macAddress[3], macAddress[4], macAddress[5]);
  } else {
    Serial.println("MAC address not read");
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.STA.begin();

  Serial.println("Attempting to read the MAC address");
  readMacAddress();
}

void loop() {

}