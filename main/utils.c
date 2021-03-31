
/* =====[ AKOSTAR WiFi Clock project ]====================================

   File Name:       utils.c

   Description:     Misc utility functions

   Revisions:

      REV   DATE            BY              DESCRIPTION
      ----  -----------     ----------      ------------------------------
      0.00  feb.21.2018     Peter Glen      Initial version.
      0.00  mar.18.2018     Peter Glen      Added error translation
      0.00  mar.23.2018     Peter Glen      Delayed reboot
      0.00  mar.23.2018     Peter Glen      Serial to new namespace

   ======================================================================= */

#include <string.h>
#include <ctype.h>

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
#include "esp_int_wdt.h"

// Networking
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/api.h"
#include "lwip/arch.h"

// SNTP specific includes
#include "lwip/apps/sntp.h"

#include "utils.h"
//#include "wcmesht.h"

#define NTP_RETRY_COUNT 10

// The SDK does not provide this ...
extern void esp_task_wdt_add(void*);

//////////////////////////////////////////////////////////////////////////
// Last internet time aquisition

time_t gl_lasttime = 0;

static const char *TAG ="utils";

int printauth(int auth)

{
    int ret = 0;
    switch(auth)
        {
        case WIFI_CIPHER_TYPE_NONE:      ESP_LOGI(TAG,"WIFI_CIPHER_TYPE_NONE");    break;
        case WIFI_CIPHER_TYPE_WEP40:     ESP_LOGI(TAG,"WIFI_CIPHER_TYPE_WEP40");   break;
        case WIFI_CIPHER_TYPE_WEP104:    ESP_LOGI(TAG,"WIFI_CIPHER_TYPE_WEP104");  break;
        case WIFI_CIPHER_TYPE_TKIP:      ESP_LOGI(TAG,"WIFI_CIPHER_TYPE_TKIP");    break;
        case WIFI_CIPHER_TYPE_CCMP:      ESP_LOGI(TAG,"WIFI_CIPHER_TYPE_CCMP");    break;
        case WIFI_CIPHER_TYPE_TKIP_CCMP: ESP_LOGI(TAG,"WIFI_CIPHER_TYPE_TKIP_CCMP");   break;
        case WIFI_CIPHER_TYPE_UNKNOWN:   ESP_LOGI(TAG,"WIFI_CIPHER_TYPE_UNKNOWN"); break;
        }
    return ret;
}

//////////////////////////////////////////////////////////////////////////

int     printerr(int err)

{
    int ret = 0;
#if 0
    switch(err)
        {
        case   EPERM:
            ESP_LOGI(TAG,"Operation not permitted");  break;
        case   ENOENT:
            ESP_LOGI(TAG,"No such file or directory");  break;
        case   ESRCH:
            ESP_LOGI(TAG,"No such process");  break;
        case   EINTR:
            ESP_LOGI(TAG,"Interrupted system call");  break;
        case   EIO:
            ESP_LOGI(TAG,"I/O error");  break;
        case   ENXIO:
            ESP_LOGI(TAG,"No such device or address");  break;
        case   E2BIG:
            ESP_LOGI(TAG,"Arg list too long");  break;
        case   ENOEXEC:
            ESP_LOGI(TAG,"Exec format error");  break;
        case   EBADF:
            ESP_LOGI(TAG,"Bad file number");  break;
        case   ECHILD:
            ESP_LOGI(TAG,"No child processes");  break;
        case   EAGAIN:
            ESP_LOGI(TAG,"Try again");  break;
        case   ENOMEM:
            ESP_LOGI(TAG,"Out of memory");  break;
        case   EACCES:
            ESP_LOGI(TAG,"Permission denied");  break;
        case   EFAULT:
            ESP_LOGI(TAG,"Bad address");  break;
        case   ENOTBLK:
            ESP_LOGI(TAG,"Block device required");  break;
        case   EBUSY:
            ESP_LOGI(TAG,"Device or resource busy");  break;
        case   EEXIST:
            ESP_LOGI(TAG,"File exists");  break;
        case   EXDEV:
            ESP_LOGI(TAG,"Cross-device link");  break;
        case   ENODEV:
            ESP_LOGI(TAG,"No such device");  break;
        case   ENOTDIR:
            ESP_LOGI(TAG,"Not a directory");  break;
        case   EISDIR:
            ESP_LOGI(TAG,"Is a directory");  break;
        case   EINVAL:
            ESP_LOGI(TAG,"Invalid argument");  break;
        case   ENFILE:
            ESP_LOGI(TAG,"File table overflow");  break;
        case   EMFILE:
            ESP_LOGI(TAG,"Too many open files");  break;
        case   ENOTTY:
            ESP_LOGI(TAG,"Not a typewriter");  break;
        case   ETXTBSY:
            ESP_LOGI(TAG,"Text file busy");  break;
        case   EFBIG:
            ESP_LOGI(TAG,"File too large");  break;
        case   ENOSPC:
            ESP_LOGI(TAG,"No space left on device");  break;
        case   ESPIPE:
            ESP_LOGI(TAG,"Illegal seek");  break;
        case   EROFS:
            ESP_LOGI(TAG,"Read-only file system");  break;
        case   EMLINK:
            ESP_LOGI(TAG,"Too many links");  break;
        case   EPIPE:
            ESP_LOGI(TAG,"Broken pipe");  break;
        case   EDOM:
            ESP_LOGI(TAG,"Math argument out of domain of func");  break;
        case   ERANGE:
            ESP_LOGI(TAG,"Math result not representable");  break;
        case   EDEADLK:
            ESP_LOGI(TAG,"Resource deadlock would occur");  break;
        case   ENAMETOOLONG:
            ESP_LOGI(TAG,"File name too long");  break;
        case   ENOLCK:
            ESP_LOGI(TAG,"No record locks available");  break;
        case   ENOSYS:
            ESP_LOGI(TAG,"Function not implemented");  break;
        case   ENOTEMPTY:
            ESP_LOGI(TAG,"Directory not empty");  break;
        case   ELOOP:
            ESP_LOGI(TAG,"Too many symbolic links encountered");  break;
        case   EWOULDBLOCK:
            ESP_LOGI(TAG,"Operation would block");  break;
        case   ENOMSG:
            ESP_LOGI(TAG,"No message of desired type");  break;
        case   EIDRM:
            ESP_LOGI(TAG,"Identifier removed");  break;
        case   ECHRNG:
            ESP_LOGI(TAG,"Channel number out of range");  break;
        case   EL2NSYNC:
            ESP_LOGI(TAG,"Level 2 not synchronized");  break;
        case   EL3HLT:
            ESP_LOGI(TAG,"Level 3 halted");  break;
        case   EL3RST:
            ESP_LOGI(TAG,"Level 3 reset");  break;
        case   ELNRNG:
            ESP_LOGI(TAG,"Link number out of range");  break;
        case   EUNATCH:
            ESP_LOGI(TAG,"Protocol driver not attached");  break;
        case   ENOCSI:
            ESP_LOGI(TAG,"No CSI structure available");  break;
        case   EL2HLT:
            ESP_LOGI(TAG,"Level 2 halted");  break;
        case   EBADE:
            ESP_LOGI(TAG,"Invalid exchange");  break;
        case   EBADR:
            ESP_LOGI(TAG,"Invalid request descriptor");  break;
        case   EXFULL:
            ESP_LOGI(TAG,"Exchange full");  break;
        case   ENOANO:
            ESP_LOGI(TAG,"No anode");  break;
        case   EBADRQC:
            ESP_LOGI(TAG,"Invalid request code");  break;
        case   EBADSLT:
            ESP_LOGI(TAG,"Invalid slot");  break;
        case   EBFONT:
            ESP_LOGI(TAG,"Bad font file format");  break;
        case   ENOSTR:
            ESP_LOGI(TAG,"Device not a stream");  break;
        case   ENODATA:
            ESP_LOGI(TAG,"No data available");  break;
        case   ETIME:
            ESP_LOGI(TAG,"Timer expired");  break;
        case   ENOSR:
            ESP_LOGI(TAG,"Out of streams resources");  break;
        case   ENONET:
            ESP_LOGI(TAG,"Machine is not on the network");  break;
        case   ENOPKG:
            ESP_LOGI(TAG,"Package not installed");  break;
        case   EREMOTE:
            ESP_LOGI(TAG,"Object is remote");  break;
        case   ENOLINK:
            ESP_LOGI(TAG,"Link has been severed");  break;
        case   EADV:
            ESP_LOGI(TAG,"Advertise error");  break;
        case   ESRMNT:
            ESP_LOGI(TAG,"Srmount error");  break;
        case   ECOMM:
            ESP_LOGI(TAG,"Communication error on send");  break;
        case   EPROTO:
            ESP_LOGI(TAG,"Protocol error");  break;
        case   EMULTIHOP:
            ESP_LOGI(TAG,"Multihop attempted");  break;
        case   EDOTDOT:
            ESP_LOGI(TAG,"RFS specific error");  break;
        case   EBADMSG:
            ESP_LOGI(TAG,"Not a data message");  break;
        case   EOVERFLOW:
            ESP_LOGI(TAG,"Value too large for defined data type");  break;
        case   ENOTUNIQ:
            ESP_LOGI(TAG,"Name not unique on network");  break;
        case   EBADFD:
            ESP_LOGI(TAG,"File descriptor in bad state");  break;
        case   EREMCHG:
            ESP_LOGI(TAG,"Remote address changed");  break;
        case   ELIBACC:
            ESP_LOGI(TAG,"Can not access a needed shared library");  break;
        case   ELIBBAD:
            ESP_LOGI(TAG,"Accessing a corrupted shared library");  break;
        case   ELIBSCN:
            ESP_LOGI(TAG,".lib section in a.out corrupted");  break;
        case   ELIBMAX:
            ESP_LOGI(TAG,"Attempting to link in too many shared libraries");  break;
        case   ELIBEXEC:
            ESP_LOGI(TAG,"Cannot exec a shared library directly");  break;
        case   EILSEQ:
            ESP_LOGI(TAG,"Illegal byte sequence");  break;
        case   ERESTART:
            ESP_LOGI(TAG,"Interrupted system call should be restarted");  break;
        case   ESTRPIPE:
            ESP_LOGI(TAG,"Streams pipe error");  break;
        case   EUSERS:
            ESP_LOGI(TAG,"Too many users");  break;
        case   ENOTSOCK:
            ESP_LOGI(TAG,"Socket operation on non-socket");  break;
        case   EDESTADDRREQ:
            ESP_LOGI(TAG,"Destination address required");  break;
        case   EMSGSIZE:
            ESP_LOGI(TAG,"Message too long");  break;
        case   EPROTOTYPE:
            ESP_LOGI(TAG,"Protocol wrong type for socket");  break;
        case   ENOPROTOOPT:
            ESP_LOGI(TAG,"Protocol not available");  break;
        case   EPROTONOSUPPORT:
            ESP_LOGI(TAG,"Protocol not supported");  break;
        case   ESOCKTNOSUPPORT:
            ESP_LOGI(TAG,"Socket type not supported");  break;
        case   EOPNOTSUPP:
            ESP_LOGI(TAG,"Operation not supported on transport endpoint");  break;
        case   EPFNOSUPPORT:
            ESP_LOGI(TAG,"Protocol family not supported");  break;
        case   EAFNOSUPPORT:
            ESP_LOGI(TAG,"Address family not supported by protocol");  break;
        case   EADDRINUSE:
            ESP_LOGI(TAG,"Address already in use");  break;
        case   EADDRNOTAVAIL:
            ESP_LOGI(TAG,"Cannot assign requested address");  break;
        case   ENETDOWN:
            ESP_LOGI(TAG,"Network is down");  break;
        case   ENETUNREACH:
            ESP_LOGI(TAG,"Network is unreachable");  break;
        case   ENETRESET:
            ESP_LOGI(TAG,"Network dropped connection because of reset");  break;
        case   ECONNABORTED:
            ESP_LOGI(TAG,"Software caused connection abort");  break;
        case   ECONNRESET:
            ESP_LOGI(TAG,"Connection reset by peer");  break;
        case   ENOBUFS:
            ESP_LOGI(TAG,"No buffer space available");  break;
        case   EISCONN:
            ESP_LOGI(TAG,"Transport endpoint is already connected");  break;
        case   ENOTCONN:
            ESP_LOGI(TAG,"Transport endpoint is not connected");  break;
        case   ESHUTDOWN:
            ESP_LOGI(TAG,"Cannot send after transport endpoint shutdown");  break;
        case   ETOOMANYREFS:
            ESP_LOGI(TAG,"Too many references: cannot splice");  break;
        case   ETIMEDOUT:
            ESP_LOGI(TAG,"Connection timed out");  break;
        case   ECONNREFUSED:
            ESP_LOGI(TAG,"Connection refused");  break;
        case   EHOSTDOWN:
            ESP_LOGI(TAG,"Host is down");  break;
        case   EHOSTUNREACH:
            ESP_LOGI(TAG,"No route to host");  break;
        case   EALREADY:
            ESP_LOGI(TAG,"Operation already in progress");  break;
        case   EINPROGRESS:
            ESP_LOGI(TAG,"Operation now in progress");  break;
        case   ESTALE:
            ESP_LOGI(TAG,"Stale NFS file handle");  break;
        case   EUCLEAN:
            ESP_LOGI(TAG,"Structure needs cleaning");  break;
        case   ENOTNAM:
            ESP_LOGI(TAG,"Not a XENIX named type file");  break;
        case   ENAVAIL:
            ESP_LOGI(TAG,"No XENIX semaphores available");  break;
        case   EISNAM:
            ESP_LOGI(TAG,"Is a named type file");  break;
        case   EREMOTEIO:
            ESP_LOGI(TAG,"Remote I/O error");  break;
        case   EDQUOT:
            ESP_LOGI(TAG,"Quota exceeded");  break;
        case   ENOMEDIUM:
            ESP_LOGI(TAG,"No medium found");  break;
        case   EMEDIUMTYPE:
            ESP_LOGI(TAG,"Wrong medium type");  break;
        default:
            ESP_LOGI(TAG,"Unkown");  break;
    }
    #endif
    return ret;
}

//////////////////////////////////////////////////////////////////////////
// Print a value to the DEBUG console
//

//static char buff2[256];

int     print2log(const char *ptr, ...)

{
    char *buff2 = malloc(1000);
    va_list va;
    va_start(va, ptr);

    buff2[0] = '\0';
    int ret = vsprintf(&buff2[0], ptr, va);
    ESP_LOGI(TAG,"%s", &buff2[0]);

    free(buff2);
    return ret;
}

//////////////////////////////////////////////////////////////////////////
// Parse list of names into buffers. List terminated with NULL.
// Must pass as many buffs as names.
//

int     parse_post(const char *buf, char *names[], char *mems[], int xlen)

{
    int prog = 0, len;

    //ESP_LOGI(TAG, "Called parser with buf '%s'\n", buf);

    while(true)
        {
        if(names[prog] == NULL)
            break;
        if(mems[prog] == NULL)
            break;

        char *ppp = strstr(buf, names[prog]);
        mems[prog][0] = '\0';                   // Clear target
        if(ppp)
            {
            ppp += strlen(names[prog]);

            char *ppp2 = strstr(ppp, "&");
            if(ppp2)
                {
                len = MIN(ppp2 - ppp, xlen - 1);
                }
            else
                {
                // Last one
                len = MIN(strlen(ppp), xlen - 1);
                }
            memcpy(mems[prog], ppp, len);
            // Zero terminate
            mems[prog][len] = '\0';
            }
        prog++;
        FEED_DOG
        }

    //for(int loopd = 0; loopd < prog; loopd += 1)
    //    ESP_LOGI(TAG, "name='%s' val='%s'\n", names[loopd], mems[loopd]);

    return prog;
}

//////////////////////////////////////////////////////////////////////////
// Prepare a copy of the page

char    *wcstrdup(const char *str)

{
    int tlen = strlen(str);
    char *mem = malloc(tlen + 2);
    if(!mem)
        return(mem);
    memcpy(mem, str, tlen + 1);
    return(mem);
}

//////////////////////////////////////////////////////////////////////////
//

static void    delayed_reboot_task(void *parm)

{
    esp_wifi_disconnect();
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(esp_wifi_stop());
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(esp_wifi_deinit());
    vTaskDelay(100 / portTICK_PERIOD_MS);

    //vTaskDelay(int)parm / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Rebooting ... ");
    esp_restart();
    vTaskDelete(NULL);
}

//////////////////////////////////////////////////////////////////////////
// Delayed reboot

void    delayed_reboot(int wait_ms)

{
    ESP_LOGI(TAG, "Rebooting ... ");
    // Disconnect everybody: (automatic)
    xTaskCreate(&delayed_reboot_task, "reboot_task", 1024,
                                (void*)wait_ms, 5, NULL);
    vTaskDelay(20 / portTICK_PERIOD_MS);
}

//////////////////////////////////////////////////////////////////////////
// Substitute string substr in orgx after atstr, and replace it
// with restr (result string)
// String is padded and limited to the length of substx.
//

char    *subst_str_at(char *orgx, cchar *atstr, cchar *substx, cchar *restr)

{
    char *retp = NULL;

    char *anchor = strstr(orgx, atstr);
    if(anchor)
        {
        retp = subst_str(anchor, substx, restr);
        }
    else
        {
        ESP_LOGE(TAG, "No atstr '%s' at subst_str_at.", atstr);
        }
    return retp;
}

// Erase between markers with fill
// This is used to un-select part of the HTML

char    *kill_bw_markers(char *mem, char *tag, char *tag2, char fill)

{
    char *bb = strstr(mem, tag);
    if(!bb)
        {
        ESP_LOGE(TAG, "kill_bw_markers() no first tag.");
        return NULL;
        }
    char *cc = strstr(mem, tag2);
    if(!bb)
        {
        ESP_LOGE(TAG, "kill_bw_markers() no second tag.");
        return bb;
        }
    memset(bb, fill, cc - bb);
    return bb;
}

//////////////////////////////////////////////////////////////////////////
// Substitute string substr in orgx replace it with restr (result string)
// String is padded and limited to the length of substx.
//

char    *subst_str(char *orgx, const char *substx, const char *restr)

{
    int lenx = strlen(substx);
    char *ggg = strstr(orgx, substx), *tmpstr = malloc(lenx + 4);

    if(tmpstr == NULL)
        {
        ESP_LOGE(TAG, "subst_str() no memory.");
        goto endd2;
        }
    // Normalize string to lenx / lenx
    if(ggg)
        {
        snprintf(tmpstr, lenx + 2, "%*.*s", lenx, lenx, restr);
        memcpy(ggg, tmpstr, strlen(tmpstr));
        }
    else
        {
        ESP_LOGE(TAG, "subst_str() cannot find anchor: %s", substx);
        }
    free(tmpstr);

    endd2:
    return ggg;
}

//////////////////////////////////////////////////////////////////////////
// Unescape HTML entities

int     unescape_url(char *str, char *strout, int lims)

{
    int loop, cnt = 0, len = strlen(str);

    for(loop = 0; loop < len; loop++)
        {
        if(str[loop] == '%')
            {
            if(loop + 2 < len)
                {
                if(isxdigit((int)str[loop+1]) && isxdigit((int)str[loop+2]) )
                    {
                    char chh_arr[3];
                    chh_arr[0] = str[loop+1]; chh_arr[1] = str[loop+2];
                    chh_arr[2] = '\0';
                    int val;
                    sscanf(chh_arr, "%x", &val);

                    if(cnt >= lims)
                        break;

                    strout[cnt++] = (char)(val & 0xff);
                    loop += 2;
                    }
                else
                    {
                    if(cnt >= lims)
                        break;
                    strout[cnt++] = str[loop];
                    }
                }
            else
                {
                if(cnt >= lims)
                    break;
                strout[cnt++] = str[loop];
                }
            }
        else
            {
            if(cnt >= lims)
                break;
            strout[cnt++] = str[loop];
            }
        }
    strout[cnt++] = '\0';
    return cnt;
}

//////////////////////////////////////////////////////////////////////////

char    *convmonth(int monthnum)

{
    char *month = NULL;

    if(     monthnum == 1)    month = "Jan";
    else if(monthnum == 2)    month = "Feb";
    else if(monthnum == 3)    month = "Mar";
    else if(monthnum == 4)    month = "Apr";
    else if(monthnum == 5)    month = "May";
    else if(monthnum == 6)    month = "Jun";
    else if(monthnum == 7)    month = "Jul";
    else if(monthnum == 8)    month = "Aug";
    else if(monthnum == 9)    month = "Sep";
    else if(monthnum == 10)   month = "Oct";
    else if(monthnum == 11)   month = "Nov";
    else if(monthnum == 12)   month = "Dec";

    return month;
 }

// Wrap around for faulty specs (normally: Sun = 0)

char    *convday(int daynum)

{
    char *day = NULL;

    if(     daynum == 0)    day = "Sun";
    else if(daynum == 1)    day = "Mon";
    else if(daynum == 2)    day = "Tue";
    else if(daynum == 3)    day = "Wed";
    else if(daynum == 4)    day = "Thu";
    else if(daynum == 5)    day = "Fri";
    else if(daynum == 6)    day = "Sat";
    else if(daynum == 7)    day = "Sun";

    return day;
 }


//////////////////////////////////////////////////////////////////////////
// Get Internet time. Returns time_t. Tries a couple of times,
// then gives up. (configure above)
//
// (print time to Debug Console)
//
//  struct tm {
//               int tm_sec;    /* Seconds (0-60) */
//               int tm_min;    /* Minutes (0-59) */
//               int tm_hour;   /* Hours (0-23) */
//               int tm_mday;   /* Day of the month (1-31) */
//               int tm_mon;    /* Month (0-11) */
//               int tm_year;   /* Year - 1900 */
//               int tm_wday;   /* Day of the week (0-6, Sunday = 0) */
//               int tm_yday;   /* Day in the year (0-365, 1 Jan = 0) */
//               int tm_isdst;  /* Daylight saving time */
//           };

//////////////////////////////////////////////////////////////////////////
// These are the time servers that we connect in random order.

char *servers[] = {
    "129.6.15.27",  "129.6.15.28",  "129.6.15.29",  "129.6.15.30",
    "128.138.140.44", "128.138.141.172",
    "132.163.97.1", "132.163.97.2", "132.163.97.3", "132.163.97.4"
    };

//////////////////////////////////////////////////////////////////////////

char    *rand_serv_name()

{
    int sss = (sizeof(servers) / sizeof(char*));
    int randx = rand() % sss;
    //printf("RND NTP: '%s'\n", servers[randx]);
    return(servers[randx]);
}

// Let the ntp server do its thing ...

static void    delayed_ntp_task(void *parm)

{
    static int got_one = false;

    if(got_one)
        {
        vTaskDelete(NULL);
        return;
        }
    got_one = true;

    ip_addr_t addr, addr2, addr3;

    // Got the NTP servers for the official time. Configure it on main page.
    ESP_LOGI(TAG, "Init SNTP subsystem.");

    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // Configure the project to have N number of time servers
    // We configured three.

    inet_pton(AF_INET, rand_serv_name(), &addr);
    sntp_setserver(0, &addr);

    inet_pton(AF_INET, rand_serv_name(), &addr2);
    sntp_setserver(1, &addr2);

    inet_pton(AF_INET, rand_serv_name(), &addr3);
    sntp_setserver(2, &addr3);

    sntp_init();

    // Pre - wait. This permits the process to start before we
    // interrogate for success
    vTaskDelay(300 / portTICK_PERIOD_MS);

    int     retry = 0;     time_t  now = 0;   struct  tm timeinfo = { 0 };

    //ESP_LOGI(TAG, "Getting internet time ...");
    //time_t nnn = sntp_get_current_timestamp();
    //ESP_LOGI(TAG, "got SNTP time %d", nnn);

    while(true)
        {
        time(&now);
        //ESP_LOGI(TAG, "Got time from server: 0x%lx", now);

        localtime_r(&now, &timeinfo);
        //ESP_LOGI(TAG, "try: %02d/%02d/%04d %02d:%02d:%02d",
        //                timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
        //                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

        // Valid time?
        if(timeinfo.tm_year > (2016 - 1900))
            {
            //ESP_LOGI(TAG, "Got internet time (dd/mm/yyyy hh:mm:ss)");
            ESP_LOGI(TAG, "Got Time: %02d/%02d/%04d %02d:%02d:%02d",
                    timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
                        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
            break;
            }

        if(retry >= NTP_RETRY_COUNT)
            {
            ESP_LOGI(TAG, "Too many NTP tries ... giving up.");
            break;
            }

        retry++;
        ESP_LOGI(TAG, "Waiting for system time to be set... "
                            "(try %d of %d)", retry, NTP_RETRY_COUNT);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        }

     // Valid time?
    if(timeinfo.tm_year > (2016 - 1900))
        {
        gl_lasttime = now;
        //wc_hop_cnt = 1;
        }

    // We let the NTP process live some more
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "deInit SNTP subsystem.");
    sntp_stop();

    // We let the NTP finish the call before we die
    vTaskDelay(50 / portTICK_PERIOD_MS);

    got_one = false;
    vTaskDelete(NULL);
}

//////////////////////////////////////////////////////////////////////////
// Just like the name says, get time from the internet.
// Note that whe actual time may arrive later. Keep calling time(&now)
// for actual progress.

time_t  internet_gettime()

{
    time_t now;
    xTaskCreate(&delayed_ntp_task, "delayed_ntp_task", 4096, NULL, 5, NULL);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    time(&now);

    return now;
}

//////////////////////////////////////////////////////////////////////////
// Remember the first time this clock aquired the time.
// This will be used as warranty age

int64_t     add_firstcount(int64_t cnt)

{
    err_t err;
    int64_t first_count = 0;

    nvs_handle my_handle;
    err = nvs_open("akostar", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%d) opening NVS handle!", err);
        goto err3;
        }
    err = nvs_get_i64(my_handle, "first_count", &first_count);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Warn: (%d) first_count getting NVS value!", err);
        }

    // Value of zero means no count has ben saved
    if(first_count == 0)
        {
        first_count = cnt;
        err = nvs_set_i64(my_handle, "first_count", first_count);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "Error (%d) writing NVS!", err);
            goto err4;
            }
        err = nvs_commit(my_handle);
        if (err != ESP_OK) {
            ESP_LOGI(TAG, "Error (%d)commit to NVS!", err);
            goto err4;
            }
        }

  err4:
    nvs_close(my_handle);

  err3:

  return first_count;

}

//////////////////////////////////////////////////////////////////////////
// Increment boot_count. Return new count. Only increment once.

int    inc_bootcount()

{
    static int boot_count = 0, was_counted = 0;
    err_t err;

    if(was_counted)
        return boot_count;

    nvs_handle my_handle;
    err = nvs_open("akostar", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%d) opening NVS handle!", err);
        goto err3;
        }
    err = nvs_get_i32(my_handle, "boot_count", &boot_count);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Warn: (%d) getting boot_count NVS value!", err);
        }
    boot_count ++;          // HERE

    err = nvs_set_i32(my_handle, "boot_count", boot_count);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%d) writing NVS!", err);
        goto err4;
        }
    err = nvs_commit(my_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Error (%d)commit to NVS!", err);
        goto err4;
        }

    was_counted = true;
    //(void)err;  // Stop warning

  err4:
    // Close
    nvs_close(my_handle);

  err3:
    return boot_count;
}

// Use the watchdog for reboot

void    hard_restart()

{
    //esp_int_wdt_init(1, true);
    esp_task_wdt_add(NULL);

    #if 0
    esp_wifi_disconnect();
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(esp_wifi_stop());
    vTaskDelay(100 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(esp_wifi_deinit());
    vTaskDelay(100 / portTICK_PERIOD_MS);
    #endif

    //vTaskDelay(int)parm / portTICK_PERIOD_MS);

    while(true);
}

//////////////////////////////////////////////////////////////////////////

int     get_mem_usage()

{
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_DEFAULT);
    return info.total_allocated_bytes;
}

// EOF











