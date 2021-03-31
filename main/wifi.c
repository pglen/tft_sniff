
/* =====[ AKOSTAR WiFi Clock project ]====================================

   File Name:       wifi.c

   Description:     Wifi Scan Related support modules

   Revisions:

      REV   DATE            BY              DESCRIPTION
      ----  -----------     ----------      ------------------------------
      0.00  feb.09.2019     Peter Glen      Initial version.

   ======================================================================= */

// Notes:
// 1.   Wifi firmware version: c202b34 - hangs tcp/ip for 30 sec erratically
//      ESP-IDF v3.1-dev-562-g84788230 2nd stage bootloader
//
// 2.    Wifi firmware version: ebd3e5d - same error
//       ESP-IDF v3.1-dev-402-g672f8b05 2nd stage bootloader

// STATION vs ACCESS POINT  (quote from a relevant article)
//
// Any device capable of behaving like a wireless client can be called a station.
// Connecting to other AP's, Routers etc. You don't really hear the term being
// thrown around a lot. in your case its referring to the devices ability to
// bridge a wireless network by acting like a standard client.

// This module sets the WIFI to SOFT-AP mode

// Station mode (aka STA mode or WiFi client mode). ESP32 connects to an access point.
// AP mode (aka Soft-AP mode or Access Point mode). Stations connect to the ESP32.
// Combined AP-STA mode (ESP32 is concurrently an access point and a station
// connected to another access point).

// We do most of the semaphore ops even if the semaphore fails ...
// ... predictable data concept

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_smartconfig.h"
#include "esp_wps.h"

// Networking
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"

#include "utils.h"
#include "wifi.h"

// Reach back to the project root
//#include "../../wclock/main/wclock.h"

#ifndef PIN2STR
#define PIN2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7]
#define PINSTR "%c%c%c%c%c%c%c%c"
#endif

#define CREATE_SEMA(xSemaphore2)                                    \
        vSemaphoreCreateBinary( xSemaphore2 );                      \

#define TAKE_SEMA(xSemaphore2, xtag, ticks)                         \
        if(!xSemaphoreTake( xSemaphore2, ( TickType_t ) ticks ))    \
            {                                                       \
            ESP_LOGE(xtag, "Could not take semaphore.");            \
            }                                                       \

#define GIVE_SEMA(xSemaphore2)                                      \
        xSemaphoreGive( xSemaphore2 );                              \


static SemaphoreHandle_t iSemaphore  = NULL;

// -----------------------------------------------------------------------
// Exposed variables

static int wifi_connected = false;
static int wifi_configured = false;

// Exported values

const int WIFI_START_BIT    = BIT0;
const int CONNECTED_BIT     = BIT1;
const int SCANNED_BIT       = BIT2;
const int AP_START_BIT      = BIT3;
const int ESPTOUCH_DONE_BIT = BIT4;
const int ESPTOUCH_TOUT_BIT = BIT5;
const int CONNECTING_BIT    = BIT6;
const int SCANNING_BIT      = BIT7;
const int AP_END_BIT        = BIT8;

const int WPS_DONE_BIT      = BIT10;
const int WPS_ERR_BIT       = BIT11;
const int WPS_TOUT_BIT      = BIT12;

//const int STA_START_BIT     = BIT0;     // Alias, not used

/*
    The event group allows multiple bits for each event,
    but we only care about some events -
    Are we connected to the AP with an IP?
    Anyone requested the STA IP?
    Is init done?
    Is WPS done?
 */

EventGroupHandle_t wifi_event_group;

uint16_t num_chs = 0;
wifi_ap_record_t ap_recs[MAX_CHANNELS + 1];

//char *apname = NULL, *appass = NULL, *stname = NULL, *stpass = NULL;
char apname[PASS_SIZE], appass[PASS_SIZE], stname[PASS_SIZE], stpass[PASS_SIZE];

// Private to this module

static const char *TAG = "WiFiSubsys";
static  int inited = 0;

wifi_ap_record_t  aprec;

int trig_started = 0;

//////////////////////////////////////////////////////////////////////////
// CONFIG

#if CONFIG_WIFI_ALL_CHANNEL_SCAN
#define DEFAULT_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#elif CONFIG_WIFI_FAST_SCAN
#define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
#else
#define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
#endif /*CONFIG_SCAN_METHOD*/

#if CONFIG_WIFI_CONNECT_AP_BY_SIGNAL
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#elif CONFIG_WIFI_CONNECT_AP_BY_SECURITY
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY
#else
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#endif /*CONFIG_SORT_METHOD*/

#if CONFIG_FAST_SCAN_THRESHOLD
#define DEFAULT_RSSI CONFIG_FAST_SCAN_MINIMUM_SIGNAL
#if CONFIG_EXAMPLE_OPEN
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#elif CONFIG_EXAMPLE_WEP
#define DEFAULT_AUTHMODE WIFI_AUTH_WEP
#elif CONFIG_EXAMPLE_WPA
#define DEFAULT_AUTHMODE WIFI_AUTH_WPA_PSK
#elif CONFIG_EXAMPLE_WPA2
#define DEFAULT_AUTHMODE WIFI_AUTH_WPA2_PSK
#else
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#endif
#else
#define DEFAULT_RSSI -127
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#endif /*CONFIG_FAST_SCAN_THRESHOLD*/

wifi_config_t wifi_config = {
        .sta = {
            //.ssid = DEFAULT_SSID,
            //.password = DEFAULT_PWD,
            // Porting to new location (remember com port)
            .ssid="guest",
            .password="12345678",
            .scan_method = DEFAULT_SCAN_METHOD,
            .sort_method = DEFAULT_SORT_METHOD,
            .threshold.rssi = DEFAULT_RSSI,
            .threshold.authmode = DEFAULT_AUTHMODE,
            },
    };

    wifi_config_t ap_wifi_config = {
        .ap = {
                .ssid="WiFiClock",
                .password="12345678",
                .ssid_len = 0,
                .channel = 0,
                .authmode= WIFI_AUTH_WPA_WPA2_PSK,
                //.authmode= WIFI_AUTH_OPEN,
                .ssid_hidden = 0,
                .max_connection = 4,
                .beacon_interval = 1000,
                }
    };

    wifi_config_t wifi_smart_config;

//////////////////////////////////////////////////////////////////////////
//

void    print_wifi_bits(EventBits_t nnn)

{
    if(nnn & WIFI_START_BIT) printf("%s ",     "WIFI_START_BIT"    );
    if(nnn & CONNECTED_BIT) printf("%s ",      "CONNECTED_BIT"     );
    if(nnn & SCANNED_BIT) printf("%s "  ,      "SCANNED_BIT"       );
    if(nnn & AP_START_BIT) printf("%s " ,      "AP_START_BIT"      );
    if(nnn & ESPTOUCH_DONE_BIT) printf("%s ",  "ESPTOUCH_DONE_BIT" );
    if(nnn & ESPTOUCH_TOUT_BIT) printf("%s ",  "ESPTOUCH_TOUT_BIT" );
    if(nnn & CONNECTING_BIT) printf("%s "   ,  "CONNECTING_BIT"    );
    if(nnn & SCANNING_BIT) printf("%s "     ,  "SCANNING_BIT"      );

    printf("\n");
}

//////////////////////////////////////////////////////////////////////////
//

static esp_err_t event_handler(void *ctx, system_event_t *event)

{
    //ESP_LOGI(TAG, "Got WiFI event: ctx -> %p event_id: %d", ctx, event->event_id);

    switch (event->event_id) {

        case SYSTEM_EVENT_STA_START:
            //ESP_LOGI(TAG, "SYSTEM_EVENT_STA_START");
            //ESP_ERROR_CHECK(esp_wifi_connect());
            xEventGroupSetBits(wifi_event_group, WIFI_START_BIT);
            xEventGroupClearBits(wifi_event_group, AP_END_BIT);
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
            //ESP_LOGI(TAG, "Got IP: %s",
            //         ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
            wifi_connected = true;
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_DISCONNECTED");
            wifi_connected = false;
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);

            // Reconnect to wifi if disconnected
            //ESP_ERROR_CHECK(esp_wifi_connect());
            break;

        case SYSTEM_EVENT_SCAN_DONE:
            //ESP_LOGI(TAG, "SYSTEM_EVENT_SCAN_DONE");
            xEventGroupSetBits(wifi_event_group, SCANNED_BIT);
            xEventGroupClearBits(wifi_event_group, SCANNING_BIT);
            break;

        case SYSTEM_EVENT_AP_START:
            xEventGroupSetBits(wifi_event_group, AP_START_BIT);
            break;

        case SYSTEM_EVENT_STA_STOP:
            xEventGroupClearBits(wifi_event_group, WIFI_START_BIT);
            break;

        case SYSTEM_EVENT_AP_STOP:
            xEventGroupClearBits(wifi_event_group, AP_START_BIT);
            break;

        case SYSTEM_EVENT_STA_LOST_IP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_LOST_IP");
            xEventGroupSetBits(wifi_event_group, AP_END_BIT);
            break;

        case SYSTEM_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STACONNECTED");

            #if 0
            // This got obsolete, TCP connection maintained across
            // reboots and disconnects
            struct station_info stations;
            ESP_ERROR_CHECK(esp_wifi_get_station_list(&stations));
            struct station_list infoList;
            ESP_ERROR_CHECK(tcpip_adapter_get_sta_list(stations, &infoList));
            //struct station_list *head = infoList;
            while(infoList != NULL) {
                printf("mac: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x " IPSTR " %d\n",
                   infoList->mac[0],infoList->mac[1],infoList->mac[2],
                   infoList->mac[3],infoList->mac[4],infoList->mac[5],
                   IP2STR(&(infoList->ip)),
                   (uint32_t)(infoList->ip.addr));
                infoList = STAILQ_NEXT(infoList, next);
                }
            //ESP_ERROR_CHECK(esp_adapter_free_sta_list(head));
            //ESP_ERROR_CHECK(esp_wifi_free_station_list());

            // Make sure that the system settles
            //vTaskDelay(100 / portTICK_PERIOD_MS);

            // Start DNS Server
            //captdnsInit();
            //ESP_LOGI(TAG, "Started DNS server.");

            // Start HTTP Server
            //init_httpd(event->event_info.sta_connected.aid);
            //ESP_LOGI(TAG, "Started HTTP server on %d.", event->event_info.sta_connected.aid);
            #endif

            break;

        case SYSTEM_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_AP_STADISCONNECTED");

            #if 0
            // Terminate HTTP Server and DNS server
            deinit_httpd(event->event_info.sta_disconnected.aid);
            ESP_LOGI(TAG, "Terminated HTTP server.");

            captdnsdeInit();
            ESP_LOGI(TAG, "Terminated DNS server on %d.", event->event_info.sta_disconnected.aid);
            #endif
            break;

        case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
            /*
             * the function esp_wifi_wps_start() only get ssid & password
             * so call the function esp_wifi_connect() here
             */
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_SUCCESS");
            ESP_ERROR_CHECK(esp_wifi_wps_disable());
            xEventGroupSetBits(wifi_event_group, WPS_DONE_BIT);
            //ESP_ERROR_CHECK(esp_wifi_connect());
            break;

        case SYSTEM_EVENT_STA_WPS_ER_FAILED:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_FAILED");
            //ESP_ERROR_CHECK(esp_wifi_wps_disable());
            //ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
            //ESP_ERROR_CHECK(esp_wifi_wps_start(0));
            xEventGroupSetBits(wifi_event_group, WPS_ERR_BIT);
            break;

        case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_TIMEOUT");
            //ESP_ERROR_CHECK(esp_wifi_wps_disable());
            //ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
            //ESP_ERROR_CHECK(esp_wifi_wps_start(0));
            xEventGroupSetBits(wifi_event_group, WPS_TOUT_BIT);
            break;

        case SYSTEM_EVENT_STA_WPS_ER_PIN:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_WPS_ER_PIN");
            /*show the PIN code here*/
            ESP_LOGI(TAG, "WPS_PIN = "PINSTR, PIN2STR(event->event_info.sta_er_pin.pin_code));

        default:
            break;
    }
    return ESP_OK;
}

///////////////////////////////////////////////////////////////////////////
// Save the fact that we got a WPS config

static int     write_wps_flag(int fff)

{
    char wstr[12];
    nvs_handle my_handle;
    err_t err, ret = 0;

    itoa(fff, wstr, 10);
    ESP_LOGI(TAG, "Write WPS flag: '%s'\n", wstr);

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%d) opening NVS handle!", err);
        ret = -1;
        goto err2;
        }
    err = nvs_set_str(my_handle, "wps", wstr);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%d) writing WPS flag NVS!", err);
        ret = -1;
        goto err2;
        }

    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%d) on committing WPS flag NVS!", err);
        ret = -1;
        goto err2;
        }

    ESP_LOGI(TAG, "WPS flag = '%s'", wstr);

   err2:
    // Close
    nvs_close(my_handle);

    return ret;
}

///////////////////////////////////////////////////////////////////////////
// return false for no flag, and 0 flag

static int     read_wps_flag()

{
    char wstr[12];
    nvs_handle my_handle;
    err_t err;

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%d) opening NVS handle!", err);
        return 0;
        }
    unsigned int slen = sizeof(wstr);
    err = nvs_get_str(my_handle, "wps", wstr, &slen);
    if (err != ESP_OK) {
        //ESP_LOGI(TAG, "Warn (%d) reading wps flag from NVS!", err);
        nvs_close(my_handle);
        // If there was no WPS entry, assume off
        return 0;
        }
    ESP_LOGI(TAG, "WPS flag = '%s'", wstr);

    // Close
    nvs_close(my_handle);

    //ESP_LOGI(TAG, "Read WPS flag %d\n", atoi(wstr));
    return(atoi(wstr));
}

/*
 * Initialize Wi-Fi as access point
 */

void    wcwifi_ap_init(char *ssid_str, char *pass_str)

{
    if(inited == 0)
        {
        ESP_LOGI(TAG, "Please call wifi_preinit first.");
        return;
        }

    if(ssid_str[0] != '\0' && strlen(pass_str) >= 8)
        {
        strncpy((char*)ap_wifi_config.ap.password, pass_str,
                                    sizeof(ap_wifi_config.ap.password)-1);

        strncpy((char*)ap_wifi_config.ap.ssid,  ssid_str,
                                    sizeof(ap_wifi_config.ap.ssid)-1);
        }
    else
        {
        // Keep original settings
        }

    ESP_LOGI(TAG, "Init AP: '%s' '%s'",
                                (char*)ap_wifi_config.ap.ssid,
                                    (char*)ap_wifi_config.ap.password);

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_wifi_sta_get_ap_info(&aprec);
}

/*
 * Initialize Wi-Fi station
 */

void    wcwifi_sta_init(char *sta_str, char *sta_pass)

{
    if(inited == 0)
        {
        ESP_LOGI(TAG, "Please call wifi_preinit first.");
        return;
        }

    if(read_wps_flag())
        {
        // Do nothing
        ESP_LOGI(TAG, "Using WPS configured parameters.\n");
        //memset(wifi_config.sta.ssid,  0, sizeof(wifi_config.sta.ssid));
        //memset(wifi_config.sta.password,  0, sizeof(wifi_config.sta.password));
        }
    else if(sta_str[0] != '\0')
        {
        ESP_LOGI(TAG, "Using HTTP configured parameters");

        strncpy((char*)wifi_config.sta.ssid,  sta_str,
                                    sizeof(wifi_config.sta.ssid)-1);
        strncpy((char*)wifi_config.sta.password, sta_pass,
                                    sizeof(wifi_config.sta.password)-1);

        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        }
    else
        {
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        }

    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_sta_get_ap_info(&aprec);
}

int     wifi_disconnect()

{
    ESP_LOGI(TAG, "Disconnecting from: '%s' pass: '%s'\n",
                    wifi_config.sta.ssid, wifi_config.sta.password);

    if(!is_wifi_connected())
        {
        ESP_LOGI(TAG, "Not connected, cannot disconnect\n");
        return -1;
        }
    xEventGroupClearBits(wifi_event_group, CONNECTING_BIT);

    esp_wifi_disconnect();

    return 0;
}

//////////////////////////////////////////////////////////////////////////
// Connect our station to access point

int     wifi_connect()

{
    ESP_LOGI(TAG, "Connecting to: '%s' pass: '%s'\n",
                    wifi_config.sta.ssid, wifi_config.sta.password);

    EventBits_t uxBits = 0;
    uxBits = xEventGroupWaitBits(wifi_event_group, SCANNING_BIT,
                                  false, true, 20 / portTICK_PERIOD_MS);

    //ESP_LOGI(TAG, "wifi_connect() uxBits %x\n", uxBits);

    if(uxBits & SCANNING_BIT)
        {
        ESP_LOGI(TAG, "Cannot connect, scanning in progress ... \n");

        // Allow it to break in the second time
        //xEventGroupClearBits(wifi_event_group, SCANNING_BIT);

        return wifi_connected;
        }

    xEventGroupSetBits(wifi_event_group, CONNECTING_BIT);

    ESP_ERROR_CHECK(esp_wifi_connect());

    // Wait for connect, max 15 sec
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, 15000 / portTICK_PERIOD_MS);

    xEventGroupClearBits(wifi_event_group, CONNECTING_BIT);

    esp_wifi_sta_get_ap_info(&aprec);

    ESP_LOGI(TAG, "Connected to: '%s' channel=%d strenth=%d dB\n",
                            aprec.ssid, aprec.primary, aprec.rssi);
    return wifi_connected;
}

//////////////////////////////////////////////////////////////////////////

static void  wcwifi_scan_task(void * parm)

{
    static wifi_scan_config_t scfg;

    memset(&scfg, 0, sizeof(scfg));
    scfg.show_hidden = TRUE;

    // Prevent re-scan if the data less than 30 sec. old
    // (disabled it)
    //int ttt = time(NULL);
    //if(ttt - old_scan < 30)
    //    return;
    //old_scan = ttt;

    while(true)
        {
        TAKE_SEMA(iSemaphore, TAG, portMAX_DELAY);

        xEventGroupClearBits(wifi_event_group, SCANNED_BIT);

        ESP_LOGI(TAG, "Started WiFi scan, setting scanning bit ...");

        xEventGroupSetBits(wifi_event_group, SCANNING_BIT);
        trig_started = true;
        esp_wifi_scan_start(&scfg, true);
        trig_started = false;

        ESP_LOGI(TAG, "Started WiFi scan, re setting scanning bit ...");
        xEventGroupClearBits(wifi_event_group, SCANNING_BIT);

        vTaskDelay(50 / portTICK_PERIOD_MS);
        }

    //ESP_LOGI(TAG, "Ended WiFi scan.");

    vTaskDelete(NULL);
}

int     wcwifi_scan_start(void)
{

    int ret = 0;

    EventBits_t uxBits = 0;
    uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTING_BIT,
                                  false, true, 10 / portTICK_PERIOD_MS);

    //ESP_LOGI(TAG, "wcwifi_scan_start() uxBits %x\n", uxBits);

    if(uxBits & CONNECTING_BIT)
        {
        // Signal it could not start scan
        ESP_LOGI(TAG, "Connecting bit set.\n");
        ret = -1;
        }
    else
        {
        GIVE_SEMA(iSemaphore);
        }
    return 0;
}

//////////////////////////////////////////////////////////////////////////
// Get the result of the scan ...

void    wcwifi_scan_get(void)

{
    xEventGroupWaitBits(wifi_event_group, SCANNED_BIT,
                        false, true, 5000 / portTICK_PERIOD_MS);

    xEventGroupClearBits(wifi_event_group, SCANNED_BIT);

    // Just to be sure
    //xEventGroupClearBits(wifi_event_group, SCANNING_BIT);

    esp_wifi_scan_get_ap_num(&num_chs);

    ESP_LOGI(TAG, "Got %d channels", num_chs);
    memset(ap_recs, 0, sizeof(ap_recs));
    num_chs = MIN(MAX_CHANNELS, num_chs);
    ESP_ERROR_CHECK(
        esp_wifi_scan_get_ap_records(&num_chs, ap_recs));

    for(int loopc = 0; loopc < num_chs; loopc++)
        {
        ESP_LOGI(TAG, "Got AP '%s' (%s) on channel %d auth: %d signal: %d ",
                                ap_recs[loopc].ssid,
                                    ap_recs[loopc].bssid,
                                        ap_recs[loopc].primary,
                                            ap_recs[loopc].authmode,
                                                ap_recs[loopc].rssi
                                        );
        printauth(ap_recs[loopc].authmode);

        //ESP_LOGI(TAG, "AP '%s' ch: %d ",
        //                        ap_recs[loopc].ssid,
        //                                ap_recs[loopc].primary
        //                                );
        // Feed watchdog
        }
    FEED_DOG
    trig_started = false;
    //ESP_LOGI(TAG, "Done scan.");
}

//////////////////////////////////////////////////////////////////////////
// Read to globals in this module

void    read_wifi_config()

{
    nvs_handle my_handle;
    err_t err;

    if(inited == 0)
        {
        ESP_LOGI(TAG, "Warining call wifi_preinit first.");
        wifi_preinit();
        }
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%d) opening NVS handle!", err);
        return;
        }
    apname[0] = 0;
    unsigned int slen = PASS_SIZE;
    err = nvs_get_str(my_handle, "apname", apname, &slen);
    if (err != ESP_OK) {
        //ESP_LOGI(TAG, "Error (%d) reading apname from NVS!", err);
        }
    ESP_LOGI(TAG, "Config AP = '%s'", apname);

    appass[0] = 0;
    unsigned int plen = PASS_SIZE;
    err = nvs_get_str(my_handle, "appass", appass, &plen);
    if (err != ESP_OK) {
        //ESP_LOGI(TAG, "Error (%d) reading cpass from NVS!", err);
        }
    ESP_LOGI(TAG, "Config PASS = '%s'", appass);

    // ----------------------------------------------------
    stname[0] = 0;
    unsigned int stlen = PASS_SIZE;
    err = nvs_get_str(my_handle, "stname", stname, &stlen);
    if (err != ESP_OK) {
        //ESP_LOGI(TAG, "Error (%d) reading stname from NVS!", err);
        }
    ESP_LOGI(TAG, "Config ST = '%s'", stname);

    stpass[0] = 0;
    unsigned int pslen = PASS_SIZE;
    err = nvs_get_str(my_handle, "stpass", stpass, &pslen);
    if (err != ESP_OK) {
        //ESP_LOGI(TAG, "Error (%d) reading spass from NVS!", err);
        }
    ESP_LOGI(TAG, "Config ST PASS = '%s'", stpass);

    // Close
    nvs_close(my_handle);
}

// -------------------------------------------------------------------
// Allocate stuctures etc ... call this before eny other

void    wifi_preinit()

{
    apname[0] = '\0';     appass[0] = '\0';
    stname[0] = '\0';     stpass[0] = '\0';

    wifi_event_group = xEventGroupCreate();

    //xEventGroupClearBits(wifi_event_group, 0xffff);

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = true;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // It defaults to APSTA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    //ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    // Disable WiFi power manager
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    // Decorate current default with mac addr[5]

    // Copy in AP params use mac address to make it unique
    uint8_t mac[6] = { 0 };
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);
    //ESP_LOGI(TAG,"sta mac: %.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
    //              mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    char tmp[8];
    snprintf(tmp, sizeof(tmp), "_%02x%02x",  mac[4] & 0xff, mac[5] & 0xff);
    strcat((char*)ap_wifi_config.ap.ssid, tmp);
    ESP_LOGI(TAG,"Decorated sta ssid %s", ap_wifi_config.ap.ssid);

    xTaskCreate(wcwifi_scan_task, "wcwifi_scan_task", 4096, NULL, 3, NULL);
    CREATE_SEMA(iSemaphore); TAKE_SEMA(iSemaphore, TAG, portMAX_DELAY);

    inited = 1;
}

int     is_wifi_configured()

{
    return(wifi_configured);
}

int is_wifi_connected()

{
    return(wifi_connected);
}

#if 0

char tmp[64];

#define HEX_COLSIZE 16


void    promisc_callback(void* buf, wifi_promiscuous_pkt_type_t ptype)

{
    wifi_promiscuous_pkt_t *ptr = (wifi_promiscuous_pkt_t*)buf;

    if(ptype !=  WIFI_PKT_MGMT)
        return;

    ESP_LOGI(TAG, "packet len: %d\n", ptr->rx_ctrl.sig_len);
    time_t ttt = ptr->rx_ctrl.timestamp;
    ESP_LOGI(TAG, "timestamp: %ld\n", ttt);

    struct  tm timeinfo = { 0 };
    localtime_r(&ttt, &timeinfo);

    snprintf(tmp, sizeof(tmp),
        "%s, %02d/%02d/%04d", convday(timeinfo.tm_wday), timeinfo.tm_mday,
                 timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
    ESP_LOGI(TAG, "%s\n", tmp);

    #if 0
    // RAW packet
    for (char* ptr2 = (char*)ptr->payload; ptr2 < buflen; ptr2++)
        {
        if(isalnum((int)*ptr2))
            ESP_LOGI(TAG, "%c", *ptr2);
        else
           ESP_LOGI(TAG, ".");
        }
    ESP_LOGI(TAG, "\n\n");
    #endif

    #if 0
    uint8_t hexdump_cols = 0;
    uint8_t offset = 0;
    char* buflen = (char *)buf + ptr->rx_ctrl.sig_len;
    // Hexdump (wireshark-friendly)
    for (char* ptr = buf; ptr < buflen; ptr+=HEX_COLSIZE) {
        // print offset
        ESP_LOGI(TAG, " %06X ", offset);

        for (hexdump_cols=0; hexdump_cols < HEX_COLSIZE; hexdump_cols++)
            ESP_LOGI(TAG, " %02X", *(ptr+hexdump_cols*sizeof(char)));

        offset = offset + HEX_COLSIZE;
        ESP_LOGI(TAG, "\n");
    }
    ESP_LOGI(TAG, "\n\n");
    #endif
}

void   set_promiscuous()

{
    esp_wifi_set_promiscuous_rx_cb(promisc_callback);
    esp_wifi_set_promiscuous(true);
}

#endif

#if 0
static void     sc_callback(smartconfig_status_t status, void *pdata)

{
    ESP_LOGI(TAG, "smartconfig event: %d (%x) %p", status, status, pdata);

    switch (status) {

        case SC_STATUS_WAIT:
            ESP_LOGI(TAG, "SC_STATUS_WAIT");
            break;

        case SC_STATUS_FIND_CHANNEL:
            ESP_LOGI(TAG, "SC_STATUS_FINDING_CHANNEL %p", pdata);
            break;

        case SC_STATUS_GETTING_SSID_PSWD:
            ESP_LOGI(TAG, "SC_STATUS_GETTING_SSID_PSWD");
            break;

        case SC_STATUS_LINK:
            ESP_LOGI(TAG, "SC_STATUS_LINK");
            wifi_config_t *wifi_config = pdata;
            memcpy(&wifi_smart_config, pdata, sizeof(wifi_smart_config));
            ESP_LOGI(TAG, "SSID:%s", wifi_config->sta.ssid);
            ESP_LOGI(TAG, "PASSWORD:%s", wifi_config->sta.password);
            ESP_ERROR_CHECK( esp_wifi_disconnect() );
            ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_config) );
            ESP_ERROR_CHECK( esp_wifi_connect() );
            break;

        case SC_STATUS_LINK_OVER:
            ESP_LOGI(TAG, "SC_STATUS_LINK_OVER");
            if (pdata != NULL) {
                uint8_t phone_ip[4] = { 0 };
                memcpy(phone_ip, (uint8_t* )pdata, 4);
                ESP_LOGI(TAG, "Phone ip: %d.%d.%d.%d", phone_ip[0], phone_ip[1], phone_ip[2], phone_ip[3]);
            }
            xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
            break;
        default:
            break;
    }
}

#endif

static int scinited = 0;

static void     smartconfig_task(void * parm)

{
    EventBits_t uxBits;

    if(! scinited)
        {
        scinited = true;
        //ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
        //ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_AIRKISS) );
        //ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_AIRKISS) );
        ESP_ERROR_CHECK( esp_esptouch_set_timeout(40));
        }

    //ESP_ERROR_CHECK( esp_smartconfig_start(sc_callback) );

    while (1)
        {
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT |
                        ESPTOUCH_DONE_BIT | ESPTOUCH_TOUT_BIT, true, false, portMAX_DELAY);

        if(uxBits & CONNECTED_BIT) {
            wifi_connected = true;
            wifi_configured = true;
            ESP_LOGI(TAG, "WiFi Connected to ap ");
            }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "smartconfig over ");
            break;
            }
        if(uxBits & ESPTOUCH_TOUT_BIT) {
            ESP_LOGI(TAG, "smartconfig timeout ");
            break;
            }
        }

    esp_smartconfig_stop();
    vTaskDelete(NULL);
}

//////////////////////////////////////////////////////////////////////////////
// Start Smartconfig process.

int     smartconfig()

{
    ESP_LOGI(TAG, "Smartconfig version %s\n", esp_smartconfig_get_version());

    xTaskCreate(smartconfig_task, "smartconfig_task", 4096, NULL, 3, NULL);

    // Will wait ...
    xEventGroupWaitBits(wifi_event_group, ESPTOUCH_DONE_BIT,
                        false, true, 45000 / portTICK_PERIOD_MS);

    if(!wifi_configured)
        {
        xEventGroupSetBits(wifi_event_group, ESPTOUCH_TOUT_BIT);
        }

    ESP_LOGI(TAG, "Returned from smartconfig %d\n", wifi_configured);
    return wifi_configured;
}

extern void    input_init();

//////////////////////////////////////////////////////////////////////////

bool wps_cb (int status)

{
    ESP_LOGI(TAG, "WPS clallback %d\n", status);
    return 0;
}

//////////////////////////////////////////////////////////////////////////
// This decorates the request for the WPS request.

#define ESP_WPS_MODE      WPS_TYPE_PBC
#define ESP_MANUFACTURER  "AKOSTAR"
#define ESP_MODEL_NUMBER  "ESP32"
#define ESP_MODEL_NAME    "WIFI CLOCK"
#define ESP_DEVICE_NAME     "ESP WIFI STATION"

static esp_wps_config_t config = WPS_CONFIG_INIT_DEFAULT(WPS_TYPE_PBC);

void wpsInitConfig()

{
#if 0
  config.crypto_funcs = &g_wifi_default_wps_crypto_funcs;
  config.wps_type = ESP_WPS_MODE;
  strcpy(config.factory_info.manufacturer, ESP_MANUFACTURER);
  strcpy(config.factory_info.model_number, ESP_MODEL_NUMBER);
  strcpy(config.factory_info.model_name, ESP_MODEL_NAME);
  strcpy(config.factory_info.device_name, ESP_DEVICE_NAME);
  #endif
}

//////////////////////////////////////////////////////////////////////////

static void    wps_task(void *pvParameters)

{
    int delay = (int)pvParameters;

    ESP_LOGI(TAG, "start wps task ... delay %d", delay);

    if(delay)
        vTaskDelay(delay / portTICK_PERIOD_MS);

    int cnt = 20;

    //show_error(false, cnt, false);

    wifi_config_t wifi_config2;
    memcpy(&wifi_config2, &wifi_config, sizeof(wifi_config2));

    //printf("ssid '%s' pass '%s'\n", (char*)wifi_config.sta.ssid,
    //                    (char*)wifi_config.sta.password);

    esp_wifi_disconnect();
    vTaskDelay(200 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(esp_wifi_stop());
    vTaskDelay(200 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(esp_wifi_deinit());
    vTaskDelay(200 / portTICK_PERIOD_MS);

    // New life for WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = true;

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    vTaskDelay(200 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    vTaskDelay(200 / portTICK_PERIOD_MS);

    EventBits_t uxBits;

    wpsInitConfig();
    ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
    ESP_ERROR_CHECK(esp_wifi_wps_start(0));

    while (1)
        {
        //show_error(false, cnt, false);
        uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT |
                WPS_DONE_BIT | WPS_TOUT_BIT | WPS_ERR_BIT, true, false,
                                    //portMAX_DELAY);
                                    1000 / portTICK_PERIOD_MS);

        if((uxBits & WPS_DONE_BIT) != 0)
            {
            break;
            }
        if(cnt-- <= 0)
            {
            ESP_LOGI(TAG, "\n");
            break;
            }
        }

    if((uxBits & WPS_DONE_BIT) != 0)
        {
        ESP_LOGI(TAG, "Got WPS information.");

        // Save new parameters
        //printf("post ssid '%s' pass '%s'\n", (char*)wifi_config.sta.ssid,
        //                (char*)wifi_config.sta.password);

        write_wps_flag(true);
        ESP_ERROR_CHECK(esp_wifi_connect());
        //show_error(false, 44, false);

        //wifi_ap_record_t apr;
        //esp_wifi_sta_get_ap_info(&apr);

        }
    else
        {
        write_wps_flag(false);

        // If WPS failed, restart the whole thing
        //show_error(true, 99, true);

        // If WPS failed, restart WiFi
        ESP_LOGI(TAG, "Could not obtain WPS information.");

        // We do not mess with the failed WiFi, simply reboot
        esp_wifi_disconnect();
        vTaskDelay(200 / portTICK_PERIOD_MS);

        ESP_ERROR_CHECK(esp_wifi_stop());
        vTaskDelay(200 / portTICK_PERIOD_MS);

        ESP_ERROR_CHECK(esp_wifi_deinit());
        vTaskDelay(200 / portTICK_PERIOD_MS);

        #if 0
        // New life for the device
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));
        vTaskDelay(200 / portTICK_PERIOD_MS);

        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        vTaskDelay(200 / portTICK_PERIOD_MS);
        #endif
        }
    ESP_LOGI(TAG, "end wps...");
    vTaskDelete(NULL);
}

//////////////////////////////////////////////////////////////////////////
// Pushed into a task ...

void start_wps(int delay)

{
    //ESP_LOGI(TAG, "start wps...");
    xTaskCreate(wps_task, "wps_task", 4096, (void *)delay, 2, NULL);
}

// EOF
















