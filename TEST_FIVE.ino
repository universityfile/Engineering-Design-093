int pin = A0; //GPIO 36 on the board, SYMBOL IS VP OR UP

// recommended threshold values by testing
//high_threshold = 3000; takes into account general weather moisture which can affect soil moisture
// acutal test show 

void setup() {
  // put your setup code here, to run once:
  Serial.begin(19200);
  pinMode( pin, INPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  Serial.print("Without Mean ");
  float oneReading = analogRead(pin);
  Serial.println(oneReading);
  delay(10);
  float meanReading; 
  for(int i = 0; i < 100; i++){
    meanReading = meanReading + analogRead(pin);
    delay(10);
  }
  meanReading = meanReading / 100.00;
  Serial.println(meanReading);
  meanReading = 0;
}
