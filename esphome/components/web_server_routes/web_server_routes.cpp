/**
 * Web Server Route Handler
 *
 * @brief A specialized component for managing HTTP routes on ESP32 devices. It provides
 * an abstraction layer over the ESP-IDF httpd server, ensuring robust memory management
 * for HTTP headers
 *
 * @author fschroedter
 * @copyright MIT License
 */

#include "web_server_routes.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include "esphome/components/web_server_idf/web_server_idf.h"
#include <esp_http_server.h>
#include <functional>
#include <list>
#include <string>
#include <vector>

namespace esphome {
namespace web_server_routes {

void WebServerRoutes::add_route(const std::string &path, const std::string &key, route_action_t action,
                                const std::string &content_type, const std::string &content_disposition) {
  this->routes_.push_back({path, key, std::move(action), content_type, content_disposition});
}

void WebServerRoutes::setup() {
  // Backup solution: If base was not set via YAML, search globally.
  if (this->base_ == nullptr) {
    if (web_server_base::global_web_server_base != nullptr) {
      this->base_ = web_server_base::global_web_server_base;
      ESP_LOGD(TAG, "No web_server_id set, using global web server instance.");
    } else {
      // If nothing was found globally, we will try again later.
      // The web server might still be booting up.
      this->set_timeout(1000, [this]() { this->setup(); });
      return;
    }
  }

  // Check if the server within the base is already initialized.
  auto *server = this->base_->get_server();
  if (server == nullptr) {
    // Retry if server is not initialized yet
    this->set_timeout(500, [this]() { this->setup(); });
    return;
  }

  server->addHandler(new RouteHandler(this));
}

void WebServerRoutes::send(const std::string &data) {
  return this->send_binary(reinterpret_cast<const uint8_t *>(data.c_str()), data.length());
}

void WebServerRoutes::send(const char *format, ...) {
  va_list arg;
  va_start(arg, format);

  // Determine the length of the resulting string
  va_list arg_copy;
  va_copy(arg_copy, arg);
  int len = vsnprintf(nullptr, 0, format, arg_copy);
  va_end(arg_copy);  // Clean up the copy

  if (len > 0) {
    // Create a buffer with enough space
    std::vector<char> buf(len + 1);
    vsnprintf(buf.data(), buf.size(), format, arg);

    this->send(std::string(buf.data()));
  }

  va_end(arg);  // Clean up variadic arguments and ensure stack stability.
}

void WebServerRoutes::send_binary(const uint8_t *data, size_t len) {
  if (!this->check_request_() || len == 0) {
    return;
  }

  uint8_t max_retries = 5;
  esp_err_t res = ESP_OK;

  for (uint8_t i = 0; i < max_retries; i++) {
    res = httpd_resp_send_chunk(this->current_req_, reinterpret_cast<const char *>(data), len);

    if (res == ESP_OK) {
      return;
    }

    if (res == ESP_ERR_HTTPD_RESP_SEND) {
      // Buffer full: Wait briefly and give the TCP stack time for ACKs.
      ESP_LOGW(TAG, "Buffer full (chunk jam), waiting for TCP ACKs... (Attempt %d/%d)", i + 1, max_retries);

      // 30ms is a good value to wait for Wi-Fi acknowledgement
      delay(30);
      continue;
    } else {
      // Critical error (e.g., client closed socket)
      ESP_LOGE(TAG, "Critical transmission error: %s", esp_err_to_name(res));
      break;
    }
  }

  // If we arrive here, the sending failed.
  this->reset_request_context_();
}

void WebServerRoutes::set_header(const std::string &field, const std::string &value) {
  if (!this->check_request_()) {
    return;
  }

  // If the header already exists, we do nothing and keep the first value.
  for (size_t i = 0; i < current_headers_.size(); i += 2) {
    if (iequals_(*current_headers_[i], field)) {
      ESP_LOGD(TAG, "Header '%s' already set, ignoring subsequent update.", field.c_str());
      return;
    }
  }

  // Add field name
  current_headers_.push_back(std::make_unique<std::string>(field));
  const char *field_ptr = current_headers_.back()->c_str();

  // Add value
  current_headers_.push_back(std::make_unique<std::string>(value));
  const char *value_ptr = current_headers_.back()->c_str();

  esp_err_t res = ESP_OK;

  // Register with ESP-IDF
  if (iequals_(field, "Content-Type")) {
    res = httpd_resp_set_type(this->current_req_, value_ptr);
  } else {
    res = httpd_resp_set_hdr(this->current_req_, field_ptr, value_ptr);
  }

  ESP_LOGD(TAG, "Header [registered]: %s [ %s ]", field_ptr, value_ptr)

  if (res != ESP_OK) {
    ESP_LOGW(TAG, "Set header failed: %s", esp_err_to_name(res));
    return;
  }
}

void WebServerRoutes::set_content_size(size_t size) {  //
  this->set_header("Content-Length", std::to_string(size));
}

void WebServerRoutes::set_content_type(const std::string &type) {  //
  this->set_header("Content-Type", type.c_str());
}

void WebServerRoutes::set_content_disposition(const std::string &disposition) {  //
  this->set_header("Content-Disposition", disposition.c_str());
}

void WebServerRoutes::set_filename(const std::string &filename) {
  ESP_LOGI(TAG, "filename: %s", filename.c_str());
  ESP_LOGI(TAG, filename.c_str());

  std::string value = "attachment; filename=" + filename;

  ESP_LOGI(TAG, "value: %s", value.c_str());
  ESP_LOGI(TAG, value.c_str());
  this->set_content_disposition(value);
}

std::string WebServerRoutes::get_query_param(const std::string &key) {
  if (!this->check_request_()) {
    return "";
  }

  size_t query_len = httpd_req_get_url_query_len(this->current_req_);
  if (query_len == 0) {
    return "";
  }

  char *query_str = new char[query_len + 1];
  if (httpd_req_get_url_query_str(this->current_req_, query_str, query_len + 1) != ESP_OK) {
    delete[] query_str;
    return "";
  }

  char value[query_len + 1];
  esp_err_t res = httpd_query_key_value(query_str, key.c_str(), value, query_len + 1);
  delete[] query_str;

  if (res != ESP_OK) {
    return "";
  }

  return std::string(value);
}

std::string WebServerRoutes::get_key_value() {
  if (!this->check_request_()) {
    return "";
  }

  if (this->current_route_ == nullptr) {
    ESP_LOGW(TAG, "Called outside of active request!");
    return "";
  }

  std::string key = this->current_route_->key;
  if (key.empty())
    return "";

  return this->get_query_param(key);
}

bool WebServerRoutes::check_request_() {
  if (this->current_req_ == nullptr) {
    ESP_LOGW(TAG, "Request method invoked without an active HTTP session.");
    return false;
  }
  return true;
}
void WebServerRoutes::reset_request_context_() {
  this->current_req_ = nullptr;
  this->current_route_ = nullptr;
  this->is_busy_ = false;
  this->current_headers_.clear();
}

void WebServerRoutes::handle_native_request_(httpd_req_t *req, RouteEntry &route) {
  this->current_req_ = req;
  this->current_route_ = &route;
  this->is_busy_ = true;

  this->set_header("Cache-Control", "no-cache");
  this->set_header("Connection", "close");

  if (!route.content_type.empty()) {
    this->set_header("Content-Type", route.content_type.c_str());
  }
  if (!route.content_disposition.empty()) {
    this->set_header("Content-Disposition", route.content_disposition.c_str());
  }

  route.action(*this);

  esp_err_t res = httpd_resp_send_chunk(req, nullptr, 0);
  if (res != ESP_OK) {
    ESP_LOGW(TAG, "Final chunk failed: %s", esp_err_to_name(res));
  }

  this->reset_request_context_();
}

bool WebServerRoutes::iequals_(const std::string &a, const std::string &b) {
  return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
  });
}

}  // namespace web_server_routes
}  // namespace esphome