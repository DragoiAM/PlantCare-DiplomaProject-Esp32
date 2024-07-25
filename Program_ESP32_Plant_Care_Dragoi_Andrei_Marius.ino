#include <WiFi.h> 
#include <DHT.h> 
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <WebServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>


//Date de conectare la ESP
const char* ssid = "PlantCareDevice";
const char* password = "123456789";
Preferences preferences;

WebServer server(80);

// Static IP address
IPAddress staticIP(192, 168, 1, 106); 
IPAddress gateway(192, 168, 1, 1);    
IPAddress subnet(255, 255, 255, 0); 

String userId, plantId, lightId, airSensorsId, soilSensorsId,pumpId;

#define API_KEY "AIzaSyDo2wKuiFhAozNRowLtDrUJbVsGTAv0JKU"
#define FIREBASE_PROJECT_ID "plantcarelicenta"


#define LED_PIN 2 // Define the GPIO pin where the LED is connected
#define PHOTO_RESISTOR_PIN 34 // Define the analog pin for the photoresistor
#define RELAY_PIN 5 // Define the GPIO pin for the relay
#define DHTPIN 15 // Define the GPIO pin for the DHT11 sensor
#define DS18B20_PIN 4 // Define the GPIO pin for the DS18B20 sensor

#define SOIL_MOISTURE_PIN 32 // Define the analog pin for the soil moisture sensor

#define SOIL_MOISTURE_PIN 32 // Define the analog pin for the soil moisture sensor

// Adjusted calibration values for the soil moisture sensor based on your observations.
#define SOIL_MOISTURE_MIN 4095 // Previously wet soil (100% moisture), now indicates the air value
#define SOIL_MOISTURE_MAX 0    // Previously dry soil (air), now indicates the water value

// Define the type of sensor used
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

OneWire oneWire(DS18B20_PIN); // Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature sensors(&oneWire); // Pass our oneWire reference to Dallas Temperature sensor 

#define RESET_BUTTON_PIN 13 // Change this to your chosen GPIO pin

// Initialize Firebase Auth and Config
FirebaseAuth auth;
FirebaseConfig config;
FirebaseData fbdo;
String ssidWiFi ="";
String spasswordWiFi ="";
bool wifiConnected = false;
bool dataGet = false;
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT); // Initialize the GPIO pin as an output
  pinMode(PHOTO_RESISTOR_PIN, INPUT); // Initialize the photoresistor pin as an input
  pinMode(RELAY_PIN, OUTPUT); // Initialize the GPIO pin for the relay as an output
  digitalWrite(RELAY_PIN, LOW); // Initially, turn the relay off (assuming low level means off)
  pinMode(SOIL_MOISTURE_PIN, INPUT); // Initialize the soil moisture sensor pin as an input
  dht.begin(); // Initialize the DHT11 sensor

  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP); // Use internal pull-up resistor
  
  sensors.begin(); // Start up the library for the DS18B20 sensor
  
   // Simple debounce
    if (digitalRead(RESET_BUTTON_PIN) == LOW) { // Assuming the button is active low
    delay(100); // Simple debounce
    if (digitalRead(RESET_BUTTON_PIN) == LOW) { // Check again to confirm
      Serial.println("Reset button pressed, clearing stored data...");

      preferences.begin("PlantCare", false); // Open Preferences with my-app namespace in RW mode
      preferences.clear(); // Clears all keys under the opened Preferences
      preferences.end();

      Serial.println("Data cleared, performing a software reset.");
      ESP.restart(); // Restart the ESP
    }
  }
    else{
      preferences.begin("PlantCare", true); 
    }
  
  if (preferences.getString("ssid") != "") { // Check if there's stored data
    ssidWiFi = preferences.getString("ssid");
    spasswordWiFi = preferences.getString("password");
    userId =   preferences.getString("userId", userId);
    plantId=  preferences.getString("plantId", plantId);
    lightId=preferences.getString("lightId", lightId);
    airSensorsId=   preferences.getString("airSensorsId", airSensorsId);
    soilSensorsId=   preferences.getString("soilSensorsId", soilSensorsId);
    pumpId=   preferences.getString("pumpId", pumpId);
    Serial.println("User ID: " + userId);
        Serial.println("Plant ID: " + plantId);
        Serial.println("Light ID: " + lightId);
        Serial.println("Air Sensors ID: " + airSensorsId);
        Serial.println("Soil Sensors ID: " + soilSensorsId);
        Serial.println("Pump ID: " + pumpId);
     connectToWiFi(ssidWiFi,spasswordWiFi);
     config.api_key = API_KEY;
  
        Firebase.begin(&config, &auth);

        // Authenticate anonymously
        if (Firebase.signUp(&config, &auth, "", "")) { // Empty strings for anonymous auth
          Serial.println("Firebase Anonymous Auth succeeded");
        } else {
          Serial.println("Firebase Anonymous Auth failed");
          Serial.println("Reason: " + String(fbdo.errorReason()));
        }
        Firebase.reconnectWiFi(true);
        dataGet = true;
  }else{
    digitalWrite(LED_PIN, HIGH); // Turn the LED on
  
  // Set ESP32 as an access point with a static IP
  WiFi.softAP(ssid, password);
  WiFi.softAPConfig(staticIP, gateway, subnet);
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.on("/setwifi", HTTP_POST, []() {
    String new_ssid = server.arg("ssid");
    String new_password = server.arg("password");
    WiFi.begin(new_ssid.c_str(), new_password.c_str());

    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
      delay(500);
      Serial.print(".");
    }
    preferences.begin("PlantCare", false); 
    preferences.putString("ssid", new_ssid);
    preferences.putString("password", new_password);
    preferences.end();
    if(WiFi.status() == WL_CONNECTED) {
      wifiConnected = true;
      server.send(200, "text/plain", "Connected to WiFi");
    } else {
      server.send(200, "text/plain", "Failed to connect");
    }
  });

  // Route to receive and display text
  server.on("/saveData", HTTP_POST, []() {
        // Parse and store the data from the POST request
        userId = server.arg("userId");
        plantId = server.arg("plantId");
        lightId = server.arg("lightId");
        airSensorsId = server.arg("airSensorsId");
        soilSensorsId = server.arg("soilSensorsId");
        pumpId = server.arg("pumpId");

        // Print the received data to Serial
        Serial.println("Received data:");
        Serial.println("User ID: " + userId);
        Serial.println("Plant ID: " + plantId);
        Serial.println("Light ID: " + lightId);
        Serial.println("Air Sensors ID: " + airSensorsId);
        Serial.println("Soil Sensors ID: " + soilSensorsId);
        Serial.println("Pump ID: " + pumpId);

        preferences.begin("PlantCare", false); 
        preferences.putString("userId", userId);
        preferences.putString("plantId", plantId);
        preferences.putString("lightId", lightId);
        preferences.putString("airSensorsId", airSensorsId);
        preferences.putString("soilSensorsId", soilSensorsId);
        preferences.putString("pumpId", pumpId);
        preferences.end();
        // Send a response back to the client
        server.send(200, "text/plain", "Data received successfully");
        config.api_key = API_KEY;
  
        Firebase.begin(&config, &auth);

        // Authenticate anonymously
        if (Firebase.signUp(&config, &auth, "", "")) { // Empty strings for anonymous auth
          Serial.println("Firebase Anonymous Auth succeeded");
        } else {
          Serial.println("Firebase Anonymous Auth failed");
          Serial.println("Reason: " + String(fbdo.errorReason()));
        }
        Firebase.reconnectWiFi(true);
        dataGet = true;
        
    });

  server.begin();
  }
  preferences.end();
}

void loop() {
  server.handleClient();
  delay(2000);
  if(dataGet && wifiConnected)
  {
    server.close(); // Close the server
    WiFi.softAPdisconnect(true); // Disconnect from the AP mode
    blinkLED(LED_PIN, 5000);// Blink LED with a 5-second interval
    int photoResistorValue = readPhotoResistorValue(); // Store the returned photoresistor value in a variable
  Serial.print("Photoresistor Value: ");
  Serial.println(photoResistorValue); // Log the value to the console
   // Store the photoResistorValue in Firestore
  storeLightSensorValue(userId, plantId, lightId, photoResistorValue);

  float humidity = dht.readHumidity(); // Read humidity
  float temperature = dht.readTemperature(); // Read temperature in Celsius
  
  // Check if any reads failed and exit early (to try again).
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.print("%  Temperature: ");
    Serial.print(temperature);
    Serial.println("°C ");
    storeDhtSensorValue(userId, plantId, airSensorsId, humidity, temperature);
  }
  int soilMoisturePercentage = readSoilMoistureValue();
  Serial.print("Soil Moisture: ");
  Serial.print(soilMoisturePercentage);
  Serial.println("%");
  
  sensors.requestTemperatures(); // Send the command to get temperatures
  float ds18b20Temperature = sensors.getTempCByIndex(0); // Read temperature in Celsius
  Serial.print("DS18B20 Temperature: ");
  Serial.print(ds18b20Temperature);
  Serial.println("°C");
  storeSoilSensorData(userId, plantId, soilSensorsId, soilMoisturePercentage, ds18b20Temperature);


  Serial.println(getMinWaterPercentage(userId, plantId, soilSensorsId));
  if(getMinWaterPercentage(userId, plantId, soilSensorsId)>soilMoisturePercentage)
  {
    turnRelayOn(userId,plantId,pumpId);
  }
  else {
  turnRelayOff(userId,plantId,pumpId);
  }
  
  if (digitalRead(RESET_BUTTON_PIN) == LOW) { // Assuming the button is active low
    delay(100); // Simple debounce
    if (digitalRead(RESET_BUTTON_PIN) == LOW) { // Check again to confirm
      Serial.println("Reset button pressed, clearing stored data...");

      preferences.begin("PlantCare", false); // Open Preferences with my-app namespace in RW mode
      preferences.clear(); // Clears all keys under the opened Preferences
      preferences.end();

      Serial.println("Data cleared, performing a software reset.");
      ESP.restart(); // Restart the ESP
    }
  }
  } 
}
void blinkLED(int pin, int delayTime) {
  digitalWrite(pin, HIGH); // Turn the LED on
  delay(delayTime / 10); // Wait for a fraction of delayTime
  digitalWrite(pin, LOW); // Turn the LED off
  delay(delayTime); // Wait for delayTime
}
// Modified function to read from the photoresistor and return the value
int readPhotoResistorValue() {
  int photoResistorMinValue = 0;    // Valoarea minimă a citirii fotorezistorului în condiții de lumină scăzută
  int photoResistorMaxValue = 4095; // Valoarea maximă a citirii fotorezistorului în condiții de lumină intensă
  int photoResistorPercentage = map(analogRead(PHOTO_RESISTOR_PIN), photoResistorMinValue, photoResistorMaxValue, 0, 100);// Read the value from the photoresistor and transform in procentage
  return photoResistorPercentage; // Percentege from the photoresistor and return it
}
void turnRelayOn(const String& userId, const String& plantId, const String& documentId) {
  digitalWrite(RELAY_PIN, HIGH); // Turn the relay on
  Serial.println("Relay is ON");

  // Construct the document path
  String documentPath = "users/" + userId + "/Plante/" + plantId + "/Pompe/" + documentId;

  // Construct JSON payload for the Firestore PATCH request
  String payload = "{\"fields\": {\"uda\": {\"booleanValue\": true}}}";

  // Prepare the update mask parameter
  String updateMask = "uda";

  // Send PATCH request to Firestore
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), payload.c_str(), updateMask.c_str(), "", "", "")) {
    Serial.println("Pump status 'uda' field set to true successfully.");
  } else {
    Serial.println("Failed to update pump status 'uda' field: " + fbdo.errorReason());
  }
}



void turnRelayOff(const String& userId, const String& plantId, const String& documentId) {
  digitalWrite(RELAY_PIN, LOW); // Turn the relay off
  Serial.println("Relay is OFF");

  // Construct the document path
  String documentPath = "users/" + userId + "/Plante/" + plantId + "/Pompe/" + documentId;

  // Construct JSON payload for the Firestore PATCH request
  String payload = "{\"fields\": {\"uda\": {\"booleanValue\": false}}}";

  // Prepare the update mask parameter
  String updateMask = "uda";

  // Send PATCH request to Firestore
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), payload.c_str(), updateMask.c_str(), "", "", "")) {
    Serial.println("Pump status 'uda' field set to true successfully.");
  } else {
    Serial.println("Failed to update pump status 'uda' field: " + fbdo.errorReason());
  }
}
// Function to read from the soil moisture sensor and return the moisture level as a percentage
int readSoilMoistureValue() {
  int soilMoistureValue = analogRead(SOIL_MOISTURE_PIN); // Read the raw value from the soil moisture sensor
  
  // Invert the mapping based on new calibration values. The raw value is mapped such that a higher value 
  // indicates less moisture, and a lower value indicates more moisture.
  int soilMoisturePercentage = map(soilMoistureValue, SOIL_MOISTURE_MAX, SOIL_MOISTURE_MIN, 100, 0);
  
  // Constrain the percentage to ensure it remains within 0-100%
  soilMoisturePercentage = constrain(soilMoisturePercentage, 0, 100);
  
  return soilMoisturePercentage;
}
float readDS18B20Temperature() {
  sensors.requestTemperatures(); // Send the command to get temperature readings
  return sensors.getTempCByIndex(0); // Returns the temperature in Celsius for the first sensor found on the wire
}


void storeLightSensorValue(const String& userId, const String& plantId, const String& documentId, int lightValue) {
  // Assuming documentId is known and valid
  String documentPath = "users/" + userId + "/Plante/" + plantId + "/SenzorLumina/" + documentId;

  // Construct JSON payload for the Firestore PATCH request
  String payload = "{\"fields\": {\"lumina\": {\"integerValue\": \"" + String(lightValue) + "\"}}}";

  // Prepare the update mask parameter (if your library version supports it)
  // This parameter tells Firestore which fields to update.
  String updateMask = "lumina";

  // Send PATCH request to Firestore
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), payload.c_str(), updateMask.c_str(), "", "", "")) {
    Serial.println("Sensor value updated successfully.");
  } else {
    Serial.println("Failed to update sensor value: " + fbdo.errorReason());
  }
}
void storeDhtSensorValue(const String& userId, const String& plantId, const String& documentId, float humidity, float temperature) {
  // Assuming documentId is known and valid
  String documentPath = "users/" + userId + "/Plante/" + plantId + "/SenzoriAer/" + documentId;

  // Construct JSON payload for the Firestore PATCH request with both humidity and temperature
  String payload = "{\"fields\": {\"senzor_umiditate\": {\"doubleValue\": \"" + String(humidity) + "\"}, \"senzor_temperatura\": {\"doubleValue\": \"" + String(temperature) + "\"}}}";

  // Prepare the update mask parameter for both fields (if your library version supports it)
  // This parameter tells Firestore which fields to update.
  String updateMask = "senzor_umiditate,senzor_temperatura";

  // Send PATCH request to Firestore
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), payload.c_str(), updateMask.c_str(), "", "", "")) {
    Serial.println("DHT sensor values updated successfully.");
  } else {
    Serial.println("Failed to update DHT sensor values: " + fbdo.errorReason());
  }
}
void storeSoilSensorData(const String& userId, const String& plantId, const String& documentId, int soilMoisturePercentage, float ds18b20Temperature) {
  // Assuming documentId is known and valid
  String documentPath = "users/" + userId + "/Plante/" + plantId + "/SenzoriSol/" + documentId;

  // Construct JSON payload for the Firestore PATCH request with soil moisture and DS18B20 temperature
  String payload = "{\"fields\": {\"senzor_umiditate\": {\"doubleValue\": \"" + String(soilMoisturePercentage) + "\"}, \"senzor_temperatura\": {\"doubleValue\": \"" + String(ds18b20Temperature) + "\"}}}";

  // Prepare the update mask parameter for both fields (if your library version supports it)
  // This parameter tells Firestore which fields to update.
  String updateMask = "senzor_umiditate,senzor_temperatura";

  // Send PATCH request to Firestore
  if (Firebase.Firestore.patchDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), payload.c_str(), updateMask.c_str(), "", "", "")) {
    Serial.println("Soil sensor data updated successfully.");
  } else {
    Serial.println("Failed to update soil sensor data: " + fbdo.errorReason());
  }
}
void connectToWiFi(String ssid, String password) {
  digitalWrite(LED_PIN, HIGH); // Turn the LED on to indicate the start of the connection process
  Serial.println("Connecting to WiFi...");

  WiFi.begin(ssid, password); // Start the process of connecting to Wi-Fi

  while (WiFi.status() != WL_CONNECTED) { // Wait until the connection is established
    delay(500); // Wait for half a second
    Serial.print(".");
  }
  wifiConnected=true;
  digitalWrite(LED_PIN, LOW); // Turn the LED off to indicate that the connection has been established
  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP()); // Print the ESP32 IP address on the network
}
float getMinWaterPercentage(const String& userId, const String& plantId, const String& documentId) {
  // Assuming you have a specific document structure, we will construct the path to the desired document
  String documentPath = "users/" + userId + "/Plante/" + plantId + "/SenzoriSol/" + documentId;

  // Attempt to get the document from Firestore
  if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), "")) {
    Serial.println("Successfully retrieved the document.");
    String payload = fbdo.payload();
    // Parse the JSON payload to extract the minWaterPercentage value
    DynamicJsonDocument doc(1024); // Adjust the size based on your expected document size
    deserializeJson(doc, payload);
    float minWaterPercentage = doc["fields"]["min_umiditate"]["doubleValue"]; // Adjust the path as needed
    if(minWaterPercentage == 0)
    {
      minWaterPercentage = doc["fields"]["min_umiditate"]["integerValue"];
    }
    return minWaterPercentage;
  } else {
    Serial.println("Failed to retrieve the document: " + fbdo.errorReason());
    return -1; // Return an error code or handle this situation as you see fit
  }
}


