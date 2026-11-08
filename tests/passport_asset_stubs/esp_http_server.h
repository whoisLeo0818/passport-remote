#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#define HTTPD_400_BAD_REQUEST 400
#define HTTPD_404_NOT_FOUND 404
#define HTTPD_500_INTERNAL_SERVER_ERROR 500
#define HTTPD_SOCK_ERR_TIMEOUT -2
typedef struct {size_t content_len,at,fail_at;const uint8_t *body;int status,timeouts;} httpd_req_t;
int httpd_req_recv(httpd_req_t *,char *,size_t);
esp_err_t httpd_resp_send_err(httpd_req_t *,int,const char *);
esp_err_t httpd_resp_set_type(httpd_req_t *,const char *);
esp_err_t httpd_resp_sendstr(httpd_req_t *,const char *);
esp_err_t httpd_resp_send(httpd_req_t *,const char *,size_t);
esp_err_t httpd_resp_send_chunk(httpd_req_t *,const char *,size_t);
