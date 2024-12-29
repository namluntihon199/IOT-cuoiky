#include <Arduino.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include "secrets/wifi.h"
#include "wifi_connect.h"
#include <WiFiClientSecure.h>
#include "ca_cert.h"
#include "secrets/mqtt.h"
#include <PubSubClient.h>
#include <Ticker.h>

float temperature = 0;
float humidity = 0;
float dustDensity = 0;
float lightLevel = 0;

#define DHTPIN 19      // Chân kết nối DHT11
#define DHTTYPE DHT11  // Loại cảm biến
#define LED_PIN 25     // Chân PWM điều khiển độ sáng đèn
#define BUZZER_PIN 21  // Chân điều khiển còi
#define DUST_LED 32    // Chân điều khiển LED của cảm biến bụi
#define DUST_VO 34     // Chân đọc tín hiệu cảm biến bụi (ADC)
#define LIGHT_SENSOR_PIN 33 // Chân đọc cảm biến ánh sáng (ADC)

DHT dht(DHTPIN, DHTTYPE);

namespace {
    const char *ssid = WiFiSecrets::ssid;
    const char *password = WiFiSecrets::pass;
    const char *Temp_topic = "temperature";
    const char *Hum_topic = "humidity";
    const char *Dust_topic = "dustdensity";
    const char *Light_topic = "light level";
    const char *led_brightness = "led";  // Chủ đề MQTT để điều khiển độ sáng đèn
    const char *buzzer = "dcbuzzer";     // Chủ đề MQTT để điều khiển còi
    unsigned int publish_count = 0;
    uint16_t keepAlive = 15;    // seconds (default is 15)
    uint16_t socketTimeout = 5; // seconds (default is 15)
}

WiFiClientSecure tlsClient;
PubSubClient mqttClient(tlsClient);

Ticker mqttPulishTicker;

void mqttTempsensorPublish()
{
    // Đọc dữ liệu từ cảm biến DHT11
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();

    // Kiểm tra dữ liệu cảm biến
    if (isnan(temperature) || isnan(humidity)) {
        Serial.println("Failed to read from DHT sensor!");
    }

    // Đọc dữ liệu từ cảm biến bụi
    digitalWrite(DUST_LED, HIGH);
    delayMicroseconds(320);
    int dustADC = analogRead(DUST_VO);
    digitalWrite(DUST_LED, LOW);
    delayMicroseconds(280);

    float voltage = dustADC * (3.3 / 4095.0); // Chuyển đổi giá trị ADC sang điện áp
    dustDensity = (voltage - 0.9) * 1000 / 5.0; // Công thức tính mật độ bụi
    if (dustDensity < 0) dustDensity = 0;

    // Đọc dữ liệu từ cảm biến ánh sáng
    int lightADC = analogRead(LIGHT_SENSOR_PIN);
    lightLevel = lightADC * (100 / 4095.0); // Chuyển đổi giá trị ADC sang điện áp

    // Đăng tải dữ liệu lên MQTT
    mqttClient.publish(Temp_topic, String(temperature).c_str(), false);
    mqttClient.publish(Hum_topic, String(humidity).c_str(), false);
    mqttClient.publish(Dust_topic, String(dustDensity).c_str(), false);
    mqttClient.publish(Light_topic, String(lightLevel).c_str(), false);

    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.print(" °C | Humidity: ");
    Serial.print(humidity);
    Serial.print(" % | Dust Density: ");
    Serial.print(dustDensity);
    Serial.print(" µg/m³ | Light Level: ");
    Serial.print(lightLevel);
    Serial.println("%");
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    if (strcmp(topic, led_brightness) == 0)
    {
        char brightnessStr[length + 1];
        memcpy(brightnessStr, payload, length);
        brightnessStr[length] = '\0';
        int brightness = atoi(brightnessStr);  // Chuyển đổi từ chuỗi sang giá trị số

        // Điều khiển độ sáng đèn LED
        Serial.print("LED brightness: ");
        Serial.println(brightness);
        
        // Áp dụng giá trị PWM cho LED (LED_PIN)
        analogWrite(LED_PIN, brightness * 255 / 10);  // Độ sáng nhận được là từ 0 đến 10, chuyển thành giá trị PWM 0-255
    }
    if (strcmp(topic, buzzer) == 0)
    {
        char commandStr[length + 1];
        memcpy(commandStr, payload, length);
        commandStr[length] = '\0';
        int command = atoi(commandStr);  // Chuyển đổi từ chuỗi sang giá trị số
        digitalWrite(BUZZER_PIN, command);
    }
}

void mqttReconnect()
{
    while (!mqttClient.connected())
    {
        Serial.println("Attempting MQTT connection...");
        String client_id = "esp32-client-";
        client_id += String(WiFi.macAddress());
        if (mqttClient.connect(client_id.c_str(), MQTT::username, MQTT::pass))
        {
            Serial.print(client_id);
            Serial.println(" connected");
            mqttClient.subscribe(led_brightness);  // Đăng ký nhận tin nhắn từ chủ đề điều khiển độ sáng LED
            mqttClient.subscribe(buzzer);          // Đăng ký nhận tin nhắn từ chủ đề điều khiển còi
        }
        else
        {
            Serial.print("MQTT connect failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 1 seconds");
            delay(1000);
        }
    }
}

void setup()
{
    Serial.begin(115200);
    delay(10);
    setup_wifi(ssid, password);
    tlsClient.setCACert(ca_cert);

    mqttClient.setCallback(mqttCallback);
    mqttClient.setServer(MQTT::broker, MQTT::port);
    mqttPulishTicker.attach(1, mqttTempsensorPublish);  // Đặt thời gian cập nhật cảm biến

    dht.begin();
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(DUST_LED, OUTPUT);
}

void loop()
{
    if (!mqttClient.connected())
    {
        mqttReconnect();
    }
    mqttClient.loop();  // Lắng nghe các tin nhắn MQTT
}
