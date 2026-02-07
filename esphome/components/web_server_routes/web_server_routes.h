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
#include <optional>
#include <string>
#include <vector>

namespace esphome {
namespace web_server_routes {

static const char *const TAG = "web_server_routes";

class WebServerRoutes;

class WebServerRoutes : public Component {
 public:
  struct RouteEntry {
    using route_action_t = std::function<void(WebServerRoutes &)>;

    RouteEntry(std::string id, std::string path, std::string key, route_action_t action)
        : id(id), path(path), key(key), action_(std::move(action)) {}

    std::string id;
    std::string path;
    std::string key;
    std::vector<std::pair<std::string, std::string>> headers;
    route_action_t action_;

    void set_responder(route_action_t action) { this->action_ = std::move(action); }

    void set_content_type(std::string content_type) {  //
      set_header("Content-Type", content_type);
    }

    void set_content_disposition(std::string content_disposition) {
      set_header("Content-Disposition", content_disposition);
    }

    void set_filename(std::string filename) {
      std::string content_disposition = "attachment; filename=" + filename;
      set_content_disposition(content_disposition);
    }

    std::string get_header(std::string field) {
      for (const auto &item : this->headers) {
        if (esphome::str_lower_case(item.first) == esphome::str_lower_case(field)) {
          return item.second;  // Return the first match found
        }
      }
      return "";
    }

    void add_header(std::string field, std::string value) {
      trim(field);
      trim(value);
      if (!field.empty() && !value.empty()) {
        this->headers.push_back({field, value});
      }
    }

    void add_header(std::string raw_header) {
      auto [field, value] = parse_header_(raw_header);
      add_header(field, value);
    }

    void set_header(std::string raw_header) {
      auto [field, value] = parse_header_(raw_header);
      if (!field.empty()) {
        set_header(field, value);
      }
    }

    void set_header(std::string field, std::string value) {
      for (auto &item : this->headers) {
        if (esphome::str_lower_case(item.first) == esphome::str_lower_case(field)) {
          item.second = value;  // Update existing
          return;
        }
      }
      add_header(field, value);
    }

    void set_headers(std::vector<std::string> raw_headers) {
      this->headers.clear();  // Clear existing headers
      for (const auto &h : raw_headers) {
        add_header(h);
      }
    }

    void execute_(WebServerRoutes &it) {
      if (this->action_) {
        this->action_(it);
      }
    }

   private:
    void trim(std::string &s) {
      s.erase(0, s.find_first_not_of(" \t\n\r"));
      size_t last = s.find_last_not_of(" \t\n\r");
      if (last != std::string::npos) {
        s.erase(last + 1);
      }
    }

    std::pair<std::string, std::string> parse_header_(std::string raw_header) {
      size_t pos = raw_header.find(':');
      if (pos == std::string::npos) {
        // Return empty string as field if no colon is found
        return {"", ""};
      }

      std::string field = raw_header.substr(0, pos);
      std::string value = raw_header.substr(pos + 1);

      trim(field);
      trim(value);

      return {field, value};
    }
  };

  class RouteHandler : public esphome::web_server_idf::AsyncWebHandler {
   public:
    explicit RouteHandler(WebServerRoutes *parent) : parent_(parent) {}

    bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
      std::string url = request->url();
      for (auto &route_ptr : this->parent_->routes_) {
        auto &route = *route_ptr;
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

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
      // Log active socket count only when debug logging is enabled
      const size_t max_fds = 16;
      int client_fds[max_fds];
      size_t client_count = max_fds;

      if (httpd_get_client_list(net_req->handle, &client_count, client_fds) == ESP_OK) {
        ESP_LOGD(TAG, "Active sockets: %zu", client_count);
      }
#endif  // ESPHOME_LOG_LEVEL_DEBUG

      this->parent_->handle_native_request_(net_req, *(this->matched_route_));
      this->matched_route_ = nullptr;
    }

   protected:
    WebServerRoutes *parent_;
    mutable RouteEntry *matched_route_{nullptr};  // mutable allows assignment within the const method canHandle
  };

  void set_web_server_base(web_server_base::WebServerBase *base) { this->base_ = base; }
  RouteEntry *add_route(RouteEntry *route);
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void setup() override;
  bool is_transmitting() { return this->is_busy_; }

  esp_err_t send(const std::string &data);
  esp_err_t send(const char *format, ...);  // Sends a formatted string using variadic arguments
  esp_err_t send_binary(const char *data, size_t len);

  void send_header(const std::string &field, const std::string &value);  // Sets HTTP headers
  void send_content_size(size_t size);
  void send_content_type(const std::string &type);
  void send_content_disposition(const std::string &disposition);
  void send_filename(const std::string &filename);
  std::string get_query_param(const std::string &key);
  std::string get_key_value();
  void set_unique_header_fields(const bool state) { this->use_unique_header_fields_ = state; }

 protected:
  bool check_request_();
  void reset_request_context_();
  void handle_native_request_(httpd_req_t *req, RouteEntry &route);
  std::optional<std::string> has_header_(const std::string &field) const;

  web_server_base::WebServerBase *base_;
  httpd_req_t *current_req_{nullptr};
  RouteEntry *current_route_{nullptr};
  std::vector<std::unique_ptr<RouteEntry>> routes_;
  bool is_busy_{false};
  bool use_unique_header_fields_{true};

  /**
   * Stores HTTP headers with stable memory addresses.
   * ESP-IDF stores only pointers; unique_ptr ensures strings remain at fixed
   * locations even if the vector reallocates, preventing pointer invalidation.
   */
  std::vector<std::unique_ptr<std::string>> current_headers_;
};

}  // namespace web_server_routes
}  // namespace esphome