#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_http_server.h>
#include <ArduinoWebsockets.h>
#include "camera_pins.h"

using namespace websockets;

#define RELAY_PIN 12

WebsocketsClient client;

class HTTPCamera {
public:
    HTTPCamera();
    void begin(httpd_handle_t server);

private:
    void startCameraServer(httpd_handle_t server);
    static esp_err_t stream_handler(httpd_req_t *req);
    // static esp_err_t toggle_light_handler(httpd_req_t *req);
    static esp_err_t toggle_relay_handler(httpd_req_t *req);
};

HTTPCamera::HTTPCamera() {}

void HTTPCamera::begin(httpd_handle_t server) {
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    Serial.println();

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_VGA; // Adjust the frame size as needed
    config.jpeg_quality = 10; // Adjust the JPEG quality (0-63, lower means higher quality)
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }

    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);
  
    pinMode(FLASH_GPIO_NUM, OUTPUT);
    digitalWrite(FLASH_GPIO_NUM, LOW);

    startCameraServer(server);

    Serial.print("Camera Ready! Use 'http://");
    Serial.print(WiFi.localIP());
    Serial.println("/stream' to view the stream");

    // Connect to WebSocket server
    client.onMessage([](WebsocketsMessage message) {
        Serial.print("Got Message: ");
        Serial.println(message.data());

        // if (message.data() == "toggle_relay") {
        //     Serial.println("Object Detected...! Relay active for 2 seconds");
        //     digitalWrite(RELAY_PIN, LOW); // Activate relay
        //     delay(5000); // Keep relay on for 5 seconds
        //     digitalWrite(RELAY_PIN, HIGH); // Deactivate relay
        // }
    });

    client.onEvent([](WebsocketsEvent event, String data) {
        if (event == WebsocketsEvent::ConnectionOpened) {
            Serial.println("Connected to WebSocket server");
        } else if (event == WebsocketsEvent::ConnectionClosed) {
            Serial.println("Disconnected from WebSocket server");
        } else if (event == WebsocketsEvent::GotPing) {
            client.pong();
        }
    });

    // client.connect("ws://10.148.0.3:5000/socket.io/");
    // client.connect("ws://192.168.100.10:5000");
}

void HTTPCamera::startCameraServer(httpd_handle_t server) {
    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL
    };

    if (httpd_register_uri_handler(server, &stream_uri) != ESP_OK) {
        Serial.println("Failed to register stream URI");
    }

    httpd_uri_t toggle_relay_uri = {
        .uri = "/toggle-relay",
        .method = HTTP_GET,
        .handler = toggle_relay_handler,
        .user_ctx = NULL
    };

    if (httpd_register_uri_handler(server, &toggle_relay_uri) != ESP_OK) {
        Serial.println("Failed to register toggle-relay URI");
    }
}

esp_err_t HTTPCamera::stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[64];

    res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
    if (res != ESP_OK) {
        return res;
    }

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        } else {
            if (fb->format != PIXFORMAT_JPEG) {
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb);
                fb = NULL;
                if (!jpeg_converted) {
                    Serial.println("JPEG compression failed");
                    res = ESP_FAIL;
                }
            } else {
                _jpg_buf_len = fb->len;
                _jpg_buf = fb->buf;
            }
        }
        if (res == ESP_OK) {
            size_t hlen = snprintf(part_buf, 64, "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", _jpg_buf_len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, "\r\n--frame\r\n", 12);
        }
        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if (_jpg_buf) {
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if (res != ESP_OK) {
            break;
        }
        delay(10); 
    }
    return res;
}

esp_err_t HTTPCamera::toggle_relay_handler(httpd_req_t *req) {
    Serial.println("Object Detected...! Relay active for 5 seconds");
    digitalWrite(RELAY_PIN, LOW); // Activate relay
    delay(5000); // Keep relay on for 5 seconds
    digitalWrite(RELAY_PIN, HIGH); // Deactivate relay

    httpd_resp_send(req, "Relay toggled", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}
