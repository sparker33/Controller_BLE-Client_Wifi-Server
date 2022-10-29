#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

#define DHTPIN 26  // DHT22 data pin connection definition
#define DHTTYPE DHT22 // DHT22 is sensor type
#define bleServerName "DHT22_ESP32_1"
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "6e035bc0-676d-4ba1-a75b-494232949852"

// Initialize DHT library
DHT dht = DHT(DHTPIN, DHTTYPE);

// Data Vars
float temp;
float tempF;
// Timer vars
unsigned long lastTime = 0;
unsigned long timerDelay = 30000;
bool deviceConnected = false;

// Set up Temperature BLE package Characteristic and Descriptor
BLECharacteristic dhtTemperatureFahrenheitCharacteristics("81d66e80-9793-4194-82e5-2a096d9d4ef0", BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor dhtTemperatureFahrenheitDescriptor(BLEUUID((uint16_t)0x2902));



//Setup callbacks onConnect and onDisconnect
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  };
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

void setup() {
  // Start serial communication
  Serial.begin(115200);

  // Init the DHT sensor
  dht.begin();

  // Create the BLE Device
  BLEDevice::init(bleServerName);

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *dhtService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristics and Create a BLE Descriptor
  dhtService->addCharacteristic(&dhtTemperatureFahrenheitCharacteristics);
  dhtTemperatureFahrenheitDescriptor.setValue("DHT temperature Fahrenheit");
  dhtTemperatureFahrenheitCharacteristics.addDescriptor(&dhtTemperatureFahrenheitDescriptor);

  // Start the service
  dhtService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  if (deviceConnected) {
    if ((millis() - lastTime) > timerDelay) {
      // Read temperature as Celsius (the default)
      temp = dht.readTemperature();
      // Fahrenheit
      tempF = 1.8*temp +32;
  
      //Notify temperature reading from BME sensor
      static char temperatureFTemp[6];
      dtostrf(tempF, 6, 2, temperatureFTemp);
      //Set temperature Characteristic value and notify connected client
      dhtTemperatureFahrenheitCharacteristics.setValue(temperatureFTemp);
      dhtTemperatureFahrenheitCharacteristics.notify();
      Serial.print("Temperature Fahrenheit: ");
      Serial.print(tempF);
      Serial.print(" F");
      
      lastTime = millis();
    }
  }
}