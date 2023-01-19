#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_wifi.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_vfs.h>
#include <esp_http_server.h>
#include <driver/gpio.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define WIFI_SSID   "esp32"
#define WIFI_PSWD   "12345678"

#define GPIO_LED1   GPIO_NUM_18
#define GPIO_LED2   GPIO_NUM_19
#define GPIO_LED3   GPIO_NUM_21

#define N_LEDS      3

#define WIFI_RDY    (1 << 0)
#define LED0_WAIT   (1 << 1)
#define LED1_WAIT   (1 << 2)
#define LED2_WAIT   (1 << 3)

#define SCRATCH_BUFSIZE     (8192)

static int32_t g_leds[N_LEDS] = {GPIO_LED1, GPIO_LED2, GPIO_LED3};
static int32_t g_led_stat[N_LEDS] = {0, 0, 0};
static EventGroupHandle_t gh_evt = NULL;
static esp_netif_t * gp_wifi_ap = NULL;
static EventGroupHandle_t g_wifi_event_group = NULL;

typedef struct
{
    char base_path[ESP_VFS_PATH_MAX + 1];       /**< Percorso base per la
                                                memorizzazione dei file. */
    char scratch[SCRATCH_BUFSIZE];              /**< Buffer per la
                                                memorizzazione temporanea
                                                durante il trasferimento di
                                                file.*/
} file_server_data_t;

esp_err_t file_server_init(void);

static void
task_server (void * p_parameter)
{
    EventBits_t ret = 0;
    file_server_init();

    for (;;)
    {
        ret = xEventGroupWaitBits(gh_evt, LED0_WAIT | LED1_WAIT | LED2_WAIT, pdTRUE, pdFALSE, portMAX_DELAY);

        printf("azione attivata su bit %d\n", ret);

        if (LED0_WAIT & ret)
        {
            gpio_set_level(g_leds[0], g_led_stat[0]);
        }
        
        if (LED1_WAIT & ret)
        {
            gpio_set_level(g_leds[1], g_led_stat[1]);
        }

        if (LED2_WAIT & ret)
        {
            gpio_set_level(g_leds[2], g_led_stat[2]);
        }
    }
}

static esp_err_t
webpage_handler (httpd_req_t * p_req)
{
    esp_err_t ret = ESP_OK;     /* Valore di ritorno. */

    (void) httpd_resp_set_type(p_req, "text/html");

    httpd_resp_sendstr_chunk(p_req, "<!DOCTYPE html><html>");
    httpd_resp_sendstr_chunk(p_req, "<head><meta name=\"viewport\" "
          "content=\"width=device-width, initial-scale=1\">");
    httpd_resp_sendstr_chunk(p_req, "<link rel=\"icon\" href=\"data:,\">");
    httpd_resp_sendstr_chunk(p_req, "<style>html { font-family: Helvetica; "
          "display: inline-block; margin: 0px auto; "
          "text-align: center;}");
    httpd_resp_sendstr_chunk(p_req, ".button { background-color: "
          "#4CAF50; border: none; color: white; "
          "padding: 16px 40px;");
    httpd_resp_sendstr_chunk(p_req, "text-decoration: none; "
          "font-size: 30px; margin: 2px; cursor: pointer;}");
    httpd_resp_sendstr_chunk(p_req, ".button2 {background-color: #555555;}"
          "</style></head>");
    httpd_resp_sendstr_chunk(p_req, "<body><h1>ESP32 Event Groups "
          "(eventgr.ino)</h1>");

    for (int32_t idx = 0; idx < N_LEDS; ++idx)
    {
        bool state = g_led_stat[idx];
        printf("\tstato led%d = %d\n", g_leds[idx], state);
        char temp[32] = {0};

        snprintf(temp, sizeof temp, "<p>LED%d - State ", idx);
        httpd_resp_sendstr_chunk(p_req, temp);

        if (state)
        {
            httpd_resp_sendstr_chunk(p_req, "on</p>");
        }
        else
        {
            httpd_resp_sendstr_chunk(p_req, "off</p>");
        }

        httpd_resp_sendstr_chunk(p_req, "<p><a href=\"");
        snprintf(temp, sizeof temp, "/led%d/%d", idx, !state);
        httpd_resp_sendstr_chunk(p_req, temp);
        httpd_resp_sendstr_chunk(p_req, "\"><button class=\"button\">");

        if (state)
        {
            httpd_resp_sendstr_chunk(p_req, "OFF");
        }
        else
        {
            httpd_resp_sendstr_chunk(p_req, "ON");
        }

        httpd_resp_sendstr_chunk(p_req, "</button></a></p>");
#if 1
        httpd_resp_sendstr_chunk(p_req, "<script>");
        httpd_resp_sendstr_chunk(p_req, "window.setTimeout( function() {");
        httpd_resp_sendstr_chunk(p_req, "window.location.reload();");
        httpd_resp_sendstr_chunk(p_req, "}, 1000);");
        httpd_resp_sendstr_chunk(p_req, "</script>");
#endif
        httpd_resp_sendstr_chunk(p_req, "</body></html>");
    }

    (void) httpd_resp_send_chunk(p_req, NULL, 0);
    
    return ret;
}   /* webpage_handler() */

static esp_err_t
led0_handler (httpd_req_t * p_req)
{
    esp_err_t ret = ESP_OK;     /* Valore di ritorno. */
    printf("led0 %s\n", (char *)p_req->uri);
    char * p_string = (char *) p_req->uri + sizeof("/led0/") - 1;
    bool onoff = p_string[0] - '0';

    printf("LED 0 = %d\n", onoff);

    g_led_stat[0] = onoff;

    xEventGroupSetBits(gh_evt, LED0_WAIT);
    
    return ret;
}   /* led0_handler() */

static esp_err_t
led1_handler (httpd_req_t * p_req)
{
    esp_err_t ret = ESP_OK;     /* Valore di ritorno. */
    printf("led1 %s\n", (char *)p_req->uri);
    char * p_string = (char *) p_req->uri + sizeof("/led1/") - 1;
    bool onoff = p_string[0] - '0';

    printf("LED 1 = %d\n", onoff);

    g_led_stat[1] = onoff;

    xEventGroupSetBits(gh_evt, LED1_WAIT);
    
    return ret;
}   /* led1_handler() */

static esp_err_t
led2_handler (httpd_req_t * p_req)
{
    esp_err_t ret = ESP_OK;     /* Valore di ritorno. */
    printf("led2 %s\n", (char *)p_req->uri);
    char * p_string = (char *) p_req->uri + sizeof("/led2/") - 1;
    bool onoff = p_string[0] - '0';

    printf("LED 2 = %d\n", onoff);

    g_led_stat[2] = onoff;

    xEventGroupSetBits(gh_evt, LED2_WAIT);
    
    return ret;
}   /* led2_handler() */

esp_err_t
file_server_init (void)
{
    esp_err_t ret = ESP_FAIL;       /* Valore di ritorno della funzione. */
    static file_server_data_t * p_server_data = NULL;   /* Variabile usata da
                                                         * apposito handler. */
    httpd_handle_t server = NULL;   /* Istanza dello handler relativo al server
                                     * HTTP. */
    // Per configurare correttamente la struttura di base, va sempre eseguito
    // HTTPD_DEFAULT_CONFIG.
    //
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    // Dichiarazione di tutte le strutture associate alle varie chiamate HTTP.
    //
    httpd_uri_t open_main_page;
    httpd_uri_t open_led0;
    httpd_uri_t open_led1;
    httpd_uri_t open_led2;

    xEventGroupWaitBits(gh_evt, WIFI_RDY, pdFALSE, pdFALSE, portMAX_DELAY);

    // Si deve fornire il percorso valido, il webserver supporta solo il
    // percorso '/spiffs'.
    //
    if (p_server_data != NULL)
    {
        ret = ESP_ERR_INVALID_STATE;
    }
    else
    {
        // Alloca memoria per la struttura del file server.
        //
        p_server_data = calloc(1, sizeof(file_server_data_t));
        // Allocazione in memoria non riuscita.
        //
        if (NULL == p_server_data)
        {
            ret = ESP_ERR_NO_MEM;
        }
        else
        {
            // Funzione di verifica che l'URI coincida con il template. Lo
            // stesso handler può rispondere a diversi URI che però
            // corrispondono allo schema particolare.
            //
            config.uri_match_fn = httpd_uri_match_wildcard;

            // Fa partire il server web creando una istanza HTTP e allocando
            // memoria e risorse per esso in base alla configurazione
            // specificata.
            //
            if (httpd_start(&server, &config) != ESP_OK)
            {
                ret = ESP_FAIL;
            }
            // Impostazione dell'apertura della pagina principale, attraverso la
            // definizione dell'URI, del metodo URI, della funzione legata a
            // tale URI.
            //
            else
            {
                open_main_page.uri = "/";
                open_main_page.method = HTTP_GET;
                open_main_page.handler = webpage_handler;
                open_main_page.user_ctx = p_server_data;
                ESP_ERROR_CHECK(httpd_register_uri_handler(server, &open_main_page));

                open_led0.uri = "/led0/*";
                open_led0.method = HTTP_GET;
                open_led0.handler = led0_handler;
                open_led0.user_ctx = p_server_data;
                ESP_ERROR_CHECK(httpd_register_uri_handler(server, &open_led0));

                open_led1.uri = "/led1/*";
                open_led1.method = HTTP_GET;
                open_led1.handler = led1_handler;
                open_led1.user_ctx = p_server_data;
                ESP_ERROR_CHECK(httpd_register_uri_handler(server, &open_led1));

                open_led2.uri = "/led2/*";
                open_led2.method = HTTP_GET;
                open_led2.handler = led2_handler;
                open_led2.user_ctx = p_server_data;
                ESP_ERROR_CHECK(httpd_register_uri_handler(server, &open_led2));

                ret = ESP_OK;
            }
        }
    }

    return ret;
}   /* file_server_init() */

static void
wifi_event_handler (void * arg, esp_event_base_t event_base,
                    int32_t event_id, void * event_data)
{
    switch (event_id)
    {
        // Una stazione si connette al soft-AP di ESP32.
        //
        case WIFI_EVENT_AP_STACONNECTED:
        {
            wifi_event_ap_staconnected_t * event =
                                (wifi_event_ap_staconnected_t *) event_data;
        }
        break;
        // Una stazione si disconnette dal soft-AP di ESP32.
        //
        case WIFI_EVENT_AP_STADISCONNECTED:
        {
            wifi_event_ap_stadisconnected_t * event =
                                (wifi_event_ap_stadisconnected_t *) event_data;
        }
        break;
        // Avvia il servizio AP.
        //
        case WIFI_EVENT_AP_START:
            xEventGroupSetBits(gh_evt, WIFI_RDY);
            printf("Inizia il servizio da access point\n");
        break;
        // Arresta il servizio AP.
        //
        case WIFI_EVENT_AP_STOP:
            printf("Chiusura del servizio da access point");
        break;
        // Avvia il servizio STA.
        //
        case (WIFI_EVENT_STA_START):
            if (WIFI_EVENT == event_base)
            {
                // Connessione di ESP32 a un AP con i parametri di
                // configurazione stabiliti in fase di inizializzazione.
                //
                (void) esp_wifi_connect();
            }
        break;
        // Arresta il servizio STA, si disconnette dalla stazione.
        //
        case WIFI_EVENT_STA_STOP:
        break;
        // Connessione a un AP.
        //
        case WIFI_EVENT_STA_CONNECTED:
        break;
        // Stazione disconnessa dall'AP perché l'AP ha interrotto la
        // comunicazione.
        //
        case (WIFI_EVENT_STA_DISCONNECTED):
            if (WIFI_EVENT == event_base)
            {
            }
        break;
        // Client DHCP ottiene un IPV4 dal server DHCP, o IPV4 cambia.
        //
        case IP_EVENT_STA_GOT_IP:
            if (IP_EVENT == event_base)
            {
            }
        break;
        // Avviene prima di un crash, per evitare problemi produco uno
        // spegnimento della rete WiFi.
        // Cercare di fare in modo che ciò non avvenga mai!
        //
        default:
        break;
    }
    return;
}   /* wifi_event_handler() */

static void
init_http (const char * const p_ssid, const char * const p_pswd)
{
    // Struttura per ospitare IP, gateway e maschera riguardante l'indirizzo.
    //
    esp_netif_ip_info_t ipInfo;
    // Prima di impostare l'IP dell'AP, occorre prima fermare il server DHCP.
    //
	ESP_ERROR_CHECK(esp_netif_dhcps_stop(gp_wifi_ap));
    IP4_ADDR(&ipInfo.ip, 192, 168, 1, 1);
    IP4_ADDR(&ipInfo.gw, 192, 168, 1, 1);
	IP4_ADDR(&ipInfo.netmask, 255, 255, 255, 0);
    // Imposta l'informazione dell'indirizzo IP dell'interfaccia.
    //
	ESP_ERROR_CHECK(esp_netif_set_ip_info(gp_wifi_ap, &ipInfo));
    // Fa partire il server DHCP (solo se è stata abilitata l'interfacccia
    // dell'oggetto).
    //
	ESP_ERROR_CHECK(esp_netif_dhcps_start(gp_wifi_ap));
    // Crea un nuovo gruppo di eventi FreeRTOS e restituisce il gestore a cui
    // fare riferimento.
    //
    g_wifi_event_group = xEventGroupCreate();
    // Ferma il comportamento da stazione e libera le risorse.
    //
    ESP_ERROR_CHECK(esp_wifi_stop());
    // Imposta una modalità nulla per il WiFi.
    //
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    // Struttura per la configurazione della stazione WiFi.
    //
    wifi_config_t wifi_access_point_config = {
        .ap = {
            .max_connection = 1,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    /**************************************************************************
     * WIFI - ACCESS POINT - SSID                                             *
     **************************************************************************/
    // Copia dei dati SSID e password indicati.
    //
    (void) strcpy((char *) wifi_access_point_config.ap.ssid, p_ssid);

    /**************************************************************************
     * WIFI - ACCESS POINT - PASSWORD                                         *
     **************************************************************************/
    (void) strcpy((char *) wifi_access_point_config.ap.password, p_pswd);
    
    // Imposta il modo operativo del WiFi (tra STA, soft-AP, STA+soft-AP).
    //
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    // Imposta la configurazione.
    //
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_access_point_config));
    // Fa partire il WiFi in base alla configurazione corrente.
    //
    ESP_ERROR_CHECK(esp_wifi_start());

    return;
}

void
wifi_interface_init (void)
{
    // Componente di astrazione dell'interfaccia di rete dell'adattatore TCP/IP
    // (per WiFi STA, WiFi AP, Ethernet).
    //
    ESP_ERROR_CHECK(esp_netif_init());
    // Crea un evento loop di default per gli eventi di sistema (es. WiFi).
    //
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Macro WIFI_INIT_CONFIG_DEFAULT **SEMPRE** usata per inizializzare la
    // configurazione ai valori di default, così tutti i campi sono
    // correttamente inseriti in cfg.
    //
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // Inizializza l'allocazione di risorse WiFi (buffer, struttura di
    // controllo, driver, ...).
    //
    ESP_ERROR_CHECK((esp_wifi_init(&cfg)));
    // Crea access point + station WiFi.
    //
    gp_wifi_ap = esp_netif_create_default_wifi_ap();
    // Questo è necessario per gli eventi legati all'access point attivo.
    //
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    return;
}   /* wifi_init_interface() */

esp_err_t
nvs_init (void)
{
    // Inizializza la partizione NVS di default (chiamata nvs nella tabella di
    // partizione).
    // 1) Legge configurazioni di sicurezza dalla prima chiaver NVS in tabella.
    // 2) Se essa è vuota, ne genera e salva una nuova.
    // 3) Internamente chiama "nvs_flash_secure_init()" con le informazioni
    //    sicure create.
    //
    esp_err_t ret = nvs_flash_init();
    nvs_handle_t g_nvs_handle;
    nvs_stats_t nvs_stats;
    // ESP_ERR_NVS_NO_FREE_PAGES -> memoria NVS non ha pagine vuote.
    // ESP_ERR_NVS_NEW_VERSION_FOUND -> NVS ha dati in un nuovo formato non
    // riconosciuto in questa versione.
    //
    if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) ||
        (ret == ESP_ERR_NVS_NEW_VERSION_FOUND))
    {
        // nvs_flash_erase() cancella la partizione NVS di default.
        //
        ESP_ERROR_CHECK(nvs_flash_erase());
        // Ritenta l'inizializzazione.
        //
        ret = nvs_flash_init();
    }

    return ret;
}   /* nvs_init() */

void
app_main (void)
{
    int32_t app_cpu = xPortGetCoreID();
    BaseType_t ret = 0;

    for (uint32_t idx = 0; idx < N_LEDS; ++idx)
    {
        gpio_pad_select_gpio(g_leds[idx]);
        ESP_ERROR_CHECK(gpio_set_direction(g_leds[idx], GPIO_MODE_OUTPUT));
        ESP_ERROR_CHECK(gpio_set_level(g_leds[idx], g_led_stat[idx]));
    }

    gh_evt = xEventGroupCreate();;
    assert(gh_evt != NULL);

    ret = xTaskCreatePinnedToCore(task_server, "http", 2100, NULL, 1, NULL, app_cpu);
    assert(pdPASS == ret);

    nvs_init();
    wifi_interface_init();
    init_http(WIFI_SSID, WIFI_PSWD);
}
