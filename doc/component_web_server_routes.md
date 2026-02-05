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

* **id**: (Optional): The main component ID is the global C++ reference used to call methods like `set_responder` from other lambdas.
* **path**: (Optional, Default: "download") Base URL path for the web server.
* **routes**: (Required) List of individual route definitions.
    * **id** (Optional): The route_id is a unique string used by set_responder to identify and update the logic of a specific route at runtime.
    * **key**: (Optional) Unique identifier. Routes without a key act as a fallback if no specific key-based route matches.
    * **path**: (Optional) Overrides the root path for this specific route.
    * **subpath**: (Optional) An additional path segment appended to the base route. It defines a nested URL structure (e.g., `/route/subpath`)
    * **content_type**: (Optional) Sets the initial HTTP header. Example: `application/json` or `text/plain` 
    * **content_disposition**: (Optional) Example: `inline`,  `attachment` or `attachment; filename=data.txt`
    * **filename**: (Optional) Define the filename in the HTTP Header. This attribute cannot be used together with `content_disposition`.
    * **lambda**: (Required) The C++ code block executed when the route is called.



## Lambda API functions

* `send(value: string)`: Sends data to the client. Supports variadic (printf-style) formatting.
* `send_binary(data: uint8_t*, len: int)`: Sends raw binary data to the client. Useful for transmitting files, buffers, or non-text payloads.
* `set_header(field: string, value: string)`: Registers an HTTP header. Only applied if the attribute was not set in YAML or previously in the lambda.

* `set_content_size(size: int)`: A wrapper for `set_header()` that sets the HTTP `Content-Length` header, allowing clients to determine the total download size in advance and enabling progress tracking, validation, and more efficient resource management.
* `set_content_type(value: string)`: A wrapper for `set_header()` to define the Content-Type. 
* `set_content_disposition(value: string)`: A wrapper for `set_header` to define both the disposition mode and a filename.
* `set_filename(value: string)`: Convenience function and a wrapper for `set_header()` to set the `Content-Disposition` header with a specific filename.

* `get_query_param(field: string)`: Retrieves the value of a specific parameter from the URL query string (e.g., ?file=data.txt).
* `get_key_value()`: Returns the string value of the `key` attribute defined in the YAML for the current route. This function serves as a wrapper for `get_query_param()`, specifically retrieving the parameter that matches the configured key.
* `set_responder(route_id: string, callback: function)`: Assigns a dynamic responder function to a route by its string-based route `id`.
  
Note: All set header functions follow the "First Come, First Served" principle. HTTP headers defined in YAML take precedence over subsequent changes in the lambda.


## How-To
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
  path: infos
  routes:
    - filename: uptime.txt
      subpath: uptime
      lambda: |-        
        // Sends a formatted string using variadic arguments (printf-style).
        it.send("Uptime: %lu ms", millis());
```

### Advanced Data Handling & Custom Header Implementation
Creating mutiple routes with various configurations.

**Endpoints provided by this configuration:**<br>
- `GET http://<HOSTNAME>/info/hello-world`<br>
- `GET http://<HOSTNAME>/download/binary?date=2026_02_03`<br>
- `GET http://<HOSTNAME>/info/transfer?json-data=1&filename=my_data.json` 

```yaml
web_server_routes:
  path: info
  routes:
    # Generic routes (no key) act as a fallback and are processed only if no specific key-based route matches.
    - subpath: hello-world      # Sub path is added to global path
      content_type: application/json
      content_disposition: inline
      lambda: |-
        it.send("{\"message\": \"Hello World!\"}");

    # Send data with send_binary()
    - key: date
      path: download            # Override global path
      subpath: binary           # Sub path is added to path
      lambda: |-
        it.set_content_type("text/plain");
        it.set_content_disposition("inline");

        // "Web Server Routes" encoded as byte array (ASCII)
        const uint8_t sample_data[] = {
            0x57, 0x65, 0x62, 0x20, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72,  // W e b (space) S e r v e r
            0x20, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x73                     // (space) R o u t e s
        };

        // Transmit a raw byte buffer as a binary response to the client
        it.send_binary(sample_data, sizeof(sample_data));

        // Extract the specified query parameter from the request and send its value as the response
        std::string date = it.get_query_param("date");
        it.send("\nDate: %s", date);

    # Specific key-based route
    - key: json-data
      path: info/transfer   # Override global path
      lambda: |-
        // Set Content-Type header
        it.set_header("content_type", "application/json; charset=utf-8");

        // Set dynamic filename from query parameter
        std::string my_filename = it.get_query_param("filename");
        it.set_filename(my_filename);

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
  id: my_extened_webserver
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
        id(my_extened_webserver).set_responder("my_route", [](auto &it) {
          it.send("Response A: System is ACTIVE");
        });
        
    # Overwrite the logic for "my_route" with a different behavior
    on_turn_off:
      lambda: |-
        id(my_extened_webserver).set_responder("my_route", [](auto &it) {
          it.send("Response B: System is STANDBY");
        });
```


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

