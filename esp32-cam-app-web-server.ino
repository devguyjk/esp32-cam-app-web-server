#include "esp_camera.h"
#include <WiFi.h>
#include <mbedtls/md5.h> // Include MD5 library
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h> // For mDNS discovery
#include <WiFiClientSecure.h>

// Your camera model definition
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
#define LED_FLASH_PIN 4 // GPIO pin connected to the LED (check your board)
#include "camera_pins.h"
#include "templates.h"

// Forward declarations for web server handlers and helper functions
void handleCapture();
void handleRoot();
void setupLedFlash(int pin);
String generateShortUniqueId();
IPAddress getAssignedIP(String cameraId);
void handleHello(); // This is not defined in your code. If not used, consider removing.
void handleStream();
void handleSetBW();
void handleCameraSetting(); // This is correctly declared and defined.
void handleSettings(); // This is declared but not explicitly defined as a forward declaration here. Add it.


// ===========================

// Enter your WiFi credentials
// ===========================
const char *ssid = "juke-fiber-ofc";
const char *password = "swiftbubble01";

// Camera ID to IP mapping
struct CameraMapping {
  const char* id;
  IPAddress ip;
};

CameraMapping cameraIPs[] = {
  {"150deaa5", IPAddress(192, 168, 4, 74)},
  {"5e80ed5f", IPAddress(192, 168, 4, 107)},
  {"2ddb4fdb", IPAddress(192, 168, 4, 75)},
  {"c4df83c4", IPAddress(192, 168, 4, 77)},
};
const int numCameras = sizeof(cameraIPs) / sizeof(cameraIPs[0]);

// Server configuration for snapshot upload
const int serverPort = 5000; // Replace with your server's port

WebServer server(80); // Web server on port 80

// Activity log buffer
String logBuffer[300];
int logIndex = 0;
int logCount = 0;

void addLog(String message) {
  logBuffer[logIndex] = String(millis()) + ": " + message;
  logIndex = (logIndex + 1) % 300;
  if (logCount < 300) logCount++;
}

void handleCapture() {
  addLog("Handling capture endpoint");

  bool useFlash = server.hasArg("flash") && server.arg("flash") == "true";
  
  if (useFlash) {
    digitalWrite(LED_FLASH_PIN, HIGH);
    delay(200);
    addLog("Flash activated");
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    if (useFlash) digitalWrite(LED_FLASH_PIN, LOW);
    addLog("Camera frame buffer failed");
    server.send(500, "text/plain", "Camera frame buffer failed");
    return;
  }

  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);

  if (useFlash) digitalWrite(LED_FLASH_PIN, LOW);

  addLog("Snapshot sent (" + String(fb->len) + " bytes)");
}

void handleRoot() {
  String cameraId = generateShortUniqueId();
  String ipAddr = WiFi.localIP().toString();
  String html = String(rootPageTemplate);
  html.replace("TITLE_PLACEHOLDER", "Sani Flush Cam " + cameraId + " - " + ipAddr);
  html.replace("HEADER_PLACEHOLDER", getCommonHeader(cameraId, ipAddr));
  server.send(200, "text/html", html);
}

void setupLedFlash(int pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW); // Initialize LED as off
}

// Ensure generateShortUniqueId is defined before setup if not forward declared
String generateShortUniqueId() {
  // Get Chip ID
  uint64_t chipId = ESP.getEfuseMac();

  // Create input data for MD5 hash
  uint8_t inputData[8];
  memcpy(inputData, &chipId, 8); // Copy chip ID into byte array

  // Calculate MD5 hash
  uint8_t md5Hash[16];
  mbedtls_md5(inputData, 8, md5Hash);

  // Convert hash to hexadecimal string
  char md5String[33]; // 16 bytes * 2 hex chars + null terminator
  for (int i = 0; i < 16; i++) {
    sprintf(md5String + (i * 2), "%02x", md5Hash[i]);
  }

  // Return the first 8 characters of the MD5 hash
  return String(md5String).substring(0, 8);
}

IPAddress getAssignedIP(String cameraId) {
  for (int i = 0; i < numCameras; i++) {
    if (String(cameraIPs[i].id) == cameraId) {
      return cameraIPs[i].ip;
    }
  }
  return IPAddress(0, 0, 0, 0); // Return invalid IP if not found
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;

  // --- Highest Resolution Setting ---
  config.frame_size = FRAMESIZE_UXGA; // Keep UXGA for max resolution

  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY; // Best for single captures
  config.fb_location = CAMERA_FB_IN_PSRAM;

  // --- JPEG Quality Setting (Lower value = Higher Quality, Larger File) ---
  config.jpeg_quality = 8; // Good setting, keeps high quality

  config.fb_count = 1; // For single captures, 1 frame buffer is usually sufficient.

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 8; // Keep at 8 or lower for max quality
      config.fb_count = 2; // Keep 2 frame buffers if PSRAM is found for smoother operation if needed
      config.grab_mode = CAMERA_GRAB_LATEST; // Good for streaming, for single capture WHEN_EMPTY is also fine.
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
      config.jpeg_quality = 12; // If no PSRAM, you might need higher compression
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();

  // OV5640 base configuration - fix upside down video
  s->set_vflip(s, 0);  // Changed from 1 to 0 to flip vertically
  s->set_hmirror(s, 1); // Changed from 0 to 1 to mirror horizontally

  // If you are consistently getting a red tint with OV2640,
  // and the above OV3660 block is NOT being hit,
  // you might need to adjust AWB manually or fine-tune saturation here.
  // For OV2640, set_saturation is typically applied globally if not specifically in a PID block.

  // Ensure Auto Exposure and Auto White Balance are enabled for best light adaptation
  s->set_exposure_ctrl(s, 1); // Enable Auto Exposure (AEC)
  s->set_gain_ctrl(s, 1);     // Enable Auto Gain Control (AGC)
  s->set_whitebal(s, 1);      // Enable Auto White Balance (AWB)
  s->set_awb_gain(s, 1);      // Enable AWB gain (should be enabled if AWB is on)

  // Moderate gain ceiling for better image quality
  s->set_gainceiling(s, GAINCEILING_32X); // Reduced gain to prevent overexposure and noise

  // --- Crucial Adjustments for your Red Tint and Sharpness ---
  // Try these values and observe the changes.

  // 1. White Balance Mode (AWB_MODE_AUTO is 0, but other modes can be tested)
  // If AWB is struggling, you can try different modes or manual RGB gain.
  // For the red tint, we'll try to let AWB do its job first and ensure it's not being overridden.
  // If `set_whitebal` is 1 (Auto), then `set_wb_mode` might not have an effect or it
  // selects a specific preset for AWB. Default (0) is usually auto.
  // If the red tint persists after other changes, you might need to try AWB_INCANDESCENT (1)
  // or AWB_FLUORESCENT (2) to see if it corrects it.
  // For now, let's keep it at 0 (auto) if AWB is enabled.
  s->set_wb_mode(s, 0); // 0 = Auto. Ensure this is explicitly set if you suspect issues.

  // Saturation already set per-camera above
  // s->set_saturation(s, 2); // Commented out - using per-camera settings

  // 3. Sharpness: Maximum for crispest image
  s->set_sharpness(s, 2); // -2 to 2, maximum sharpness

  // 4. Denoise: Minimal to preserve maximum detail
  s->set_denoise(s, 1); // Slight denoise for cleaner image while preserving detail

  // Brightness and contrast already set per-camera above
  // s->set_brightness(s, 0); // Commented out - using per-camera settings
  // s->set_contrast(s, 2);   // Commented out - using per-camera settings

  // Image corrections (keep these enabled as you have them, they are beneficial)
  s->set_lenc(s, 1);    // Enable Lens Correction (corrects distortion)
  s->set_wpc(s, 1);     // Enable White Pixel Correction
  s->set_bpc(s, 1);     // Enable Black Pixel Correction (can sometimes introduce artifacts, test it)
  s->set_raw_gma(s, 1); // Enable Gamma Correction (improves tonal representation)

  // Ensure no special effects are applied by default
  s->set_special_effect(s, 0); // 0 = No effect (Normal)

  // If you are certain your camera is an OV2640 and not OV3660, you can remove the specific `if (s->id.PID == OV3660_PID)` block completely.
  // Otherwise, ensure the settings within it are suitable or overridden later.
  // The `s->set_vflip(s, 1);` might still be needed depending on how your camera module is oriented.

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  // Get camera ID and check for static IP assignment
  String cameraId = generateShortUniqueId();
  IPAddress assignedIP = getAssignedIP(cameraId);
  
  if (assignedIP != IPAddress(0, 0, 0, 0)) {
    // Configure static IP if assigned
    IPAddress gateway(192, 168, 4, 1);  // Adjust to your router's gateway
    IPAddress subnet(255, 255, 252, 0); // Your router's subnet mask
    WiFi.config(assignedIP, gateway, subnet);
    Serial.println("Using static IP: " + assignedIP.toString());
  }
  
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("UniqueID: " + cameraId);

  // Initialize mDNS
  if (!MDNS.begin(cameraId.c_str())) {
    Serial.println("Error setting up mDNS responder!");
  } else {
    Serial.println("mDNS responder started");
    // Add service to mDNS
    MDNS.addService("http", "tcp", 80);
  }

  // Configure camera sensor with per-camera settings
  Serial.printf("Camera ID: %s, Sensor PID: 0x%02X\n", cameraId.c_str(), s->id.PID);
  
  // Per-camera fine-tuning for OV5640 variations - balanced brightness, improve sharpness
  if (cameraId == "150deaa5" || cameraId == "031da19d") {
    // Good cameras - optimized settings
    s->set_brightness(s, 0);   // Balanced brightness (middle between 0 and -1)
    s->set_contrast(s, 1);     // Increase contrast for sharpness
    s->set_saturation(s, 0);   // Keep saturation neutral
    s->set_wb_mode(s, 0);      // Auto WB
  } else {
    // Problematic cameras - adjusted settings
    s->set_brightness(s, -1);  // Balanced brightness (middle between 0 and -2)
    s->set_contrast(s, 2);     // Higher contrast for sharpness
    s->set_saturation(s, -1);  // Reduce saturation
    s->set_wb_mode(s, 1);      // Try different WB mode
  }

  // Define the /set_bw endpoint
  server.on("/set_bw", HTTP_GET, handleSetBW);
  // Define the /capture endpoint
  server.on("/capture", HTTP_GET, handleCapture);

  // Define stream page
  server.on("/stream", HTTP_GET, []() {
    String cameraId = generateShortUniqueId();
    String ipAddr = WiFi.localIP().toString();
    String html = String(streamPageTemplate);
    html.replace("TITLE_PLACEHOLDER", "Sani Flush Cam Stream " + cameraId + " - " + ipAddr);
    html.replace("HEADER_PLACEHOLDER", getCommonHeader(cameraId, ipAddr));
    server.send(200, "text/html", html);
  });
  // Define raw stream
  server.on("/stream_raw", HTTP_GET, handleStream);
  // Define settings page
  server.on("/settings", HTTP_GET, handleSettings);
  // Define settings API
  server.on("/api/settings", HTTP_GET, handleCameraSetting);
  // Define WiFi status endpoint
  server.on("/api/wifi", HTTP_GET, []() {
    String json = "{";
    json += "\"ssid\":\"" + String(WiFi.SSID()) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI());
    json += "}";
    server.send(200, "application/json", json);
  });
  // Define log endpoint
  server.on("/api/log", HTTP_GET, []() {
    String html = "";
    int start = logCount < 300 ? 0 : logIndex;
    for (int i = 0; i < logCount; i++) {
      int idx = (start + i) % 300;
      html += logBuffer[idx] + "<br>";
    }
    server.send(200, "text/html", html);
  });
  // Define current values endpoint
  server.on("/api/values", HTTP_GET, []() {
    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
      server.send(500, "text/plain", "Camera sensor not available");
      return;
    }
    String json = "{";
    json += "\"brightness\":" + String(s->status.brightness) + ",";
    json += "\"contrast\":" + String(s->status.contrast) + ",";
    json += "\"saturation\":" + String(s->status.saturation) + ",";
    json += "\"sharpness\":" + String(s->status.sharpness) + ",";
    json += "\"ae_level\":" + String(s->status.ae_level) + ",";
    json += "\"gainceiling\":" + String(s->status.gainceiling) + ",";
    json += "\"wb_mode\":" + String(s->status.wb_mode) + ",";
    json += "\"denoise\":" + String(s->status.denoise) + ",";
    json += "\"aec\":" + String(s->status.aec ? "true" : "false") + ",";
    json += "\"agc\":" + String(s->status.agc ? "true" : "false") + ",";
    json += "\"awb\":" + String(s->status.awb ? "true" : "false") + ",";
    json += "\"awb_gain\":" + String(s->status.awb_gain ? "true" : "false") + ",";
    json += "\"lenc\":" + String(s->status.lenc ? "true" : "false") + ",";
    json += "\"wpc\":" + String(s->status.wpc ? "true" : "false") + ",";
    json += "\"bpc\":" + String(s->status.bpc ? "true" : "false") + ",";
    json += "\"raw_gma\":" + String(s->status.raw_gma ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
  });
  // Define the root endpoint for a simple UI
  server.on("/", HTTP_GET, handleRoot);

  // Start the web server
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  server.handleClient();
  delay(10); // Small delay to keep the ESP32 responsive
}
void handleCameraSetting() {
  Serial.println("handleCameraSetting called");
  if (!server.hasArg("setting") || !server.hasArg("action")) {
    server.send(400, "text/plain", "Missing 'setting' or 'action' parameters. Use /settings?setting=<name>&action=<+/-/toggle>");
    return;
  }

  String settingName = server.arg("setting");
  String actionStr = server.arg("action");
  
  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    server.send(500, "text/plain", "Camera sensor not available.");
    return;
  }

  String response_msg;
  bool setting_applied = false;

  // Handle toggle actions
  if (actionStr == "toggle") {
    if (settingName == "aec") {
      int current = s->status.aec;
      if (s->set_exposure_ctrl(s, !current) == 0) {
        response_msg = "Auto Exposure " + String(!current ? "enabled" : "disabled");
        setting_applied = true;
      }
    }
    else if (settingName == "agc") {
      int current = s->status.agc;
      if (s->set_gain_ctrl(s, !current) == 0) {
        response_msg = "Auto Gain " + String(!current ? "enabled" : "disabled");
        setting_applied = true;
      }
    }
    else if (settingName == "awb") {
      int current = s->status.awb;
      if (s->set_whitebal(s, !current) == 0) {
        response_msg = "Auto White Balance " + String(!current ? "enabled" : "disabled");
        setting_applied = true;
      }
    }
    else if (settingName == "awb_gain") {
      int current = s->status.awb_gain;
      if (s->set_awb_gain(s, !current) == 0) {
        response_msg = "AWB Gain " + String(!current ? "enabled" : "disabled");
        setting_applied = true;
      }
    }
    else if (settingName == "lenc") {
      int current = s->status.lenc;
      if (s->set_lenc(s, !current) == 0) {
        response_msg = "Lens Correction " + String(!current ? "enabled" : "disabled");
        setting_applied = true;
      }
    }
    else if (settingName == "wpc") {
      int current = s->status.wpc;
      if (s->set_wpc(s, !current) == 0) {
        response_msg = "White Pixel Correction " + String(!current ? "enabled" : "disabled");
        setting_applied = true;
      }
    }
    else if (settingName == "bpc") {
      int current = s->status.bpc;
      if (s->set_bpc(s, !current) == 0) {
        response_msg = "Black Pixel Correction " + String(!current ? "enabled" : "disabled");
        setting_applied = true;
      }
    }
    else if (settingName == "raw_gma") {
      int current = s->status.raw_gma;
      if (s->set_raw_gma(s, !current) == 0) {
        response_msg = "Gamma Correction " + String(!current ? "enabled" : "disabled");
        setting_applied = true;
      }
    }
  }
  // Handle direct value setting
  else if (server.hasArg("value")) {
    int8_t new_val = server.arg("value").toInt();
    
    if (settingName == "brightness") {
      new_val = constrain(new_val, -2, 2);
      if (s->set_brightness(s, new_val) == 0) {
        response_msg = "Brightness set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "contrast") {
      new_val = constrain(new_val, -2, 2);
      if (s->set_contrast(s, new_val) == 0) {
        response_msg = "Contrast set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "saturation") {
      new_val = constrain(new_val, -2, 2);
      if (s->set_saturation(s, new_val) == 0) {
        response_msg = "Saturation set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "sharpness") {
      new_val = constrain(new_val, -2, 2);
      if (s->set_sharpness(s, new_val) == 0) {
        response_msg = "Sharpness set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "ae_level") {
      new_val = constrain(new_val, -2, 2);
      if (s->set_ae_level(s, new_val) == 0) {
        response_msg = "AE Level set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "gainceiling") {
      new_val = constrain(new_val, 0, 6);
      if (s->set_gainceiling(s, (gainceiling_t)new_val) == 0) {
        response_msg = "Gain Ceiling set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "wb_mode") {
      new_val = constrain(new_val, 0, 4);
      if (s->set_wb_mode(s, new_val) == 0) {
        response_msg = "White Balance Mode set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "denoise") {
      new_val = constrain(new_val, 0, 8);
      if (s->set_denoise(s, new_val) == 0) {
        response_msg = "Denoise set to " + String(new_val);
        setting_applied = true;
      }
    }
  }
  // Handle +/- actions
  else {
    char action = actionStr[0];
    int8_t current_val, new_val;
    
    if (settingName == "brightness") {
      current_val = s->status.brightness;
      if (action == '+' && current_val < 2) {
        new_val = current_val + 1;
      } else if (action == '-' && current_val > -2) {
        new_val = current_val - 1;
      } else {
        new_val = current_val; // Don't change if at limit
      }
      if (s->set_brightness(s, new_val) == 0) {
        response_msg = "Brightness set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "contrast") {
      current_val = s->status.contrast;
      if (action == '+' && current_val < 2) {
        new_val = current_val + 1;
      } else if (action == '-' && current_val > -2) {
        new_val = current_val - 1;
      } else {
        new_val = current_val;
      }
      if (s->set_contrast(s, new_val) == 0) {
        response_msg = "Contrast set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "saturation") {
      current_val = s->status.saturation;
      if (action == '+' && current_val < 2) {
        new_val = current_val + 1;
      } else if (action == '-' && current_val > -2) {
        new_val = current_val - 1;
      } else {
        new_val = current_val;
      }
      if (s->set_saturation(s, new_val) == 0) {
        response_msg = "Saturation set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "sharpness") {
      current_val = s->status.sharpness;
      if (action == '+' && current_val < 2) {
        new_val = current_val + 1;
      } else if (action == '-' && current_val > -2) {
        new_val = current_val - 1;
      } else {
        new_val = current_val;
      }
      if (s->set_sharpness(s, new_val) == 0) {
        response_msg = "Sharpness set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "ae_level") {
      current_val = s->status.ae_level;
      if (action == '+' && current_val < 2) {
        new_val = current_val + 1;
      } else if (action == '-' && current_val > -2) {
        new_val = current_val - 1;
      } else {
        new_val = current_val;
      }
      if (s->set_ae_level(s, new_val) == 0) {
        response_msg = "AE Level set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "gainceiling") {
      current_val = s->status.gainceiling;
      new_val = current_val + (action == '+' ? 1 : -1);
      new_val = constrain(new_val, 0, 6);
      if (s->set_gainceiling(s, (gainceiling_t)new_val) == 0) {
        response_msg = "Gain Ceiling set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "wb_mode") {
      current_val = s->status.wb_mode;
      new_val = current_val + (action == '+' ? 1 : -1);
      new_val = constrain(new_val, 0, 4);
      if (s->set_wb_mode(s, new_val) == 0) {
        response_msg = "White Balance Mode set to " + String(new_val);
        setting_applied = true;
      }
    }
    else if (settingName == "denoise") {
      current_val = s->status.denoise;
      new_val = current_val + (action == '+' ? 1 : -1);
      new_val = constrain(new_val, 0, 8);
      if (s->set_denoise(s, new_val) == 0) {
        response_msg = "Denoise set to " + String(new_val);
        setting_applied = true;
      }
    }
  }

  if (setting_applied) {
    server.send(200, "text/plain", response_msg);
  } else {
    server.send(500, "text/plain", "Failed to apply setting: " + settingName);
  }
}

void handleSettings() {
  String cameraId = generateShortUniqueId();
  String ipAddr = WiFi.localIP().toString();
  String html = String(settingsPageTemplate);
  html.replace("TITLE_PLACEHOLDER", "Sani Flush Cam Settings " + cameraId + " - " + ipAddr);
  html.replace("HEADER_PLACEHOLDER", getCommonHeader(cameraId, ipAddr));
  server.send(200, "text/html", html);
}

void handleSettingsOld() {
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>ESP32 Camera Settings</title>
      <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f5f5f5; }
        .container { max-width: 1200px; margin: 0 auto; }
        .video-container { text-align: center; margin-bottom: 30px; }
        .settings-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 20px; }
        .setting-group { background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .setting-group h3 { margin-top: 0; color: #333; border-bottom: 2px solid #007bff; padding-bottom: 10px; }
        .setting-item { display: flex; justify-content: space-between; align-items: center; margin: 15px 0; }
        .setting-controls { display: flex; gap: 10px; align-items: center; }
        button { padding: 8px 15px; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }
        .btn-dec { background: #dc3545; color: white; }
        .btn-inc { background: #28a745; color: white; }
        .btn-toggle { background: #007bff; color: white; }
        button:hover { opacity: 0.8; }
        .value { font-weight: bold; min-width: 30px; text-align: center; }
        .mode-controls { text-align: center; margin: 20px 0; }
        .mode-controls label { margin: 0 15px; }
        .wifi-status{position:fixed;top:10px;right:10px;background:rgba(255,255,255,0.9);padding:8px;border-radius:5px;font-size:11px;border:1px solid #ccc;display:flex;align-items:center;gap:8px}
        .wifi-bars{display:flex;gap:1px;align-items:end}
        .bar{width:4px;height:12px;background:#ddd}
        .bar.active{background:var(--bar-color)}
      </style>
    </head>
    <body>
      <div class="wifi-status">
        <div id="wifiText">WiFi: Loading...</div>
        <div class="wifi-bars" id="wifiBars">
          <div class="bar" style="height:3px"></div>
          <div class="bar" style="height:5px"></div>
          <div class="bar" style="height:7px"></div>
          <div class="bar" style="height:9px"></div>
          <div class="bar" style="height:11px"></div>
          <div class="bar" style="height:13px"></div>
          <div class="bar" style="height:15px"></div>
          <div class="bar" style="height:17px"></div>
        </div>
      </div>
      <div class="container">
        <h1 style="text-align: center; color: #333;">ESP32 Camera Settings</h1>
        
        <div class="video-container">
          <img id="preview" src="/capture" style="width:90%; max-width: 640px; border:2px solid #007bff; border-radius: 8px;">
          <br><br>
          <label><input type="radio" name="refresh" value="manual" checked onchange="setRefresh(this.value)"> Manual Refresh</label>
          <label><input type="radio" name="refresh" value="auto" onchange="setRefresh(this.value)"> Auto Refresh (3s)</label>
          <label><input type="checkbox" id="useFlash" checked> Use Flash</label>
          <button onclick="refreshPreview()">Refresh Now</button>
        </div>
        
        <div class="mode-controls">
          <label><input type="radio" name="mode" value="color" onchange="setMode(this.value)" checked> Color</label>
          <label><input type="radio" name="mode" value="bw" onchange="setMode(this.value)"> Black & White</label>
        </div>
        
        <div class="settings-grid">
          <div class="setting-group">
            <h3>Exposure & Light</h3>
            <div class="setting-item">
              <span>Auto Exposure (AEC)</span>
              <div class="setting-controls">
                <span class="value" id="aec">ON</span>
                <button class="btn-toggle" onclick="toggleSetting('aec')">Toggle</button>
              </div>
            </div>
            <div class="setting-item">
              <span>AE Level</span>
              <div class="setting-controls">
                <button class="btn-dec" onclick="adjustSetting('ae_level', '-')">-</button>
                <span class="value" id="ae_level">0</span>
                <button class="btn-inc" onclick="adjustSetting('ae_level', '+')">+</button>
              </div>
            </div>
            <div class="setting-item">
              <span>Auto Gain (AGC)</span>
              <div class="setting-controls">
                <span class="value" id="agc">ON</span>
                <button class="btn-toggle" onclick="toggleSetting('agc')">Toggle</button>
              </div>
            </div>
            <div class="setting-item">
              <span>Gain Ceiling</span>
              <div class="setting-controls">
                <button class="btn-dec" onclick="adjustSetting('gainceiling', '-')">-</button>
                <span class="value" id="gainceiling">4</span>
                <button class="btn-inc" onclick="adjustSetting('gainceiling', '+')">+</button>
              </div>
            </div>
          </div>
          
          <div class="setting-group">
            <h3>Image Quality</h3>
            <div class="setting-item">
              <span>Brightness</span>
              <div class="setting-controls">
                <button class="btn-dec" onclick="adjustSetting('brightness', '-')">-</button>
                <span class="value" id="brightness">0</span>
                <button class="btn-inc" onclick="adjustSetting('brightness', '+')">+</button>
              </div>
            </div>
            <div class="setting-item">
              <span>Contrast</span>
              <div class="setting-controls">
                <button class="btn-dec" onclick="adjustSetting('contrast', '-')">-</button>
                <span class="value" id="contrast">0</span>
                <button class="btn-inc" onclick="adjustSetting('contrast', '+')">+</button>
              </div>
            </div>
            <div class="setting-item">
              <span>Saturation</span>
              <div class="setting-controls">
                <button class="btn-dec" onclick="adjustSetting('saturation', '-')">-</button>
                <span class="value" id="saturation">0</span>
                <button class="btn-inc" onclick="adjustSetting('saturation', '+')">+</button>
              </div>
            </div>
            <div class="setting-item">
              <span>Sharpness</span>
              <div class="setting-controls">
                <button class="btn-dec" onclick="adjustSetting('sharpness', '-')">-</button>
                <span class="value" id="sharpness">0</span>
                <button class="btn-inc" onclick="adjustSetting('sharpness', '+')">+</button>
              </div>
            </div>
          </div>
          
          <div class="setting-group">
            <h3>White Balance</h3>
            <div class="setting-item">
              <span>Auto White Balance</span>
              <div class="setting-controls">
                <span class="value" id="awb">ON</span>
                <button class="btn-toggle" onclick="toggleSetting('awb')">Toggle</button>
              </div>
            </div>
            <div class="setting-item">
              <span>WB Mode</span>
              <div class="setting-controls">
                <button class="btn-dec" onclick="adjustSetting('wb_mode', '-')">-</button>
                <span class="value" id="wb_mode">0</span>
                <button class="btn-inc" onclick="adjustSetting('wb_mode', '+')">+</button>
              </div>
            </div>
            <div class="setting-item">
              <span>AWB Gain</span>
              <div class="setting-controls">
                <span class="value" id="awb_gain">ON</span>
                <button class="btn-toggle" onclick="toggleSetting('awb_gain')">Toggle</button>
              </div>
            </div>
          </div>
          
          <div class="setting-group">
            <h3>Corrections</h3>
            <div class="setting-item">
              <span>Lens Correction</span>
              <div class="setting-controls">
                <span class="value" id="lenc">ON</span>
                <button class="btn-toggle" onclick="toggleSetting('lenc')">Toggle</button>
              </div>
            </div>
            <div class="setting-item">
              <span>White Pixel Correction</span>
              <div class="setting-controls">
                <span class="value" id="wpc">ON</span>
                <button class="btn-toggle" onclick="toggleSetting('wpc')">Toggle</button>
              </div>
            </div>
            <div class="setting-item">
              <span>Black Pixel Correction</span>
              <div class="setting-controls">
                <span class="value" id="bpc">ON</span>
                <button class="btn-toggle" onclick="toggleSetting('bpc')">Toggle</button>
              </div>
            </div>
            <div class="setting-item">
              <span>Gamma Correction</span>
              <div class="setting-controls">
                <span class="value" id="raw_gma">ON</span>
                <button class="btn-toggle" onclick="toggleSetting('raw_gma')">Toggle</button>
              </div>
            </div>
            <div class="setting-item">
              <span>Denoise</span>
              <div class="setting-controls">
                <button class="btn-dec" onclick="adjustSetting('denoise', '-')">-</button>
                <span class="value" id="denoise">0</span>
                <button class="btn-inc" onclick="adjustSetting('denoise', '+')">+</button>
              </div>
            </div>
          </div>
        </div>
      </div>

      <script>
        function setMode(mode) {
          fetch("/set_bw?mode=" + mode)
            .then(resp => resp.text())
            .then(data => console.log(data))
            .catch(err => console.error("Error setting mode:", err));
        }
        
        let autoRefreshInterval;
        
        function adjustSetting(setting, action) {
          fetch(`/api/settings?setting=${setting}&action=${action}`)
            .then(resp => resp.text())
            .then(data => {
              console.log(data);
              if (data.includes('set to')) {
                const value = data.split('set to ')[1];
                const element = document.getElementById(setting);
                if (element) element.textContent = value;
              }
              setTimeout(refreshPreview, 500);
            })
            .catch(err => console.error("Error adjusting " + setting, err));
        }
        
        function toggleSetting(setting) {
          // This function now also calls the API to update the setting on the ESP32
          fetch(`/api/settings?setting=${setting}&action=toggle`)
            .then(resp => resp.text())
            .then(data => {
              console.log(data);
              loadCurrentValues(); // Refresh all values after toggle
              refreshPreview(); // Refresh the image to see changes
            })
            .catch(err => console.error("Error toggling " + setting, err));
        }
        

        
        function refreshPreview() {
          const useFlash = document.getElementById('useFlash').checked;
          const flashParam = useFlash ? '&flash=true' : '';
          document.getElementById('preview').src = '/capture?t=' + Date.now() + flashParam;
        }
        
        function refreshPreview() {
          const useFlash = document.getElementById('useFlash').checked;
          const flashParam = useFlash ? '&flash=true' : '';
          document.getElementById('preview').src = '/capture?t=' + Date.now() + flashParam;
        }
        
        function setRefresh(mode) {
          if (autoRefreshInterval) clearInterval(autoRefreshInterval);
          if (mode === 'auto') {
            autoRefreshInterval = setInterval(refreshPreview, 3000);
          }
        }
        
        function loadCurrentValues() {
          fetch('/api/values')
            .then(resp => resp.json())
            .then(data => {
              Object.keys(data).forEach(key => {
                const element = document.getElementById(key);
                if (element) {
                  if (typeof data[key] === 'boolean') {
                    element.textContent = data[key] ? 'ON' : 'OFF';
                    element.style.color = data[key] ? '#28a745' : '#dc3545';
                  } else {
                    element.textContent = data[key];
                  }
                }
              });
            })
            .catch(err => console.error('Error loading values:', err));
        }
        
        // Initial load of values and start auto-refresh if selected
        function updateWifiStatus(){fetch('/api/wifi').then(r=>r.json()).then(d=>{const rssi=d.rssi;let bars=0,color='#ff0000',text='Poor';if(rssi>=-50){bars=8;color='#00ff00';text='Excellent'}else if(rssi>=-60){bars=6;color='#80ff00';text='Good'}else if(rssi>=-70){bars=4;color='#ffff00';text='Fair'}else if(rssi>=-80){bars=2;color='#ff8000';text='Weak'}else{bars=1;color='#ff0000';text='Poor'}document.getElementById('wifiText').innerHTML=d.ssid+'<br>'+text+' ('+rssi+'dBm)';const barElements=document.querySelectorAll('#wifiBars .bar');barElements.forEach((bar,i)=>{if(i<bars){bar.classList.add('active');bar.style.setProperty('--bar-color',color)}else{bar.classList.remove('active')}})})}
        
        window.onload = () => {
            loadCurrentValues();
            updateWifiStatus();
            setInterval(updateWifiStatus,10000);
        };
      </script>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void uploadSnapshot() {
  camera_fb_t* fb = esp_camera_fb_get(); // Get a frame buffer from the camera
  if (!fb) {
    Serial.println("Camera frame buffer failed to get!");
    return;
  }

  Serial.println("Snapshot captured. Sending to server...");

  WiFiClient client; // Use a WiFiClient for the HTTP connection
  HTTPClient http;   // HTTPClient library instance

  // Construct the full URL for your Flask server's /upload endpoint
  // Example: http://192.168.1.100:5000/upload
  String serverUrl = "http://" + WiFi.localIP().toString() + ":" + String(serverPort) + "/upload";
  http.begin(client, serverUrl); // Begin the connection to the server

  // Set HTTP headers for the POST request
  http.addHeader("Content-Type", "image/jpeg"); // Tell the server it's a JPEG image
  
  // Optional: You can send a filename in a custom header if your Python server expects it
  // (Your Python server code might already be looking for a 'Filename' header)
  // char filename[30];
  // sprintf(filename, "esp32cam_%lu.jpg", millis()); // Example filename with current uptime
  // http.addHeader("Filename", filename); 

  // Send the image data (from fb->buf) with its length (fb->len) as the POST request body
  int httpResponseCode = http.POST(fb->buf, fb->len);

  // --- Handle the server's response ---
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    String response = http.getString(); // Get the response body from the server
    Serial.print("Server response: ");
    Serial.println(response);
  } else {
    Serial.print("Error sending snapshot. HTTP Error code: ");
    Serial.println(httpResponseCode);
    Serial.println(http.errorToString(httpResponseCode)); // Print a more descriptive error message
  }

  http.end(); // Close the connection
  esp_camera_fb_return(fb); // IMPORTANT: Return the frame buffer to release memory
}

void handleStream() {
  WiFiClient client = server.client();

  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(response);

  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      continue;
    }

    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");

    esp_camera_fb_return(fb);

    delay(100); // Adjust for desired frame rate (e.g. 10fps)
  }
}

void handleSetBW() {
  if (!server.hasArg("mode")) {
    server.send(400, "text/plain", "Missing 'mode' parameter");
    return;
  }

  String mode = server.arg("mode");
  sensor_t *s = esp_camera_sensor_get();

  if (mode == "bw") {
    s->set_special_effect(s, 2); // Grayscale
  } else if (mode == "color") {
    s->set_special_effect(s, 0); // Normal color
  } else {
    server.send(400, "text/plain", "Invalid mode. Use 'bw' or 'color'.");
    return;
  }

  server.send(200, "text/plain", "Mode set to " + mode);
}