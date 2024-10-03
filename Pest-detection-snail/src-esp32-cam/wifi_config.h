#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <EEPROM.h>
#include <WiFi.h>
#include <esp_http_server.h>

class WiFiConfig {
public:
  WiFiConfig();
  void begin();
  void handleClient();
  void startConfigServer(httpd_handle_t server);
  static esp_err_t config_handler(httpd_req_t *req);
  static esp_err_t config_page_handler(httpd_req_t *req);
  static void str_replace(char *target, const char *old, const char *new_val);

private:
  void setupAPMode();
void print_stored_wifi_credentials();

char ssid[32];
char password[64];
bool is_config_mode;
};

#endif // WIFI_CONFIG_H