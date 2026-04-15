/**
 * esp32_bridge.ino — micro-ROS UART↔UDP WiFi bridge
 * Board: ESP32-S3 Dev Module, USB Mode: USB-OTG (TinyUSB)
 *
 * STM32 USART6 (PC6/PC7) ↔ ESP32-S3 GPIO17/18 ↔ WiFi UDP → Agent
 */

#include <WiFi.h>
#include <WiFiUdp.h>

const char* WIFI_SSID  = "Auro_IoT";
const char* WIFI_PASS  = "***REDACTED***";
const char* AGENT_IP   = "10.10.9.249";
const int   AGENT_PORT = 8888;
const int   LOCAL_PORT = 8888;

const int   UART_RX    = 17;  // ← PC6 (STM32 USART6 TX)
const int   UART_TX    = 18;  // → PC7 (STM32 USART6 RX)
const int   UART_BAUD  = 921600;

WiFiUDP udp;
uint8_t buf[4096];

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("=== micro-ROS UART-UDP bridge ===");

    Serial1.begin(UART_BAUD, SERIAL_8N1, UART_RX, UART_TX);
    Serial.printf("UART: RX=GPIO%d TX=GPIO%d @ %d\n", UART_RX, UART_TX, UART_BAUD);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.printf("WiFi: %s ", WIFI_SSID);
    while (WiFi.status() != WL_CONNECTED) { delay(200); Serial.print("."); }
    Serial.printf("\nIP: %s\n", WiFi.localIP().toString().c_str());

    udp.begin(LOCAL_PORT);
    Serial.printf("UDP → %s:%d\nBridge running.\n", AGENT_IP, AGENT_PORT);
}

void loop() {
    int n = Serial1.available();
    if (n > 0) {
        n = min(n, (int)sizeof(buf));
        Serial1.readBytes(buf, n);
        udp.beginPacket(AGENT_IP, AGENT_PORT);
        udp.write(buf, n);
        udp.endPacket();
    }

    int p = udp.parsePacket();
    if (p > 0) {
        int r = udp.read(buf, sizeof(buf));
        if (r > 0) Serial1.write(buf, r);
    }
}
