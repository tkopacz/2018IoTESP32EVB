// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef IOTHUB_CLIENT_SAMPLE_MQTT_H
#define IOTHUB_CLIENT_SAMPLE_MQTT_H

#ifdef __cplusplus
  extern "C" {
#endif

	void iothub_client_sample_mqtt_run(void);
	void tkSendEvents(void);


/*
 * Macro to check the outputs of TWDT functions and trigger an abort if an
 * incorrect code is returned.
 */

#define CHECK_ERROR_CODE(returned, expected) ({                        \
            if(returned != expected){                                  \
                printf("ERROR\n");                                     \
                abort();                                               \
            }                                                          \
})


#ifdef __cplusplus
}
#endif

#endif /* IOTHUB_CLIENT_SAMPLE_MQTT_H */
