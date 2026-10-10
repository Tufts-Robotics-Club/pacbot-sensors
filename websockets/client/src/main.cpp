#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <secrets.h>

WebSocketsClient webSocket;

void onWebSocketEvent(
    WStype_t type,
    uint8_t* payload,
    size_t length
) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.println("Connected to server!");
            webSocket.sendTXT("Hello from ESP32!");
            break;

        case WStype_DISCONNECTED:
            Serial.println("Disconnected from server");
            break;

        case WStype_TEXT:
            Serial.print("Server says: ");
            Serial.write(payload, length);
            Serial.println();
            break;

        default:
            break;
    }
}

void setup() {
    Serial.begin(115200);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to Wi-Fi...");
    }

    Serial.print("Connected to ");
    Serial.println(WIFI_SSID);

    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());

    webSocket.begin(SERVER_IP, SERVER_PORT, "/");
    webSocket.onEvent(onWebSocketEvent);
    webSocket.setReconnectInterval(3000);
}

void loop() {
    webSocket.loop();
}