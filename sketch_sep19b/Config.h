#ifndef CONFIG_H
#define CONFIG_H

// --- Hardware & Display Settings ---
#define SCREEN_WIDTH      240  
#define SCREEN_HEIGHT     320  
#define DISPLAY_ROTATION  0    

// --- Color Scheme (565 RGB) ---
#define COLOR_BG          0x0000 // Black background
#define COLOR_TEXT        0x07E0 // Bright Green terminal text

// --- Network & Ollama API Default Configurations ---
#define DEFAULT_SSID        "M324"
#define DEFAULT_PASSPHRASE  "[Passphrase]"
#define DEFAULT_OLLAMA_IP   "[IP Address]"
#define DEFAULT_OLLAMA_PORT 11434
#define DEFAULT_MODEL_NAME  "[model:tag]"

#endif // CONFIG_H
