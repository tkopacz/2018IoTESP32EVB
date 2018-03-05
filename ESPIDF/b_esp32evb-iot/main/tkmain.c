// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "tk_mqtt_demo.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_err.h"
#include "esp_event_loop.h"
#include "esp_event.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_eth.h"
#include "esp_wifi.h"

#include "rom/ets_sys.h"
#include "rom/gpio.h"

#include "soc/dport_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_sig_map.h"

#include "tcpip_adapter.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "soc/emac_ex_reg.h"
#include "driver/periph_ctrl.h"

#include <esp_task_wdt.h>

#include "config.h"

/* Global */
/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;
static const char *TAG = "eth_azure_iot_adc";

/* ---------------------------------------------------------------------------------------------------------------------------------------
WIFI 
*/


/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;


static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    // wifi_config_t wifi_config = {
    //     .sta = {
    //         .ssid = EXAMPLE_WIFI_SSID,
    //         .password = EXAMPLE_WIFI_PASS,
    //     },
    // };
    wifi_config_t wifi_config;
    strncpy((char*)wifi_config.sta.ssid,(char*)EXAMPLE_WIFI_SSID,sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password,(char*)EXAMPLE_WIFI_PASS,sizeof(wifi_config.sta.password));

    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}


/* ----------------------- ----------------------- ----------------------- ----------------------- ----------------------- -----------------------
    ETH -----------------------
*/
static EventGroupHandle_t eth_event_group;
ip4_addr_t ip;
ip4_addr_t gw;
ip4_addr_t msk;


#ifdef CONFIG_PHY_LAN8720
#include "eth_phy/phy_lan8720.h"
#define DEFAULT_ETHERNET_PHY_CONFIG phy_lan8720_default_ethernet_config
#endif
#ifdef CONFIG_PHY_TLK110
#include "eth_phy/phy_tlk110.h"
#define DEFAULT_ETHERNET_PHY_CONFIG phy_tlk110_default_ethernet_config
#endif

#define PIN_PHY_POWER CONFIG_PHY_POWER_PIN
#define PIN_SMI_MDC   CONFIG_PHY_SMI_MDC_PIN
#define PIN_SMI_MDIO  CONFIG_PHY_SMI_MDIO_PIN

#ifdef CONFIG_PHY_USE_POWER_PIN
/* This replaces the default PHY power on/off function with one that
   also uses a GPIO for power on/off.

   If this GPIO is not connected on your device (and PHY is always powered), you can use the default PHY-specific power
   on/off function rather than overriding with this one.
*/
static void phy_device_power_enable_via_gpio(bool enable)
{
    assert(DEFAULT_ETHERNET_PHY_CONFIG.phy_power_enable);

    if (!enable) {
        /* Do the PHY-specific power_enable(false) function before powering down */
        DEFAULT_ETHERNET_PHY_CONFIG.phy_power_enable(false);
    }

    gpio_pad_select_gpio(PIN_PHY_POWER);
    gpio_set_direction(PIN_PHY_POWER,GPIO_MODE_OUTPUT);
    if(enable == true) {
        gpio_set_level(PIN_PHY_POWER, 1);
        ESP_LOGD(TAG, "phy_device_power_enable(TRUE)");
    } else {
        gpio_set_level(PIN_PHY_POWER, 0);
        ESP_LOGD(TAG, "power_enable(FALSE)");
    }

    // Allow the power up/down to take effect, min 300us
    vTaskDelay(1);

    if (enable) {
        /* Run the PHY-specific power on operations now the PHY has power */
        DEFAULT_ETHERNET_PHY_CONFIG.phy_power_enable(true);
    }
}
#endif

static void eth_gpio_config_rmii(void)
{
    // RMII data pins are fixed:
    // TXD0 = GPIO19
    // TXD1 = GPIO22
    // TX_EN = GPIO21
    // RXD0 = GPIO25
    // RXD1 = GPIO26
    // CLK == GPIO0
    phy_rmii_configure_data_interface_pins();
    // MDC is GPIO 23, MDIO is GPIO 18
    phy_rmii_smi_configure_pins(PIN_SMI_MDC, PIN_SMI_MDIO);
}


esp_err_t eth_event_cb(void *ctx, system_event_t *event)
{
    ESP_LOGI(TAG, "eth_event_cb, event: %d", event->event_id )
    if( event->event_id == SYSTEM_EVENT_ETH_GOT_IP ) {
        ip = event->event_info.got_ip.ip_info.ip;
        gw = event->event_info.got_ip.ip_info.gw;
        msk = event->event_info.got_ip.ip_info.netmask;
        xEventGroupSetBits(eth_event_group, CONNECTED_BIT);
    }
    
    return ESP_OK;
}


void initializeETH()
{
    ESP_LOGI(TAG, "initializeETH");
    esp_err_t ret = ESP_OK;
    tcpip_adapter_init();
    //TK
    ESP_ERROR_CHECK( esp_event_loop_init(eth_event_cb, NULL) );
    
    //esp_event_loop_init(NULL, NULL);

    eth_config_t config = DEFAULT_ETHERNET_PHY_CONFIG;
    /* Set the PHY address in the example configuration */
    config.phy_addr = (eth_phy_base_t)CONFIG_PHY_ADDRESS;
    config.gpio_config = eth_gpio_config_rmii;
    config.tcpip_input = tcpip_adapter_eth_input;
    config.clock_mode = (eth_clock_mode_t)CONFIG_PHY_CLOCK_MODE;

#ifdef CONFIG_PHY_USE_POWER_PIN
    /* Replace the default 'power enable' function with an example-specific
       one that toggles a power GPIO. */
    config.phy_power_enable = phy_device_power_enable_via_gpio;
#endif

    eth_event_group = xEventGroupCreate();
    ret = esp_eth_init(&config);

    if(ret == ESP_OK) {
        esp_eth_enable();
    }
    ESP_LOGI(TAG, "initializeETH - end");
}


/* ---------------------------------------------------------------------------------------------------------------------------------------
   AZURE 
*/

void azure_task(void *pvParameter)
{
    #ifdef TK_ETH
        const TickType_t xTicksToWait = 60*1000 / portTICK_PERIOD_MS; // 60s
        ESP_LOGI(TAG, "Waiting for ETH ...");
    
        EventBits_t uxBits = xEventGroupWaitBits(eth_event_group, CONNECTED_BIT,
                            false, true, xTicksToWait);
        if (!(uxBits & CONNECTED_BIT)) {
            ESP_LOGI(TAG, "Timeout waiting for ETH, reset");
            esp_restart();
        }
        ESP_LOGI(TAG, "Connected to ETH");
    #else
        ESP_LOGI(TAG, "Waiting for WiFi access point ...");
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to access point success");
    #endif

    tkSendEvents();
}



void app_main()
{
    //Called by make menuconfig
    //CHECK_ERROR_CODE(esp_task_wdt_init(20,true),ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_init(2*360,true),ESP_OK); //1 message per 10 minutes

    esp_err_t result;
    nvs_flash_init();
    #ifdef TK_ETH
        initializeETH();
    #else
        initialise_wifi();
    #endif


    TaskHandle_t t;

    xTaskCreate(&azure_task, "azure_task", 8192, NULL, 5, NULL);


}
