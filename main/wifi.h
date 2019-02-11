 
/* =====[ AKOSTAR WiFi Clock project ]====================================

   File Name:       wifi.h 
   
   Description:     Wifi Related support modules

   Revisions:

      REV   DATE            BY              DESCRIPTION
      ----  -----------     ----------      ------------------------------
      0.00  mar.16.2018     Peter Glen      Initial version.

   ======================================================================= */

#define MAX_CHANNELS 10
#define PASS_SIZE 48

// Exported values

extern const int WIFI_START_BIT    ;
extern const int CONNECTED_BIT     ;
extern const int SCANNED_BIT       ;
extern const int AP_START_BIT      ;
extern const int ESPTOUCH_DONE_BIT ;
extern const int ESPTOUCH_TOUT_BIT ;
extern const int CONNECTING_BIT    ;
extern const int SCANNING_BIT      ;
extern const int AP_END_BIT        ;

extern const int WPS_DONE_BIT      ;
extern const int WPS_ERR_BIT       ;
extern const int WPS_TOUT_BIT      ;

extern EventGroupHandle_t wifi_event_group;

extern wifi_ap_record_t  aprec;

void print_wifi_bits(EventBits_t nnn);

// Expose scan specific variables and functions

extern uint16_t num_chs;
extern wifi_ap_record_t ap_recs[];

extern wifi_config_t ap_wifi_config;
extern char apname[], appass[], stname[], stpass[];
        
// Expose functions

void    wifi_preinit();
int     is_wifi_connected();
int     is_wifi_configured();
void    read_wifi_config();

int     wifi_connect();
int     wifi_disconnect();

void    wcwifi_ap_init(char *ssid_str, char *pass_str);
void    wcwifi_sta_init(char *sta_str, char *sta_pass);

int     wcwifi_scan_start(void);
void    wcwifi_scan_get(void);
void    set_promiscuous();
int     smartconfig();
void    start_wps(int delay);

// EOF





















