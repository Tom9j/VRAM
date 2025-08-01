/*
 * VRAM System - M5StickC Plus2 Client
 * Dynamic memory management with server communication
 * 
 * This sketch implements a Virtual RAM system that dynamically loads
 * resources from a server based on memory availability and usage patterns.
 */

#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "memory_manager.h"
#include "resource_cache.h"
#include "wifi_manager.h"

// Configuration
#define MEMORY_THRESHOLD_PERCENT 90
#define CLEANUP_PERCENTAGE 30
#define SERVER_CHECK_INTERVAL 30000  // 30 seconds
#define MEMORY_CHECK_INTERVAL 5000   // 5 seconds

// Global objects
MemoryManager memoryManager;
ResourceCache resourceCache;
WiFiManager wifiManager;

// System state
struct SystemState {
  bool serverConnected = false;
  unsigned long lastServerCheck = 0;
  unsigned long lastMemoryCheck = 0;
  int totalRequests = 0;
  int failedRequests = 0;
  float avgResponseTime = 0.0;
} systemState;

void setup() {
  // Initialize M5StickC Plus2
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setTextColor(GREEN);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::Orbitron_Light_24);
  M5.Display.setTextSize(1);
  
  Serial.begin(115200);
  delay(1000);
  
  // Initialize display
  displayBootScreen();
  
  // Initialize memory manager
  memoryManager.begin();
  
  // Initialize resource cache
  resourceCache.begin();
  
  // Initialize WiFi
  displayStatus("Connecting WiFi...");
  if (!wifiManager.connect()) {
    displayError("WiFi Failed!");
    ESP.restart();
  }
  
  displayStatus("WiFi Connected");
  delay(1000);
  
  // Test server connection
  displayStatus("Testing Server...");
  testServerConnection();
  
  // Load initial resources
  loadInitialResources();
  
  displayStatus("System Ready!");
  delay(2000);
}

void loop() {
  M5.update();
  
  unsigned long currentTime = millis();
  
  // Check memory usage periodically
  if (currentTime - systemState.lastMemoryCheck > MEMORY_CHECK_INTERVAL) {
    checkMemoryUsage();
    systemState.lastMemoryCheck = currentTime;
  }
  
  // Check server connection periodically
  if (currentTime - systemState.lastServerCheck > SERVER_CHECK_INTERVAL) {
    checkServerConnection();
    systemState.lastServerCheck = currentTime;
  }
  
  // Handle button presses
  if (M5.BtnA.wasPressed()) {
    handleButtonA();
  }
  
  if (M5.BtnB.wasPressed()) {
    handleButtonB();
  }
  
  if (M5.BtnPWR.wasPressed()) {
    handlePowerButton();
  }
  
  // Update display
  updateDisplay();
  
  delay(100);
}

void displayBootScreen() {
  M5.Display.clear();
  M5.Display.setTextSize(2);
  M5.Display.drawString("VRAM System", M5.Display.width() / 2, 30);
  M5.Display.setTextSize(1);
  M5.Display.drawString("M5StickC Plus2", M5.Display.width() / 2, 60);
  M5.Display.drawString("Initializing...", M5.Display.width() / 2, 90);
}

void displayStatus(const char* message) {
  M5.Display.clear();
  M5.Display.drawString("VRAM System", M5.Display.width() / 2, 20);
  M5.Display.setTextColor(YELLOW);
  M5.Display.drawString(message, M5.Display.width() / 2, 60);
  M5.Display.setTextColor(GREEN);
}

void displayError(const char* message) {
  M5.Display.clear();
  M5.Display.setTextColor(RED);
  M5.Display.drawString("ERROR", M5.Display.width() / 2, 20);
  M5.Display.drawString(message, M5.Display.width() / 2, 60);
  M5.Display.setTextColor(GREEN);
}

void testServerConnection() {
  HTTPClient http;
  http.begin(wifiManager.getServerURL() + "/api/health");
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    systemState.serverConnected = true;
    Serial.println("Server connection successful");
  } else {
    systemState.serverConnected = false;
    Serial.printf("Server connection failed: %d\n", httpCode);
  }
  
  http.end();
}

void loadInitialResources() {
  displayStatus("Loading Resources...");
  
  // Load critical configuration first
  requestResource("config_main", PRIORITY_CRITICAL);
  delay(500);
  
  // Load important libraries
  requestResource("lib_sensor", PRIORITY_IMPORTANT);
  delay(500);
  
  // Load UI resources
  requestResource("ui_strings", PRIORITY_IMPORTANT);
  delay(500);
  
  Serial.println("Initial resources loaded");
}

bool requestResource(const String& resourceId, int priority) {
  if (!systemState.serverConnected) {
    Serial.println("Server not connected");
    return false;
  }
  
  unsigned long startTime = millis();
  systemState.totalRequests++;
  
  HTTPClient http;
  String url = wifiManager.getServerURL() + "/api/resources/" + resourceId;
  
  // Add compression parameter for large resources
  if (priority <= PRIORITY_NORMAL) {
    url += "?compress=true";
  }
  
  http.begin(url);
  http.setTimeout(10000);
  
  int httpCode = http.GET();
  bool success = false;
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Parse JSON response
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      String data = doc["data"];
      bool compressed = doc["compressed"] | false;
      int size = doc["size"] | 0;
      
      // Decompress if necessary
      if (compressed) {
        // Handle compressed data (hex decode + gzip decompress)
        data = decompressHexData(data);
      }
      
      // Store in cache
      success = resourceCache.store(resourceId, data, priority, size);
      
      if (success) {
        Serial.printf("Resource %s loaded (%d bytes)\n", resourceId.c_str(), size);
      }
    } else {
      Serial.printf("JSON parse error: %s\n", error.c_str());
    }
  } else {
    Serial.printf("HTTP error for resource %s: %d\n", resourceId.c_str(), httpCode);
    systemState.failedRequests++;
  }
  
  // Update response time statistics
  unsigned long responseTime = millis() - startTime;
  systemState.avgResponseTime = (systemState.avgResponseTime * (systemState.totalRequests - 1) + responseTime) / systemState.totalRequests;
  
  http.end();
  return success;
}

String decompressHexData(const String& hexData) {
  // Convert hex string to bytes
  int len = hexData.length() / 2;
  uint8_t* bytes = (uint8_t*)malloc(len);
  
  for (int i = 0; i < len; i++) {
    String byteString = hexData.substring(i * 2, i * 2 + 2);
    bytes[i] = (uint8_t)strtol(byteString.c_str(), NULL, 16);
  }
  
  // For simplicity, we'll just return the hex decoded data
  // In a full implementation, you'd use a gzip library here
  String result = String((char*)bytes);
  free(bytes);
  
  return result;
}

void checkMemoryUsage() {
  MemoryInfo memInfo = memoryManager.getMemoryInfo();
  
  // Check if memory usage is too high
  if (memInfo.usagePercent >= MEMORY_THRESHOLD_PERCENT) {
    Serial.println("Memory threshold exceeded, triggering cleanup");
    displayStatus("Cleaning Memory...");
    
    // Calculate how much to free (30% of total memory)
    size_t targetFreeBytes = (memInfo.totalHeap * CLEANUP_PERCENTAGE) / 100;
    
    // Use LRU algorithm to free memory
    int freedResources = resourceCache.freeMemory(targetFreeBytes);
    
    Serial.printf("Freed %d resources\n", freedResources);
    
    // Force garbage collection
    memoryManager.forceGarbageCollection();
    
    // Update memory info
    memInfo = memoryManager.getMemoryInfo();
    Serial.printf("Memory after cleanup: %d%% used\n", memInfo.usagePercent);
  }
}

void checkServerConnection() {
  HTTPClient http;
  http.begin(wifiManager.getServerURL() + "/api/health");
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  bool wasConnected = systemState.serverConnected;
  systemState.serverConnected = (httpCode == HTTP_CODE_OK);
  
  if (!wasConnected && systemState.serverConnected) {
    Serial.println("Server connection restored");
  } else if (wasConnected && !systemState.serverConnected) {
    Serial.println("Server connection lost");
  }
  
  http.end();
}

void handleButtonA() {
  // Button A: Request a demo resource
  Serial.println("Button A: Requesting demo resource");
  displayStatus("Loading Resource...");
  
  if (requestResource("data_sample", PRIORITY_NORMAL)) {
    displayStatus("Resource Loaded!");
  } else {
    displayError("Load Failed!");
  }
  
  delay(2000);
}

void handleButtonB() {
  // Button B: Show memory status
  Serial.println("Button B: Showing memory status");
  MemoryInfo memInfo = memoryManager.getMemoryInfo();
  
  M5.Display.clear();
  M5.Display.setTextSize(1);
  M5.Display.drawString("Memory Status", M5.Display.width() / 2, 20);
  
  char buffer[64];
  sprintf(buffer, "Used: %d%%", memInfo.usagePercent);
  M5.Display.drawString(buffer, M5.Display.width() / 2, 40);
  
  sprintf(buffer, "Free: %d KB", memInfo.freeHeap / 1024);
  M5.Display.drawString(buffer, M5.Display.width() / 2, 60);
  
  sprintf(buffer, "Cached: %d", resourceCache.getResourceCount());
  M5.Display.drawString(buffer, M5.Display.width() / 2, 80);
  
  delay(3000);
}

void handlePowerButton() {
  // Power button: Show system statistics
  Serial.println("Power button: Showing system stats");
  
  M5.Display.clear();
  M5.Display.setTextSize(1);
  M5.Display.drawString("System Stats", M5.Display.width() / 2, 15);
  
  char buffer[64];
  sprintf(buffer, "Requests: %d", systemState.totalRequests);
  M5.Display.drawString(buffer, M5.Display.width() / 2, 35);
  
  sprintf(buffer, "Failed: %d", systemState.failedRequests);
  M5.Display.drawString(buffer, M5.Display.width() / 2, 50);
  
  sprintf(buffer, "Avg RT: %.1fms", systemState.avgResponseTime);
  M5.Display.drawString(buffer, M5.Display.width() / 2, 65);
  
  sprintf(buffer, "Server: %s", systemState.serverConnected ? "OK" : "FAIL");
  M5.Display.setTextColor(systemState.serverConnected ? GREEN : RED);
  M5.Display.drawString(buffer, M5.Display.width() / 2, 80);
  M5.Display.setTextColor(GREEN);
  
  delay(3000);
}

void updateDisplay() {
  static unsigned long lastUpdate = 0;
  
  if (millis() - lastUpdate < 1000) return;  // Update every second
  lastUpdate = millis();
  
  M5.Display.clear();
  
  // Title
  M5.Display.setTextSize(2);
  M5.Display.drawString("VRAM", M5.Display.width() / 2, 20);
  
  // Status indicators
  M5.Display.setTextSize(1);
  
  // Memory usage
  MemoryInfo memInfo = memoryManager.getMemoryInfo();
  char buffer[64];
  sprintf(buffer, "Mem: %d%%", memInfo.usagePercent);
  
  if (memInfo.usagePercent >= MEMORY_THRESHOLD_PERCENT) {
    M5.Display.setTextColor(RED);
  } else if (memInfo.usagePercent >= 70) {
    M5.Display.setTextColor(YELLOW);
  } else {
    M5.Display.setTextColor(GREEN);
  }
  M5.Display.drawString(buffer, M5.Display.width() / 2, 50);
  
  // Server connection status
  M5.Display.setTextColor(systemState.serverConnected ? GREEN : RED);
  M5.Display.drawString(systemState.serverConnected ? "Server: OK" : "Server: FAIL", M5.Display.width() / 2, 70);
  
  // Resource count
  M5.Display.setTextColor(GREEN);
  sprintf(buffer, "Resources: %d", resourceCache.getResourceCount());
  M5.Display.drawString(buffer, M5.Display.width() / 2, 90);
  
  // Button hints
  M5.Display.setTextSize(0.5);
  M5.Display.drawString("A:Load B:Mem PWR:Stats", M5.Display.width() / 2, 110);
}