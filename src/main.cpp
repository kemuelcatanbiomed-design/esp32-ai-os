#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include "mbedtls/base64.h"

#define OS_VERSION "1.3.0"

// ============================================================
// PUT YOUR REAL CREDENTIALS AND REPO NAME HERE:
// ============================================================
const char* DEFAULT_SSID         = "EFMS";
const char* DEFAULT_PASS         = "12345678";
const char* DEFAULT_GEMINI_KEY   = "AIzaSyBc1hWtp4gNhoonyhC-xViaN76O2BP00gI"; 
const char* GITHUB_TOKEN         = "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"; 
const char* GITHUB_REPO          = "kemuelcatanbiomed-design/esp32-ai-os"; 
// ============================================================

#define COLOR_BG        0x0000 
#define COLOR_CARD      0x18E3 
#define COLOR_ACCENT    0x07E0 
#define COLOR_CYAN      0x07FF 
#define COLOR_WHITE     0xFFFF 
#define COLOR_WARN      0xFBE0 
#define COLOR_ERR       0xF800 

TFT_eSPI tft = TFT_eSPI();
Preferences prefs;
String wifiSsid, wifiPass, geminiApiKey;

void renderHeader(String statusText, uint16_t statusColor) {
  tft.fillRect(0, 0, tft.width(), 26, COLOR_CARD);
  tft.drawLine(0, 26, tft.width(), 26, COLOR_ACCENT);
  tft.setTextFont(1);
  tft.setTextSize(1);
  tft.setTextColor(COLOR_WHITE, COLOR_CARD);
  tft.drawString("AI-OS v" + String(OS_VERSION), 6, 8);
  tft.setTextColor(statusColor, COLOR_CARD);
  tft.drawString(statusText, tft.width() - (statusText.length() * 6) - 8, 8);
}

void printToDisplay(String text, uint16_t textColor = COLOR_WHITE) {
  tft.fillRect(0, 27, tft.width(), tft.height() - 27, COLOR_BG);
  tft.setCursor(6, 36);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(textColor, COLOR_BG);
  tft.setTextWrap(true, true);
  tft.println(text);
}

String base64Encode(const String& input) {
  size_t outputLen = 0;
  mbedtls_base64_encode(nullptr, 0, &outputLen, (const unsigned char*)input.c_str(), input.length());
  unsigned char* buffer = (unsigned char*)malloc(outputLen + 1);
  mbedtls_base64_encode(buffer, outputLen + 1, &outputLen, (const unsigned char*)input.c_str(), input.length());
  buffer[outputLen] = '\0';
  String encoded = String((char*)buffer);
  free(buffer);
  return encoded;
}

String getGitHubFileSHA(const String& remotePath) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.github.com/repos/" + String(GITHUB_REPO) + "/contents/" + remotePath;
  String sha = "";

  if (http.begin(client, url)) {
    http.addHeader("Authorization", "Bearer " + String(GITHUB_TOKEN));
    http.addHeader("User-Agent", "ESP32-AI-OS");
    http.addHeader("Accept", "application/vnd.github+json");

    if (http.GET() == 200) {
      JsonDocument doc;
      deserializeJson(doc, http.getString());
      sha = doc["sha"].as<String>();
    }
    http.end();
  }
  return sha;
}

bool saveToCloud(const String& remotePath, const String& content, const String& commitMsg) {
  if (WiFi.status() != WL_CONNECTED) return false;
  renderHeader("Saving Cloud...", COLOR_WARN);

  String currentSha = getGitHubFileSHA(remotePath);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.github.com/repos/" + String(GITHUB_REPO) + "/contents/" + remotePath;

  if (http.begin(client, url)) {
    http.addHeader("Authorization", "Bearer " + String(GITHUB_TOKEN));
    http.addHeader("User-Agent", "ESP32-AI-OS");
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("Content-Type", "application/json");

    JsonDocument payload;
    payload["message"] = commitMsg;
    payload["content"] = base64Encode(content);
    if (currentSha.length() > 0) payload["sha"] = currentSha;

    String body;
    serializeJson(payload, body);
    int httpCode = http.PUT(body);
    http.end();

    renderHeader("Online", COLOR_ACCENT);
    return (httpCode == 200 || httpCode == 201);
  }
  renderHeader("Save Error", COLOR_ERR);
  return false;
}

String readFromCloud(const String& remotePath) {
  if (WiFi.status() != WL_CONNECTED) return "";
  renderHeader("Reading Cloud...", COLOR_WARN);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.github.com/repos/" + String(GITHUB_REPO) + "/contents/" + remotePath;
  String content = "";

  if (http.begin(client, url)) {
    http.addHeader("Authorization", "Bearer " + String(GITHUB_TOKEN));
    http.addHeader("User-Agent", "ESP32-AI-OS");
    http.addHeader("Accept", "application/vnd.github.raw+json");

    if (http.GET() == 200) content = http.getString();
    http.end();
  }
  renderHeader("Online", COLOR_ACCENT);
  return content;
}

void dispatchJulesEvolution(const String& title, const String& spec) {
  renderHeader("Calling Jules...", COLOR_WARN);
  printToDisplay("Opening task for Google Jules on GitHub...", COLOR_CYAN);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.github.com/repos/" + String(GITHUB_REPO) + "/issues";

  if (http.begin(client, url)) {
    http.addHeader("Authorization", "Bearer " + String(GITHUB_TOKEN));
    http.addHeader("User-Agent", "ESP32-AI-OS");
    http.addHeader("Accept", "application/vnd.github+json");
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["title"] = "[Jules Auto-Task] " + title;
    doc["body"]  = "### Autonomous Upgrade Spec\n" + spec +
                   "\n\n**Requirement:** Modify PlatformIO C++ firmware, maintain TFT_eSPI rendering, compile, and release binary.";
    doc["labels"].add("jules");

    String body;
    serializeJson(doc, body);
    int code = http.POST(body);
    if (code == 201) {
      Serial.println("[OS] Task logged to GitHub for Jules.");
      printToDisplay("Jules task created!\nJules is building the new C++ code on GitHub.", COLOR_ACCENT);
    } else {
      Serial.printf("[OS] Issue creation failed: %d\n", code);
      printToDisplay("Failed to assign Jules. Check GitHub PAT permissions.", COLOR_ERR);
    }
    http.end();
  }
  renderHeader("Online", COLOR_ACCENT);
}

void performOTA(const String& binUrl) {
  if (WiFi.status() != WL_CONNECTED) {
    printToDisplay("Cannot update: WiFi offline.", COLOR_ERR);
    return;
  }
  renderHeader("Flashing OTA...", COLOR_ERR);
  printToDisplay("Downloading latest binary from GitHub Releases...\nDo not power off board.", COLOR_WARN);

  WiFiClientSecure otaClient;
  otaClient.setInsecure();

  t_httpUpdate_return ret = httpUpdate.update(otaClient, binUrl);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      printToDisplay("Update failed:\n" + httpUpdate.getLastErrorString(), COLOR_ERR);
      renderHeader("Update Failed", COLOR_ERR);
      break;
    case HTTP_UPDATE_NO_UPDATES:
      printToDisplay("No update available on GitHub.", COLOR_WARN);
      renderHeader("Online", COLOR_ACCENT);
      break;
    case HTTP_UPDATE_OK:
      printToDisplay("Flashing completed!\nRebooting...", COLOR_ACCENT);
      break;
  }
}

void processWithGemini(const String& prompt) {
  if (WiFi.status() != WL_CONNECTED) {
    printToDisplay("Network offline. Cannot reach Gemini.", COLOR_ERR);
    return;
  }

  renderHeader("Thinking...", COLOR_CYAN);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  String url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + geminiApiKey;

  if (https.begin(client, url)) {
    https.addHeader("Content-Type", "application/json");

    JsonDocument req;
    JsonObject contentObj = req["contents"].add<JsonObject>();
    JsonObject partObj = contentObj["parts"].add<JsonObject>();

    String systemInstructions = 
      "You are a personal assistant engine running on an ESP32 connected to a TFT display. "
      "Decide on the JSON response format:\n"
      "1. Normal talk/solution: {\"action\":\"answer\",\"content\":\"<concise text fit for small screen>\"}\n"
      "2. Long-term memory store: {\"action\":\"save\",\"file\":\"<filename.json>\",\"data\":\"<data string>\",\"content\":\"<confirmation>\"}\n"
      "3. New command/firmware evolution: {\"action\":\"evolve\",\"title\":\"<title>\",\"spec\":\"<C++ logic>\",\"content\":\"<msg>\"}\n"
      "User command: " + prompt;

    partObj["text"] = systemInstructions;
    String reqBody;
    serializeJson(req, reqBody);

    int httpCode = https.POST(reqBody);
    if (httpCode == 200) {
      JsonDocument res;
      deserializeJson(res, https.getString());
      const char* rawJson = res["candidates"][0]["content"]["parts"][0]["text"];

      JsonDocument decision;
      DeserializationError err = deserializeJson(decision, rawJson);

      if (!err) {
        String action = decision["action"].as<String>();
        String content = decision["content"].as<String>();

        if (action == "answer") {
          Serial.println("\n[Gemini]: " + content);
          printToDisplay(content, COLOR_WHITE);
        } else if (action == "save") {
          printToDisplay(content, COLOR_CYAN);
          saveToCloud("data/" + decision["file"].as<String>(), decision["data"].as<String>(), "Saved by Gemini");
        } else if (action == "evolve") {
          printToDisplay(content, COLOR_WARN);
          dispatchJulesEvolution(decision["title"].as<String>(), decision["spec"].as<String>());
        }
      } else {
        Serial.println(rawJson);
        printToDisplay(String(rawJson), COLOR_WHITE);
      }
    } else {
      printToDisplay("HTTP Error: " + String(httpCode), COLOR_ERR);
    }
    https.end();
  }
  renderHeader("Online", COLOR_ACCENT);
}

void executeCommand(String cmd) {
  if (cmd == "help") {
    Serial.println("Commands:");
    Serial.println("  sysinfo                  - View board stats on TFT & Serial");
    Serial.println("  update                   - Fetch compiled binary from GitHub Releases");
    Serial.println("  cloud read <path>        - Display cloud document");
    Serial.println("  cloud write <path> <val> - Write directly to GitHub repo");
    Serial.println("  reboot                   - Soft restart ESP32");
    Serial.println("  <any natural text>       - Query Gemini or ask for new OS features");
  } 
  else if (cmd == "sysinfo") {
    String stats = "OS Ver: " + String(OS_VERSION) + "\n" +
                   "Free RAM: " + String(ESP.getFreeHeap() / 1024) + " KB\n" +
                   "Uptime: " + String(millis() / 1000) + " s\n" +
                   "IP: " + WiFi.localIP().toString() + "\n" +
                   "Signal: " + String(WiFi.RSSI()) + " dBm";
    Serial.println(stats);
    printToDisplay(stats, COLOR_CYAN);
  } 
  else if (cmd == "update") {
    String otaUrl = "https://github.com/" + String(GITHUB_REPO) + "/releases/latest/download/firmware.bin";
    performOTA(otaUrl);
  } 
  else if (cmd.startsWith("cloud read ")) {
    String path = cmd.substring(11);
    path.trim();
    String content = readFromCloud(path);
    Serial.println(content);
    printToDisplay(content, COLOR_WHITE);
  } 
  else if (cmd.startsWith("cloud write ")) {
    int split = cmd.indexOf(' ', 12);
    if (split != -1) {
      String path = cmd.substring(12, split);
      String val = cmd.substring(split + 1);
      saveToCloud(path, val, "Manual write from shell");
      printToDisplay("Committed to GitHub:\n" + path, COLOR_ACCENT);
    }
  } 
  else if (cmd == "reboot") {
    printToDisplay("Restarting...", COLOR_WARN);
    delay(500);
    ESP.restart();
  } 
  else {
    processWithGemini(cmd);
  }
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BG);
  renderHeader("Booting...", COLOR_WARN);
  printToDisplay("Starting ESP32 AI-OS...\nConnecting to Wi-Fi...", COLOR_WHITE);

  prefs.begin("ai_os", false);
  wifiSsid = prefs.getString("ssid", DEFAULT_SSID);
  wifiPass = prefs.getString("pass", DEFAULT_PASS);
  geminiApiKey = prefs.getString("gemini_key", DEFAULT_GEMINI_KEY);

  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    renderHeader("Online", COLOR_ACCENT);
    printToDisplay("AI-OS Ready.\nIP: " + WiFi.localIP().toString() + "\nSerial terminal listening.", COLOR_CYAN);
  } else {
    renderHeader("Offline", COLOR_ERR);
    printToDisplay("Wi-Fi failed.\nUpdate credentials in code or NVS.", COLOR_ERR);
  }

  Serial.print("\nai-os> ");
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      executeCommand(line);
      Serial.print("ai-os> ");
    }
  }
}
