#include <Arduino.h>
#include "BLEDevice.h"
#include <Wire.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include "SPIFFS.h"
#include "control_logic.h"

// Replace with your network credentials
const char* ssid = "NETGEAR59";
const char* password = "moderntuba214";

// Set your Static IP address
IPAddress local_IP(192, 168, 1, 184);
// Set your Gateway IP address
IPAddress gateway(192, 168, 1, 1);

IPAddress subnet(255, 255, 0, 0);

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Async Events
AsyncEventSource events("/events");

// BLE Server name (the other device name running the server sketch)
#define bleServerName "DHT22_ESP32_1"

// Declare callback function
static void temperatureNotifyCallback(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify);

/* UUID's of the service, characteristic that we want to read*/
// BLE Service
static BLEUUID dhtServiceUUID("6e035bc0-676d-4ba1-a75b-494232949852");
//Temperature Fahrenheit Characteristic
static BLEUUID temperatureCharacteristicUUID("81d66e80-9793-4194-82e5-2a096d9d4ef0");

//Flags stating if should begin connecting and if the connection is up
static boolean doConnect = false;
static boolean connected = false;

//Address of the peripheral device. Address will be found during scanning...
static BLEAddress *pServerAddress;
 
//Characteristic that we want to read
static BLERemoteCharacteristic* temperatureCharacteristic;

//Activate notify
const uint8_t notificationOn[] = {0x1, 0x0};
const uint8_t notificationOff[] = {0x0, 0x0};

//Variable to store temperature
int temperatureInt = 70;

//Flags to check whether new temperature readings are available
boolean newTemperature = false;

//Connect to the BLE Server that has the name, Service, and Characteristics
bool connectToServer(BLEAddress pAddress) {
   BLEClient* pClient = BLEDevice::createClient();
 
  // Connect to the remove BLE Server.
  pClient->connect(pAddress);
  Serial.println(" - Connected to server");
 
  // Obtain a reference to the service we are after in the remote BLE server.
  BLERemoteService* pRemoteService = pClient->getService(dhtServiceUUID);
  if (pRemoteService == nullptr) {
    Serial.print("Failed to find our service UUID: ");
    Serial.println(dhtServiceUUID.toString().c_str());
    return (false);
  }
 
  // Obtain a reference to the characteristics in the service of the remote BLE server.
  temperatureCharacteristic = pRemoteService->getCharacteristic(temperatureCharacteristicUUID);

  if (temperatureCharacteristic == nullptr) {
    Serial.print("Failed to find our characteristic UUID");
    return false;
  }
  Serial.println(" - Found our characteristics");
 
  //Assign callback functions for the Characteristics
  temperatureCharacteristic->registerForNotify(temperatureNotifyCallback);
  return true;
}

//Callback function that gets called, when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == bleServerName) { //Check if the name of the advertiser matches
      advertisedDevice.getScan()->stop(); //Scan can be stopped, we found what we are looking for
      pServerAddress = new BLEAddress(advertisedDevice.getAddress()); //Address of advertiser is the one we need
      doConnect = true; //Set indicator, stating that we are ready to connect
      Serial.println("Device found. Connecting!");
    }
  }
};
 
// When the BLE Server sends a new temperature reading with the notify property
static void temperatureNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  //store temperature value
  static char* temperatureChar;
  temperatureChar = (char*)pData;
  static int tempInt;
  tempInt = String(temperatureChar).toInt();
  if (tempInt != 0) {
    temperatureInt = tempInt;
  }
  newTemperature = true;
}

// Process web page requests
String processor(const String& var){
  if (var =="CURRENT_TEMP") {
    return String(temperatureInt);
  }
  else if (var == "SET_TEMP"){
    return String(set_temp);
  }
  return String();
}

void setup() {
  // Start serial communication
  Serial.begin(115200);
  Serial.println("Starting Aapplication...");

  // Initialize control logic
  setupControl();

  // Initialize SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  
  // Configures static IP address and hostname
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Add Event handler for server
  server.addHandler(&events);

  // Print ESP32 Local IP Address
  Serial.println(WiFi.localIP());

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  // Route to load style.css file
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
  });
  // Route to increment control set point
  server.on("/set-up", HTTP_GET, [](AsyncWebServerRequest *request) {
    set_temp = set_temp + 1;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });
  // Route to increment control set point
  server.on("/set-down", HTTP_GET, [](AsyncWebServerRequest *request) {
    set_temp = set_temp - 1;
    request->send(SPIFFS, "/index.html", String(), false, processor);
  });

  // Start server
  server.begin();

  //Init BLE device
  Serial.println("Initiating BLE connection...");
  BLEDevice::init("");
  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(30);
}

void loop() {
  // Control loop rate
  int time_now = millis();

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect) {
    if (connectToServer(*pServerAddress)) {
      Serial.println("We are now connected to the BLE Server.");
      //Activate the Notify property of each Characteristic
      temperatureCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      connected = true;
    } else {
      Serial.println("We have failed to connect to the server; Restart your device to scan for nearby BLE server again.");
      // consider putting Init BLE device doe here
    }
    doConnect = false;
  }

  // If new temperature readings are available, publish to server
  if (newTemperature){
    events.send(String(temperatureInt).c_str(), "temp", time_now); // time_now is used as an int message IDs
    newTemperature = false;
    Serial.print("Temperature: ");
    Serial.print(temperatureInt);
    Serial.println(" F");
  }

  // Serial.println(temperatureInt);
  updateControl(temperatureInt);

  while (millis() < time_now + 1000) {}
}