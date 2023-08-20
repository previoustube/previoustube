//   SPDX-FileCopyrightText: 2023 Ian Levesque <ian@ianlevesque.org>
//   SPDX-License-Identifier: MIT
//
//

#include "webserver.h"
#include <esp_event.h>
#include <esp_http_server.h>
#include <sys/param.h>

ESP_EVENT_DECLARE_BASE(WEBSERVER_EVENTS);

enum {
  WEBSERVER_EVENT_WEBHOOK,
};

const size_t MAX_BODY_SIZE = 256;

static webhook_callback_t s_webhook_callback = nullptr;

auto get_handler(httpd_req_t *req) -> esp_err_t {
  const char *resp = "OK";
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

struct sized_event_data {
  uint8_t content[MAX_BODY_SIZE];
  size_t length;
};

auto webhook_handler(httpd_req_t *req) -> esp_err_t {
  sized_event_data event_data{};

  /* Truncate if content length larger than the buffer */
  size_t recv_size = MIN(req->content_len, sizeof(event_data.content));

  int ret = httpd_req_recv(req, reinterpret_cast<char *>(event_data.content),
                           recv_size);
  if (ret <= 0) {
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_408(req);
    }
    return ESP_FAIL;
  }

  event_data.length = recv_size;

  ESP_ERROR_CHECK(esp_event_post(WEBSERVER_EVENTS, WEBSERVER_EVENT_WEBHOOK,
                                 &event_data, sizeof(event_data),
                                 portMAX_DELAY));

  /* Send a simple response */
  const char *resp = "OK";
  httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

static const httpd_uri_t uri_get = {.uri = "/",
                                    .method = HTTP_GET,
                                    .handler = get_handler,
                                    .user_ctx = nullptr};

static const httpd_uri_t uri_webhook = {.uri = "/webhook",
                                        .method = HTTP_POST,
                                        .handler = webhook_handler,
                                        .user_ctx = nullptr};

static void webhook_event_handler([[maybe_unused]] void *handler_args,
                                  [[maybe_unused]] esp_event_base_t base,
                                  [[maybe_unused]] int32_t id,
                                  void *event_data) {
  const auto *data = static_cast<const sized_event_data *>(event_data);
  if (s_webhook_callback != nullptr) {
    s_webhook_callback(data->content, data->length);
  }
}

void webserver_init(webhook_callback_t callback) {
  s_webhook_callback = callback;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  httpd_handle_t server = nullptr;
  ESP_ERROR_CHECK(httpd_start(&server, &config));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WEBSERVER_EVENTS, WEBSERVER_EVENT_WEBHOOK, webhook_event_handler, nullptr,
      nullptr));

  httpd_register_uri_handler(server, &uri_get);
  httpd_register_uri_handler(server, &uri_webhook);
}

ESP_EVENT_DEFINE_BASE(WEBSERVER_EVENTS);
