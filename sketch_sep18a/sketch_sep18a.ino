#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include "Config.h"

// --- Custom Pin definitions for XPT2046 Touchscreen on CYD ---
#define XPT2046_IRQ   36
#define XPT2046_MOSI  32
#define XPT2046_MISO  39
#define XPT2046_CLK   25
#define XPT2046_CS    33

// Dedicated SPI instance for the touch controller
SPIClass touchSpi = SPIClass(VSPI);

// Drivers
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

// Runtime parameters
String wifiSSID = DEFAULT_SSID;
String wifiPass = DEFAULT_PASSPHRASE;
String ollamaIP = DEFAULT_OLLAMA_IP;
uint16_t ollamaPort = DEFAULT_OLLAMA_PORT;
String modelName = DEFAULT_MODEL_NAME;

// Layout Dimensions
const uint16_t headerHeight = 20;
const uint16_t kbdTop = 140;       
uint16_t cursorX = 0;
uint16_t cursorY = headerHeight + 2;
const uint8_t charWidth = 6;
const uint8_t charHeight = 8;

// Input Buffer
String userBuffer = "";

// 3-Row Minimal Touch Keyboard Layout
const char keyLayout[3][11] = {
  {'Q','W','E','R','T','Y','U','I','O','P','\0'},
  {'A','S','D','F','G','H','J','K','L','\0'},
  {'Z','X','C','V','B','N','M','<','=','\0'} // '<' = Backspace, '=' = Enter/Send
};

// Raw ADC touch bounds for CYD screen mapping
const int TOUCH_MIN_X = 200;
const int TOUCH_MAX_X = 3700;
const int TOUCH_MIN_Y = 240;
const int TOUCH_MAX_Y = 3800;

// Forward declarations
void drawKeyboard();
void handleTouch();
void printTerminalText(const String &text, uint16_t color = COLOR_TEXT);
void sendPromptToOllama(const String &userPrompt);

void setup() {
  Serial.begin(115200);

  // Initialize TFT Display
  tft.begin();
  tft.setRotation(DISPLAY_ROTATION);
  tft.fillScreen(COLOR_BG);

  // Initialize custom SPI bus for Touch Controller
  touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSpi);
  ts.setRotation(1); // Match display orientation

  // Draw initial header UI
  tft.fillRect(0, 0, SCREEN_WIDTH, headerHeight, COLOR_HEADER_BG);
  tft.setTextColor(COLOR_HEADER_TXT, COLOR_HEADER_BG);
  tft.setCursor(4, 6);
  tft.print("OLLAMA TERMINAL | XPT2046 TOUCH");

  drawKeyboard();

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());
  while (WiFi.status() != WL_CONNECTED) { delay(250); }

  printTerminalText("Ready. Type prompt & press '='...\n", COLOR_PROMPT);
}

void loop() {
  handleTouch();
  
  // Physical serial backup
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      printTerminalText("\nUser> " + input + "\n", COLOR_PROMPT);
      sendPromptToOllama(input);
    }
  }
  delay(20);
}

// Render keyboard layout with TFT_eSPI
void drawKeyboard() {
  tft.fillRect(0, kbdTop, SCREEN_WIDTH, SCREEN_HEIGHT - kbdTop, 0x18C3);
  tft.setTextColor(0xFFFF, 0x18C3);

  for (int r = 0; r < 3; r++) {
    for (int c = 0; keyLayout[r][c] != '\0'; c++) {
      int x = c * 32 + 2;
      int y = kbdTop + (r * 32) + 2;
      tft.drawRoundRect(x, y, 28, 28, 3, 0x7BEF);
      tft.setCursor(x + 10, y + 10);
      tft.print(keyLayout[r][c]);
    }
  }
}

// Read touch input over custom SPI bus
void handleTouch() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    // Map raw ADC values to TFT pixel coordinates
    int t_x = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, SCREEN_WIDTH);
    int t_y = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, SCREEN_HEIGHT);

    // Constrain mapped coordinates to screen limits
    t_x = constrain(t_x, 0, SCREEN_WIDTH);
    t_y = constrain(t_y, 0, SCREEN_HEIGHT);

    // Check if tap fell inside keyboard grid
    if (t_y >= kbdTop) {
      int col = t_x / 32;
      int row = (t_y - kbdTop) / 32;

      if (row >= 0 && row < 3 && col >= 0 && col < 10) {
        char key = keyLayout[row][col];
        if (key != '\0') {
          if (key == '<') { // Backspace
            if (userBuffer.length() > 0) {
              userBuffer.remove(userBuffer.length() - 1);
              printTerminalText("<", COLOR_ERROR);
            }
          } else if (key == '=') { // Enter / Send
            if (userBuffer.length() > 0) {
              String promptToSend = userBuffer;
              userBuffer = "";
              printTerminalText("\nUser> " + promptToSend + "\n", COLOR_PROMPT);
              sendPromptToOllama(promptToSend);
            }
          } else { // Character key
            userBuffer += key;
            String sKey = String(key);
            printTerminalText(sKey, COLOR_TEXT);
          }
          delay(200); // Debounce press
        }
      }
    }
  }
}

void printTerminalText(const String &text, uint16_t color) {
  tft.setTextColor(color, COLOR_BG);

  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c == '\n') {
      cursorX = 0;
      cursorY += charHeight;
    } else if (c == '<') {
      if (cursorX >= charWidth) cursorX -= charWidth;
      tft.fillRect(cursorX, cursorY, charWidth, charHeight, COLOR_BG);
    } else {
      tft.drawChar(cursorX, cursorY, c, color, COLOR_BG, 1);
      cursorX += charWidth;
      if (cursorX >= SCREEN_WIDTH - charWidth) {
        cursorX = 0;
        cursorY += charHeight;
      }
    }

    if (cursorY >= kbdTop - charHeight) {
      tft.fillRect(0, headerHeight, SCREEN_WIDTH, kbdTop - headerHeight, COLOR_BG);
      cursorY = headerHeight + 2;
      cursorX = 0;
    }
  }
}

void sendPromptToOllama(const String &userPrompt) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String targetURL = "http://" + ollamaIP + ":" + String(ollamaPort) + "/api/generate";

  http.begin(targetURL);
  http.setTimeout(15000);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<JSON_DOC_SIZE> requestDoc;
  requestDoc["model"] = modelName;
  requestDoc["prompt"] = userPrompt;
  requestDoc["stream"] = true;

  String requestBody;
  serializeJson(requestDoc, requestBody);

  int httpCode = http.POST(requestBody);

  if (httpCode == HTTP_CODE_OK) {
    WiFiClient* stream = http.getStreamPtr();
    StaticJsonDocument<JSON_DOC_SIZE> responseDoc;

    while (http.connected() && (stream->available() || stream->connected())) {
      if (stream->available()) {
        String line = stream->readStringUntil('\n');
        DeserializationError err = deserializeJson(responseDoc, line);

        if (!err) {
          const char* token = responseDoc["response"];
          if (token) printTerminalText(String(token), COLOR_TEXT);
          if (responseDoc["done"]) break;
        }
      }
      yield();
    }
    printTerminalText("\n", COLOR_TEXT);
  } else {
    printTerminalText("Error: " + String(httpCode) + "\n", COLOR_ERROR);
  }

  http.end();
}
