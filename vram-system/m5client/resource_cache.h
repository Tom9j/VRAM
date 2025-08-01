/*
 * Resource Cache for VRAM System
 * Implements intelligent caching with LRU algorithm and priority management
 */

#ifndef RESOURCE_CACHE_H
#define RESOURCE_CACHE_H

#include <Arduino.h>
#include <map>
#include <vector>

// Priority levels
#define PRIORITY_CRITICAL   1
#define PRIORITY_IMPORTANT  2
#define PRIORITY_NORMAL     3
#define PRIORITY_LOW        4

// Cache configuration
#define MAX_CACHE_SIZE      (256 * 1024)  // 256KB cache limit
#define MAX_RESOURCE_SIZE   (64 * 1024)   // 64KB per resource limit
#define CACHE_ENTRY_OVERHEAD 64           // Estimated overhead per entry

// Cache entry structure
struct CacheEntry {
  String resourceId;
  String data;
  int priority;
  size_t size;
  unsigned long accessTime;
  unsigned long createTime;
  int accessCount;
  CacheEntry* prev;
  CacheEntry* next;
};

class ResourceCache {
private:
  // LRU linked list
  CacheEntry* head;
  CacheEntry* tail;
  
  // Hash map for O(1) access
  std::map<String, CacheEntry*> cacheMap;
  
  // Cache statistics
  size_t totalCacheSize;
  size_t maxCacheSize;
  int totalEntries;
  int cacheHits;
  int cacheMisses;
  int evictions;
  
  // Internal methods
  void moveToHead(CacheEntry* entry);
  void removeEntry(CacheEntry* entry);
  void addToHead(CacheEntry* entry);
  CacheEntry* removeTail();
  bool shouldEvict(CacheEntry* entry, int newPriority);
  size_t calculateEntrySize(const String& data);
  
public:
  ResourceCache();
  ~ResourceCache();
  
  // Initialization
  void begin();
  void setMaxCacheSize(size_t maxSize);
  
  // Cache operations
  bool store(const String& resourceId, const String& data, int priority, size_t dataSize = 0);
  String get(const String& resourceId);
  bool contains(const String& resourceId);
  bool remove(const String& resourceId);
  void clear();
  
  // Memory management
  int freeMemory(size_t targetBytes);
  void optimizeCache();
  bool makeSpaceFor(size_t requiredSize, int priority);
  
  // Cache information
  int getResourceCount() { return totalEntries; }
  size_t getCacheSize() { return totalCacheSize; }
  size_t getMaxCacheSize() { return maxCacheSize; }
  float getCacheUtilization() { return (float)totalCacheSize / maxCacheSize; }
  
  // Statistics
  void printCacheStats();
  void resetStats();
  int getCacheHits() { return cacheHits; }
  int getCacheMisses() { return cacheMisses; }
  float getHitRate() { return (float)cacheHits / (cacheHits + cacheMisses); }
  
  // Cache maintenance
  void cleanupExpired(unsigned long maxAge = 3600000);  // 1 hour default
  std::vector<String> getResourcesByPriority(int priority);
  void updatePriority(const String& resourceId, int newPriority);
};

// Implementation
ResourceCache::ResourceCache() {
  head = nullptr;
  tail = nullptr;
  totalCacheSize = 0;
  maxCacheSize = MAX_CACHE_SIZE;
  totalEntries = 0;
  cacheHits = 0;
  cacheMisses = 0;
  evictions = 0;
}

ResourceCache::~ResourceCache() {
  clear();
}

void ResourceCache::begin() {
  Serial.println("ResourceCache: Initializing...");
  clear();
  Serial.printf("Cache initialized with max size: %d bytes\n", maxCacheSize);
}

void ResourceCache::setMaxCacheSize(size_t maxSize) {
  maxCacheSize = maxSize;
  
  // If current cache exceeds new limit, trigger cleanup
  if (totalCacheSize > maxCacheSize) {
    optimizeCache();
  }
}

bool ResourceCache::store(const String& resourceId, const String& data, int priority, size_t dataSize) {
  // Calculate entry size
  size_t entrySize = dataSize > 0 ? dataSize : calculateEntrySize(data);
  
  // Check if resource is too large
  if (entrySize > MAX_RESOURCE_SIZE) {
    Serial.printf("Resource %s too large (%d bytes), max allowed: %d\n", 
                  resourceId.c_str(), entrySize, MAX_RESOURCE_SIZE);
    return false;
  }
  
  // Check if resource already exists
  if (contains(resourceId)) {
    // Update existing entry
    CacheEntry* entry = cacheMap[resourceId];
    totalCacheSize -= entry->size;
    
    entry->data = data;
    entry->size = entrySize;
    entry->priority = priority;
    entry->accessTime = millis();
    entry->accessCount++;
    
    totalCacheSize += entrySize;
    moveToHead(entry);
    
    Serial.printf("Updated cached resource: %s (%d bytes)\n", 
                  resourceId.c_str(), entrySize);
    return true;
  }
  
  // Make space if necessary
  if (!makeSpaceFor(entrySize + CACHE_ENTRY_OVERHEAD, priority)) {
    Serial.printf("Cannot make space for resource %s (%d bytes)\n", 
                  resourceId.c_str(), entrySize);
    return false;
  }
  
  // Create new cache entry
  CacheEntry* entry = new CacheEntry();
  entry->resourceId = resourceId;
  entry->data = data;
  entry->priority = priority;
  entry->size = entrySize;
  entry->accessTime = millis();
  entry->createTime = millis();
  entry->accessCount = 1;
  entry->prev = nullptr;
  entry->next = nullptr;
  
  // Add to cache
  addToHead(entry);
  cacheMap[resourceId] = entry;
  totalCacheSize += entrySize + CACHE_ENTRY_OVERHEAD;
  totalEntries++;
  
  Serial.printf("Cached new resource: %s (%d bytes, priority: %d)\n", 
                resourceId.c_str(), entrySize, priority);
  
  return true;
}

String ResourceCache::get(const String& resourceId) {
  auto it = cacheMap.find(resourceId);
  if (it != cacheMap.end()) {
    CacheEntry* entry = it->second;
    
    // Update access information
    entry->accessTime = millis();
    entry->accessCount++;
    
    // Move to head (most recently used)
    moveToHead(entry);
    
    cacheHits++;
    return entry->data;
  }
  
  cacheMisses++;
  return "";
}

bool ResourceCache::contains(const String& resourceId) {
  return cacheMap.find(resourceId) != cacheMap.end();
}

bool ResourceCache::remove(const String& resourceId) {
  auto it = cacheMap.find(resourceId);
  if (it != cacheMap.end()) {
    CacheEntry* entry = it->second;
    
    totalCacheSize -= (entry->size + CACHE_ENTRY_OVERHEAD);
    totalEntries--;
    
    removeEntry(entry);
    cacheMap.erase(it);
    delete entry;
    
    Serial.printf("Removed cached resource: %s\n", resourceId.c_str());
    return true;
  }
  
  return false;
}

void ResourceCache::clear() {
  CacheEntry* current = head;
  while (current != nullptr) {
    CacheEntry* next = current->next;
    delete current;
    current = next;
  }
  
  head = nullptr;
  tail = nullptr;
  cacheMap.clear();
  totalCacheSize = 0;
  totalEntries = 0;
  
  Serial.println("Cache cleared");
}

int ResourceCache::freeMemory(size_t targetBytes) {
  int freedResources = 0;
  size_t freedBytes = 0;
  
  Serial.printf("Attempting to free %d bytes from cache\n", targetBytes);
  
  // Start from least recently used (tail) and work backwards
  CacheEntry* current = tail;
  while (current != nullptr && freedBytes < targetBytes) {
    CacheEntry* prev = current->prev;
    
    // Don't remove critical resources unless absolutely necessary
    if (current->priority == PRIORITY_CRITICAL && freedBytes > targetBytes / 2) {
      current = prev;
      continue;
    }
    
    freedBytes += current->size + CACHE_ENTRY_OVERHEAD;
    freedResources++;
    
    Serial.printf("Evicting resource: %s (%d bytes, priority: %d)\n", 
                  current->resourceId.c_str(), current->size, current->priority);
    
    // Remove the entry
    String resourceId = current->resourceId;
    current = prev;
    remove(resourceId);
    evictions++;
  }
  
  Serial.printf("Freed %d resources (%d bytes)\n", freedResources, freedBytes);
  return freedResources;
}

void ResourceCache::optimizeCache() {
  Serial.println("Optimizing cache...");
  
  if (totalCacheSize <= maxCacheSize) {
    Serial.println("Cache optimization not needed");
    return;
  }
  
  size_t targetReduction = totalCacheSize - (maxCacheSize * 0.8);  // Leave 20% headroom
  freeMemory(targetReduction);
}

bool ResourceCache::makeSpaceFor(size_t requiredSize, int priority) {
  if (totalCacheSize + requiredSize <= maxCacheSize) {
    return true;  // Already have space
  }
  
  size_t spaceNeeded = (totalCacheSize + requiredSize) - maxCacheSize;
  
  // Try to free space by removing lower priority items first
  CacheEntry* current = tail;
  size_t freedSpace = 0;
  
  while (current != nullptr && freedSpace < spaceNeeded) {
    CacheEntry* prev = current->prev;
    
    // Remove if lower priority or same priority but older
    if (shouldEvict(current, priority)) {
      freedSpace += current->size + CACHE_ENTRY_OVERHEAD;
      String resourceId = current->resourceId;
      current = prev;
      remove(resourceId);
      evictions++;
    } else {
      current = prev;
    }
  }
  
  return freedSpace >= spaceNeeded;
}

bool ResourceCache::shouldEvict(CacheEntry* entry, int newPriority) {
  // Always evict lower priority
  if (entry->priority > newPriority) {
    return true;
  }
  
  // For same priority, consider age and access frequency
  if (entry->priority == newPriority) {
    unsigned long age = millis() - entry->createTime;
    unsigned long timeSinceAccess = millis() - entry->accessTime;
    
    // Evict if not accessed recently and low access count
    if (timeSinceAccess > 300000 && entry->accessCount < 3) {  // 5 minutes
      return true;
    }
  }
  
  return false;
}

size_t ResourceCache::calculateEntrySize(const String& data) {
  return data.length() + sizeof(CacheEntry);
}

void ResourceCache::moveToHead(CacheEntry* entry) {
  if (entry == head) return;
  
  removeEntry(entry);
  addToHead(entry);
}

void ResourceCache::removeEntry(CacheEntry* entry) {
  if (entry->prev) {
    entry->prev->next = entry->next;
  } else {
    head = entry->next;
  }
  
  if (entry->next) {
    entry->next->prev = entry->prev;
  } else {
    tail = entry->prev;
  }
}

void ResourceCache::addToHead(CacheEntry* entry) {
  entry->prev = nullptr;
  entry->next = head;
  
  if (head) {
    head->prev = entry;
  }
  head = entry;
  
  if (!tail) {
    tail = entry;
  }
}

CacheEntry* ResourceCache::removeTail() {
  if (!tail) return nullptr;
  
  CacheEntry* lastEntry = tail;
  removeEntry(lastEntry);
  return lastEntry;
}

void ResourceCache::printCacheStats() {
  Serial.println("\n=== Cache Statistics ===");
  Serial.printf("Entries: %d\n", totalEntries);
  Serial.printf("Cache Size: %d / %d bytes (%.1f%%)\n", 
                totalCacheSize, maxCacheSize, getCacheUtilization() * 100);
  Serial.printf("Cache Hits: %d\n", cacheHits);
  Serial.printf("Cache Misses: %d\n", cacheMisses);
  Serial.printf("Hit Rate: %.1f%%\n", getHitRate() * 100);
  Serial.printf("Evictions: %d\n", evictions);
  
  Serial.println("\n=== Cached Resources ===");
  CacheEntry* current = head;
  int index = 0;
  while (current != nullptr && index < 10) {  // Show first 10
    unsigned long age = millis() - current->createTime;
    unsigned long lastAccess = millis() - current->accessTime;
    
    Serial.printf("%d. %s (%d bytes, P%d, age: %lums, last: %lums, hits: %d)\n",
                  ++index, current->resourceId.c_str(), current->size, 
                  current->priority, age, lastAccess, current->accessCount);
    current = current->next;
  }
  Serial.println("========================\n");
}

void ResourceCache::resetStats() {
  cacheHits = 0;
  cacheMisses = 0;
  evictions = 0;
  Serial.println("Cache statistics reset");
}

void ResourceCache::cleanupExpired(unsigned long maxAge) {
  CacheEntry* current = tail;
  int cleaned = 0;
  
  while (current != nullptr) {
    CacheEntry* prev = current->prev;
    unsigned long age = millis() - current->accessTime;
    
    // Remove expired non-critical resources
    if (age > maxAge && current->priority > PRIORITY_CRITICAL) {
      String resourceId = current->resourceId;
      remove(resourceId);
      cleaned++;
    }
    
    current = prev;
  }
  
  if (cleaned > 0) {
    Serial.printf("Cleaned up %d expired resources\n", cleaned);
  }
}

std::vector<String> ResourceCache::getResourcesByPriority(int priority) {
  std::vector<String> resources;
  
  CacheEntry* current = head;
  while (current != nullptr) {
    if (current->priority == priority) {
      resources.push_back(current->resourceId);
    }
    current = current->next;
  }
  
  return resources;
}

void ResourceCache::updatePriority(const String& resourceId, int newPriority) {
  auto it = cacheMap.find(resourceId);
  if (it != cacheMap.end()) {
    it->second->priority = newPriority;
    Serial.printf("Updated priority for %s to %d\n", resourceId.c_str(), newPriority);
  }
}

#endif // RESOURCE_CACHE_H