// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>

#include "iothub_client.h"
#include "iothub_message.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/crt_abstractions.h"
#include "azure_c_shared_utility/platform.h"
#include "iothubtransportmqtt.h"
#include <driver/adc.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include <esp_system.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_partition.h"
#include "esp_log.h"
#include <sys/time.h>
#include "math.h"
#include "dht11.h"
#include "config.h"

#ifdef MBED_BUILD_TIMESTAMP
#include "certs.h"
#endif // MBED_BUILD_TIMESTAMP

static int callbackCounter;
static char msgText[1024];
static char propText[1024];
static int vald = 0;
static int DelayMs = 5000;

/*-----------------------------------------------------------------
Hardware connections
Light Sensor:

Sensor     ESP32DevKitC
A0      -> SVP, GPIO36, third on left from top (esp chip). ESP-EVB - 31
GND     -> GND, first on right
VCC     -> 3.3V, first on left
D0      -> IO23, near GND. ESP-EVB - other, need to be GPIO04 (5)
--
Led:
GND
IO33

Call Method (not Messages):
LedOn
LedOff
Reset
SetDelay {"DelayMs":5000}
-----------------------------------------------------------------*/

/*
 * Sample example for Olimexino-328 Rev.C and thermistor 10 kOhm, B=3435
 * In order to work connect: 
 * one of the outputs of thermistor to 3.3V;
 * the other one to A0;
 * A0 through 10 kOhm to GND.
 * 
 *    ^ 3.3V
 *    |
 *   _|_
 *  |NTC|
 *  |10K|
 *  |___|
 *    |
 *    |
 *    +------------ A0
 *    |
 *   _|_
 *  |   |
 *  |10K|
 *  |___|
 *    |
 * ___|___  GND
 *   ___
 * 
 * 
#define	R0	((float)10000)
#define	B	((float)3435)
// R0 = 10000 [ohm]
// B  = 3435
// T0 = 25 [C] = 298.15, [K]
// r = (ADC_MAX * R0) / (ADC_VAL) - R0
// R_ = R0 * e ^ (-B / T0), [ohm] --> const ~= 0.09919 (10K);
// T = B/ln (r/R_), [K]

  tmp = analogRead (A0);
  r = ((1023.0*R0)/(float)tmp)-R0;
  temperature = B/log(r/0.09919) - 273.15;	// log is ln in this case
*/


#define OLIMEX_REL1_PIN     32
#define OLIMEX_REL2_PIN     33
#define OLIMEX_BUT_PIN      34

//EVB 31
#define PIN_IN_ANALOG 36
//EVB 6, DHT11
#define PIN_IN_DIGITAL 05
//EVB 13
#define PIN_OUT_DIGITAL 12 

#define BLINK_COUNT 10

static IOTHUBMESSAGE_DISPOSITION_RESULT ReceiveMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContextCallback)
{
    int *counter = (int *)userContextCallback;
    const char *buffer;
    size_t size;
    MAP_HANDLE mapProperties;
    const char *messageId;
    const char *correlationId;
    const char *userDefinedContentType;
    const char *userDefinedContentEncoding;

    // Message properties
    if ((messageId = IoTHubMessage_GetMessageId(message)) == NULL)
    {
        messageId = "<null>";
    }

    if ((correlationId = IoTHubMessage_GetCorrelationId(message)) == NULL)
    {
        correlationId = "<null>";
    }

    if ((userDefinedContentType = IoTHubMessage_GetContentTypeSystemProperty(message)) == NULL)
    {
        userDefinedContentType = "<null>";
    }

    if ((userDefinedContentEncoding = IoTHubMessage_GetContentEncodingSystemProperty(message)) == NULL)
    {
        userDefinedContentEncoding = "<null>";
    }

    // Message content
    if (IoTHubMessage_GetByteArray(message, (const unsigned char **)&buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        (void)printf("unable to retrieve the message data\r\n");
    }
    else
    {
        (void)printf("Received Message [%d]\r\n Message ID: %s\r\n Correlation ID: %s\r\n Content-Type: %s\r\n Content-Encoding: %s\r\n Data: <<<%.*s>>> & Size=%d\r\n",
                     *counter, messageId, correlationId, userDefinedContentType, userDefinedContentEncoding, (int)size, buffer, (int)size);
    }

    // Retrieve properties from the message
    mapProperties = IoTHubMessage_Properties(message);
    if (mapProperties != NULL)
    {
        const char *const *keys;
        const char *const *values;
        size_t propertyCount = 0;
        if (Map_GetInternals(mapProperties, &keys, &values, &propertyCount) == MAP_OK)
        {
            if (propertyCount > 0)
            {
                size_t index;

                printf(" Message Properties:\r\n");
                for (index = 0; index < propertyCount; index++)
                {
                    (void)printf("\tKey: %s Value: %s\r\n", keys[index], values[index]);
                }
                (void)printf("\r\n");
            }
        }
    }

    /* Some device specific action code goes here... */
    (*counter)++;
    return IOTHUBMESSAGE_ACCEPTED;
}

static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *userContextCallback)
{
    //Passing address to IOTHUB_MESSAGE_HANDLE
    IOTHUB_MESSAGE_HANDLE *msg = (IOTHUB_MESSAGE_HANDLE *)userContextCallback;
    (void)printf("Confirmation %d result = %s\r\n", callbackCounter, ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
    /* Some device specific action code goes here... */
    callbackCounter++;
    IoTHubMessage_Destroy(*msg);
}

static int DeviceMethodCallback(const char *method_name, const unsigned char *payload, size_t size, unsigned char **response, size_t *resp_size, void *userContextCallback)
{
    (void)userContextCallback;

    printf("\r\nDevice Method called\r\n");
    printf("Device Method name:    %s\r\n", method_name);
    printf("Device Method payload: %.*s\r\n", (int)size, (const char *)payload);

    int status = 200;
    char *RESPONSE_STRING = "{ \"Response\": \"This is the response from the device\" }";

    if (strcmp(method_name, "LedOn") == 0)
    {
        RESPONSE_STRING = "{ \"Response\": \"LedOn\" }";
        gpio_set_level((gpio_num_t)PIN_OUT_DIGITAL, 1);
    }
    else if (strcmp(method_name, "LedOff") == 0)
    {
        RESPONSE_STRING = "{ \"Response\": \"LedOff\" }";
        gpio_set_level((gpio_num_t)PIN_OUT_DIGITAL, 0);
    }
    else if (strcmp(method_name, "Reset") == 0)
    {
        RESPONSE_STRING = "{ \"Response\": \"Reset\" }";
        esp_restart();
    }
    else if (strcmp(method_name, "SetDelay") == 0)
    {
        //TK: TODO, not working!
        //Looking for simple JSON lib for ESP32
        char resp[200];
        sprintf_s(resp, sizeof(resp), "{ \"Response\": \"JSON Like {\"DelayMs\":1} required\" }");
        //Simplification, message look like: {"DelayMs":1}, 11
        //                                   01234567890123
        //                                   {\"DelayMs\":1}
        if (strlen((const char *)payload) >= 16)
        {
            const char *num = (const char *)payload + 14;
            int l = strtol(num, NULL, 10);
            if (l > 0 && l < 100000)
            {
                DelayMs = l;
                sprintf_s(resp, sizeof(resp), "{ \"Response\": \"Delay, set to %d\" }", DelayMs);
            }
            else
            {
                sprintf_s(resp, sizeof(resp), "{ \"Response\": \"Delay, invalid number:%.*s \" }", (int)size-14, (const char *)num);
            }
        }
        RESPONSE_STRING = resp;
    }
    else
    {
        RESPONSE_STRING = "{ \"Response\": \"INVALID_COMMAND\" }";
        status = 500;
    }
    printf("\r\nResponse status: %d\r\n", status);
    printf("Response payload: %s\r\n\r\n", RESPONSE_STRING);

    *resp_size = strlen(RESPONSE_STRING);
    if ((*response = (unsigned char*)malloc(*resp_size)) == NULL)
    {
        status = -1;
    }
    else
    {
        (void)memcpy(*response, RESPONSE_STRING, *resp_size);
    }
    return status;
}

//TK: TODO
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
}

void tkSendEvents()
{
    //Setup adc
    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_11db); //Connect light sensor to 3.3 V
    int val = adc1_get_raw(ADC1_CHANNEL_0);                    //GPIO36
    //HAL
    int valh = hall_sensor_read();
    //Setup pin to read - complicated, Z:\TSGIT\2017ESP32\esp-idf\examples\peripherals\gpio\main\gpio_example_main.c
    //TODO
    //Led
    gpio_pad_select_gpio((gpio_num_t)PIN_OUT_DIGITAL);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction((gpio_num_t)PIN_OUT_DIGITAL, GPIO_MODE_OUTPUT);

    printf("blink_task - loop\n");
    for (int i = 0; i < BLINK_COUNT; i++)
    {
        gpio_set_level((gpio_num_t)PIN_OUT_DIGITAL, 0);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_level((gpio_num_t)PIN_OUT_DIGITAL, 1);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        printf("BLINK %d\n", i);
    }

    //setup IoT Hub Client
    //LL - we need to setup manually thread - no support for pthreads on ESP32 newlib
    IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;
    IOTHUB_MESSAGE_HANDLE msg;

    //For callback (counter)
    int receiveContext = 0;

    printf("Before platform_init\n");
    if (platform_init() != 0)
    {
        (void)printf("Failed to initialize the platform.\r\n");
    }
    else
    {
	    printf("Before IoTHubClient_LL_CreateFromConnectionString\n");
        if ((iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol)) == NULL)
        {
            (void)printf("ERROR: iotHubClientHandle is NULL!\r\n");
        }
        else
        {
            if (IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, ReceiveMessageCallback, &receiveContext) != IOTHUB_CLIENT_OK)
            {
                (void)printf("ERROR: IoTHubClient_LL_SetMessageCallback..........FAILED!\r\n");
            }

            if (IoTHubClient_LL_SetDeviceMethodCallback(iotHubClientHandle, DeviceMethodCallback, &receiveContext) != IOTHUB_CLIENT_OK)
            {
                (void)printf("ERROR: IoTHubClient_LL_SetDeviceMethodCallback..........FAILED!\r\n");
            }

            char buff[100];

            while (1)
            {
                val = adc1_get_raw(ADC1_CHANNEL_0); //GPIO36
                valh = hall_sensor_read();
                time_t now = time (0);
                strftime (buff, sizeof(buff), "%Y-%m-%dT%H:%M:%S.0000000Z", localtime (&now));
                //2018-02-20T10:10:25.9258370Z
                //2018-02-20T21:43:21.0000000Z

                // float vol = (val * 5.0) / 4096.0;
                // float temp = 100.0 * vol;

                float vol = ((val) * 4.9) / 4096.0;
                float temp = 10.0 * vol;
                getData(0); //Will set humidity,temperature
                sprintf_s(msgText, sizeof(msgText), 
                
"{\"deviceId\":\"ESP32_IDF_EVB\",\"msgType\":\"esp32idfevb\",\"dt\":\"%s\", \"callbackCounter\":%d, \"dhthum\":%d,  \"dhttemp\":%d, \"temp\":%f, \"data\":%d,\"datad\":%d,\"hall\":%d}",
buff,callbackCounter,
getHumidity(),getTemperature(), temp, val, vald, valh);
                (void)printf("%s\r\n", msgText);
                if ((msg = IoTHubMessage_CreateFromByteArray((const unsigned char *)msgText, strlen(msgText))) == NULL)
                {
                    (void)printf("ERROR: iotHubMessageHandle is NULL!\r\n");
                }
                else
                {

                    (void)IoTHubMessage_SetMessageId(msg, "MSG_ID");
                    (void)IoTHubMessage_SetCorrelationId(msg, "CORE_ID");
                    (void)IoTHubMessage_SetContentTypeSystemProperty(msg, "application%2Fjson");
                    (void)IoTHubMessage_SetContentEncodingSystemProperty(msg, "utf-8");

                    MAP_HANDLE propMap = IoTHubMessage_Properties(msg);
                    (void)sprintf_s(propText, sizeof(propText), val > 500 ? "true" : "false");
                    if (Map_AddOrUpdate(propMap, "valAlert", propText) != MAP_OK)
                    {
                        (void)printf("ERROR: Map_AddOrUpdate Failed!\r\n");
                    }

                    if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, msg, SendConfirmationCallback, &msg) != IOTHUB_CLIENT_OK)
                    //if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, msg, NULL, NULL) != IOTHUB_CLIENT_OK)
                    {
                        (void)printf("ERROR: IoTHubClient_SendEventAsync..........FAILED!\r\n");
                    }
                    IOTHUB_CLIENT_STATUS status;
                    IoTHubClient_LL_DoWork(iotHubClientHandle);
                    ThreadAPI_Sleep(100);
                    //Wait till
                    while ((IoTHubClient_LL_GetSendStatus(iotHubClientHandle, &status) == IOTHUB_CLIENT_OK) && (status == IOTHUB_CLIENT_SEND_STATUS_BUSY))
                    {
                        IoTHubClient_LL_DoWork(iotHubClientHandle);
                        ThreadAPI_Sleep(100);
                    }

                    //Callback is responsible for destroying message
                    //IoTHubMessage_Destroy(msg);

                    ThreadAPI_Sleep(DelayMs);
                }
            }
        }
    printf("END????\n");
        
    }
}

// //TK: Not used
// int main(void)
// {
//     return 0;
// }
