#include "LGFX.hpp"
#include "NimBLEUUID.h"
#include <Arduino.h>
#include <NimBLEDevice.h>

LGFX display;

// UUIDs
#define UUID_SERVICE_WEIGHT "fff0"
#define UUID_CHAR_WEIGHT "fff4"

#define UUID_SERVICE_PRESSURE "0fff"
#define UUID_CHAR_PRESSURE "ff02"

NimBLEAdvertisedDevice *foundWeight = nullptr;
NimBLEAdvertisedDevice *foundPressure = nullptr;

NimBLEClient *clientWeight = nullptr;
NimBLEClient *clientPressure = nullptr;

NimBLERemoteCharacteristic *charWeight = nullptr;
NimBLERemoteCharacteristic *charPressure = nullptr;

float lastWeight = 0.0;
float lastFlowRate = 0.0;
float lastPressure = 0.0;
int lastBattery = 0;

void showData() {
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.printf("Weight: %.2f g\n", lastWeight);
  display.printf("Flow:   %.2f g/s\n", lastFlowRate);
  display.printf("Press:  %.2f bar\n", lastPressure);
  display.printf("Battery: %d%%\n", lastBattery);
}

class MyAdvertisedDeviceCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *device) override {
    device->getServiceUUID();
    if (device->isAdvertisingService(NimBLEUUID(UUID_SERVICE_WEIGHT)) &&
        !foundWeight) {
      foundWeight = new NimBLEAdvertisedDevice(*device);
      Serial.println("Found weight device");
    }
    if (device->isAdvertisingService(NimBLEUUID(UUID_SERVICE_PRESSURE)) &&
        !foundPressure) {
      foundPressure = new NimBLEAdvertisedDevice(*device);
      Serial.println("Found pressure device");
    }
    // if (device->haveServiceUUID()) {
    //   Serial.println("device discovered");
    //   NimBLEUUID uuid = device->getServiceUUID();
    //
    //   if (uuid.equals(NimBLEUUID(UUID_SERVICE_WEIGHT)) && !foundWeight) {
    //     foundWeight = new NimBLEAdvertisedDevice(*device);
    //     Serial.println("Found weight device");
    //   }
    //
    //   if (uuid.equals(NimBLEUUID(UUID_SERVICE_PRESSURE)) && !foundPressure) {
    //     foundPressure = new NimBLEAdvertisedDevice(*device);
    //     Serial.println("Found pressure device");
    //   }
    // }
  }

  void onScanEnd(const NimBLEScanResults &results, int reason) override {
    Serial.printf("scan end: %d\n", reason);
    for (int i = 0; i < results.getCount(); i++) {
      const NimBLEAdvertisedDevice *device = results.getDevice(i);
      Serial.printf("device adv count: %d\n", device->getServiceDataCount());
      Serial.printf("device has svc uuid %d\n", device->haveServiceUUID());

      if (device->haveServiceUUID()) {
        Serial.println(device->toString().c_str());
      }

      if (device->isAdvertisingService(NimBLEUUID(UUID_SERVICE_WEIGHT)) &&
          !foundWeight) {
        foundWeight = new NimBLEAdvertisedDevice(*device);
        Serial.println("Found weight device");
      }
      if (device->isAdvertisingService(NimBLEUUID(UUID_SERVICE_PRESSURE)) &&
          !foundPressure) {
        foundPressure = new NimBLEAdvertisedDevice(*device);
        Serial.println("Found pressure device");
      }
    }
  }
};

bool connectTo(NimBLEAdvertisedDevice *adv, const char *svcUUID,
               const char *charUUID, NimBLEClient *&client,
               NimBLERemoteCharacteristic *&characteristic) {
  if (!adv)
    return false;

  client = NimBLEDevice::createClient();
  if (!client->connect(adv)) {
    Serial.println("Connection failed");
    return false;
  }

  NimBLERemoteService *service = client->getService(svcUUID);
  if (!service) {
    Serial.println("Service not found");
    client->disconnect();
    return false;
  }

  characteristic = service->getCharacteristic(charUUID);
  if (!characteristic || !characteristic->canRead()) {
    Serial.println("Characteristic not found or unreadable");
    client->disconnect();
    return false;
  }

  return true;
}

void readWeight() {
  if (!charWeight)
    return;

  auto data = charWeight->readValue();
  if (data.length() < 20)
    return;

  uint32_t weight_raw =
      (uint32_t)data[7] << 16 | (uint32_t)data[8] << 8 | (uint32_t)data[9];
  uint16_t flow_raw = (uint16_t)data[11] << 8 | (uint16_t)data[12];
  uint8_t battery = data[13];

  lastWeight = weight_raw / 100.0f;
  lastFlowRate = flow_raw / 100.0f;
  lastBattery = battery;
}

void readPressure() {
  if (!charPressure)
    return;

  auto data = charPressure->readValue();
  if (data.length() < 6)
    return;

  uint16_t pressure_raw = (uint16_t)data[2] << 8 | (uint16_t)data[3];
  lastPressure = pressure_raw / 100.0f;
}

void setup() {
  Serial.begin(115200);
  display.init();
  display.setRotation(3);
  display.fillScreen(TFT_BLACK);
  display.setTextFont(&fonts::Font2);

  NimBLEDevice::init("");
  // NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  Serial.println("Starting BLE scan...");

  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(new MyAdvertisedDeviceCallbacks());
  scan->setActiveScan(true);
  scan->start(13000, false); // scan for 5 seconds (non-blocking)

  unsigned long start = millis();
  while ((!foundWeight || !foundPressure) && millis() - start < 15000) {
    delay(100); // wait for both devices to be discovered
  }

  scan->stop();

  if (foundWeight) {
    connectTo(foundWeight, UUID_SERVICE_WEIGHT, UUID_CHAR_WEIGHT, clientWeight,
              charWeight);
  }
  if (foundPressure) {
    connectTo(foundPressure, UUID_SERVICE_PRESSURE, UUID_CHAR_PRESSURE,
              clientPressure, charPressure);
  }
}

void loop() {
  readWeight();
  readPressure();
  showData();
  delay(1000);
}
