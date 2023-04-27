#include <stdio.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "driver/gpio.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "cJSON.h"

// defines global variables
#define DEVICE_NAME "MY TEST BLE DEVICE"    // Define device name
uint8_t ble_addr_type;                      // Global handle for BLE stack
void ble_app_advertise(void);               // Define function for BLE advertise

// Define GPIO LED pin
#define BLINK_GPIO GPIO_NUM_2

// Define global variables for first characheristic service
uint32_t buttery_lvl = 0;
char wifi_ssid[50];
char wifi_pass[50];
uint16_t buttery_lvl_handle;

// Define global variables for second characheristic service
uint32_t acc_lvl = 0;
uint32_t gyro_lvl = 0;
uint64_t gps_pos = 0;
bool led_state = 0;

// Define handle for notify msg
uint16_t acc_lvl_handle;
uint16_t gyro_lvl_handle;
uint16_t gps_pos_handle;
uint16_t led_state_handle;

// define handler for suspens and resume led task
TaskHandle_t led_task_handle = NULL;

// function for mapping value
uint64_t map(uint64_t x, uint64_t in_min, uint64_t in_max, uint64_t out_min, uint64_t out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// function for gen random string with variable length 
void getRandomStr(char* output, int len){
    char* chars = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz1234567890";     // Array with all supporting chars for WiFi SSID and Password
    for(int i = 0; i< len; i++){
        uint32_t rand_int = esp_random();                                               // Default esp idf function for gen int32 random number
        int random_index = map(rand_int, 0, 0xFFFFFFFF, 0, strlen(chars));              // Mapping from 0-MAXINT32 to 0-chars array length
        output[i] = chars[random_index];                                                // Set value from array to Pointer of char array
    }
}

// define tasks
// task for blink 2 times per second, 5 second
void blink_task(void *pvParameter){
    vTaskSuspend(led_task_handle);                  // if this task called, we suspend another task for led
    for(int i = 0; i < 20; i++){
        vTaskDelay(500 / portTICK_PERIOD_MS);       // Delay
        led_state = 0;                              // Set led_state
        ble_gatts_chr_updated(led_state_handle);    // Notify about status LED
        gpio_set_level(BLINK_GPIO, led_state);      // Set LED output lvl


        vTaskDelay(500 / portTICK_PERIOD_MS);
        led_state = 1;
        ble_gatts_chr_updated(led_state_handle);
        gpio_set_level(BLINK_GPIO, led_state);
    }
    vTaskResume(led_task_handle);                   // Resume another task for led
    vTaskDelete(NULL);                              // Delete current task from stack
}

// task for generate random value for accelerometer variable
void acc_vtask(void *pvParameter){
    while(1){
        uint32_t value = esp_random();
        acc_lvl = map(value, 0, 0xFFFFFFFF, 0, 0x00FFFFFF);     // Map value to 3 bytes

        ble_gatts_chr_updated(acc_lvl_handle);                  // Notify BLE about acceleration variable changes

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

// task for generate random value for gyroscope variable
void gyro_vtask(void *pvParameter){
    while(1){
        uint32_t value = esp_random();
        gyro_lvl = map(value, 0, 0xFFFFFFFF, 0, 0x00FFFFFF);

        ble_gatts_chr_updated(gyro_lvl_handle);

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

// task for generate random value for GPS variable
void gps_vtask(void *pvParameter){
    while(1){
        uint32_t value_l = esp_random();
        uint32_t value_h = esp_random();
        gps_pos = (((uint64_t) value_l) << 32) | value_h;           // Mix two 32 bit variable to one 64 bit variable

        ble_gatts_chr_updated(gps_pos_handle);

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

// task for blink led "always"
void led_vtask(void *pvParameter){
    while(1){
        vTaskDelay(8000 / portTICK_PERIOD_MS);                      // delay

        led_state = 1;                                              // global led status
        ble_gatts_chr_updated(led_state_handle);                    // notify ble about led status
        gpio_set_level(BLINK_GPIO, led_state);                      // set LED output level

        vTaskDelay(2000 / portTICK_PERIOD_MS);   

        led_state = 0;
        ble_gatts_chr_updated(led_state_handle);
        gpio_set_level(BLINK_GPIO, led_state);
    }
}

// task for generate random value for battery variable
void batt_wifi_data_vtask(void *pvParameter){
    while(1){
        getRandomStr(wifi_ssid, 50);                            // Gen wifi ssid with function what we declare before
        getRandomStr(wifi_pass, 50);   

        uint32_t value = esp_random();                          // Nothing new)
        buttery_lvl = map(value, 0, 0xFFFFFFFF, 0, 100);

        ble_gatts_chr_updated(buttery_lvl_handle);          

        vTaskDelay(1000 * 60 / portTICK_PERIOD_MS);  
    }
}


// define gatt_svr handler for different characteristics
static int gatt_svr_buttery_lvl(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    char buffer[4];                                         // Create buffer for send data to BLE characteristic
    sprintf(buffer, "%d", buttery_lvl);                     // Formating data to buffer

    os_mbuf_append(ctxt->om, buffer, sizeof(buffer));       // Append buffer to ble stack buffer and this function call response to BLE client
    return 0;
}

static int gatt_svr_wifi_ssid(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){                                       // One function for read and write from BLE characteristic, check characteristic flag
        memset(&wifi_ssid[0], 0, sizeof(wifi_ssid));                                    // Clear ssid variable before copy
        memcpy(wifi_ssid, ctxt->om->om_data, ctxt->om->om_len>50?50:ctxt->om->om_len);  // Copy from BLE stack buffer to global variable with checkout size

        os_mbuf_append(ctxt->om, wifi_ssid, sizeof(wifi_ssid));                         // "Respone"
        
        xTaskCreate(blink_task, "blink_task", 1000, NULL, 1, NULL);                     // Create task for blink led

    }
    else if(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR){                                   // Response current ssid variable to BLE stack
        os_mbuf_append(ctxt->om, wifi_ssid, sizeof(wifi_ssid));
    }
    return 0;
}

static int gatt_svr_wifi_pass(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){                                       // Similar to gatt_svr_wifi_ssid()
        memset(&wifi_pass[0], 0, sizeof(wifi_pass));
        memcpy(wifi_ssid, ctxt->om->om_data, ctxt->om->om_len>50?50:ctxt->om->om_len);

        os_mbuf_append(ctxt->om, wifi_pass, sizeof(wifi_pass));
        
        xTaskCreate(blink_task, "blink_task", 1000, NULL, 1, NULL);

    }
    else if(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR){
        os_mbuf_append(ctxt->om, wifi_pass, sizeof(wifi_pass));
    }
    return 0;
}


static int gatt_svr_acc(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    char buffer[3];                                      // Similar to gatt_svr_buttery_lvl()
    sprintf(buffer, "%02X", acc_lvl);

    os_mbuf_append(ctxt->om, buffer, sizeof(buffer));
    return 0;
}

static int gatt_svr_gyro(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    char buffer[3];                                     // Similar to gatt_svr_buttery_lvl()
    sprintf(buffer, "%02X", gyro_lvl);

    os_mbuf_append(ctxt->om, buffer, sizeof(buffer));
    return 0;
}

static int gatt_svr_gps(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    char buffer[8];                                     // Similar to gatt_svr_buttery_lvl()
    sprintf(buffer, "%07llX", gps_pos);

    os_mbuf_append(ctxt->om, buffer, sizeof(buffer));
    return 0;
}

static int gatt_svr_led(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    char buffer[10];                                    // Similar to gatt_svr_buttery_lvl()
    sprintf(buffer, "LED:%s", led_state?"True":"False");

    os_mbuf_append(ctxt->om, buffer, sizeof(buffer));
    return 0;
}

//define characteristics table  For a better understanding of this, I recommend reviewing the nimBLE datasheet.
static const struct ble_gatt_svc_def gat_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID128_DECLARE(0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0x00, 0x00, 0x00, 0x01),
     .characteristics = (struct ble_gatt_chr_def[]){
         {.uuid = BLE_UUID16_DECLARE(0x0001),
          .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
          .val_handle = &buttery_lvl_handle,
          .access_cb = gatt_svr_buttery_lvl},
         {.uuid = BLE_UUID16_DECLARE(0x0002),
          .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
          .access_cb = gatt_svr_wifi_ssid},
         {.uuid = BLE_UUID16_DECLARE(0x0003),
          .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
          .access_cb = gatt_svr_wifi_pass},
         {0}}},
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid =  BLE_UUID128_DECLARE(0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0x00, 0x00, 0x00, 0x02),
     .characteristics = (struct ble_gatt_chr_def[]){
         {.uuid = BLE_UUID16_DECLARE(0x0001),
          .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
          .val_handle = &acc_lvl_handle,
          .access_cb = gatt_svr_acc},
         {.uuid = BLE_UUID16_DECLARE(0x0002),
          .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
          .val_handle = &gyro_lvl_handle,
          .access_cb = gatt_svr_gyro},
         {.uuid = BLE_UUID16_DECLARE(0x0003),
          .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
          .val_handle = &gps_pos_handle,
          .access_cb = gatt_svr_gps},
         {.uuid = BLE_UUID16_DECLARE(0x0004),
          .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY,
          .val_handle = &led_state_handle,
          .access_cb = gatt_svr_led},
         {0}}},
    {0}};


// define ble gap events handler
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)        // Handling events from BLE stack and proccessing
    {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("GAP", "BLE_GAP_EVENT_CONNECT %s", event->connect.status == 0 ? "OK" : "Failed");
        if (event->connect.status != 0)
        {
            ble_app_advertise();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("GAP", "BLE_GAP_EVENT_DISCONNECT");
        ble_app_advertise();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI("GAP", "BLE_GAP_EVENT_ADV_COMPLETE");
        ble_app_advertise();
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI("GAP", "BLE_GAP_EVENT_SUBSCRIBE");
        break;
    default:
        break;
    }
    return 0;
}


// define supporting function for ble stack (default setup for advertise, don`t touch -_- )
void ble_app_advertise(void)
{
    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));

    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_DISC_LTD;
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.name = (uint8_t *)ble_svc_gap_device_name();
    fields.name_len = strlen(ble_svc_gap_device_name());
    fields.name_is_complete = 1;

    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

// define supporting function for ble stack
void ble_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type);
    ble_app_advertise();
}

// define supporting function for ble stack
void host_task(void *param)
{
    nimble_port_run();
}

// MAIN!!!!
void app_main(void)
{

    gpio_reset_pin(BLINK_GPIO);                             // Configure GPIO for led pin
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    esp_nimble_hci_and_controller_init();                   // Start nimBLE server, and config. I recommend not to touch
    nimble_port_init();

    ble_svc_gap_device_name_set(DEVICE_NAME);
    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_gatts_count_cfg(gat_svcs);
    ble_gatts_add_svcs(gat_svcs);

    ble_hs_cfg.sync_cb = ble_app_on_sync;       
    nimble_port_freertos_init(host_task);                   // Up to here)

    // Create sub-tasks for parallel execution
    xTaskCreate(acc_vtask, "acc_task", 1000, NULL, 1, NULL);
    xTaskCreate(gyro_vtask, "gyro_task", 1000, NULL, 1, NULL);
    xTaskCreate(gps_vtask, "gps_task", 1000, NULL, 1, NULL);
    xTaskCreate(led_vtask, "led_task", 1000, NULL, 1, &led_task_handle); 

    xTaskCreate(batt_wifi_data_vtask, "batt_wifi_data_task", 1000, NULL, 1, NULL);

}
