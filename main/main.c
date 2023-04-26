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


#define DEVICE_NAME "MY TEST BLE DEVICE"
uint8_t ble_addr_type;
void ble_app_advertise(void);

char * wifi_response_to_ble = "";

uint32_t buttery_lvl = 0;
char wifi_ssid[50];
char wifi_pass[50];
uint16_t buttery_lvl_handle;

#define BLINK_GPIO GPIO_NUM_2

uint32_t acc_lvl = 0;
uint32_t gyro_lvl = 0;
uint64_t gps_pos = 0;
bool led_state = 0;

TaskHandle_t led_task_handle = NULL;

uint16_t acc_lvl_handle;
uint16_t gyro_lvl_handle;
uint16_t gps_pos_handle;
uint16_t led_state_handle;

uint64_t map(uint64_t x, uint64_t in_min, uint64_t in_max, uint64_t out_min, uint64_t out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void getRandomStr(char* output, int len){
    char* chars = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz1234567890";
    for(int i = 0; i< len; i++){
        uint32_t rand_int = esp_random();
        int random_index = map(rand_int, 0, 0xFFFFFFFF, 0, strlen(chars));
        output[i] = chars[random_index];
    }
}

void blink_task(void *pvParameter){
    vTaskSuspend(led_task_handle);
    for(int i = 0; i < 20; i++){
        vTaskDelay(500 / portTICK_PERIOD_MS);
        led_state = 0;
        ble_gatts_chr_updated(led_state_handle);
        gpio_set_level(BLINK_GPIO, led_state);


        vTaskDelay(500 / portTICK_PERIOD_MS);
        led_state = 1;
        ble_gatts_chr_updated(led_state_handle);
        gpio_set_level(BLINK_GPIO, led_state);
    }
    vTaskResume(led_task_handle);
    vTaskDelete(NULL);
    return 0;
}

void acc_vtask(void *pvParameter){
    while(1){
        uint32_t value = esp_random();
        acc_lvl = map(value, 0, 0xFFFFFFFF, 0, 0x00FFFFFF);

        ble_gatts_chr_updated(acc_lvl_handle);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void gyro_vtask(void *pvParameter){
    while(1){
        uint32_t value = esp_random();
        gyro_lvl = map(value, 0, 0xFFFFFFFF, 0, 0x00FFFFFF);

        ble_gatts_chr_updated(gyro_lvl_handle);

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void gps_vtask(void *pvParameter){
    while(1){
        uint32_t value_l = esp_random();
        uint32_t value_h = esp_random();
        gps_pos = (((uint32_t) value_l) << 32) | value_h;

        ble_gatts_chr_updated(gps_pos_handle);

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void led_vtask(void *pvParameter){
    while(1){
        vTaskDelay(8000 / portTICK_PERIOD_MS);

        led_state = 1; 
        ble_gatts_chr_updated(led_state_handle);
        gpio_set_level(BLINK_GPIO, led_state);

        vTaskDelay(2000 / portTICK_PERIOD_MS);   

        led_state = 0;
        ble_gatts_chr_updated(led_state_handle);
        gpio_set_level(BLINK_GPIO, led_state);
    }
}

void batt_wifi_data_vtask(void *pvParameter){
    while(1){
        getRandomStr(wifi_ssid, 50);
        getRandomStr(wifi_pass, 50);   

        uint32_t value = esp_random();
        buttery_lvl = map(value, 0, 0xFFFFFFFF, 0, 100);

        ble_gatts_chr_updated(buttery_lvl_handle);

        vTaskDelay(1000 * 60 / portTICK_PERIOD_MS);  
    }
}



static int gatt_svr_buttery_lvl(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    char buffer[4];
    sprintf(buffer, "%d", buttery_lvl);

    os_mbuf_append(ctxt->om, buffer, sizeof(buffer));
    return 0;
}

static int gatt_svr_wifi_ssid(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){
        memset(&wifi_ssid[0], 0, sizeof(wifi_ssid));
        memcpy(wifi_ssid, ctxt->om->om_data, ctxt->om->om_len>50?50:ctxt->om->om_len);

        os_mbuf_append(ctxt->om, wifi_ssid, sizeof(wifi_ssid));
        
        xTaskCreate(blink_task, "blink_task", 1000, NULL, 1, NULL);

    }
    else if(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR){
        os_mbuf_append(ctxt->om, wifi_ssid, sizeof(wifi_ssid));
    }
    return 0;
}

static int gatt_svr_wifi_pass(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    if(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR){
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
    char buffer[16];
    sprintf(buffer, "%zu", acc_lvl);

    os_mbuf_append(ctxt->om, buffer, sizeof(buffer));
    return 0;
}

static int gatt_svr_gyro(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    char buffer[16];
    sprintf(buffer, "%zu", gyro_lvl);

    os_mbuf_append(ctxt->om, buffer, sizeof(buffer));
    return 0;
}

static int gatt_svr_gps(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    char buffer[32];
    sprintf(buffer, "%" PRIu64, gps_pos);

    os_mbuf_append(ctxt->om, buffer, sizeof(buffer));
    return 0;
}

static int gatt_svr_led(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg){
    char buffer[10];
    sprintf(buffer, "LED:%s", led_state?"True":"False");

    os_mbuf_append(ctxt->om, buffer, sizeof(buffer));
    return 0;
}

static const struct ble_gatt_svc_def gat_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = BLE_UUID32_DECLARE(0x0010000),
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
     .uuid = BLE_UUID32_DECLARE(0x0020000),
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


static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
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

void ble_app_on_sync(void)
{
    ble_hs_id_infer_auto(0, &ble_addr_type);
    ble_app_advertise();
}

void host_task(void *param)
{
    nimble_port_run();
}

void app_main(void)
{

    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);

    nvs_flash_init();

    esp_nimble_hci_and_controller_init();
    nimble_port_init();

    ble_svc_gap_device_name_set(DEVICE_NAME);
    ble_svc_gap_init();
    ble_svc_gatt_init();

    ble_gatts_count_cfg(gat_svcs);
    ble_gatts_add_svcs(gat_svcs);

    ble_hs_cfg.sync_cb = ble_app_on_sync;
    nimble_port_freertos_init(host_task);

    xTaskCreate(acc_vtask, "acc_task", 1000, NULL, 1, NULL);
    xTaskCreate(gyro_vtask, "gyro_task", 1000, NULL, 1, NULL);
    xTaskCreate(gps_vtask, "gps_task", 1000, NULL, 1, NULL);
    xTaskCreate(led_vtask, "led_task", 1000, NULL, 1, &led_task_handle);

    xTaskCreate(batt_wifi_data_vtask, "batt_wifi_data_task", 1000, NULL, 1, NULL);

}
