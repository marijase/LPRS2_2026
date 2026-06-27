void setup() {
  Serial.begin(115200);
  pinMode(12, OUTPUT);
}

void loop() {
  Serial.println("Hello ESP32");
  delay(1000);
  digitalWrite(12, HIGH); 
  delay(1000);                     
  digitalWrite(12, LOW);  
  delay(1000);                     
}