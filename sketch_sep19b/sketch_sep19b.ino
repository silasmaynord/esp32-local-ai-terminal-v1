#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "Config.h"

// --- Hardware Pin Definitions (ESP32 CYD) ---
#define XPT2046_IRQ   36
#define XPT2046_MOSI  32
#define XPT2046_MISO  39
#define XPT2046_CLK   25
#define XPT2046_CS    33

SPIClass touchSpi = SPIClass(VSPI);

TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

#define TOUCH_MIN_X 200
#define TOUCH_MAX_X 3800
#define TOUCH_MIN_Y 200
#define TOUCH_MAX_Y 3800

// --- Screen Layout Metrics (Portrait) ---
const int portraitWidth = 240;
const int portraitHeight = 320;

const int toolbarHeight = 22;                     // Top navigation bar
const int kbdTop = 236;                          // Keyboard top boundary
const int inputLineHeight = 12;                  // Prompt line height
const int viewHeight = kbdTop - toolbarHeight - inputLineHeight; // Text area (~202px)

const int charWidth = 6;
const int charHeight = 8;
const int visibleLines = viewHeight / charHeight; // 25 visible text lines max

// --- Dynamic Line Storage Buffer ---
#define MAX_TEXT_LINES 200
String textLines[MAX_TEXT_LINES];
int totalLineCount = 0;
int currentLineOffset = 0; // Top line index currently displayed

// Input Buffer
String userBuffer = "";

// Persistent Ollama Settings
String ollamaIP = DEFAULT_OLLAMA_IP;
int ollamaPort = DEFAULT_OLLAMA_PORT;
String modelName = DEFAULT_MODEL_NAME;

// Persistent Conversation Context Array
JsonDocument contextDoc;

// --- Keyboard Layout Manager ---
enum KbdLayoutMode {
  MODE_ALPHA_LOWER,
  MODE_ALPHA_UPPER,
  MODE_NUMERIC,
  MODE_SYMBOL
};

KbdLayoutMode currentMode = MODE_ALPHA_LOWER;
int currentPage = 0;

struct Key {
  int x, y, w, h;
  char val;
  const char* label;
  bool isAction;
};

const char* alphaLowerChars = "abcdefghijklmnopqrstuvwxyz";
const char* alphaUpperChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const char* numericChars    = "0123456789+-*/=.,:;!?()";
const char* symbolChars     = "@#$%^&*_~|<>[]{}'\"`\\";

Key activeKeys[24];
size_t activeKeyCount = 0;

// --- Forward Declarations ---
void drawKeyboard();
void buildKeyboardLayout();
void renderToolbar();
void renderTextView();
void renderInputLine();
void appendTextToLines(const String &text);
void sendPromptToOllama(const String &userPrompt);
void handleTouch();
String sanitizeUtf8ToAscii(const String &input);

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(0); // Portrait (240x320)
  tft.fillScreen(COLOR_BG);

  touchSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSpi);
  ts.setRotation(0);

  buildKeyboardLayout();
  drawKeyboard();

  WiFi.begin(DEFAULT_SSID, DEFAULT_PASSPHRASE);
  uint8_t attempt = 0;
  while (WiFi.status() != WL_CONNECTED && attempt < 20) {
    delay(500);
    attempt++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    appendTextToLines("System Ready.");
  } else {
    appendTextToLines("Wi-Fi Connection Failed.");
  }

  renderToolbar();
  renderTextView();
  renderInputLine();
}

void loop() {
  handleTouch();
  delay(20);
}

// Format raw text block into discrete wrapped line entries
void appendTextToLines(const String &text) {
  String currentLineStr = "";

  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];

    if (c == '\n') {
      if (totalLineCount < MAX_TEXT_LINES) {
        textLines[totalLineCount++] = currentLineStr;
      }
      currentLineStr = "";
    } else {
      currentLineStr += c;
      if (currentLineStr.length() >= (portraitWidth / charWidth)) {
        if (totalLineCount < MAX_TEXT_LINES) {
          textLines[totalLineCount++] = currentLineStr;
        }
        currentLineStr = "";
      }
    }
  }

  if (currentLineStr.length() > 0 && totalLineCount < MAX_TEXT_LINES) {
    textLines[totalLineCount++] = currentLineStr;
  }

  // Auto-scroll offset down to reveal latest line
  if (totalLineCount > visibleLines) {
    currentLineOffset = totalLineCount - visibleLines;
  } else {
    currentLineOffset = 0;
  }
}

// Render Top Line-Navigation Toolbar
void renderToolbar() {
  tft.fillRect(0, 0, portraitWidth, toolbarHeight, COLOR_BG);
  tft.drawFastHLine(0, toolbarHeight - 1, portraitWidth, COLOR_TEXT);

  // [▲ Up] Button
  tft.drawRoundRect(4, 2, 48, toolbarHeight - 4, 3, COLOR_TEXT);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString("^ Up", 12, 5, 1);

  // [▼ Dn] Button
  tft.drawRoundRect(58, 2, 48, toolbarHeight - 4, 3, COLOR_TEXT);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.drawString("v Dn", 66, 5, 1);

  // Line counter indicator
  String statusStr = "L " + String(currentLineOffset + 1) + "/" + String(totalLineCount);
  tft.drawString(statusStr, 114, 5, 1);
}

// Render Main Text Viewing Window
void renderTextView() {
  tft.fillRect(0, toolbarHeight, portraitWidth, viewHeight, COLOR_BG);

  uint16_t curY = toolbarHeight + 2;
  tft.setTextColor(COLOR_TEXT, COLOR_BG);

  int maxLinesToDraw = min(visibleLines, totalLineCount - currentLineOffset);

  for (int i = 0; i < maxLinesToDraw; i++) {
    int lineIdx = currentLineOffset + i;
    if (lineIdx >= 0 && lineIdx < totalLineCount) {
      tft.drawString(textLines[lineIdx], 0, curY, 1);
      curY += charHeight;
    }
  }
}

// Render Active Live Typing Prompt Above Keyboard
void renderInputLine() {
  int inputY = kbdTop - inputLineHeight;

  tft.fillRect(0, inputY, portraitWidth, inputLineHeight, COLOR_BG);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);

  String displayText = "> " + userBuffer + "_";

  int maxChars = (portraitWidth / charWidth) - 1;
  if (displayText.length() > maxChars) {
    displayText = "> " + displayText.substring(displayText.length() - maxChars + 2);
  }

  tft.drawString(displayText, 0, inputY + 2, 1);
}

void buildKeyboardLayout() {
  activeKeyCount = 0;
  const char* charSet = alphaLowerChars;

  if (currentMode == MODE_ALPHA_UPPER) charSet = alphaUpperChars;
  else if (currentMode == MODE_NUMERIC) charSet = numericChars;
  else if (currentMode == MODE_SYMBOL)  charSet = symbolChars;

  size_t totalChars = strlen(charSet);
  int charsPerPage = 12;
  int totalPages = (totalChars + charsPerPage - 1) / charsPerPage;
  if (currentPage >= totalPages) currentPage = 0;

  int rowH = 26;
  int keyW = 38;

  int charStartIdx = currentPage * charsPerPage;
  for (int i = 0; i < charsPerPage; i++) {
    int idx = charStartIdx + i;
    if (idx >= totalChars) break;

    int r = i / 6;
    int c = i % 6;

    char cVal = charSet[idx];
    static char labels[24][2];
    labels[i][0] = cVal;
    labels[i][1] = '\0';

    activeKeys[activeKeyCount++] = {
      c * keyW + 2,
      kbdTop + (r * rowH) + 2,
      keyW - 2,
      rowH - 2,
      cVal,
      labels[i],
      false
    };
  }

  int y3 = kbdTop + (2 * rowH) + 2;

  if (totalPages > 1) {
    activeKeys[activeKeyCount++] = { 2, y3, 30, rowH - 2, '>', ">", true };
  } else {
    activeKeys[activeKeyCount++] = { 2, y3, 30, rowH - 2, 'M', "123", true };
  }

  if (currentMode == MODE_ALPHA_LOWER) {
    activeKeys[activeKeyCount++] = { 34, y3, 34, rowH - 2, '^', "ABC", true };
    activeKeys[activeKeyCount++] = { 70, y3, 34, rowH - 2, '#', "123", true };
  } else if (currentMode == MODE_ALPHA_UPPER) {
    activeKeys[activeKeyCount++] = { 34, y3, 34, rowH - 2, '^', "abc", true };
    activeKeys[activeKeyCount++] = { 70, y3, 34, rowH - 2, '#', "123", true };
  } else if (currentMode == MODE_NUMERIC) {
    activeKeys[activeKeyCount++] = { 34, y3, 34, rowH - 2, 'A', "abc", true };
    activeKeys[activeKeyCount++] = { 70, y3, 34, rowH - 2, '$', "SYM", true };
  } else {
    activeKeys[activeKeyCount++] = { 34, y3, 34, rowH - 2, 'A', "abc", true };
    activeKeys[activeKeyCount++] = { 70, y3, 34, rowH - 2, '#', "123", true };
  }

  activeKeys[activeKeyCount++] = { 106, y3, 44, rowH - 2, ' ', "SPC", true };
  activeKeys[activeKeyCount++] = { 152, y3, 40, rowH - 2, '<', "BS",  true };
  activeKeys[activeKeyCount++] = { 194, y3, 44, rowH - 2, '=', "SEND", true };
}

void drawKeyboard() {
  tft.fillRect(0, kbdTop, portraitWidth, portraitHeight - kbdTop, COLOR_BG);

  for (size_t i = 0; i < activeKeyCount; i++) {
    Key k = activeKeys[i];
    tft.drawRoundRect(k.x, k.y, k.w, k.h, 3, COLOR_TEXT);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);

    int txtX = k.x + (k.w / 2) - ((int)strlen(k.label) * charWidth / 2);
    int txtY = k.y + (k.h / 2) - (charHeight / 2);
    tft.drawString(k.label, txtX, txtY, 1);
  }
}

String sanitizeUtf8ToAscii(const String &input) {
  String clean = "";
  clean.reserve(input.length());

  for (size_t i = 0; i < input.length(); i++) {
    uint8_t c = (uint8_t)input[i];
    if (c == '`' || c == '\'') continue; 

    if (c < 128) {
      clean += (char)c;
    } else if (c == 0xE2 && (i + 2) < input.length()) {
      uint8_t c2 = (uint8_t)input[i + 1];
      uint8_t c3 = (uint8_t)input[i + 2];
      if (c2 == 0x80) {
        if (c3 == 0x98 || c3 == 0x99) clean += "";
        else if (c3 == 0x9C || c3 == 0x9D) clean += '"';
        else if (c3 == 0x93 || c3 == 0x94) clean += '-';
        else if (c3 == 0xA6) clean += "...";
        else clean += ' ';
        i += 2;
      } else {
        i += 2;
      }
    } else if ((c & 0xE0) == 0xC0 && (i + 1) < input.length()) {
      uint8_t c2 = (uint8_t)input[i + 1];
      if (c == 0xC3) {
        if (c2 >= 0x80 && c2 <= 0x85) clean += 'A';
        else if (c2 >= 0x88 && c2 <= 0x8B) clean += 'E';
        else if (c2 >= 0x8C && c2 <= 0x8F) clean += 'I';
        else if (c2 >= 0x92 && c2 <= 0x96) clean += 'O';
        else if (c2 >= 0x99 && c2 <= 0x9C) clean += 'U';
        else if (c2 >= 0xA0 && c2 <= 0xA5) clean += 'a';
        else if (c2 >= 0xA8 && c2 <= 0xAB) clean += 'e';
        else if (c2 >= 0xAC && c2 <= 0xAF) clean += 'i';
        else if (c2 >= 0xB2 && c2 <= 0xB6) clean += 'o';
        else if (c2 >= 0xB9 && c2 <= 0xBC) clean += 'u';
        else clean += '?';
      } else {
        clean += '?';
      }
      i += 1;
    }
  }
  return clean;
}

void handleTouch() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();

    int t_x = map(p.x, TOUCH_MIN_X, TOUCH_MAX_X, 0, portraitWidth);
    int t_y = map(p.y, TOUCH_MIN_Y, TOUCH_MAX_Y, 0, portraitHeight);

    t_x = constrain(t_x, 0, portraitWidth);
    t_y = constrain(t_y, 0, portraitHeight);

    // Toolbar Navigation Touch
    if (t_y < toolbarHeight) {
      if (t_x >= 4 && t_x <= 52) { // [^ Up] button bounds
        if (currentLineOffset > 0) {
          currentLineOffset--;
          renderToolbar();
          renderTextView();
        }
        delay(120);
        return;
      }
      if (t_x >= 58 && t_x <= 106) { // [v Dn] button bounds
        if (currentLineOffset < (totalLineCount - visibleLines)) {
          currentLineOffset++;
          renderToolbar();
          renderTextView();
        }
        delay(120);
        return;
      }
      return;
    }

    // Keyboard Touch
    if (t_y >= kbdTop) {
      for (size_t i = 0; i < activeKeyCount; i++) {
        Key k = activeKeys[i];
        if (t_x >= k.x && t_x <= (k.x + k.w) && t_y >= k.y && t_y <= (k.y + k.h)) {
          
          if (k.isAction) {
            if (k.val == '>') {
              currentPage++;
              buildKeyboardLayout();
              drawKeyboard();
            } else if (k.val == '^') {
              currentMode = (currentMode == MODE_ALPHA_LOWER) ? MODE_ALPHA_UPPER : MODE_ALPHA_LOWER;
              currentPage = 0;
              buildKeyboardLayout();
              drawKeyboard();
            } else if (k.val == '#') {
              currentMode = MODE_NUMERIC;
              currentPage = 0;
              buildKeyboardLayout();
              drawKeyboard();
            } else if (k.val == '$') {
              currentMode = MODE_SYMBOL;
              currentPage = 0;
              buildKeyboardLayout();
              drawKeyboard();
            } else if (k.val == 'A') {
              currentMode = MODE_ALPHA_LOWER;
              currentPage = 0;
              buildKeyboardLayout();
              drawKeyboard();
            } else if (k.val == '<') {
              if (userBuffer.length() > 0) {
                userBuffer.remove(userBuffer.length() - 1);
                renderInputLine();
              }
            } else if (k.val == '=') {
              if (userBuffer.length() > 0) {
                String promptToSend = userBuffer;
                userBuffer = "";
                renderInputLine();
                sendPromptToOllama(promptToSend);
              }
            } else if (k.val == ' ') {
              userBuffer += ' ';
              renderInputLine();
            }
          } else {
            userBuffer += k.val;
            renderInputLine();
          }
          delay(180);
          break;
        }
      }
    }
  }
}

void sendPromptToOllama(const String &userPrompt) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  // Append user input line to display
  appendTextToLines("\nUser> " + userPrompt);
  renderToolbar();
  renderTextView();

  HTTPClient http;
  String targetURL = "http://" + ollamaIP + ":" + String(ollamaPort) + "/api/generate";

  http.begin(targetURL);
  http.setTimeout(90000);
  http.addHeader("Content-Type", "application/json");

  JsonDocument requestDoc;
  requestDoc["model"] = modelName;
  requestDoc["prompt"] = userPrompt;
  
  requestDoc["system"] = R"(You are a terminal assistant on a small screen (240px x 320px) in portrait orientation, with green ASCII text on a black screen.
Absolute rule 1: Only use ASCII characters in your responses.
Absolute rule 2: DO NOT use markdown, backticks, code blocks, or triple quotes.)";

  JsonObject options = requestDoc["options"].to<JsonObject>();
  options["temperature"] = 0.7;

  if (contextDoc.containsKey("context")) {
    requestDoc["context"] = contextDoc["context"];
  }

  String requestBody;
  serializeJson(requestDoc, requestBody);

  int httpCode = http.POST(requestBody);

  if (httpCode == HTTP_CODE_OK) {
    WiFiClient* stream = http.getStreamPtr();
    JsonDocument responseDoc;

    String responseAccumulator = "";
    bool isDone = false;

    while (http.connected() && !isDone) {
      if (stream->available()) {
        String line = stream->readStringUntil('\n');
        line.trim();

        if (line.length() > 0) {
          responseDoc.clear();
          DeserializationError err = deserializeJson(responseDoc, line);

          if (!err) {
            const char* token = responseDoc["response"];
            if (token) {
              String cleanToken = sanitizeUtf8ToAscii(String(token));
              if (cleanToken.length() > 0) {
                responseAccumulator += cleanToken;
              }
            }
            
            if (responseDoc["done"].as<bool>()) {
              isDone = true;
              if (responseDoc.containsKey("context")) {
                contextDoc["context"] = responseDoc["context"];
              }
            }
          }
        }
      }
      vTaskDelay(1);
    }

    appendTextToLines("\n" + responseAccumulator + "\n");
    renderToolbar();
    renderTextView();

  } else {
    appendTextToLines("\nError: " + String(httpCode));
    renderToolbar();
    renderTextView();
  }

  http.end();
}
