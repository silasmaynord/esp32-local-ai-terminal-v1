#ifndef CONFIG_H
#define CONFIG_H

// --- Hardware & Display Settings ---
#define SCREEN_WIDTH      320  // Landscape width
#define SCREEN_HEIGHT     240  // Landscape height
#define DISPLAY_ROTATION  1    // Landscape orientation (1 or 3)

// --- Color Scheme (565 RGB) ---
#define COLOR_BG          0x0000 // Black background
#define COLOR_TEXT        0x07E0 // Bright Green terminal text
#define COLOR_PROMPT      0x07FF // Cyan prompt text
#define COLOR_HEADER_BG   0x0010 // Dark slate header bar
#define COLOR_HEADER_TXT  0xFFFF // White header text
#define COLOR_ERROR       0xF800 // Red highlight for connection errors

// --- Network & Ollama API Default Configurations ---
#define DEFAULT_SSID        "ssid"
#define DEFAULT_PASSPHRASE  "password"
#define DEFAULT_OLLAMA_IP   "192.168.0.0"
#define DEFAULT_OLLAMA_PORT 11434
#define DEFAULT_MODEL_NAME  "qwen3.6"

// --- Buffer Sizes ---
#define MAX_HTTP_PAYLOAD    1024
#define JSON_DOC_SIZE       512

#endif // CONFIG_H
