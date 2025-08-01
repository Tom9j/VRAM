/*
 * VRAM System - Basic Usage Example
 * Demonstrates core functionality of the VRAM system
 * 
 * This example shows how to:
 * - Initialize the VRAM system
 * - Connect to WiFi and server
 * - Request and cache resources
 * - Monitor memory usage
 * - Handle automatic cleanup
 */

#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Include VRAM system headers
#include "../m5client/memory_manager.h"
#include "../m5client/resource_cache.h"
#include "../m5client/wifi_manager.h"

// Configuration - CHANGE THESE FOR YOUR SETUP
const char* WIFI_SSID = "your_wifi_ssid";
const char* WIFI_PASSWORD = "your_wifi_password";
const char* SERVER_URL = "http://192.168.1.100:5000";  // Change to your server IP

// Global objects
MemoryManager memoryManager;
ResourceCache resourceCache;
WiFiManager wifiManager;

// Demo state
int demoStep = 0;
unsigned long lastDemoUpdate = 0;
const unsigned long DEMO_INTERVAL = 5000;  // 5 seconds between demo steps

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
  
  Serial.println("=== VRAM System Basic Example ===");
  
  // Show welcome screen
  showWelcomeScreen();
  delay(3000);
  
  // Initialize VRAM components
  initializeVRAMSystem();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Test server connection
  testServerConnection();
  
  // Load some demo resources
  loadDemoResources();
  
  Serial.println("=== Setup Complete ===");
  showStatus("Ready!");
  delay(2000);
}

void loop() {
  M5.update();
  
  // Update WiFi manager
  wifiManager.update();
  
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
  
  // Run automated demo
  runAutomatedDemo();
  
  delay(100);
}

void showWelcomeScreen() {
  M5.Display.clear();
  M5.Display.setTextSize(2);
  M5.Display.drawString("VRAM Demo", M5.Display.width() / 2, 30);
  M5.Display.setTextSize(1);
  M5.Display.drawString("Basic Usage Example", M5.Display.width() / 2, 60);
  M5.Display.drawString("Starting...", M5.Display.width() / 2, 90);
}

void showStatus(const char* message) {
  M5.Display.clear();
  M5.Display.drawString("VRAM System", M5.Display.width() / 2, 20);
  M5.Display.setTextColor(YELLOW);
  M5.Display.drawString(message, M5.Display.width() / 2, 60);
  M5.Display.setTextColor(GREEN);
}

void showError(const char* message) {
  M5.Display.clear();
  M5.Display.setTextColor(RED);
  M5.Display.drawString("ERROR", M5.Display.width() / 2, 20);
  M5.Display.drawString(message, M5.Display.width() / 2, 60);
  M5.Display.setTextColor(GREEN);
}

void initializeVRAMSystem() {
  Serial.println("Initializing VRAM system...");
  showStatus("Init VRAM...");
  
  // Initialize memory manager
  memoryManager.begin();
  Serial.println("✓ Memory manager initialized");
  
  // Initialize resource cache
  resourceCache.begin();
  resourceCache.setMaxCacheSize(128 * 1024);  // 128KB cache for demo
  Serial.println("✓ Resource cache initialized");
  
  // Configure WiFi manager
  wifiManager.setCredentials(WIFI_SSID, WIFI_PASSWORD);
  wifiManager.setServerURL(SERVER_URL);
  wifiManager.setAutoReconnect(true);
  Serial.println("✓ WiFi manager configured");
  
  delay(1000);
}

void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  showStatus("Connecting WiFi...");
  
  if (wifiManager.connect()) {
    Serial.println("✓ WiFi connected successfully");
    showStatus("WiFi Connected!");
    
    // Print connection info
    wifiManager.printConnectionInfo();
  } else {
    Serial.println("✗ WiFi connection failed");
    showError("WiFi Failed!");
    delay(3000);
    ESP.restart();
  }
  
  delay(2000);
}

void testServerConnection() {
  Serial.println("Testing server connection...");
  showStatus("Testing Server...");
  
  if (wifiManager.testServerConnection()) {
    Serial.println("✓ Server connection successful");
    showStatus("Server OK!");
  } else {
    Serial.println("✗ Server connection failed");
    showError("Server Failed!");
    Serial.println("Make sure the Flask server is running!");
  }
  
  delay(2000);
}

void loadDemoResources() {
  Serial.println("Loading demo resources...");
  showStatus("Loading Resources...");
  
  // Load different types of resources
  loadResource("config_main", PRIORITY_CRITICAL);
  delay(500);
  
  loadResource("lib_sensor", PRIORITY_IMPORTANT);
  delay(500);
  
  loadResource("ui_strings", PRIORITY_IMPORTANT);
  delay(500);
  
  loadResource("data_sample", PRIORITY_NORMAL);
  delay(500);
  
  Serial.println("✓ Demo resources loaded");
  showStatus("Resources Loaded!");
  delay(2000);
}

bool loadResource(const String& resourceId, int priority) {
  Serial.printf("Loading resource: %s (priority: %d)\n", resourceId.c_str(), priority);
  
  HTTPClient http;
  String url = wifiManager.getServerURL() + "/api/resources/" + resourceId;
  
  http.begin(url);
  http.setTimeout(10000);
  
  int httpCode = http.GET();
  bool success = false;
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    // Parse JSON response
    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      String data = doc["data"];
      int size = doc["size"] | data.length();
      
      // Store in cache
      success = resourceCache.store(resourceId, data, priority, size);
      
      if (success) {
        Serial.printf("✓ Resource %s cached (%d bytes)\n", resourceId.c_str(), size);
      } else {
        Serial.printf("✗ Failed to cache resource %s\n", resourceId.c_str());
      }
    } else {
      Serial.printf("✗ JSON parse error for %s: %s\n", resourceId.c_str(), error.c_str());
    }
  } else {
    Serial.printf("✗ HTTP error for %s: %d\n", resourceId.c_str(), httpCode);
  }
  
  http.end();
  return success;
}

void handleButtonA() {
  Serial.println("Button A: Loading random resource");
  showStatus("Loading Resource...");
  
  // Try to load a random demo resource
  String resources[] = {"config_main", "lib_sensor", "ui_strings", "data_sample"};
  String randomResource = resources[random(0, 4)];
  
  if (loadResource(randomResource, PRIORITY_NORMAL)) {
    showStatus("Resource Loaded!");
  } else {
    showError("Load Failed!");
  }
  
  delay(2000);
}

void handleButtonB() {
  Serial.println("Button B: Showing memory status");
  
  MemoryInfo memInfo = memoryManager.getMemoryInfo();
  
  M5.Display.clear();
  M5.Display.setTextSize(1);
  M5.Display.drawString("Memory Status", M5.Display.width() / 2, 15);
  
  char buffer[64];
  
  // Memory usage
  sprintf(buffer, "Used: %d%%", memInfo.usagePercent);
  if (memInfo.usagePercent >= 90) {
    M5.Display.setTextColor(RED);
  } else if (memInfo.usagePercent >= 75) {
    M5.Display.setTextColor(YELLOW);
  } else {
    M5.Display.setTextColor(GREEN);
  }
  M5.Display.drawString(buffer, M5.Display.width() / 2, 35);
  
  // Free memory
  M5.Display.setTextColor(GREEN);
  sprintf(buffer, "Free: %d KB", memInfo.freeHeap / 1024);
  M5.Display.drawString(buffer, M5.Display.width() / 2, 50);
  
  // Cache info
  sprintf(buffer, "Cached: %d items", resourceCache.getResourceCount());
  M5.Display.drawString(buffer, M5.Display.width() / 2, 65);
  
  sprintf(buffer, "Cache: %.1f%%", resourceCache.getCacheUtilization() * 100);
  M5.Display.drawString(buffer, M5.Display.width() / 2, 80);
  
  // Hit rate
  sprintf(buffer, "Hit Rate: %.1f%%", resourceCache.getHitRate() * 100);
  M5.Display.drawString(buffer, M5.Display.width() / 2, 95);
  
  delay(4000);
}

void handlePowerButton() {
  Serial.println("Power button: Running memory cleanup demo");
  showStatus("Cleaning Memory...");
  
  // Print stats before cleanup
  Serial.println("=== Before Cleanup ===");
  memoryManager.printMemoryReport();
  resourceCache.printCacheStats();
  
  // Force memory cleanup
  size_t targetFreeBytes = 50 * 1024;  // Try to free 50KB
  int freedResources = resourceCache.freeMemory(targetFreeBytes);
  
  // Force garbage collection
  memoryManager.forceGarbageCollection();
  
  // Print stats after cleanup
  Serial.println("=== After Cleanup ===");
  memoryManager.printMemoryReport();
  resourceCache.printCacheStats();
  
  // Show result
  char buffer[64];
  sprintf(buffer, "Freed %d Resources", freedResources);
  showStatus(buffer);
  
  delay(3000);
}

void runAutomatedDemo() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastDemoUpdate < DEMO_INTERVAL) {
    return;
  }
  
  lastDemoUpdate = currentTime;
  
  switch (demoStep) {
    case 0:
      Serial.println("Demo: Testing resource retrieval");
      demonstrateResourceRetrieval();
      break;
      
    case 1:
      Serial.println("Demo: Testing cache hit/miss");
      demonstrateCacheHitMiss();
      break;
      
    case 2:
      Serial.println("Demo: Testing memory monitoring");
      demonstrateMemoryMonitoring();
      break;
      
    case 3:
      Serial.println("Demo: Testing automatic cleanup");
      demonstrateAutomaticCleanup();
      break;
      
    default:
      demoStep = -1;  // Reset to 0 on next increment
      break;
  }
  
  demoStep++;
}

void demonstrateResourceRetrieval() {
  // Try to get a cached resource
  String data = resourceCache.get("config_main");
  if (!data.isEmpty()) {
    Serial.printf("✓ Retrieved cached resource: %s\n", data.substring(0, 50).c_str());
  } else {
    Serial.println("✗ Resource not in cache, would need to fetch from server");
  }
}

void demonstrateCacheHitMiss() {
  // Access existing resource (cache hit)
  String hit = resourceCache.get("ui_strings");
  
  // Access non-existing resource (cache miss)
  String miss = resourceCache.get("nonexistent_resource");
  
  Serial.printf("Cache hit: %s, Cache miss: %s\n", 
                hit.isEmpty() ? "false" : "true",
                miss.isEmpty() ? "true" : "false");
}

void demonstrateMemoryMonitoring() {
  MemoryInfo memInfo = memoryManager.getMemoryInfo();
  
  Serial.printf("Memory usage: %d%% (%d/%d bytes)\n", 
                memInfo.usagePercent, memInfo.usedHeap, memInfo.totalHeap);
  Serial.printf("Cache utilization: %.1f%% (%d items)\n", 
                resourceCache.getCacheUtilization() * 100, 
                resourceCache.getResourceCount());
  
  if (memInfo.usagePercent >= 90) {
    Serial.println("⚠️ Memory usage critical!");
  } else if (memInfo.usagePercent >= 75) {
    Serial.println("⚠️ Memory usage high");
  }
}

void demonstrateAutomaticCleanup() {
  MemoryInfo memInfo = memoryManager.getMemoryInfo();
  
  // Simulate high memory usage condition
  if (memInfo.usagePercent < 70) {
    Serial.println("Memory usage normal, cleanup not needed");
    return;
  }
  
  Serial.println("Triggering automatic cleanup...");
  
  // Free some resources using LRU algorithm
  size_t targetBytes = (memInfo.totalHeap * 20) / 100;  // 20% of total
  int freed = resourceCache.freeMemory(targetBytes);
  
  Serial.printf("Automatic cleanup freed %d resources\n", freed);
}