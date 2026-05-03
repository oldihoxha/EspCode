#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// BLE UUIDs
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// PWM Configuration
#define PWM_PIN 18
#define PWM_CHANNEL 0
#define PWM_FREQUENCY 5000
#define PWM_RESOLUTION 8

BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Device connected!");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Device disconnected!");
    }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.print("Received value: ");
        for (int i = 0; i < rxValue.length(); i++)
          Serial.print(rxValue[i]);
        Serial.println();

        // Convert received value to PWM duty cycle (0-255)
        uint8_t pwmValue = 0;
        
        // Try to parse as integer
        if (rxValue.length() == 1) {
          // Single digit (0-9)
          pwmValue = (uint8_t)(rxValue[0] - '0');
          pwmValue = pwmValue * 25; // Scale 0-9 to 0-225
        } else {
          // Parse as full number (0-255)
          pwmValue = (uint8_t)std::stoi(rxValue);
        }

        // Constrain value to 0-255
        if (pwmValue > 255) pwmValue = 255;

        // Set PWM duty cycle
        ledcWrite(PWM_CHANNEL, pwmValue);
        Serial.print("PWM set to: ");
        Serial.println(pwmValue);
      }
    }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("Starting BLE work!");

  // Configure PWM on GPIO 18
  ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(PWM_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0); // Start with PWM off
  Serial.println("PWM initialized on GPIO 18");

  // Create the BLE Device
  BLEDevice::init("EspCode");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Set callback
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

void loop() {
  // notify changed value
  if (deviceConnected) {
    pCharacteristic->setValue("Connected");
    pCharacteristic->notify();
    delay(10);
  }
  
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  delay(2000);
}
