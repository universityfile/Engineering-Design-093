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

String networkSSID = "TestNetwork"; 
String networkPassword = "test1234";

//Database constants
const char* apiKey = "AIzaSyDZGMf2Vor-GsgPfD9IC9Qy2QfmE2soMjg";
const char* userEmail = "gardenbuddy@gmail.com";
const char* userPassword = "Test1234";
const char* projectID = "wateringsystem-1efb2";

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const char* ntpServer = "pool.ntp.org";
int timestamp;
FirebaseJson content;

long GMToffset;
int latitude = 51.44;
int longitude = 5.43;

Preferences preferences;
AsyncWebServer server(80);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Setup started");
  tryToConnectToWifi();
}

void loop(){
    sendToDatabase(1.20 , 1.40);
    delay(5000);
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

//===============================================================================================================================//
//===============================================================================================================================//

//tries to connect to WiFi without scanning
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

//===============================================================================================================================//
//===============================================================================================================================//

//get offset from GMT time zone
long getGMToffset(){
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
      Serial.println("Daily Rain: " + String(offset) + "\n");
      client.end();
      preferences.putLong("offset", offset);
      return offset;
      
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

//===============================================================================================================================//
//===============================================================================================================================//

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

//===============================================================================================================================//
//===============================================================================================================================//

//sends data to database
void sendToDatabase(float amount, float rainAmount){
  
  GMToffset = getGMToffset();
  if (GMToffset == -1){
    Serial.println("Unsuccesful");
    GMToffset = preferences.getLong("offset");
  }
  Serial.println("GMT OFFSET" + String(GMToffset));
  
  configTime(GMToffset, 0 , ntpServer);
  
  config.api_key = apiKey;
  auth.user.email = userEmail;
  auth.user.password = userPassword;
  
  Firebase.reconnectWiFi(true);
  fbdo.setResponseSize(4096);
  // Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  // Assign the maximum retry of token generation
  config.max_token_generation_retry = 10;

  // Initialize the library with the Firebase authen and config
  Firebase.begin(&config, &auth);

  if (Firebase.ready()){
        // We will create the nested document in the parent path "Data/timestamp/"
        
        timestamp = getTime();
        String temp = getLocalTime();
        Serial.print ("time: ");
        Serial.println (timestamp);
        String documentPath = "Data/" + String(timestamp);

        // double
        content.set("fields/waterAmount/doubleValue", 123.45678);
        content.set("fields/rainAmount/doubleValue", 456.789);
        content.set("fields/epoch/integerValue", timestamp);
        content.set("fields/mytimeStampLocal/stringValue", temp);
        content.set("fields/watered/booleanValue", true);
        content.set("fields/waterSaved/doubleValue", 13.555);
        
        String doc_path = "projects/";
        doc_path += projectID;
        doc_path += "/databases/(default)/documents/coll_id/doc_id"; // coll_id and doc_id are your collection id and document id;

        Serial.print("Create a document... ");

        if (Firebase.Firestore.createDocument(&fbdo, projectID, "" /* databaseId can be (default) or empty */, documentPath.c_str(), content.raw()))
            Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
        else
            Serial.println(fbdo.errorReason());
  }
}

//===============================================================================================================================//
//===============================================================================================================================//

void sendErrorToDatabase(){
  
}
