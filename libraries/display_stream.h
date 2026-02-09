#pragma once

#include "esphome/components/display/display_buffer.h"
#include <freertos/FreeRTOS.h>
#include <vector>
#include "esp_timer.h"
#include "esphome.h"

namespace esphome {
namespace display_stream {

#pragma pack(push, 1)
struct BMPHeader {
  uint16_t type{0x4D42};  // "BM"
  uint32_t fileSize;      //
  uint32_t reserved{0};   //
  uint32_t offset;        // Start of pixel data
};

struct DIBHeader {
  uint32_t size{40};        // Header size
  int32_t width;            //
  int32_t height;           //
  uint16_t planes{1};       //
  uint16_t bitCount{16};    // 16 bits per pixel
  uint32_t compression{3};  // 3 = BI_BITFIELDS
  uint32_t imageSize;       //
  int32_t xPpm{2835};       // 72 dots/inch x 39.37 inch/meter = 2835 pixel/meter
  int32_t yPpm{2835};       // 72 dots/inch x 39.37 inch/meter = 2835 pixel/meter
  uint32_t colorsUsed{0};
  uint32_t colorsImportant{0};
};

struct RGB565Masks {
  uint32_t red = 0xF800;    // Bits 11-15
  uint32_t green = 0x07E0;  // Bits 5-10
  uint32_t blue = 0x001F;   // Bits 0-4
};

struct FullBMPHeader {
  BMPHeader file;
  DIBHeader dib;
  RGB565Masks masks;
};
#pragma pack(pop)

static constexpr size_t BMP_FILE_HEADER_SIZE = 14;
static constexpr size_t BMP_DIB_HEADER_SIZE = 40;
static constexpr size_t BMP_MASKS_SIZE = 12;
static constexpr size_t TOTAL_HEADER_SIZE = BMP_FILE_HEADER_SIZE + BMP_DIB_HEADER_SIZE + BMP_MASKS_SIZE;

// Ensure the structs are packed correctly without padding
static_assert(sizeof(BMPHeader) == BMP_FILE_HEADER_SIZE, "BMPHeader size mismatch!");
static_assert(sizeof(DIBHeader) == BMP_DIB_HEADER_SIZE, "DIBHeader size mismatch!");
static_assert(sizeof(RGB565Masks) == BMP_MASKS_SIZE, "RGB565Masks size mismatch!");

class DisplayAccess : public esphome::display::DisplayBuffer {
 public:
  static uint8_t *get_raw_buffer(esphome::display::DisplayBuffer *display) {
    return static_cast<DisplayAccess *>(display)->buffer_;
  }
};

class DisplayStream {
 public:
  DisplayStream(esphome::display::DisplayBuffer *display, uint16_t max_chunk_size = 1000)
      : display_(display), max_chunk_size_(max_chunk_size) {
    if (display == nullptr) {
      ESP_LOGE(TAG, "Provided display is null!");
    }

    buffer_ = DisplayAccess::get_raw_buffer(display);

    if (buffer_ == nullptr) {
      ESP_LOGE(TAG, "Buffer is null!");
    }

    width_ = display->get_width();
    height_ = display->get_height();
    buffer_length_ = width_ * height_ * 2;
  }

  // Destructor for cleaning up
  ~DisplayStream() {
    if (snapshot_buffer_ != nullptr)
      free(snapshot_buffer_);
  }

  bool take_snapshot() {
    // Make sure we have a source and a length
    if (buffer_ == nullptr || buffer_length_ == 0) {
      return false;
    }

    // Only reserve memory if snapshott buffer is not set
    if (snapshot_buffer_ == nullptr) {
      snapshot_buffer_ = (uint8_t *) malloc(buffer_length_);
      if (snapshot_buffer_ == nullptr) {
        return false;  // Out of Memory
      }
    }

    // Copy the entire buffer at once when the display has been fully drawn.
    memcpy(snapshot_buffer_, buffer_, buffer_length_);

    // 32-bit optimized in-place swap (processing 2 pixels at a time)
    uint32_t *ptr32 = reinterpret_cast<uint32_t *>(snapshot_buffer_);
    size_t num_iterations = buffer_length_ / 4;

    // Little Endian
    for (size_t i = 0; i < num_iterations; i++) {
      uint32_t val = ptr32[i];
      // Swap bytes 0<->1 and 2<->3
      ptr32[i] = ((val << 8) & 0xFF00FF00) | ((val >> 8) & 0x00FF00FF);
    }

    return true;
  }

  bool get_bmp_chunk(std::function<void(const char *, size_t)> send_callback) {
    if (!header_sent_) {
      auto header = get_bmp_header();
      send_callback(header, TOTAL_HEADER_SIZE);
      header_sent_ = true;
      return true;
    }

    // Check if we have already sent everything
    if (current_pos_ >= buffer_length_) {
      is_streaming_ = false;
      return false;
    }

    // Snapshot ready?
    if (snapshot_buffer_ == nullptr) {
      ESP_LOGI(TAG, "Waiting for snapshot ...");
      delay(100);
      return true;  // while loop keep going. waiting for snapshot
    }

    // Calculate the size of the next chunk
    size_t remaining = buffer_length_ - current_pos_;
    size_t actual_chunk_size = std::min(remaining, max_chunk_size_);

    // Directly point to the data in our snapshot buffer
    const uint8_t *chunk_ptr = snapshot_buffer_ + current_pos_;

    // Send the data directly
    send_callback(reinterpret_cast<const char *>(chunk_ptr), actual_chunk_size);

    // Advance position
    current_pos_ += actual_chunk_size;

    // Return true if there is still data left for the next iteration
    return current_pos_ < buffer_length_;
  }

  size_t get_file_size() {
    uint32_t pixelDataSize = buffer_length_;
    return TOTAL_HEADER_SIZE + pixelDataSize;
  }

  bool is_streaming() { return is_streaming_; }
  bool needs_snapshot() {  //
    return is_streaming_ && (snapshot_buffer_ == nullptr);
  }

  void start_streaming() {
    is_streaming_ = true;
    header_sent_ = false;
    current_pos_ = 0;
  }

 private:
  static constexpr const char *TAG = "DisplayStream";
  esphome::display::DisplayBuffer *display_;  // ESPHome display object
  FullBMPHeader full_header_;                 // BMP Header
  uint8_t *buffer_{nullptr};                  // Display buffer pointer
  uint8_t *snapshot_buffer_{nullptr};         // Snapshot buffer for a fragments
  size_t buffer_length_{0};                   // Buffer length
  size_t max_chunk_size_;                     // Max chunk size for sending
  size_t current_pos_{0};                     // Current position in buffer
  bool header_sent_ = false;                  // Indicates weather header was sent
  int width_;
  int height_;
  bool is_streaming_{false};

  // Helper funktion to create the BMP image header
  const char *get_bmp_header() {
    uint32_t offsetToPixels = TOTAL_HEADER_SIZE;
    uint32_t pixelDataSize = buffer_length_;

    full_header_.file.fileSize = offsetToPixels + pixelDataSize;
    full_header_.file.offset = offsetToPixels;

    full_header_.dib.width = width_;
    full_header_.dib.height = -height_;
    full_header_.dib.imageSize = pixelDataSize;

    return reinterpret_cast<const char *>(&full_header_);
  }
};

}  // namespace display_stream
}  // namespace esphome

using DisplayStream = esphome::display_stream::DisplayStream;
extern DisplayStream *disp_stream;
DisplayStream *disp_stream = nullptr;
