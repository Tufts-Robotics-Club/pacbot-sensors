#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_websocket_client.h"
#include "lwip/sockets.h"

#include "config.h"
#include "comms.h"
#include "imu.h"
#include "motor.h"

static const char *TAG = "pacbot";

// we connect out to the server on the laptop, same as the club's arduino client
static esp_websocket_client_handle_t client;

// real queue for replies so none get lost
static QueueHandle_t reply_queue;

static EventGroupHandle_t wifi_events;
#define WIFI_GOT_IP BIT0

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_events, WIFI_GOT_IP);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "esp32 ip: " IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(wifi_events, WIFI_GOT_IP);
    }
}

static void wifi_start(void)
{
    wifi_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));

    // dont save wifi settings to flash since writing flash freezes both cores for up to 30 ms
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    wifi_config_t cfg = {
        .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    // wifi power saving adds a ton of lag so turn it off
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}

// the websocket library has no setting to turn off nagle (bundling small messages together)
// so we find its socket ourselves, the one talking to the server, and turn it off there
// without this pings took ~58 ms round trip and the imu messages showed up in clumps
static void ws_no_bundling(void)
{
    for (int fd = LWIP_SOCKET_OFFSET; fd < LWIP_SOCKET_OFFSET + CONFIG_LWIP_MAX_SOCKETS; fd++) {
        struct sockaddr_in peer;
        socklen_t len = sizeof(peer);
        if (getpeername(fd, (struct sockaddr *)&peer, &len) != 0 || peer.sin_family != AF_INET) {
            continue;
        }
        if (peer.sin_port == htons(SERVER_PORT) && peer.sin_addr.s_addr == inet_addr(SERVER_IP)) {
            int one = 1;
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
            ESP_LOGI(TAG, "turned off bundling on socket %d", fd);
            return;
        }
    }
    ESP_LOGW(TAG, "couldnt find the server socket to turn off bundling");
}

static void ws_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    esp_websocket_event_data_t *e = data;

    switch (id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "connected to server!");
        ws_no_bundling();
        comms_send("Hello from ESP32!");
        break;

    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "disconnected from server, trying again in %d ms", WS_RECONNECT_MS);
        break;

    case WEBSOCKET_EVENT_DATA:
        // only text (op code 1) that came in one piece and fits in our buffer
        if (e->op_code == 0x1 && e->payload_offset == 0 && e->data_len == e->payload_len && e->data_len < MSG_LEN) {
            char buf[MSG_LEN];
            memcpy(buf, e->data_ptr, e->data_len);
            buf[e->data_len] = '\0';
            ESP_LOGI(TAG, "server says: %s", buf);
            motor_command(buf);
        }
        break;

    default:
        break;
    }
}

static void ws_start(void)
{
    esp_websocket_client_config_t cfg = {
        .host = SERVER_IP,
        .port = SERVER_PORT,
        .path = "/",
        .transport = WEBSOCKET_TRANSPORT_OVER_TCP,
        .reconnect_timeout_ms = WS_RECONNECT_MS,
        // its own task goes on core 0 with the rest of the wifi stuff
        .task_core_id_set = true,
        .task_core_id = COMMS_CORE,
    };
    client = esp_websocket_client_init(&cfg);
    ESP_ERROR_CHECK(esp_websocket_register_events(client, WEBSOCKET_EVENT_ANY, ws_event_handler, NULL));

    // fine to start before wifi is up, it just keeps retrying until it gets through
    ESP_ERROR_CHECK(esp_websocket_client_start(client));
}

// waits for something to send then sends it if were connected
static void reply_task(void *arg)
{
    char msg[MSG_LEN];
    while (1) {
        xQueueReceive(reply_queue, msg, portMAX_DELAY);
        if (esp_websocket_client_is_connected(client)) {
            esp_websocket_client_send_text(client, msg, strlen(msg), pdMS_TO_TICKS(100));
        }
    }
}

// sends the newest imu reading to the server in the same json as the club's imu code
// {"t":ms,"a":[x,y,z],"g":[x,y,z]} with accel in mg and gyro in deg/s
static void imu_send_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();
    char msg[MSG_LEN];
    imu_data_t d;

    while (1) {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000 / IMU_SEND_HZ));

        if (!esp_websocket_client_is_connected(client) || !imu_latest(&d)) {
            continue;
        }

        int len = snprintf(msg, sizeof(msg), "{\"t\":%lld,\"a\":[%.1f,%.1f,%.1f],\"g\":[%.2f,%.2f,%.2f]}",
                           d.t_us / 1000,
                           d.accel[0] * 1000, d.accel[1] * 1000, d.accel[2] * 1000,
                           d.gyro[0], d.gyro[1], d.gyro[2]);
        esp_websocket_client_send_text(client, msg, len, pdMS_TO_TICKS(20));
    }
}

void comms_start(void)
{
    reply_queue = xQueueCreate(8, MSG_LEN);
    wifi_start();
    ws_start();
    xTaskCreatePinnedToCore(reply_task, "reply", STACK_REPLY, NULL, PRIO_REPLY, NULL, COMMS_CORE);

    if (SEND_IMU) {
        xTaskCreatePinnedToCore(imu_send_task, "imu_send", STACK_IMU_SEND, NULL, PRIO_IMU_SEND, NULL, COMMS_CORE);
    }
}

// just drops it in the queue so motor_task never has to touch the network
void comms_send(const char *text)
{
    char buf[MSG_LEN];
    strlcpy(buf, text, sizeof(buf));
    xQueueSend(reply_queue, buf, 0);
}
