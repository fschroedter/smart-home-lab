[![ESPHome](https://img.shields.io/badge/ESPHome-Powered-white?logo=esphome&logoColor=000000)](https://esphome.io/)


[← Back to Overview](../README.md)


# Web Server Routes

The ESPHome Component [Web Server Routes](https://github.com/fschroedter/smart-home-lab/tree/main/esphome/components/web_server_routes) component provides a **easy-to-use** abstraction layer over the ESP-IDF HTTP Server for ESP32 devices. It is designed for efficient data delivery and memory safety while **simplifying complex server tasks** into a developer-friendly interface, making it particularly suitable for **standalone environments operating without Home Assistant**.

It enables **sending data of any size** by streaming it in segments (e.g., from an SD card) rather than buffering the entire payload. This allows for the delivery of massive files without exceeding the ESP32's limited RAM, as the component handles the heavy lifting of sequential packet transmission through straightforward functions like `send(message)`.

```yaml
# Example minimal configuration entry
# Endpoint: http://<HOSTNAME>/download

web_server_routes:
  routes:
    - lambda: |-        
        it.send("Hello World!");
```
## Configuration variables

* **path**: (Optional, string): Base URL path for the web server. Default: `download`
* **routes** (Required): List of individual route definitions.
    * **id** (Optional, string): This unique is used by `set_responder()` to identify and update a specific route at runtime.
    * **key** (Optional, string): A query key can be used as filter as well as  carrier for data evaluated with `get_key_value()`. Routes without a key act as a fallback if no specific key-based route matches.
    * **path** (Optional, string): Overrides the global path for this specific route.
    * **cache-control** (Optional, string): Sets HTTP Header Cache-Control. Default: `no-cache`
    * **connection** (Optional, string): Sets the HTTP header to `close` by default to prevent socket exhaustion on the ESP32 by ensuring connections are not kept idle.
    * **content_type** (Optional, string): Sets the HTTP header `Content-Type`. Example: `application/json` or `text/plain` 
    * **content_disposition** (Optional, string): Sets the HTTP header `Content-Disposition` Examples for valid values: `inline`,  `attachment` or `attachment; filename=data.txt`
    * **filename** (Optional, string) Define the filename in the HTTP Header. This attribute cannot be used together with `content_disposition`.
    * **header** (Optional, list): Defines single or a list of HTTP Headers; entries in this list will override any conflicting named header attributes configurations.
    * **unique_header_fields** (Optional, boolean): Prevents sending headers with same field. Default is `true`
    * **lambda** (Required, lambda): The C++ code block executed when the route is called.


## Lambda functions

### Funktions for Internal Lambda and within `set_responder()`

* `send(value: string)`: Sends data to the client. Supports variadic (printf-style) formatting.
* `send_binary(data: char*, len: int)`: Sends raw binary data to the client. Useful for transmitting files, buffers, or non-text payloads.
* `send_header(field: string, value: string)`: Registers an HTTP header. Only applied if the attribute was not set in YAML or previously in the lambda.
* `send_content_size(size: int)`: A wrapper for `set_header()` that sets the HTTP `Content-Length` header, allowing clients to determine the total download size in advance and enabling progress tracking, validation, and more efficient resource management.
* `send_content_type(value: string)`: A wrapper for `set_header()` to define the Content-Type. 
* `send_content_disposition(value: string)`: A wrapper for `set_header` to define both the Content-Disposition mode and a filename.
* `send_filename(value: string)`: Convenience function and a wrapper for `send_content_disposition()` to set the `Content-Disposition` header with a specific filename.
* `get_key_value()`: Returns the string value of the `key` attribute defined in the YAML for the current route. This function serves as a wrapper for `get_query_param()`, specifically retrieving the parameter that matches the configured `key`.
* `get_query_param(field: string)`: Retrieves the value of a specific parameter from the URL query string (e.g., ?file=data.txt).

### Functions for External Lambdas
* `set_responder(callback: function)`: Assigns a dynamic responder function to a route by its string-based route `id`.
* `set_header(header: string)`: Adds a new or updates an existing HTTP header field.  
* `set_header(field: string, value: string)`: Adds a new or updates an existing HTTP header field.  
* `add_header(header: string)`:  Adds a of HTTP header.  
* `add_headers(headers: list)`:  Adds a bunch of HTTP headers.  
* `set_content_type(value: string)`: A wrapper for `set_header()` to define the Content-Type. 
* `set_content_disposition(value: string)`: A wrapper for `set_header()` to define both the Content-Disposition mode and a filename.
* `set_filename(value: string)`: Convenience function and a wrapper for `set_header()` to set the `Content-Disposition` header with a specific filename.

Note: All defined headers are processed successively; with the default setting `unique_header_fields: true`, only the first instance of each header field is sent.



## How-To Setup
Set up [Web Server Routes](https://github.com/fschroedter/smart-home-lab/tree/main/esphome/components/web_server_routes) component as described below.
```yaml
# Requires Web Server Component
web_server:

external_components:  
  - source: github://fschroedter/smart-home-lab  
    components: [ web_server_routes ]

# Endpoint: http://<HOSTNAME>/download
web_server_routes:
  routes:
    - lambda: |-        
        it.send("Hello World!");
```

## Configuration Examples

### Example with Download Filename
This sample sets the HTTP Header 'Content-Disposition' with 'attachment' and custom filename that indicate to the web browser to download this file.

**Endpoint provided by this configuration:**<br>
`GET http://<HOSTNAME>/infos/uptime`

```yaml
web_server_routes:  
  routes:
    - filename: uptime.txt
      path: infos/uptime
      lambda: |-        
        // Sends a formatted string using variadic arguments (printf-style).
        it.send("Uptime: %lu ms", millis());
```

### Advanced Data Handling & Custom Header Implementation
Creating mutiple routes with various configurations.

**Endpoints provided by this configuration:**<br>
- `GET http://<HOSTNAME>/info`<br>
- `GET http://<HOSTNAME>/info?date=2026_02_03`<br>
- `GET http://<HOSTNAME>/info/transfer?json-data=1&filename=my_data.json` 

```yaml
web_server_routes:
  path: info
  routes:
    # Generic routes (no key) act as a fallback and are processed only if no specific key-based route matches.
    # GET /info...
    - content_type: application/json
      content_disposition: inline
      lambda: |-
        it.send("{\"message\": \"Hello World!\"}");

    # Send data with send_binary 
    # GET /info?date=...
    - key: date
      lambda: |-
        it.send_content_type("text/plain");
        it.send_content_disposition("inline");

        // "Web Server Routes" encoded as byte array (ASCII)
        const char sample_data[] = {
            0x57, 0x65, 0x62, 0x20, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72,  // W e b (space) S e r v e r
            0x20, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x73                     // (space) R o u t e s
        };

        // Filesize allows the browser to estimate download progression
        int data_length = sizeof(sample_data);
        it.send_content_size(data_length);

        // Transmit a raw byte buffer as a binary response to the client
        it.send_binary(sample_data, data_length);

    # Specific key-based route 
    # GET /info/transfer?json-data=...
    - key: json-data
      path: info/transfer   # Override global path
      content_type: application/json; charset=utf-8
      lambda: |-
        // Set Content-Type header
        it.send_header("X-Data", "json");

        // Set dynamic filename from query parameter
        // Extract the specified query parameter from the request and set dynamic filename
        std::string my_filename = it.get_query_param("filename");
        it.send_filename(my_filename);

        // Sending the JSON body in chunks
        it.send("{");
        it.send("\"status\": \"success\",");
        it.send("\"device\": \"%s\",", "ESP32_Data_Server");
        it.send("\"uptime_ms\": %lu,", millis());
        it.send("\"sensor_active\": true");
        it.send("}");
```
### Dynamic Responder Assignment
This example shows how to programmatically define route behavior on demand. By using `set_responder`, you can inject different response logics into a pre-defined route whenever needed.

**Endpoints provided by this configuration:**<br>
- `GET http://<HOSTNAME>/download`

```yaml
web_server_routes:
  routes:
    - id: my_route

switch:
  - platform: template
    name: "Web Route Response Selector"
    id: route_selector
    optimistic: true
    
    # Define or update the logic for "my_route" at runtime
    on_turn_on:
      lambda: |-
        id(my_route).set_responder([](auto &it) {
          it.send("Response A: System is ACTIVE");
        });
        
    # Overwrite the logic for "my_route" with a different behavior
    on_turn_off:
      lambda: |-
        id(my_route).set_responder([](auto &it) {
          it.send("Response B: System is STANDBY");
        });
```
### Lambda Scope and Variable Validity
In this example, a route with the ID `my_route` is defined and subsequently modified by the on_boot lambda. 
```yaml
# Use the external lambda from on_boot for demonstration
esphome:
  name: web-server-routes-demo
  on_boot:
    then:
      - lambda: |-                                          #   <-- External lambda
          id(my_route).set_header("X-My-Pet: Dog");
          id(my_route).set_responder([](&it) {              
            it.send_header("X-Guard-Strength: 4");
            it.send_header("X-My-Pet: Elephant");
            it.send("Updated World");
          });

# Lambda in YAML configuration
web_server_routes:
  routes:
    - id: my_route
      path: info/headers
      filename: data.txt
      header: "X-My-Pet: Cat"
      lambda: |-                                             #  <-- Internal lambda
        it.send_header("X-Guard-Strength: 1");
        it.send("Hello World");
```
The header `X-My-Pet: Elephant` is not sent because `unique_header_fields` is set to `true` by default, which prevents multiple headers of the same type from being dispatched. The header `X-Guard-Strength: 1` is also not sent because the lambda intended for its transmission is overwritten by `set_responder()`.
The resulting configuration produces the following output:

**Endpoint:**<br>
`GET http://<HOSTNAME>/info/headers`

**Header:**<br>
``X-My-Pet: Dog``<br>
``X-Guard-Strength: 4``

**Content**:<br>
"Updated World"


### Example: Send BMP screenshot as image file
Full example: [send_display_bmp.yaml](https://github.com/fschroedter/smart-home-lab/blob/main/examples/web_server_routes_component/send_display_bmp.yaml)

```yaml
web_server_routes:
  routes:
    - path: download/image
      content_type: image/bmp
      filename: screenshot.bmp
      lambda: |-
        // Sending a part of the data that consists of data chunks
        while (disp_stream->get_bmp_chunk([&it](const char *data, size_t len) {
          it.send_binary(data, len);            
        })) {
          vTaskDelay(pdMS_TO_TICKS(1));
        }

display:  
  - platform: ...
    lambda: |-
      it.image(0, 0, id(my_image));
      
      // ----------------------------------------
      // Take snapshot
      if (disp_stream == nullptr) {
        auto disp_ptr = id(my_display);
        size_t chunk_size = 1152;
        disp_stream = new DisplayStream(disp_ptr, chunk_size);
      }

      if (disp_stream->needs_snapshot()) {        
        if (!disp_stream->take_snapshot()) {
          ESP_LOGE("HTTP", "Snapshot failed (Out of Memory?)");
          return;
        }
      }
```

### Example: Send Files from SD card
Soon ...





## Handling Long Loops and Large Responses

> [!TIP]
> Use `delay(1)` within long-running loops to prevent watchdog timer resets.
>

When generating long responses within a loop (e.g., iterating over hundreds of log entries), you must prevent the code from blocking the CPU for too long. If the loop runs without interruption, the **Task Watchdog Timer (TWDT)** might trigger a reboot, or the Wi-Fi stack may lose its connection.

To ensure system stability, use `delay(1)` to provide sufficient time for background tasks or `yield()` to pet the watchdog and prevent timeouts.

```cpp
// Periodically yield to system tasks to prevent reboots and disconnects
for (int i = 0; i < 10000; i++) {
    it.send("Data row content...");
    
    if (i % 20 == 0) { 
        delay(1);   // Keeps Wi-Fi alive and feeds TWDT
    }
}
``` 

<!-- **Preventing Watchdog Resets with yield():**
```cpp
// Periodically yield to system tasks to prevent reboots and disconnects
for (int i = 0; i < 10000; i++) {
    it.send("Data row content...");
    
    if (i % 100 == 0) { 
        delay(1);   // Keeps Wi-Fi alive and feeds TWDT
    } else {      
        yield();    // Otherwise just pet the watchdog and handle background tasks
    }
}
```  -->



## Performance & Packet Size Optimization

> [!TIP]
> **Aim for this limit to ensure your response fits into a single TCP packet.** When using the `send()` function, keep the total body size around **~1,200 bytes** to achieve the lowest possible latency and maximum stability for your ESP32-C6.

### The "Magic Limit"
This recommendation is based on the standard **Maximum Segment Size (MSS)** of a TCP packet over Wi-Fi, which is 1,460 bytes. When the ESP32 sends a response, the data is encapsulated into a single packet. This total payload consists of two parts:
1. **HTTP Headers:** Automatically generated (e.g., Content-Type, Content-Length, Server). These typically consume about 200–260 bytes.
2. **HTTP Body:** Your actual data (JSON, text, or binary).

### Network Behavior and Impact
| HTTP&nbsp;Body&nbsp;Size | Network&nbsp;Behavior | Impact |
| :--- | :--- | :--- |
| **<&nbsp;150 bytes** | Tiny Packet |  **Ineffizcient:** The header (approx. 100 Bytes) is almost as large as the data. High CPU load per byte.  |
| **150&nbsp;–&nbsp;1.000&nbsp;bytes** | Single Packet |  **Suboptimal:** Secure and fast, but WLAN airtime is not used efficiently. |
| **1000 – ~1.200&nbsp;bytes** | Single Packet |  **Optimal:** Lowest latency, minimal CPU usage. |
| **>  ~1.200 bytes** | Multiple Packets |  **Suboptimal:** High overhead due to fragmentation and additional ACKs |
| **> ~16.384 bytes** | Large Payload |  **Caution:** Risk of heap fragmentation; use streaming for larger data. |

