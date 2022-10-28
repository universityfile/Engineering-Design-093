#include <common.h>
#include <FirebaseFS.h>
#include <Firebase_ESP_Client.h>
#include <Utils.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#include <WiFi.h>
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
String networkSSID = "TestNetwork";
String networkPassword = "test1234";

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
  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
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
    json.set(waterAmountPath, 1.2);
    json.set(rainAmountPath, 1.25);
    json.set(savedAmountPath, 1.3);
    json.set(wateredPath, true);
    json.set(errorPath, true);
    json.set(moisturePath, 2300);
    
    Serial.printf("Set json... %s\n", Firebase.RTDB.setJSON(&fbdo, parentPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
   }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Setup started");
  
}

void loop() {
  // put your main code here, to run repeatedly:
    tryToConnectToWifi();
    sendToDatabase();   
    delay(20000);
}
