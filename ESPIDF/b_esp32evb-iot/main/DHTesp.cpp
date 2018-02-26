/******************************************************************
  DHT Temperature & Humidity Sensor library for Arduino & ESP32.
******************************************************************/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_partition.h"
#include "esp_log.h"
#include <sys/time.h>
#include "math.h"

#include "DHTesp.h"

#define NOP() asm volatile ("nop")

portMUX_TYPE microsMux = portMUX_INITIALIZER_UNLOCKED;

unsigned long IRAM_ATTR micros()
{
    static unsigned long lccount = 0;
    static unsigned long overflow = 0;
    unsigned long ccount;
    portENTER_CRITICAL_ISR(&microsMux);
    __asm__ __volatile__ ( "rsr     %0, ccount" : "=a" (ccount) );
    if(ccount < lccount){
        overflow += UINT32_MAX / CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ;
    }
    lccount = ccount;
    portEXIT_CRITICAL_ISR(&microsMux);
    return overflow + (ccount / CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ);
}

unsigned long IRAM_ATTR millis()
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void delay(uint32_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void IRAM_ATTR delayMicroseconds(uint32_t us)
{
    uint32_t m = micros();
    if(us){
        uint32_t e = (m + us);
        if(m > e){ //overflow
            while(micros() > e){
                NOP();
            }
        }
        while(micros() < e){
            NOP();
        }
    }
}

#define HIGH 1
#define LOW 0


void DHTesp::setup(gpio_num_t pin, DHT_MODEL_t model)
{
  gpio_pad_select_gpio(pin);
  DHTesp::pin = pin;
  DHTesp::model = model;
  DHTesp::resetTimer(); // Make sure we do read the sensor in the next readSensor()

  if ( model == AUTO_DETECT) {
    DHTesp::model = DHT22;
    readSensor();
    if ( error == ERROR_TIMEOUT ) {
      DHTesp::model = DHT11;
      // Warning: in case we auto detect a DHT11, you should wait at least 1000 msec
      // before your first read request. Otherwise you will get a time out error.
    }
  }

}

void DHTesp::resetTimer()
{
  DHTesp::lastReadTime = millis() - 3000;
}

float DHTesp::getHumidity()
{
  readSensor();
  if ( error == ERROR_TIMEOUT ) { // Try a second time to read
    readSensor();
  }
  return humidity;
}

float DHTesp::getTemperature()
{
  readSensor();
  if ( error == ERROR_TIMEOUT ) { // Try a second time to read
    readSensor();
  }
  return temperature;
}

TempAndHumidity DHTesp::getTempAndHumidity()
{
  readSensor();
  if ( error == ERROR_TIMEOUT ) { // Try a second time to read
    readSensor();
  }
  values.temperature = temperature;
  values.humidity = humidity;
  return values;
}

const char* DHTesp::getStatusString()
{
  switch ( error ) {
    case DHTesp::ERROR_TIMEOUT:
      return "TIMEOUT";

    case DHTesp::ERROR_CHECKSUM:
      return "CHECKSUM";

    default:
      return "OK";
  }
}

void DHTesp::readSensor()
{
  // Make sure we don't poll the sensor too often
  // - Max sample rate DHT11 is 1 Hz   (duty cicle 1000 ms)
  // - Max sample rate DHT22 is 0.5 Hz (duty cicle 2000 ms)
  unsigned long startTime = millis();
  if ( (unsigned long)(startTime - lastReadTime) < (model == DHT11 ? 999L : 1999L) ) {
    return;
  }
  lastReadTime = startTime;

  temperature = NAN;
  humidity = NAN;

  unsigned int rawHumidity = 0;
  unsigned int rawTemperature = 0;
  unsigned int data = 0;

  // Request sample
  gpio_set_direction(pin, GPIO_MODE_OUTPUT);  
  gpio_set_level(pin, 0);
  if ( model == DHT11 ) {
    delay(18);
  }
  else {
    // This will fail for a DHT11 - that's how we can detect such a device
    delayMicroseconds(800);
  }

  gpio_set_level(pin,1);
  gpio_set_direction(pin, GPIO_MODE_INPUT);  
  // pinMode(pin, INPUT);
  // digitalWrite(pin, HIGH); // Switch bus to receive data

  // We're going to read 83 edges:
  // - First a FALLING, RISING, and FALLING edge for the start bit
  // - Then 40 bits: RISING and then a FALLING edge per bit
  // To keep our code simple, we accept any HIGH or LOW reading if it's max 85 usecs long

  // ESP32 is a multi core / multi processing chip
  // It is necessary to disable task switches during the readings
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&mux);
  for ( short i = -3 ; i < 2 * 40; i++ ) {
    uint8_t age;
    startTime = micros();

    do {
      age = (unsigned long)(micros() - startTime);
      if ( age > 90 ) {
        error = ERROR_TIMEOUT;
        portEXIT_CRITICAL(&mux);
        return;
      }
    }
    while ( gpio_get_level(pin) == (i & 1) ? HIGH : LOW );

    if ( i >= 0 && (i & 1) ) {
      // Now we are being fed our 40 bits
      data <<= 1;

      // A zero max 30 usecs, a one at least 68 usecs.
      if ( age > 30 ) {
        data |= 1; // we got a one
      }
    }

    switch ( i ) {
      case 31:
        rawHumidity = data;
        break;
      case 63:
        rawTemperature = data;
        data = 0;
        break;
    }
  }

  portEXIT_CRITICAL(&mux);

  // Verify checksum

  if ( (unsigned short)(((unsigned short)rawHumidity) + (rawHumidity >> 8) + ((unsigned short)rawTemperature) + (rawTemperature >> 8)) != data ) {
    error = ERROR_CHECKSUM;
    return;
  }

  // Store readings

  if ( model == DHT11 ) {
    humidity = rawHumidity >> 8;
    temperature = rawTemperature >> 8;
  }
  else {
    humidity = rawHumidity * 0.1;

    if ( rawTemperature & 0x8000 ) {
      rawTemperature = -(int16_t)(rawTemperature & 0x7FFF);
    }
    temperature = ((int16_t)rawTemperature) * 0.1;
  }

  error = ERROR_NONE;
}

