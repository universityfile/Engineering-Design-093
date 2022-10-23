#include <Preferences.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>

//deep sleep constants
const uint64_t uS_TO_SECOND = 1000000;  //microseconds to hours
const uint64_t SECOND_TO_HOUR = 3600;

//server constants
const char* defaultSSID = "GardenBuddy";
const char* defaultPassword = "TEST1234";

//settings
String networkSSID;
String networkPassword;
float latitude;
float longitude;
char plantType;

//operations modus
bool operationMode = true;

Preferences preferences;
AsyncWebServer server(80);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Setup started");

  preferences.begin("credentials", false);
  readStorage(); 
  printStorage();

  if ( networkSSID == "" || networkPassword == "" || (longitude == 0 && latitude == 0) || plantType == '\0') {
    Serial.println("No values stored");
    
    startServer(); 
    operationMode = false;
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  if (operationMode == true){
    // go to deep sleep for 1 minute and 10 seconds
    goToSleep(0.02);
  } else {
    Serial.println("Still no input received");
    delay(3000);
  }
  
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
  WiFi.softAP(defaultSSID, defaultPassword);
  Serial.println("Starting server");
  Serial.println("SSID: " + String(defaultSSID));
  Serial.println("Password " + String(defaultPassword));

  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Access point IP: ");
  Serial.println(IP);

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
      latitude = (request->getParam(2))->value().toFloat();
      longitude = ((request->getParam(3))->value()).toFloat();
      String temp = (request->getParam(4))->value();

      // check if input is correct
      // if incorrect notify user
      // if correct store to FLASH memory and end server
      if (temp !="Small" && temp !="Medium" && temp !="Large"){
        Serial.println("Incorrect plant");
        request->send_P(201, "text/html", "Unknown type of plant. Please submit again. <br><a href=\"http://192.168.4.1\">Return to Home Page</a>");
      } else if ( tryToConnectToWifi()==false ){
        Serial.println("incorrect wifi credentials");
        request->send_P(202, "text/html", "Incorrect Network Information. Please submit again. <br><a href=\"http://192.168.4.1\">Return to Home Page</a>");
      } else if (latitude > 90 || latitude < -90 || longitude > 90 || latitude < - 90){
        Serial.println("Incorrect geolocation");
        request->send_P(203, "text/html", "Incorrect latitude and longitude. Please submit again.<br><a href=\"http://192.168.4.1\">Return to Home Page</a>");
      } else {
        Serial.println("Input is correct. Storing to memory");
        plantType = temp.charAt(0);
        preferences.putString("ssid", networkSSID);
        preferences.putString("password", networkPassword);
        preferences.putFloat("latitude", latitude);
        preferences.putFloat("longitude", longitude);
        preferences.putChar("plantType", plantType);
        printStorage();
        
        request->send(200,"text/plain","Message Received");
        operationMode = true;
        server.end();
        WiFi.softAPdisconnect(true);
      }  
    }
  });
  
  server.begin();
}

bool tryToConnectToWifi(){ 
  WiFi.begin(networkSSID.c_str(), networkPassword.c_str());
  int p = 0;
  //try to connect for 10 seconds
  while (WiFi.status() != WL_CONNECTED && p < 10) {
    delay(50);
    p++;
  }
  
  if (WiFi.status() == WL_CONNECTED){
    return true;
  } else {
    return false;
  }
  return false;
}

void goToSleep(uint64_t intervalInHours){
  esp_sleep_enable_timer_wakeup( intervalInHours * SECOND_TO_HOUR * uS_TO_SECOND  );
  Serial.println("Setup ESP32 to sleep for " + String(intervalInHours) + " hours");
  Serial.println("Going to sleep now");
  esp_deep_sleep_start();
}

void readStorage(){
  Serial.println("Reading Storage");
  networkSSID = preferences.getString("ssid", "");
  networkPassword = preferences.getString("password", "");
  latitude = preferences.getFloat("latitude", 0);
  longitude = preferences.getFloat("longitude", 0);
  plantType = preferences.getChar("plantType", 0);
  Serial.println("Reading Complete");
}

//prints values for debugging purposes
void printStorage(){
  Serial.println("Printing Storage");
  Serial.println("------------------");
  Serial.println("SSID: " + networkSSID);
  Serial.println("PASS: " + networkPassword);
  Serial.println("LAT: " + String(latitude));
  Serial.println("LONG: " + String(longitude));
  Serial.println("TYPE: " + String(plantType));
  Serial.println("------------------");
  Serial.println("Printing Complete"); 
}

void resetStorage(){
  preferences.putString("ssid", "");
  preferences.putString("password", "");
  preferences.putFloat("latitude", 0);
  preferences.putFloat("longitude", 0);
  preferences.putChar("plantType", '\0');
}
