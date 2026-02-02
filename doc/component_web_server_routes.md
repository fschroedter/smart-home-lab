[![ESPHome](https://img.shields.io/badge/ESPHome-Powered-white?logo=esphome&logoColor=000000)](https://esphome.io/)


[← Back to Overview](../README.md)


# ESPHome Component: Web Server Route

The [Web Server Route](https://github.com/fschroedter/smart-home-lab/tree/main/esphome/components/web_server_routes) component provides a robust abstraction layer over the ESP-IDF `httpd` server for ESP32 devices, designed for efficient data delivery and memory safety.
It enables **sending data of any size** by streaming it in segments (e.g., from an SD card) rather than buffering the entire payload. This allows for the delivery of massive files without exceeding the ESP32's limited RAM by utilizing functions like `send(*data, len)` to transmit raw data packets sequentially.

## Configuration variables



* **path**: (Optional, Default: "download") Global base URL path for the web server.
* **routes**: (Required) List of individual route definitions.
    * **key**: (Optional) Unique identifier. Routes without a key act as a fallback if no specific key-based route matches.
    * **path**: (Optional, Default: "download") Overrides the global path for this specific route.
    * **subpath**: (Optional) An additional path segment appended to the base route. It defines a nested URL structure (e.g., `/route/subpath`)
    * **content_type**: (Optional) Sets the initial HTTP header. Example: `application/json` or `text/plain` 
    * **content_disposition**: (Optional) Example: `inline`,  `attachment` or `attachment; filename=data.txt`
    * **lambda**: (Required) The C++ code block executed when the route is called.



## Lambda API functions

* `send(...)`: Sends data to the client. Supports variadic (printf-style) formatting for variables.
* `send_binary(*data, len)`: Sends raw binary data to the client. Useful for transmitting files, buffers, or non-text payloads.
* `set_header(field, value)`: Registers an HTTP header. Only applied if the attribute was not set in YAML or previously in the lambda.

* `set_content_disposition(value)`: A wrapper for `set_header` to define both the disposition mode and the filename.
* `set_content_type(value)`: A wrapper for `set_header()` to define the Content-Type
* `set_filename(my_filename)`: Convenience function to set the Content-Disposition header with a specific filename.

* `get_query_param(key)`: Retrieves the value of a specific parameter from the URL query string (e.g., ?file=data.txt).
* `get_key_value()`: Returns the string value of the "key" attribute defined in the YAML for the current route.

Note: All set header functions follow the "First Come, First Served" principle. HTTP headers defined in YAML take precedence over subsequent changes in the lambda.


## How-To
Set up [Web Server Route](https://github.com/fschroedter/smart-home-lab/tree/main/esphome/components/web_server_routes) component as described below.
```yaml
# Requires Web Server Component
web_server:

external_components:  
  - source: github://fschroedter/smart-home-lab  
    components: [ web_server_routes ]

```

## Examples

### Basic Example
**Endpoint provided by this configuration:**<br>
`GET http://<HOSTNAME>.local/download`

```yaml
web_server_routes:
  routes:
    - lambda: |-        
        it.send("Hello World!");
```

### Example with Download Filename
This sample sets the HTTP Header 'Content-Disposition' with 'attachment' and custom filename that indicate to the web browser to download this file.

**Endpoint provided by this configuration:**<br>
`GET http://<HOSTNAME>.local/download`

```yaml
web_server_routes:
  routes:
    - filename: uptime.txt
      lambda: |-        
        // Sends a formatted string using variadic arguments (printf-style).
        it.send("Uptime: %lu ms", millis());
```


### Advanced Example
Creating mutiple routes with various configurations.

**Endpoints provided by this configuration:**<br>
- `GET http://<HOSTNAME>.local/info/hello-world`<br>
- `GET http://<HOSTNAME>.local/download/binary?date=2026_02_03`<br>
- `GET http://<HOSTNAME>.local/info/transfer?json-data=1&filename=my_data.json` 

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

        // "Web Server Route" encoded as byte array (ASCII)
        const uint8_t sample_data[] = {
            0x57, 0x65, 0x62, 0x20, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72,  // W e b (space) S e r v e r
            0x20, 0x52, 0x6f, 0x75, 0x74, 0x65, 0x73                     // (space) R o u t e s
        };

        // Transmit a raw byte buffer as a binary response to the client
        it.send_binary(sample_data, sizeof(sample_data));

        // Extract the specified query parameter from the request and send its value as the response
        std::string date = it.get_query_param("date");
        it.send(date);


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


## Handling Long Loops and Large Responses
When generating long responses within a loop (e.g., iterating over hundreds of log entries), you must prevent the code from blocking the CPU for too long. If the loop runs without interruption, the **Task Watchdog Timer (TWDT)** might trigger a reboot, or the Wi-Fi stack may lose its connection.

To keep the system stable, include a small delay or yield to allow the ESP32 to handle background tasks:

```cpp
for (int i = 0; i < 10000; i++) {
    it.send("Data row content...");
    
    // Periodically yield to system tasks to prevent reboots and disconnects
    if (i % 20 == 0) { 
        delay(1); 
    }
}
``` 

## Performance & Packet Size Optimization

> **Aim for this limit to ensure your response fits into a single TCP packet.** When using the `send()` function, keep the total body size around **~1,200 bytes** to achieve the lowest possible latency and maximum stability for your ESP32-C6.

### The "Magic Limit"
This recommendation is based on the standard **Maximum Segment Size (MSS)** of a TCP packet over Wi-Fi, which is 1,460 bytes. When the ESP32 sends a response, the data is encapsulated into a single packet. This total payload consists of two parts:
1. **HTTP Headers:** Automatically generated (e.g., Content-Type, Content-Length, Server). These typically consume about 200–260 bytes.
2. **HTTP Body:** Your actual data (JSON, text, or binary).

### Network Behavior and Impact
| HTTP Body Size | Network Behavior |Performance| Impact |
| :--- | :--- | :--- | :--- |
| **< 150 bytes** | Tiny Packet | **Slow (Overhead)**| **Ineffizcient:** The header (approx. 100B) is almost as large as the data. High CPU load per byte.  |
| **150 - 1.000 bytes** | Single Packet | **Fast**| **Suboptimal:** Secure and fast, but WLAN airtime is not used efficiently (lots of idle time). |
| **< ~1.200 bytes** | Single Packet | **Ultra Fast**| **Optimal:** Lowest latency, minimal CPU usage. |
| **>  ~1.200 bytes** | Multiple Packets | **Fast** | **Standard:** Works fine, but requires ACKs and more processing. |
| **> ~16.384 bytes** | Large Payload | **Moderate** | **Caution:** Risk of heap fragmentation; use streaming for larger data. |

