#include <common.h>
#include <FirebaseFS.h>
#include <Firebase_ESP_Client.h>
#include <Utils.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#include <Preferences.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h" 

//Database constants
const char* apiKey = "AIzaSyDZGMf2Vor-GsgPfD9IC9Qy2QfmE2soMjg";
const char* userEmail = "gardenbuddy@gmail.com";
const char* userPassword = "Test1234";
const char* databaseURL = "https://wateringsystem-1efb2-default-rtdb.europe-west1.firebasedatabase.app/";

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

String uid;
String databasePath;
String timePath = "/time";
String timeLocalPath = "/timeLocal";
String waterAmountPath = "/water";
String rainAmountPath = "/rain";
String savedAmountPath = "/saved";
String wateredPath = "/watered";
String errorPath = "/error";
String moisturePath = "/moisture";
String parentPath;

const char* ntpServer = "pool.ntp.org";
int timestamp;
FirebaseJson json;

//settings
String networkSSID;
String networkPassword;
double latitude;
double longitude;
char plantType;

Preferences preferences;

const byte pumpPin = 21;
const byte sensorPin = A0;
bool operationMode = false;

double rain;
double moisture;
double amount;
double saved;
bool watered;
bool error;

AsyncWebServer server(80);

const double waterTypeS = 1.3;
const double waterTypeM = 2.5;
const double waterTypeL = 4;
const double literPerSecond = 0.02;
const double high_threshold = 1500;

//gets current time for timestamp
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}

String getLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return "";
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

  char timestamp[32];
  strftime(timestamp,32, "%x %X", &timeinfo);
  Serial.println(timestamp);
  
  String temp = String(timestamp);
  temp.replace("/", "-");
  return temp;
}

//get offset from GMT time zone
double getRainSetGMToffset(){
  Serial.println("Checking if still connected");
  if ((WiFi.status()==WL_CONNECTED)){
    Serial.println("Still Connected");
    
    HTTPClient client;
    client.begin("https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude) + "&longitude=" + String(longitude) + "&timezone=auto&daily=rain_sum");
    int httpCode = client.GET();
    // if code is less than 0 then the ESP32 had an internal error
    if (httpCode > 0) {
      String payload = client.getString();
      Serial.println("\nStatuscode: " + String(httpCode));
      Serial.println(payload);

      char json[500];
      payload.toCharArray(json,500);

      StaticJsonDocument<1024> doc;
      DeserializationError err = deserializeJson(doc,json);

      if (err) {
        Serial.println("ERORR: ");
        Serial.println(err.c_str());
        return -1;
      }

      long offset = doc["utc_offset_seconds"];
      Serial.println("GMT offset: " + String(offset) + "\n");
      configTime(offset, 0 , ntpServer);
      client.end();
      double rain = doc["daily"]["rain_sum"][0];
      return rain;
    } else {
      Serial.println("Error on HTTP request");
      return -1;
    }
  } else { 
    Serial.println("Connection lost");
    return -1;
  }

  Serial.println("Error???");
  return -1;
}

bool tryToConnectToWifi(){ 
  WiFi.begin(networkSSID.c_str(), networkPassword.c_str());
  int p = 0;
  //try to connect for 10 seconds
  while (WiFi.status() != WL_CONNECTED && p < 20) {
    delay(500);
    p++;
  }
  
  if (WiFi.status() == WL_CONNECTED){
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    return false;
  }
  return false;
}

void sendToDatabase(){
  config.api_key = apiKey;
  auth.user.email = userEmail;
  auth.user.password = userPassword;
  config.database_url = databaseURL;
  
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);
  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 5;

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);

  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  int counter = 0;
  while ((auth.token.uid) == "" & counter < 20) {
    Serial.print('.');
    delay(1000);
    counter = counter + 1;
  }
  if ( auth.token.uid == "" ){
    return;
  }
  // Print user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);

  // Update database path
  databasePath = "/UsersData/" + uid + "/data";
  Serial.println(WiFi.status());
  if (Firebase.ready()){
    
    //Get current timestamp
    timestamp = getTime();
    String tempStr = getLocalTime();
    Serial.print ("time: ");
    Serial.println (timestamp);
    parentPath= databasePath + "/" + String(timestamp);
    json.set(timePath, String(timestamp));
    json.set(timeLocalPath, tempStr);
    json.set(waterAmountPath, amount);
    json.set(rainAmountPath, rain);
    json.set(savedAmountPath, saved);
    json.set(wateredPath, watered);
    json.set(errorPath, error);
    json.set(moisturePath, moisture);
    
    Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
   }
}

#define uS_TO_SECOND 1000000;

void goToSleep(uint64_t intervalInSeconds){
  Serial.println("Entered sleep function");
  uint64_t temporary = intervalInSeconds * uS_TO_SECOND;
  esp_sleep_enable_timer_wakeup( temporary );
  Serial.println("Setup ESP32 to sleep for " + String(intervalInSeconds) + " seconds");
  Serial.println("Going to sleep now");
  delay(1000);    
  Serial.flush();
  esp_deep_sleep_start();
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Setup started");
  pinMode(pumpPin, OUTPUT);
  pinMode(sensorPin, INPUT);
  preferences.begin("credentials", false);
  readStorage();
  printStorage();
}

void loop() {
  // put your main code here, to run repeatedly:
    if (tryToConnectToWifi() == false){
      startServer();
      while (WiFi.status() != WL_CONNECTED){
        Serial.println("Offline mode");
        moisture = getMoisture();
        if (moisture > high_threshold){
          //turnOnPump(60);
        }
        delay(10000);
      }
    }
    moisture = getMoisture();
    rain = getRainSetGMToffset();
    if (plantType == 'S'){
      amount = waterTypeS - rain;
      saved = waterTypeS;
    } else if (plantType == 'M') {
      amount = waterTypeM - rain;
      saved = waterTypeM;
    } else if (plantType == 'L') {
      amount = waterTypeL - rain;
      saved = waterTypeL;
    }
    watered = false;
    error = false;
    if (amount > 0){
      turnOnPump(60);
      saved = rain;
      watered = true;
      if ( (getMoisture() - 10) < moisture){
        error = true;
      }
    } else {
      amount = 0;
    }
  
    Serial.println("Rain: "+ String(rain));
    Serial.println("Water needed: " + String(amount));
    Serial.println("Water saved: " + String(saved));
    Serial.print("Watered:(1 true; 0 false): ");
    Serial.println(watered);
    Serial.println("Error: ");
    Serial.println(error);
    sendToDatabase();
    //delay(6000);
    goToSleep(60);
}

//retrieves variables from Flash Storage
void readStorage(){
  networkSSID = preferences.getString("ssid", "");
  networkPassword = preferences.getString("password", "");
  latitude = preferences.getDouble("latitude", 0);
  longitude = preferences.getDouble("longitude", 0);
  plantType = preferences.getChar("plantType", 0);
}

//prints values for debugging purposes
void printStorage(){
  Serial.println("------------------");
  Serial.println("SSID: " + networkSSID);
  Serial.println("PASS: " + networkPassword);
  Serial.println("LAT: " + String(latitude));
  Serial.println("LONG: " + String(longitude));
  Serial.println("TYPE: " + String(plantType));
  Serial.println("------------------");
}

//html and css code for server website
const char index_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE HTML>
<html>
  <head>
      
  <title>Garden Buddy Settings Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html {
     font-family: Georgia;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h1 {
      font-family: Georgia;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      padding-top: 20px;
      vertical-align:middle;
      padding-bottom: 40px;
    }
    form {
      font-size: 1rem;
      margin-top: 20px;
      vertical-align:middle;
      padding-bottom: 40px;
    }
    input[type=text]{
        margin-top:8px;
        vertical-align:middle;
        margin-bottom: 10px;
        border-radius: 5px;
    }
    input[type=submit]{
        margin-top:8px;
        vertical-align:middle;
        margin-bottom: 10px;
        padding:5px ;
        border-radius: 5px;
    }
    
  </style>
  </head>
  <body>
  <h1> Garden Buddy</h1>
  <form action="/POST/">
    Network SSID: <input type="text" name="networkSSID"> 
    <br>
    Network Password: 
    <input type="text" name="networkPassword">
    <br>
    Geographical Latitude: 
    <input type="text" name="latitude">
    <br>
    Geographical Longitude: 
    <input type="text" name="longitude">
    <br>
    Plant Type: 
    <input type="text" name="plantType">
    <br>
    <input type="submit" value="Submit">
  </form>
  
  </body>
</html>)rawliteral";

// start server from which the customer inputs initial settings
void startServer(){
   
  //start access point
  WiFi.softAP("GardenBuddy", "TEST1234");
  Serial.println("Starting server");
  
  //START SERVER
  server.on("/" , HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  server.on("/POST/", HTTP_GET, [](AsyncWebServerRequest *request){
    int parametersN = request->params();
    Serial.println(parametersN);
    if (parametersN == 5){
      for(int i =0; i < parametersN; i++){
        AsyncWebParameter* p = request->getParam(i);
        
        Serial.print("Param name: ");
        Serial.println(p->name());
        Serial.print("Param value: ");
        Serial.println(p->value());
        Serial.println("-----");
      }
      //save server input to local variables
      //then store in flash memory
      networkSSID = (request->getParam(0))->value();
      networkPassword = (request->getParam(1))->value();
      latitude = (request->getParam(2))->value().toDouble();
      longitude = ((request->getParam(3))->value()).toDouble();
      String temp = (request->getParam(4))->value();

      // check if input is correct
      // if incorrect notify user
      // if correct store to FLASH memory and end server
      if (temp !="Small" && temp !="Medium" && temp !="Large"){
        Serial.println("Incorrect plant");
        request->send_P(201, "text/html", "Unknown type of plant. Please submit again. <br><a href=\"http://192.168.4.1\">Return to Home Page</a>"); 
        networkSSID = "";
        networkPassword = "";
        latitude = 0;
        longitude = 0;
      } else if (latitude > 90 || latitude < -90 || longitude > 90 || latitude < - 90){
        Serial.println("Incorrect geolocation");
        request->send_P(203, "text/html", "Incorrect latitude and longitude. Please submit again.<br><a href=\"http://192.168.4.1\">Return to Home Page</a>");
        networkSSID = "";
        networkPassword = "";
        latitude = 0;
        longitude = 0;
      } else if ( tryToConnectToWifi()==false ){
        Serial.println("incorrect wifi credentials");
        request->send_P(202, "text/html", "Incorrect Network Information. Please submit again. <br><a href=\"http://192.168.4.1\">Return to Home Page</a>");
        networkSSID = "";
        networkPassword = "";
        latitude = 0;
        longitude = 0;
      } else {
        Serial.println("Input is correct. Storing to memory");
        networkSSID = (request->getParam(0))->value();
        networkPassword = (request->getParam(1))->value();
        latitude = (request->getParam(2))->value().toDouble();
        longitude = ((request->getParam(3))->value()).toDouble();
        plantType = temp.charAt(0);
        preferences.putString("ssid", networkSSID);
        preferences.putString("password", networkPassword);
        preferences.putDouble("latitude", latitude);
        preferences.putDouble("longitude", longitude);
        preferences.putChar("plantType", plantType);
        printStorage();
        
        request->send(200,"text/plain","Message Received");
        Serial.println("Ending server");
        delay(100);
        operationMode = true;
        server.end();
        WiFi.softAPdisconnect(true);
      }  
    }
  });
  
  server.begin();
}

//checks if soil is too dry
double getMoisture(){
  double meanReading = 0; 
  for(int i = 0; i < 100; i++){
    meanReading = meanReading + analogRead(sensorPin);
    delay(10);
  }
  meanReading = meanReading / 100.00;
  Serial.println(meanReading);
  return meanReading; 
}

void turnOnPump(int intervalInSeconds){
  Serial.println("Turning pump on for " + String(intervalInSeconds) + " seconds");
  digitalWrite( pumpPin , HIGH);
  delay( intervalInSeconds * 1000);
  digitalWrite( pumpPin, LOW);
  Serial.println("Turning off pump");
  delay(100);
}
