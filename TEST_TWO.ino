
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h" 

const char* ntpServer = "pool.ntp.org";
int timestamp;

//settings
String networkSSID = "TestNetwork";
String networkPassword = "test1234";
float latitude = 51.55;
float longitude = 5.43;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Setup started");
}

void loop(){
  tryToConnectToWifi();
  getRain();
  delay(5000);
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

//gets amount of rain today
float getRain(){
  Serial.println("Checking if still connected");
  if ((WiFi.status()== WL_CONNECTED)){
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
      }

      double id = doc["latitude"];
      double rain = doc["daily"]["rain_sum"][0];
      Serial.println("Daily Rain: " + String(rain) + "mm" +"\n");
      client.end();
      return rain;
      
    } else {
      Serial.println("Error on HTTP request");
      return -1;
    }
  } else { 
    Serial.println("Connection lost");
    return -1;
  }

  Serial.println("Error??? ");
  return -1;
}
