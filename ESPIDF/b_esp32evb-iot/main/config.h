#ifndef CONFIG_H
#define CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/*String containing Hostname, Device Id & Device Key in the format:                         */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"                */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessSignature=<device_sas_token>"    */
static const char *connectionString = "HostName=pltkdpepliot2016S1.azure-devices.net;DeviceId=esp32fy18olimexespevb;SharedAccessKey=KaVZX2tcCvc07zQ2ad2jfgTBcMYUVCSH4x9QNg6VGwU=";


//WIFI IF USED
// #define EXAMPLE_WIFI_SSID "Pltkap"
// #define EXAMPLE_WIFI_PASS "unfp3339#@"

#define EXAMPLE_WIFI_SSID "CBA_F1"
#define EXAMPLE_WIFI_PASS "bgh12QW2"

//If defined 
#define  TK_ETH


#ifdef __cplusplus
}
#endif

#endif /* CONFIG_H */