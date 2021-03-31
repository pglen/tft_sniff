/* TFT example
 *
 *  This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 *  Unless required by applicable law or agreed to in writing, this
 *  software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied.
 *
 *  Revisions:
 *
 *     REV   DATE            BY              DESCRIPTION
 *     ----  -----------     ----------      ------------------------------
 *     0.00  feb.09.2019     Peter Glen      Initial version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "esp_pm.h"

// SNTP specific includes
#include "lwip/err.h"
#include "lwip/netif.h"
#include "lwip/apps/sntp.h"
#include "lwip/sockets.h"

#include "esp_wifi.h"
#include "wifi.h"
#include "tft_base.h"
#include "utils.h"

#if 0
// Project specific
#include "../../../common/v000/tft_base.h"
#include "../../../common/v000/utils.h"
#include "../../../common/v000/wifi.h"
#include "../../../common/v000/wcmesht.h"

// Servers specific
#include "../../../common/v000/httpd.h"
#include "../../../common/v000/captdns.h"
#include "../../../common/v000/wcrdate.h"
#include "../../../common/v000/wcmesht.h"
#endif

static char tmp[128] = { 0 }, tmp2[64] = { 0 }, tmp3[64] = { 0 };

// Hop count. Lower hop count represents better time. (other than 0)
//
// Values:
//
//  0       No time device contacted
//  1       Time device via SNTP
//  2-N     wcmesht time, number represents hop count

int     wc_hop_cnt = 0;

static const char *TAG = "TFT_Sniff";

static int  gottask = false;
//static int  synced = false;

char ststr[24] = "", stpass[24] = "";

void timeTask()

{
    ESP_LOGI(TAG, "Task for time started.\n");

    if(!is_wifi_connected())
        {
        wifi_connect();
        }

    if(!is_wifi_connected())
        {
        ESP_LOGI(TAG, "No wifi connection, cannot get internet time.\n");
        }
    else
        {
        ESP_LOGI(TAG, "Getting internet time.\n");
        internet_gettime();
        esp_wifi_disconnect();
        }

    #if 0
    // After getting time, disconnect ... did not work
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    #endif

    ESP_LOGI(TAG, "Task for time ended.\n");

    gottask = false;
    vTaskDelete(NULL);
}

#if 0

//
// Workaround for ping disconnect. Various sources. netif is a linked
// list -> query all ARP enried
//

//extern struct netif *netif_list;
//uint8_t etharp_request_dst(char *, char *, char *);
uint8_t etharp_request(char *, char *);
//struct eth_addr hw_eth_addr;
//char hw_eth_addr[6];

void forceARP()
{
    struct netif *netifx = netif_list;

    int cnt = 2;
    while (true)
        {
        if(cnt-- <=  0)
            break;

        if(netifx == NULL)
            break;

        ESP_LOGI(TAG, "Requestig arp %p\n", netifx);
        //etharp_request_dst((char*)netifx, (char*)&netifx->ip_addr, (char*)&hw_eth_addr);
        etharp_request((char*)netifx, (char*)&netifx->ip_addr);
        netifx = netifx->next;
        }

     // test IP address
    //char* test_ip = "192.168.4.1";
    //ip_addr_t test_ip = "192.168.4.1";
    //IP4_ADDR(&test_ip, 192,168,4,1);

    // do a search
    //int8_t result = etharp_request(TCPIP_ADAPTER_IF_STA, test_ip);
    //ESP_LOGI(TAG, "IP res %d\n",  result);
}

#endif

char *trans_authmod(int mode)

{
    char *ret ="UKN";
    switch(mode)
        {
        case WIFI_AUTH_OPEN:    ret = "OPEN"; break;
        case WIFI_AUTH_WEP:     ret = "WEP "; break;
        case WIFI_AUTH_WPA_PSK: ret = "WPA "; break;
        case WIFI_AUTH_WPA2_PSK:
        case WIFI_AUTH_WPA_WPA2_PSK:
        case WIFI_AUTH_WPA2_ENTERPRISE:
                                ret = "WPA2"; break;
        }
    return ret;
}

//////////////////////////////////////////////////////////////////////////

void app_main()

{
    esp_err_t ret; spi_device_handle_t spi;

    //heap_caps_check_integrity_all(true);
    //heap_caps_print_heap_info(MALLOC_CAP_8BIT);
    //heap_caps_print_heap_info(MALLOC_CAP_8BIT);

    // Allocate big block before anyone gets a chance
    lcd_pre_init();

    // Initialize NVS. Comes before everything else.
    INIT_APP_NVS;
    //tcpip_adapter_init();

    ESP_LOGI(TAG, "TFT sniff Demo =====================================\n");

    // Initialize the SPI subsys
    ESP_ERROR_CHECK(init_spi(&spi));

    //ESP_LOGI(TAG, "After TFT spi init.\n");

    // Initialize the LCD, ret has error
    ESP_ERROR_CHECK(lcd_init(spi));
    (void)ret;

    doublebuff = true;
    //doublebuff = false;
    fontback = TFT_BLACK;

    //ESP_LOGI(TAG, "After TFT init.\n");

    // Set timezone to Eastern Standard Time and print local time
    //setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1);
    //setenv("TZ", "EST+5 DST", 1);
    //tzset();

    // Cycle background colors
    uint16_t color = 0, cnt = 0;

    color = TFT_BLACK;
    //color = tft_color565(30, 30, 30);
    clear_screen(spi, color);

    ESP_LOGI(TAG, "After CLS init.\n");

    wifi_preinit();

    // Read WIFI parms
    read_wifi_config();

    // Init Wifi Subsystem
    wcwifi_sta_init(ststr, stpass);

    // Stop power down / power save
    esp_pm_config_esp32_t pm;   memset(&pm, 0, sizeof(pm));
    pm.max_freq_mhz = CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ;
    pm.min_freq_mhz = CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ;
    pm.light_sleep_enable = false;

    //ESP_ERROR_CHECK(esp_pm_configure(&pm));

    //ESP_LOGI(TAG, "After WiFi init.\n");

    draw_str(spi, (uint8_t*)"TFT WiFi Sniffer Ver 1.00", 32, 36, 1, TFT_WHITE);
    //draw_str(spi, (uint8_t*)"abcdefghijklmnopqrstuvwxyz 1234567890", 16, 10, 200, TFT_RED);
    //draw_str(spi, (uint8_t*)"ABCDEFGHIJKLMNOPQRSTUVWXYZ !@#$%^&*", 16, 10, 220, TFT_GREEN);
    //draw_str(spi, (uint8_t*)"Time NOT Synced", 32, 10, 200, TFT_RED);

    //ESP_LOGI(TAG, "After font init.\n");
    //set_promiscuous();

    int old_sec = 0;
    while(true)
        {
        time_t now; struct  tm timeinfo = { 0 };
        time(&now); localtime_r(&now, &timeinfo);

        if(old_sec !=  timeinfo.tm_sec)
            {
            old_sec =  timeinfo.tm_sec;

            snprintf(tmp2, sizeof(tmp2), "Scanning (%d) ... ", cnt++);
            draw_str(spi, (uint8_t*)tmp2, 16, 1, SCREEN_HEIGHT - 20, TFT_WHITE);

            wcwifi_scan_start();

            wcwifi_scan_get();

            #if 1
            draw_str(spi, (uint8_t*)"                     ",
                                                16, 1, SCREEN_HEIGHT - 20, TFT_WHITE);

            for(int loopc = 0; loopc < num_chs; loopc++)
                {
                if(loopc < 10)
                    {
                    snprintf(tmp2, sizeof(tmp2), "%-*.*s", 23, 23, ap_recs[loopc].ssid);

                    //tmp2[50] = '\0';

                    snprintf(tmp, sizeof(tmp), "%02d %s ",  loopc + 1, tmp2);

                    snprintf(tmp3, sizeof(tmp3), "ch%-2d %2ddB %s",
                                                    ap_recs[loopc].primary,
                                                            ap_recs[loopc].rssi,
                                                    trans_authmod(ap_recs[loopc].authmode)
                                                    );

                    //tmp[44] = '\0';             // Clip overshoot
                    int top = 34 + 18 * loopc;
                    int ppp = draw_str(spi, (uint8_t*)tmp, 16, 4, top, TFT_WHITE);
                    //(void)ppp;
                    tft_rect(spi, ppp, top, 210 - ppp, 16, TFT_BLACK);

                    int ppp2 = draw_str(spi, (uint8_t*)tmp3, 16, 205, top, TFT_WHITE);
                    //(void)ppp2;
                    //printf("ppp2=%d\n", ppp2);
                    tft_rect(spi, ppp2, top, SCREEN_WIDTH - (ppp2+2), 16, TFT_BLACK);
                    }
                if(cnt >= 100)
                    cnt = 0;
                }
            #endif

            }
        //forceARP();
        vTaskDelay(200 / portTICK_RATE_MS);
        printf("%d ", get_mem_usage()); fflush(stdout);
        }
}

// EOF











