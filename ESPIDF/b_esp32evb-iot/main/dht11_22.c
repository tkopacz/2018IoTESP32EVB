/* DHT11 temperature sensor library
   Usage:
   		Set DHT PIN using  setDHTPin(pin) command
   		getFtemp(); this returns temperature in F
   Sam Johnston 
   October 2016
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "rom/ets_sys.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "dht11_22.h"


int humidity = 0;
int temperature = 0;

int DHT_DATA[3] = {0,0,0};
int DHT_PIN = 5;

void setDHT11Pin(int PIN)
{
	DHT_PIN = PIN;
}
void errorHandle(int response)
{
	switch(response) {
	
		case DHT_TIMEOUT_ERROR :
			printf("DHT Sensor Timeout!\n");
		case DHT_CHECKSUM_ERROR:
			printf("CheckSum error!\n");
		case DHT_OK:
			break;
		default :
			printf("Dont know how you got here!\n");
	}
	temperature = 0;
	humidity = 0;
			
}
void sendDHT11Start()
{
  //Send start signal from ESP32 to DHT device
  gpio_set_direction(DHT_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level(DHT_PIN,0);
  //vTaskDelay(36 / portTICK_RATE_MS);
  ets_delay_us(22000);
  gpio_set_level(DHT_PIN,1);
  ets_delay_us(43);
  gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);
}

int readDHT11()
{
  //Variables used in this function
  int counter = 0;
  uint8_t bits[5];
  uint8_t byteCounter = 0;
  uint8_t cnt = 7;
  
  for (int i = 0; i <5; i++)
  {
  	bits[i] = 0;
  }
  
  sendDHT11Start();
  
  //Wait for a response from the DHT11 device
  //This requires waiting for 20-40 us 
  counter = 0;
  
  while (gpio_get_level(DHT_PIN)==1)
  {
      if(counter > 40)
      {
            return DHT_TIMEOUT_ERROR;
      }	
      counter = counter + 1;
      ets_delay_us(1);
  }
  //Now that the DHT has pulled the line low, 
  //it will keep the line low for 80 us and then high for 80us
  //check to see if it keeps low
  counter = 0;
  while(gpio_get_level(DHT_PIN)==0)
  {
  	if(counter > 80)
  	{
            return DHT_TIMEOUT_ERROR;
  	}
  	counter = counter + 1;
  	ets_delay_us(1);
  }
  counter = 0;
  while(gpio_get_level(DHT_PIN)==1)
  {
  	if(counter > 80)
  	{
            return DHT_TIMEOUT_ERROR;
  	}
  	counter = counter + 1;
  	ets_delay_us(1);
  }
  // If no errors have occurred, it is time to read data
  //output data from the DHT11 is 40 bits.
  //Loop here until 40 bits have been read or a timeout occurs
  
  for(int i = 0; i < 40; i++)
  {
      //int currentBit = 0;
      //starts new data transmission with 50us low signal
      counter = 0;
      while(gpio_get_level(DHT_PIN)==0)
  	  {
  	  	if (counter > 55)
  	  	{
            return DHT_TIMEOUT_ERROR;
  	  	}
  	  	counter = counter + 1;
  	  	ets_delay_us(1);
  	  }
  	  
  	  //Now check to see if new data is a 0 or a 1
      counter = 0;
      while(gpio_get_level(DHT_PIN)==1)
  	  {
  	  	if (counter > 75)
  	  	{
            return DHT_TIMEOUT_ERROR;
  	  	}
  	  	counter = counter + 1;
  	  	ets_delay_us(1);
  	  }  	  
  	  //add the current reading to the output data
  	  //since all bits where set to 0 at the start of the loop, only looking for 1s
  	  //look for when count is greater than 40 - this allows for some margin of error
  	  if (counter > 40)
  	  {
  	  
  	  	bits[byteCounter] |= (1 << cnt);
  	  	
  	  }
  	  //here are conditionals that work with the bit counters
  	  if (cnt == 0)
  	  {
  	  	
  	  	cnt = 7;
  	  	byteCounter = byteCounter +1;
  	  }else{
  	  
  	  	cnt = cnt -1;
  	  } 	  
  }
  humidity = bits[0];
  temperature = bits[2];
  
  uint8_t sum = bits[0] + bits[2];
  
  if (bits[4] != sum)
  {
  	return DHT_CHECKSUM_ERROR;
  }
  return -1;
}

int getTemperature()
{
	return temperature;
}
int getHumidity()
{
	return humidity;
}


/* -------------------
*/

/*----------------------------------------------------------------------------
;
;	read DHT22 sensor

copy/paste from AM2302/DHT22 Docu:

DATA: Hum = 16 bits, Temp = 16 Bits, check-sum = 8 Bits

Example: MCU has received 40 bits data from AM2302 as
0000 0010 1000 1100 0000 0001 0101 1111 1110 1110
16 bits RH data + 16 bits T data + check sum

1) we convert 16 bits RH data from binary system to decimal system, 0000 0010 1000 1100 → 652
Binary system Decimal system: RH=652/10=65.2%RH

2) we convert 16 bits T data from binary system to decimal system, 0000 0001 0101 1111 → 351
Binary system Decimal system: T=351/10=35.1°C

When highest bit of temperature is 1, it means the temperature is below 0 degree Celsius. 
Example: 1000 0000 0110 0101, T= minus 10.1°C: 16 bits T data

3) Check Sum=0000 0010+1000 1100+0000 0001+0101 1111=1110 1110 Check-sum=the last 8 bits of Sum=11101110

Signal & Timings:

The interval of whole process must be beyond 2 seconds.

To request data from DHT:

1) Sent low pulse for > 1~10 ms (MILI SEC)
2) Sent high pulse for > 20~40 us (Micros).
3) When DHT detects the start signal, it will pull low the bus 80us as response signal, 
   then the DHT pulls up 80us for preparation to send data.
4) When DHT is sending data to MCU, every bit's transmission begin with low-voltage-level that last 50us, 
   the following high-voltage-level signal's length decide the bit is "1" or "0".
	0: 26~28 us
	1: 70 us

;----------------------------------------------------------------------------*/

int getDHT22SignalLevel( int usTimeOut, bool state )
{
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&mux);
	int uSec = 0;
	while( gpio_get_level(DHT_PIN)==state ) {

		if( uSec > usTimeOut ) 
			return -1;
		
		++uSec;
		ets_delay_us(1);		// uSec delay
	}
  portEXIT_CRITICAL(&mux);
	return uSec;
}

#define MAXdhtData 5	// to complete 40 = 5*8 Bits

int readDHT22()
{
int uSec = 0;

uint8_t dhtData[MAXdhtData];
uint8_t byteInx = 0;
uint8_t bitInx = 7;

	for (int k = 0; k<MAXdhtData; k++) 
		dhtData[k] = 0;

	// == Send start signal to DHT sensor ===========

	gpio_set_direction( DHT_PIN, GPIO_MODE_OUTPUT );

	// pull down for 3 ms for a smooth and nice wake up 
	gpio_set_level( DHT_PIN, 0 );
	ets_delay_us( 3000 );			

	// pull up for 25 us for a gentile asking for data
	gpio_set_level( DHT_PIN, 1 );
	ets_delay_us( 25 );

	gpio_set_direction( DHT_PIN, GPIO_MODE_INPUT );		// change to input mode
  
	// == DHT will keep the line low for 80 us and then high for 80us ====

	uSec = getDHT22SignalLevel( 85, 0 );
//	ESP_LOGI( TAG, "Response = %d", uSec );
	if( uSec<0 ) return DHT_TIMEOUT_ERROR; 

	// -- 80us up ------------------------

	uSec = getDHT22SignalLevel( 85, 1 );
//	ESP_LOGI( TAG, "Response = %d", uSec );
	if( uSec<0 ) return DHT_TIMEOUT_ERROR;

	// == No errors, read the 40 data bits ================
  
	for( int k = 0; k < 40; k++ ) {

		// -- starts new data transmission with >50us low signal

		uSec = getDHT22SignalLevel( 56, 0 );
		if( uSec<0 ) return DHT_TIMEOUT_ERROR;

		// -- check to see if after >70us rx data is a 0 or a 1

		uSec = getDHT22SignalLevel( 75, 1 );
		if( uSec<0 ) return DHT_TIMEOUT_ERROR;

		// add the current read to the output data
		// since all dhtData array where set to 0 at the start, 
		// only look for "1" (>28us us)
	
		if (uSec > 40) {
			dhtData[ byteInx ] |= (1 << bitInx);
			}
	
		// index to next byte

		if (bitInx == 0) { bitInx = 7; ++byteInx; }
		else bitInx--;
	}

	// == get humidity from Data[0] and Data[1] ==========================

	humidity = dhtData[0];
	humidity *= 0x100;					// >> 8
	humidity += dhtData[1];
	//humidity /= 10;						// get the decimal

	// == get temp from Data[2] and Data[3]
	
	temperature = dhtData[2] & 0x7F;	
	temperature *= 0x100;				// >> 8
	temperature += dhtData[3];
	//temperature /= 10;

	if( dhtData[2] & 0x80 ) 			// negative temp, brrr it's freezing
		temperature *= -1;


	// == verify if checksum is ok ===========================================
	// Checksum is the sum of Data 8 bits masked out 0xFF
	
	if (dhtData[4] == ((dhtData[0] + dhtData[1] + dhtData[2] + dhtData[3]) & 0xFF)) 
		return DHT_OK;

	else 
		return DHT_CHECKSUM_ERROR;
}
