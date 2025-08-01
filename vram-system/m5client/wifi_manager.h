/*
 * WiFi Manager for VRAM System
 * Handles WiFi connection, reconnection, and server communication
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Default configuration
#define DEFAULT_WIFI_SSID "VRAM_Network"
#define DEFAULT_WIFI_PASSWORD "vram123456"
#define DEFAULT_SERVER_URL "http://192.168.1.100:5000"
#define WIFI_CONNECT_TIMEOUT 15000
#define WIFI_RECONNECT_INTERVAL 30000
#define CONNECTION_CHECK_INTERVAL 60000

// WiFi status
enum WiFiStatus {
  WIFI_DISCONNECTED,
  WIFI_CONNECTING,
  WIFI_CONNECTED,
  WIFI_FAILED,
  WIFI_RECONNECTING
};

// Connection statistics
struct ConnectionStats {
  unsigned long totalConnections;
  unsigned long failedConnections;
  unsigned long reconnections;
  unsigned long lastConnectTime;
  unsigned long totalUptime;
  int signalStrength;
  String lastError;
};

class WiFiManager {
private:
  String ssid;
  String password;
  String serverURL;
  WiFiStatus status;
  ConnectionStats stats;
  unsigned long lastReconnectAttempt;
  unsigned long lastConnectionCheck;
  bool autoReconnect;
  int maxReconnectAttempts;
  int reconnectAttempts;
  
  void updateConnectionStats();
  void handleConnectionFailure(const String& error);
  bool performConnection();
  
public:
  WiFiManager();
  
  // Configuration
  void setCredentials(const String& ssid, const String& password);
  void setServerURL(const String& url);
  void setAutoReconnect(bool enable);
  void setMaxReconnectAttempts(int attempts);
  
  // Connection management
  bool connect();
  bool connect(const String& ssid, const String& password);
  void disconnect();
  bool isConnected();
  bool reconnect();
  
  // Status and monitoring
  WiFiStatus getStatus() { return status; }
  String getStatusString();
  int getSignalStrength();
  String getIPAddress();
  String getMACAddress();
  String getServerURL() { return serverURL; }
  
  // Statistics
  ConnectionStats getStats() { return stats; }
  void printConnectionInfo();
  void resetStats();
  
  // Maintenance
  void update();  // Call in main loop
  bool checkConnection();
  void handleReconnection();
  
  // Network utilities
  bool ping(const String& host, int timeout = 5000);
  bool testServerConnection();
  String scanNetworks();
};

// Implementation
WiFiManager::WiFiManager() {
  ssid = DEFAULT_WIFI_SSID;
  password = DEFAULT_WIFI_PASSWORD;
  serverURL = DEFAULT_SERVER_URL;
  status = WIFI_DISCONNECTED;
  lastReconnectAttempt = 0;
  lastConnectionCheck = 0;
  autoReconnect = true;
  maxReconnectAttempts = 5;
  reconnectAttempts = 0;
  
  // Initialize stats
  memset(&stats, 0, sizeof(stats));
}

void WiFiManager::setCredentials(const String& newSSID, const String& newPassword) {
  ssid = newSSID;
  password = newPassword;
  Serial.printf("WiFi credentials set: %s\n", ssid.c_str());
}

void WiFiManager::setServerURL(const String& url) {
  serverURL = url;
  Serial.printf("Server URL set: %s\n", serverURL.c_str());
}

void WiFiManager::setAutoReconnect(bool enable) {
  autoReconnect = enable;
  Serial.printf("Auto-reconnect: %s\n", enable ? "enabled" : "disabled");
}

void WiFiManager::setMaxReconnectAttempts(int attempts) {
  maxReconnectAttempts = attempts;
}

bool WiFiManager::connect() {
  return connect(ssid, password);
}

bool WiFiManager::connect(const String& connectSSID, const String& connectPassword) {
  Serial.printf("Connecting to WiFi: %s\n", connectSSID.c_str());
  
  status = WIFI_CONNECTING;
  stats.totalConnections++;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(connectSSID.c_str(), connectPassword.c_str());
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
    delay(100);
    Serial.print(".");
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    status = WIFI_CONNECTED;
    updateConnectionStats();
    reconnectAttempts = 0;
    
    Serial.printf("WiFi connected successfully!\n");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
    
    return true;
  } else {
    status = WIFI_FAILED;
    stats.failedConnections++;
    handleConnectionFailure("Connection timeout");
    return false;
  }
}

void WiFiManager::disconnect() {
  Serial.println("Disconnecting WiFi...");
  WiFi.disconnect();
  status = WIFI_DISCONNECTED;
}

bool WiFiManager::isConnected() {
  return WiFi.status() == WL_CONNECTED && status == WIFI_CONNECTED;
}

bool WiFiManager::reconnect() {
  if (reconnectAttempts >= maxReconnectAttempts) {
    Serial.printf("Max reconnect attempts (%d) reached\n", maxReconnectAttempts);
    return false;
  }
  
  Serial.printf("Reconnection attempt %d/%d\n", reconnectAttempts + 1, maxReconnectAttempts);
  status = WIFI_RECONNECTING;
  stats.reconnections++;
  reconnectAttempts++;
  
  WiFi.disconnect();
  delay(1000);
  
  bool success = connect();
  if (success) {
    reconnectAttempts = 0;
  }
  
  return success;
}

String WiFiManager::getStatusString() {
  switch (status) {
    case WIFI_DISCONNECTED: return "Disconnected";
    case WIFI_CONNECTING: return "Connecting";
    case WIFI_CONNECTED: return "Connected";
    case WIFI_FAILED: return "Failed";
    case WIFI_RECONNECTING: return "Reconnecting";
    default: return "Unknown";
  }
}

int WiFiManager::getSignalStrength() {
  if (isConnected()) {
    return WiFi.RSSI();
  }
  return -999;
}

String WiFiManager::getIPAddress() {
  if (isConnected()) {
    return WiFi.localIP().toString();
  }
  return "0.0.0.0";
}

String WiFiManager::getMACAddress() {
  return WiFi.macAddress();
}

void WiFiManager::updateConnectionStats() {
  stats.lastConnectTime = millis();
  stats.signalStrength = WiFi.RSSI();
  
  if (status == WIFI_CONNECTED) {
    stats.totalUptime += millis() - stats.lastConnectTime;
  }
}

void WiFiManager::handleConnectionFailure(const String& error) {
  stats.lastError = error;
  Serial.printf("WiFi connection failed: %s\n", error.c_str());
}

void WiFiManager::printConnectionInfo() {
  Serial.println("\n=== WiFi Connection Info ===");
  Serial.printf("Status: %s\n", getStatusString().c_str());
  Serial.printf("SSID: %s\n", ssid.c_str());
  Serial.printf("IP Address: %s\n", getIPAddress().c_str());
  Serial.printf("MAC Address: %s\n", getMACAddress().c_str());
  Serial.printf("Signal Strength: %d dBm\n", getSignalStrength());
  Serial.printf("Server URL: %s\n", serverURL.c_str());
  
  Serial.println("\n=== Connection Statistics ===");
  Serial.printf("Total Connections: %lu\n", stats.totalConnections);
  Serial.printf("Failed Connections: %lu\n", stats.failedConnections);
  Serial.printf("Reconnections: %lu\n", stats.reconnections);
  Serial.printf("Total Uptime: %lu ms\n", stats.totalUptime);
  if (!stats.lastError.isEmpty()) {
    Serial.printf("Last Error: %s\n", stats.lastError.c_str());
  }
  Serial.println("============================\n");
}

void WiFiManager::resetStats() {
  memset(&stats, 0, sizeof(stats));
  stats.lastConnectTime = millis();
  Serial.println("WiFi statistics reset");
}

void WiFiManager::update() {
  unsigned long currentTime = millis();
  
  // Check connection status periodically
  if (currentTime - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
    checkConnection();
    lastConnectionCheck = currentTime;
  }
  
  // Handle auto-reconnection
  if (autoReconnect && !isConnected() && 
      currentTime - lastReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
    handleReconnection();
    lastReconnectAttempt = currentTime;
  }
}

bool WiFiManager::checkConnection() {
  bool wasConnected = isConnected();
  bool isStillConnected = (WiFi.status() == WL_CONNECTED);
  
  if (wasConnected && !isStillConnected) {
    Serial.println("WiFi connection lost!");
    status = WIFI_DISCONNECTED;
    handleConnectionFailure("Connection lost");
    return false;
  } else if (!wasConnected && isStillConnected) {
    Serial.println("WiFi connection restored!");
    status = WIFI_CONNECTED;
    updateConnectionStats();
    return true;
  }
  
  return isStillConnected;
}

void WiFiManager::handleReconnection() {
  if (status == WIFI_DISCONNECTED || status == WIFI_FAILED) {
    Serial.println("Attempting auto-reconnection...");
    reconnect();
  }
}

bool WiFiManager::ping(const String& host, int timeout) {
  if (!isConnected()) {
    return false;
  }
  
  HTTPClient http;
  http.begin(host);
  http.setTimeout(timeout);
  
  int httpCode = http.GET();
  http.end();
  
  return httpCode > 0;
}

bool WiFiManager::testServerConnection() {
  Serial.printf("Testing server connection: %s\n", serverURL.c_str());
  
  HTTPClient http;
  String testURL = serverURL + "/api/health";
  http.begin(testURL);
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  bool success = (httpCode == HTTP_CODE_OK);
  
  if (success) {
    String response = http.getString();
    Serial.printf("Server test successful: %s\n", response.c_str());
  } else {
    Serial.printf("Server test failed with code: %d\n", httpCode);
  }
  
  http.end();
  return success;
}

String WiFiManager::scanNetworks() {
  if (!isConnected()) {
    return "WiFi not connected";
  }
  
  Serial.println("Scanning for networks...");
  int networks = WiFi.scanNetworks();
  
  String result = "Found " + String(networks) + " networks:\n";
  
  for (int i = 0; i < networks; i++) {
    result += String(i + 1) + ". " + WiFi.SSID(i) + 
              " (" + String(WiFi.RSSI(i)) + " dBm)";
    
    if (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) {
      result += " [Encrypted]";
    }
    result += "\n";
  }
  
  return result;
}

#endif // WIFI_MANAGER_H