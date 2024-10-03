#ifndef HTTP_CAMERA_H
#define HTTP_CAMERA_H

#include "esp_camera.h"
#include <esp_http_server.h>

class HTTPCamera {
public:
  HTTPCamera();
  void begin(httpd_handle_t server);
  static esp_err_t stream_handler(httpd_req_t *req);
  static esp_err_t toggle_light_handler(httpd_req *req);

private:
  void startCameraServer(httpd_handle_t server);
};

#endif //HTTP_CAMERA_H