#include <Arduino.h>
#include <time.h>

// Times before 2010 (1970 + 40 years) are invalid
#define MIN_EPOCH 40 * 365 * 24 * 3600

#include <stdlib.h>

#include <stdio.h>
#include <stdint.h>

#include <ETH.h>

#define INPUT_ANALOG  36

#define INPUT_DIGITAL 23

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  // put your setup code here, to run once:
  analogReadResolution(10); // Default of 12 is not very linear. Recommended to use 10 or 11 depending on needed resolution.
  //TK: Potentiometer - no
  analogSetAttenuation(ADC_6db); // Default is 11db which is very noisy. Recommended to use 2.5 or 6  
}

int i_a = 0;
float mv,cel;
void loop() {
  // put your main code here, to run repeatedly:
  i_a = analogRead(INPUT_ANALOG);
  mv = ( (float)i_a/1024.0)*5000.0; 
  cel = mv/10.0;  
  Serial.print(i_a);
  Serial.print(' ');
  Serial.println(cel);

}
