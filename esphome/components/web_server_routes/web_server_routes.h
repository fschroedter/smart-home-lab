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

#pragma once

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

static const char *const TAG = "web_server_routes";

class WebServerRoutes;

using route_action_t = std::function<void(WebServerRoutes &)>;

class WebServerRoutes : public Component {
 public:
  struct RouteEntry {
    std::string path;
    std::string key;
    route_action_t action;
    std::string content_type;
    std::string content_disposition;
  };

  class RouteHandler : public esphome::web_server_idf::AsyncWebHandler {
   public:
    explicit RouteHandler(WebServerRoutes *parent) : parent_(parent) {}

    bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
      std::string url = request->url();
      for (auto &route : this->parent_->routes_) {
        if (url == route.path || url == route.path + "/") {
          if (route.key.empty() || request->hasParam(route.key)) {
            // Log handled route
            ESP_LOGI(TAG, "Path: %s", url.c_str());
            if (!route.key.empty()) {
              ESP_LOGI(TAG, "Key: %s", route.key.c_str());
            }

            this->matched_route_ = &route;
            return true;
          }
        }
      }
      this->matched_route_ = nullptr;
      return false;
    }

    void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override {
      if (request == nullptr) {
        ESP_LOGE(TAG, "Request pointer is null!");
        return;
      }

      if (this->matched_route_ == nullptr) {
        ESP_LOGW(TAG, "No matched route for URL %s", request->url().c_str());
        return;
      }

      httpd_req_t *net_req = *request;
      if (net_req == nullptr) {
        this->matched_route_ = nullptr;  // Cleanup before exiting
        return;
      }

// --- Count active sockets --------
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
      const size_t max_fds = 16;
      int client_fds[max_fds];
      size_t client_count = max_fds;

      if (httpd_get_client_list(net_req->handle, &client_count, client_fds) == ESP_OK) {
        ESP_LOGD(TAG, "Active sockets: %zu", client_count);
      }
#endif

      this->parent_->handle_native_request_(net_req, *(this->matched_route_));
      this->matched_route_ = nullptr;
    }

    RouteEntry *get_matched_route() { return this->matched_route_; }

   protected:
    WebServerRoutes *parent_;
    mutable RouteEntry *matched_route_{nullptr};
  };

  void set_web_server_base(web_server_base::WebServerBase *base) { this->base_ = base; }

  void add_route(const std::string &path, const std::string &key, route_action_t action,
                 const std::string &default_type, const std::string &content_disposition);

  bool is_transmitting() { return this->is_busy_; }
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

  void setup() override;

  void send(const std::string &data);
  void send(const char *format, ...);  // Sends a formatted string using variadic arguments
  void send_binary(const uint8_t *data, size_t len);

  void set_header(const std::string &field, const std::string &value);  // Sets HTTP headers
  void set_content_size(size_t size);
  void set_content_type(const std::string &type);
  void set_content_disposition(const std::string &disposition);
  void set_filename(const std::string &filename);
  std::string get_query_param(const std::string &key);
  std::string get_key_value();

 protected:
  bool check_request_();
  void reset_request_context_();
  void handle_native_request_(httpd_req_t *req, RouteEntry &route);
  bool iequals_(const std::string &a, const std::string &b);  // case insensitive equal

  web_server_base::WebServerBase *base_;
  std::vector<RouteEntry> routes_;
  bool is_busy_{false};
  httpd_req_t *current_req_{nullptr};
  RouteEntry *current_route_{nullptr};

  /**
   * Stores HTTP headers with stable memory addresses.
   * ESP-IDF stores only pointers; unique_ptr ensures strings remain at fixed
   * locations even if the vector reallocates, preventing pointer invalidation.
   */
  std::vector<std::unique_ptr<std::string>> current_headers_;
};

}  // namespace web_server_routes
}  // namespace esphome