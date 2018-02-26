int pinNo = 35;
void setup() {
  Serial.begin(115200);
  // put your setup code here, to run once:
  pinMode(pinNo, OUTPUT);
}
int cnt=0;
void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(pinNo,HIGH);
  delay(1000);
  digitalWrite(pinNo,LOW);
  delay(1000);
  Serial.println(cnt++);
}
