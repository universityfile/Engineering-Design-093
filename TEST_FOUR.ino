byte pumpPin = 21;

float dry_threshold = 2300;

//litre per square meter defined by amount of water needed by plant type
char plantType='M';

const float waterTypeS = 1.3;
const float waterTypeM = 2.5;
const float waterTypeL = 4;
const float area = 1; // this is to be filled after assembly on 24.10
// and used in the watering algorithm
const float litrePerSecond = 0.02;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  Serial.println("Starting in three second"); 
  pinMode(pumpPin, OUTPUT);
}

// returns soil sensor reading
float getMoisture(){
  return (dry_threshold + 1);
}


//returns amount of rain in mm
float getRain(){
  return 1.2;
}

//turns on pump for a set interval
void turnOnPump(float intervalInSeconds){
  digitalWrite( pumpPin , HIGH);
  delay( intervalInSeconds * 1000);
  digitalWrite( pumpPin, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(3000);
  Serial.println("Begin");
  float moisture;
  float amount;
  moisture = getMoisture();
      if ( moisture > dry_threshold ){
        float rainAmount;
        rainAmount = getRain();
        switch (plantType){
          case 'S':
                amount = waterTypeS - rainAmount;
                break; 
          case 'M':
                amount = waterTypeM - rainAmount;
                break;
          case 'L':
                amount = waterTypeL - rainAmount;
                break;                         
        }
        Serial.println(String(amount) + " litres needed");
        if (amount > 0){
          float timeInterval = amount / litrePerSecond;
          Serial.println(String(timeInterval) + " seconds neede to water");
          turnOnPump(timeInterval);
          if ( moisture - getMoisture() < 0 ) {
            Serial.println("Pump did not turn on or there is no water in container");
          } else {
            Serial.println("");
            Serial.println("Irrigation Completed");
          }
        }
      }
}
