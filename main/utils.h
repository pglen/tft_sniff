
/* =====[ AKOSTAR WiFi Clock project ]====================================

   File Name:       utils.h
   
   Description:     

   Revisions:

      REV   DATE            BY              DESCRIPTION
      ----  -----------     ----------      ------------------------------
      0.00  mar.01.2018     Peter Glen      Initial version.
      0.00  jan.30.2018     Peter Glen      Moved wclock utils here

   ======================================================================= */

// Misc defines for ESP32 projects

// Macro to initialize NVS

#define INIT_APP_NVS    {                               \
        esp_err_t retf = nvs_flash_init();              \
            if (retf == ESP_ERR_NVS_NO_FREE_PAGES) {    \
                ESP_ERROR_CHECK(nvs_flash_erase());     \
                retf = nvs_flash_init();                \
            }                                           \
            ESP_ERROR_CHECK( retf );  }                 \

// Feed watchdog
        
#define FEED_DOG vTaskDelay(10/portTICK_RATE_MS);

#ifndef TRUE
#define TRUE  (1==1)
#define FALSE (1==0)
#endif

#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#define LIMIT_BETWEEN(var, llow, hhigh)             \
        if((var) < (llow))  (var) = (llow);         \
            if((var) > (hhigh)) (var) = (hhigh);    \

// Shorthand
typedef const char cch_t;

// We do most of the semaphore ops even if the semaphore fails ...
// ... predictable data concept
                        
#define CREATE_SEMA(xSemaphore2)                                    \
        vSemaphoreCreateBinary( xSemaphore2 );                      \
                                                                    
#define TAKE_SEMA(xSemaphore2)                                      \
        if(!xSemaphoreTake( xSemaphore2, ( TickType_t ) 10 ))       \
            {                                                       \
            printf("Could not take semaphore %p.\n", xSemaphore2);  \
            }                                                       \
                
#define TAKE_SEMA2(xSemaphore2)                                     \
        if(!xSemaphoreTake( xSemaphore2, ( TickType_t ) 10 ))       \
                {                                                   \
                ESP_LOGE(httpTAG, "Could not take semaphore.");     \
                }                                                   \
                                                                    
#define GIVE_SEMA(xSemaphore2)                                      \
        xSemaphoreGive( xSemaphore2 );                              \
 

typedef const char cchar;

// Variables exported

extern time_t gl_lasttime;

// Misc routines for printing

int     print2log(const char *ptr, ...);
int     printauth(int auth);
int     printwifierr(int err);
int     printerr(int err);

// Parser helpers

char    *wcstrdup(const char *str);
int     parse_post(const char *buf, char *names[], char *mems[], int xlen);
char    *subst_str_at(char *orgx, cchar *atstr, cchar *substx, cchar *restr);
char    *subst_str(char *orgx, const char *substx, const char *restr);
int     unescape_url(char *str, char *strout, int lims);
char    *kill_bw_markers(char *mem, char *tag, char *tag2, char fill);

void    delayed_reboot(int wait_ms);

// Time related

time_t  internet_gettime();
int64_t add_firstcount(int64_t cnt);

char    *convday(int daynum);
char    *convmonth(int monthnum);

int     inc_bootcount();
int     get_mem_usage();
void    hard_restart();

// EOF













