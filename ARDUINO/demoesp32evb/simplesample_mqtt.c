// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdlib.h>

#include <stdio.h>
#include <stdint.h>

#include "AzureIoTHub.h"
#include "iot_configs.h"
#include "sample.h"

/*String containing Hostname, Device Id & Device Key in the format:             */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"    */
static const char* connectionString = IOT_CONFIG_CONNECTION_STRING;

BEGIN_NAMESPACE(TkEspArduinoFy18);

DECLARE_MODEL(S20180212,
WITH_DATA(ascii_char_ptr, deviceId),
WITH_DATA(ascii_char_ptr, msgType),
WITH_DATA(long, dt_epoch_s),

WITH_DATA(int, analog),
WITH_DATA(int, touch),
WITH_DATA(bool, pir)
);

END_NAMESPACE(TkEspArduinoFy18);

/*
 * 2/11/2018 8:12:27 PM> Device: [rpi3fy18], Data:[{"deviceId": "rpi3fy18","msgType":"full", "dt":"2018-02-11T18:12:26.990203", "temperature": 23.53, "pressure": 99687.09, "altitude": 137.19, "delta_total_dust_s": 1.524950000, "total_dust": 2.00, "total_dust_per_second": 1.3115, "light": 198.00, "mq_135": 27.00, "last_mean": 65.0516, "pir": 0, "action_detected": 0 }]Properties:
'temperatureAlert': 'false'
'action_detected': 'false'
 */

static char propText[1024];

void sendCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void* userContextCallback)
{
    unsigned int messageTrackingId = (unsigned int)(uintptr_t)userContextCallback;

    //(void)printf("Message Id: %u Received.\r\n", messageTrackingId);

    //(void)printf("Result Call Back Called! Result is: %s \r\n", ENUM_TO_STRING(IOTHUB_CLIENT_CONFIRMATION_RESULT, result));
}


/*this function "links" IoTHub to the serialization library*/
static IOTHUBMESSAGE_DISPOSITION_RESULT IoTHubMessage(IOTHUB_MESSAGE_HANDLE message, void* userContextCallback)
{
    IOTHUBMESSAGE_DISPOSITION_RESULT result;
    const unsigned char* buffer;
    size_t size;
    if (IoTHubMessage_GetByteArray(message, &buffer, &size) != IOTHUB_MESSAGE_OK)
    {
        printf("unable to IoTHubMessage_GetByteArray\r\n");
        result = IOTHUBMESSAGE_ABANDONED;
    }
    else
    {
        /*buffer is not zero terminated*/
        char* temp = malloc(size + 1);
        if (temp == NULL)
        {
            printf("failed to malloc\r\n");
            result = IOTHUBMESSAGE_ABANDONED;
        }
        else
        {
            (void)memcpy(temp, buffer, size);
            temp[size] = '\0';
            EXECUTE_COMMAND_RESULT executeCommandResult = EXECUTE_COMMAND(userContextCallback, temp);
            result =
                (executeCommandResult == EXECUTE_COMMAND_ERROR) ? IOTHUBMESSAGE_ABANDONED :
                (executeCommandResult == EXECUTE_COMMAND_SUCCESS) ? IOTHUBMESSAGE_ACCEPTED :
                IOTHUBMESSAGE_REJECTED;
            free(temp);
        }
    }
    return result;
}

//TK --------------------------------

static int delayms=1000;
int tkGetDelay() {
  return delayms;
}

static int shape;
int tkGetShape() {
  return shape;
}

int receiveContext = 0;

static int DeviceMethodCallback(const char* method_name, const unsigned char* payload, size_t size, unsigned char** response, size_t* resp_size, void* userContextCallback)
{
    int result=0;
    
    return result;

    (void)userContextCallback;

    printf("\r\nDevice Method called\r\n");
    printf("Device Method name:    %s\r\n", method_name);
    printf("Device Method payload: %.*s\r\n", (int)size, (const char *)payload);

    int status = 200;
    char *RESPONSE_STRING = "{ \"Response\": \"This is the response from the device\" }";

    if (strcmp(method_name, "LedOn") == 0)
    {
        RESPONSE_STRING = "{ \"Response\": \"LedOn\" }";
        shape=1;
    }
    else if (strcmp(method_name, "LedOff") == 0)
    {
        RESPONSE_STRING = "{ \"Response\": \"LedOff\" }";
        shape=0;
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
                delayms = l;
                sprintf_s(resp, sizeof(resp), "{ \"Response\": \"Delay, set to %d\" }", delayms);
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
    if ((*response = malloc(*resp_size)) == NULL)
    {
        status = -1;
    }
    else
    {
        (void)memcpy(*response, RESPONSE_STRING, *resp_size);
    }
    return status;


}


//TK
IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;
S20180212* tkMessage;

void tk_init(void) {
  if (platform_init() != 0)
  {
    (void)printf("Failed to initialize platform.\r\n");
  }
  else
  {
    if (serializer_init(NULL) != SERIALIZER_OK)
    {
        (void)printf("Failed on serializer_init\r\n");
    }
  }
  iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);
  if (iotHubClientHandle == NULL)
  {
      (void)printf("Failed on IoTHubClient_LL_Create\r\n");
  }
#ifdef SET_TRUSTED_CERT_IN_SAMPLES
  // For mbed add the certificate information
  if (IoTHubClient_LL_SetOption(iotHubClientHandle, "TrustedCerts", certificates) != IOTHUB_CLIENT_OK)
  {
      (void)printf("failure to set option \"TrustedCerts\"\r\n");
  }
#endif // SET_TRUSTED_CERT_IN_SAMPLES

  tkMessage = CREATE_MODEL_INSTANCE(TkEspArduinoFy18, S20180212);

  //if (IoTHubClient_LL_SetMessageCallback(iotHubClientHandle, IoTHubMessage, tkMessage) != IOTHUB_CLIENT_OK)
  //{
  //    printf("unable to IoTHubClient_SetMessageCallback\r\n");
  //}

  if (IoTHubClient_LL_SetDeviceMethodCallback(iotHubClientHandle, DeviceMethodCallback, &receiveContext ) != IOTHUB_CLIENT_OK)
  {
     (void)printf("Failed on IoTHubClient_SetDeviceMethodCallback\r\n");
  }

  tkMessage->deviceId = "esp32fy18olimexespevb";
  tkMessage->msgType = "S20180212";

  printf("INIT END");

}

void tksendMessage(const unsigned char* buffer, size_t size, S20180212 *msg) {
    static unsigned int messageTrackingId;
    IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromByteArray(buffer, size);
    if (messageHandle == NULL)
    {
        printf("unable to create a new IoTHubMessage\r\n");
    }
    else
    {
        MAP_HANDLE propMap = IoTHubMessage_Properties(messageHandle);
        (void)sprintf_s(propText, sizeof(propText), msg->pir ? "true" : "false");
        if (Map_AddOrUpdate(propMap, "action_detected", propText) != MAP_OK)
        {
            (void)printf("ERROR: Map_AddOrUpdate Failed!\r\n");
        }

        if (IoTHubClient_LL_SendEventAsync(iotHubClientHandle, messageHandle, sendCallback, (void*)(uintptr_t)messageTrackingId) != IOTHUB_CLIENT_OK)
        {
            printf("failed to hand over the message to IoTHubClient");
        }
        else
        {
            IOTHUB_CLIENT_STATUS status;
            IoTHubClient_LL_DoWork(iotHubClientHandle);
            ThreadAPI_Sleep(100);
            //Wait till
            while ((IoTHubClient_LL_GetSendStatus(iotHubClientHandle, &status) == IOTHUB_CLIENT_OK) && (status == IOTHUB_CLIENT_SEND_STATUS_BUSY))
            {
                IoTHubClient_LL_DoWork(iotHubClientHandle);
                ThreadAPI_Sleep(100);
            }
        }
        IoTHubMessage_Destroy(messageHandle);
    }
    messageTrackingId++;
}

void tkSendMessagePublic(int Analog, int Touch, bool PIR,int dt_epoch_s) {
  tkMessage->dt_epoch_s=dt_epoch_s;
  tkMessage->analog = Analog;
  tkMessage->pir = PIR;
  tkMessage->touch=Touch;

  unsigned char* destination;
  size_t destinationSize;
  if (SERIALIZE(&destination, &destinationSize, tkMessage->deviceId, tkMessage->msgType, tkMessage->dt_epoch_s,tkMessage->analog,tkMessage->touch,tkMessage->pir) != CODEFIRST_OK)
  {
      (void)printf("Failed to serialize\r\n");
  }
  else
  {
      tksendMessage(destination, destinationSize, tkMessage);
      free(destination);
  }
}




//WORKING

/*  
 *   
 *   Attempting to connect to SSID: Pltkap
6.6.
Connected to wifi
Eepoch time failed, retry!
NTP : 1518382765
Info: IoT Hub SDK for C, version 1.1.29

INIT END

SetSampling

"{\"waitms\":10}"

17

Error: Time:Sun Feb 11 20:59:39 2018 File:C:\Users\tkopa\Documents\Arduino\libraries\AzureIoTHub\src\sdk\commanddecoder.c Func:CommandDecoder_ExecuteMethod Line:734 Decoding JSON to a multi tree failed

failed to EXECUTE_METHOD

Error: Time:Sun Feb 11 20:59:39 2018 File:C:\Users\tkopa\Documents\Arduino\libraries\AzureIoTHub\src\sdk\iothubtransport_mqtt_common.c Func:mqtt_notification_callback Line:1233 Failure: IoTHubClient_LL_DeviceMethodComplete


 *   
 *   
 *   
 *   
 */
 /* PROBLEM WITH JSON SERIALIZER - BUG
    int result=0;
    METHODRETURN_HANDLE methodResult;
    char* payloadZeroTerminated = (char*)malloc(size + 1);
    if (payloadZeroTerminated == 0)
    {
        printf("failed to malloc\r\n");
        *resp_size = 0;
        *response = NULL;
        result = -1;
    }
    else
    {
        (void)memcpy(payloadZeroTerminated, payload, size);
        payloadZeroTerminated[size] = '\0';
        printf("\r\n");
        printf(method_name);
        printf("\r\n");
        printf(payloadZeroTerminated);
        printf("\r\n");
        printf("%d",size);
        printf("\r\n");

        //execute method - userContextCallback is of type deviceModel
        if (size>2) {
          methodResult = EXECUTE_METHOD(userContextCallback, method_name, payloadZeroTerminated);
          if (methodResult == NULL)
          {
              //EXECUTE_COMMAND(userContextCallback, method_name);
              printf("failed to EXECUTE_METHOD\r\n");
              *resp_size = 0;
              *response = NULL;
              result = -1;
          }
          else
          {
              //get the serializer answer and push it in the networking stack
              const METHODRETURN_DATA* data = MethodReturn_GetReturn(methodResult);
              if (data == NULL)
              {
                  printf("failed to MethodReturn_GetReturn\r\n");
                  *resp_size = 0;
                  *response = NULL;
                  result = -1;
              }
              else
              {
                  result = data->statusCode;
                  if (data->jsonValue == NULL)
                  {
                      char* resp = "{}";
                      *resp_size = strlen(resp);
                      *response = (unsigned char*)malloc(*resp_size);
                      (void)memcpy(*response, resp, *resp_size);
                  }
                  else
                  {
                      *resp_size = strlen(data->jsonValue);
                      *response = (unsigned char*)malloc(*resp_size);
                      (void)memcpy(*response, data->jsonValue, *resp_size);
                  }
              }
              MethodReturn_Destroy(methodResult);
          }
        } else {
          EXECUTE_COMMAND(userContextCallback, method_name);
        }
        free(payloadZeroTerminated);

    }
    */
/* 20180212 - not working
,
WITH_ACTION(Reboot),
//WITH_METHOD(SendText,ascii_char_ptr,msg),
WITH_METHOD(SetSampling, int, waitms)
*/
/*
EXECUTE_COMMAND_RESULT Reboot(S20180212* device)
{
    (void)device;
    esp_restart();
    return EXECUTE_COMMAND_SUCCESS;
}

METHODRETURN_HANDLE SendText(S20180212* device,ascii_char_ptr msg)
{
    METHODRETURN_HANDLE result = MethodReturn_Create(0, "{\"Message\":\"OK\"}");
    return result;
}

METHODRETURN_HANDLE SetSampling(S20180212* device, int waitms)
{
    (void)device;
    (void)printf("Setting sleep time to %d.\r\n", waitms);
    METHODRETURN_HANDLE result = MethodReturn_Create(0, "{\"Message\":\"OK\"}");
    return result;
}
*/

