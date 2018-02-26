#ifndef CONFIG_H
#define CONFIG_H

// #ifdef __cplusplus
// extern "C" {
// #endif

/*String containing Hostname, Device Id & Device Key in the format:                         */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessKey=<device_key>"                */
/*  "HostName=<host_name>;DeviceId=<device_id>;SharedAccessSignature=<device_sas_token>"    */
static const char *connectionString = "...";



#define EXAMPLE_WIFI_SSID "aaa"
#define EXAMPLE_WIFI_PASS "bbb"

//If defined 
#define  TK_ETH


// #ifdef __cplusplus
// }
// #endif

#endif /* CONFIG_H */