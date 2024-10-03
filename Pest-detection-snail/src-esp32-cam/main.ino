#include "http_camera.h"
#include "wifi_config.h"
#include "camera_pins.h"

WiFiConfig wifiConfig;
HTTPCamera camera;
httpd_handle_t server = NULL;

void setup() {

  Serial.begin(115200);
  wifiConfig.begin();

  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW);

  // Start the HTTP server
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  if (httpd_start(&server, &config) == ESP_OK) {
    wifiConfig.startConfigServer(server);
    camera.begin(server); 
  } else {
    Serial.println("Error starting HTTP server");
  }
}

void loop() {

  wifiConfig.handleClient();
  
}