/******************************************************************
Based on :
  https://github.com/beegee-tokyo/arduino-DHTesp

 ******************************************************************/

#ifndef dhtesp_h
#define dhtesp_h


struct TempAndHumidity {
  float temperature;
  float humidity;
};


class DHTesp
{
public:

  typedef enum {
    AUTO_DETECT,
    DHT11,
    DHT22,
    AM2302,  // Packaged DHT22
    RHT03    // Equivalent to DHT22
  }
  DHT_MODEL_t;

  typedef enum {
    ERROR_NONE = 0,
    ERROR_TIMEOUT,
    ERROR_CHECKSUM
  }
  DHT_ERROR_t;

  TempAndHumidity values;

  void setup(gpio_num_t pin, DHT_MODEL_t model=AUTO_DETECT);
  void resetTimer();

  float getTemperature();
  float getHumidity();
  TempAndHumidity getTempAndHumidity();

  DHT_ERROR_t getStatus() { return error; };
  const char* getStatusString();

  DHT_MODEL_t getModel() { return model; }

  int getMinimumSamplingPeriod() { return model == DHT11 ? 1000 : 2000; }

  short getNumberOfDecimalsTemperature() { return model == DHT11 ? 0 : 1; };
  short getLowerBoundTemperature() { return model == DHT11 ? 0 : -40; };
  short getUpperBoundTemperature() { return model == DHT11 ? 50 : 125; };

  short getNumberOfDecimalsHumidity() { return 0; };
  short getLowerBoundHumidity() { return model == DHT11 ? 20 : 0; };
  short getUpperBoundHumidity() { return model == DHT11 ? 90 : 100; };

  static float toFahrenheit(float fromCelcius) { return 1.8 * fromCelcius + 32.0; };
  static float toCelsius(float fromFahrenheit) { return (fromFahrenheit - 32.0) / 1.8; };

protected:
  void readSensor();

  float temperature;
  float humidity;

  gpio_num_t pin;

private:
  DHT_MODEL_t model;
  DHT_ERROR_t error;
  unsigned long lastReadTime;
};

#endif /*dhtesp_h*/
