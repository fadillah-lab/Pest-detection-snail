// #include <Arduino.h>
#include <WiFi.h>
#include "wifi_config.h"

#define EEPROM_SIZE 96
#define SSID_ADDR 0
#define PASSWORD_ADDR 32

// Static IP configuration
IPAddress local_IP(192,168,100,9); // Change to your desired static IP
IPAddress gateway(192,168,100,1);    // Change to your network's gateway
IPAddress subnet(255,255,255,0);   // Change to your network's subnet mask

WiFiConfig::WiFiConfig()
: is_config_mode(false) {
}

void WiFiConfig::begin() {
  EEPROM.begin(EEPROM_SIZE);

  //
  EEPROM.get(SSID_ADDR, ssid);
  EEPROM.get(PASSWORD_ADDR, password);

  //
  print_stored_wifi_credentials();

  //
  if (ssid[0] == 0xFF || ssid[0] == 0x00) {
    setupAPMode();
    return;
  }

    // Configure static IP
  if (!WiFi.config(local_IP, gateway, subnet)) {
    Serial.println("STA Failed to configure");
  }

  //
  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    setupAPMode();
  }else {
    Serial.println("");
    Serial.println("WiFi Connected"); 
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Config URL: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/config");
  }
}

void WiFiConfig::handleClient() {
  //
}

void WiFiConfig::setupAPMode() {
  is_config_mode = true;
  WiFi.softAP("ESP32_Config");
  delay(100);
  Serial.println("Setting up AP mode");
  Serial.print("AP IP address");
  Serial.println(WiFi.softAPIP());
  Serial.print("Config URL: http://");
  Serial.print(WiFi.softAPIP());
  Serial.println("/config");
}

esp_err_t WiFiConfig::config_handler(httpd_req_t *req) {
  char content[100];
  int ret, len = httpd_req_recv(req, content, sizeof(content));
  if (len <= 0) {
    if (len == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }

  //
  content[len] = '\0';
  char ssid[32] = { 0 };
  char password[64] = { 0 };
  char *ssid_ptr = strstr(content, "ssid=");
  char *password_ptr = strstr(content, "password=");

  if (ssid_ptr && password_ptr) {
    ssid_ptr += 5;
    password_ptr += 9;

    char *ssid_end = strstr(ssid_ptr, "&");
    if (ssid_end) {
      *ssid_end = '\0';
    }
  
    strncpy(ssid, ssid_ptr, sizeof(ssid) - 1);
    strncpy(password, password_ptr, sizeof(password) - 1);

    //
    str_replace(ssid, "+", " ");
    str_replace(password, "+", " ");

    //
    Serial.print("Received SSID: ");
    Serial.println(ssid);
    Serial.print("Received Password: ");
    Serial.println(password);

    EEPROM.put(SSID_ADDR, ssid);
    EEPROM.put(PASSWORD_ADDR, password);
    EEPROM.commit();

    httpd_resp_sendstr(req, "Configuration saved, Restart the device.");
  } else {
    httpd_resp_sendstr(req, "Invalid input");
  }
  
  return ESP_OK;
}

esp_err_t WiFiConfig::config_page_handler(httpd_req_t *req) {
  const char *html_form = "<!DOCTYPE html><html><body>"
                          "<h1>Configure WiFi</h1>"
                          "<form action=\"/config\" method=\"post\">"
                          "<SSID:<br><input type=\"text\" name=\"ssid\"><br>"
                          "Password:<br><input type=\"text\" name=\"password\"><br><br>"
                          "<input type=\"submit\" value=\"Submit\">"
                          "</form></body></html>";

  httpd_resp_send(req, html_form, strlen(html_form));
  return ESP_OK;
}

void WiFiConfig::startConfigServer(httpd_handle_t server) {
  httpd_uri_t config_uri = {
        .uri = "/config",
        .method = HTTP_POST,
        .handler = config_handler,
        .user_ctx = NULL
};

  httpd_uri_t config_page_uri = {
        .uri = "/config",
        .method = HTTP_GET,
        .handler = config_page_handler,
        .user_ctx = NULL
};

if (httpd_register_uri_handler(server, &config_uri) == ESP_OK) {
    Serial.println("Config URI registered"); 
  } else {
    Serial.println("Error registering config URI");
  }
if (httpd_register_uri_handler(server, &config_page_uri) == ESP_OK) {
    Serial.println("Config page URI registered"); 
  } else {
    Serial.println("Error registering config page URI");
  }  
}

//
void WiFiConfig::print_stored_wifi_credentials() {
  Serial.print("Stored SSID: ");
  Serial.println(ssid);
  Serial.print("Stored Password: ");
  Serial.println(password);
}

void WiFiConfig::str_replace(char *target, const char *old, const char *new_val) {
  char buffer[256];
  char *insert_point = &buffer[0];
  const char *tmp = target;
  size_t old_len = strlen(old);
  size_t new_len = strlen(new_val);

  while (true) {
    const char *p = strstr(tmp, old);
    if(p == NULL) {
      strcpy(insert_point, tmp);
      break;
    }
    memcpy(insert_point, tmp, p - tmp);
    insert_point += p - tmp;
    memcpy(insert_point, new_val, new_len);
    insert_point += new_len;
    tmp = p + old_len;
  }
  strcpy(target, buffer);
}