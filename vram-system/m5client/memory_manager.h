/*
 * Memory Manager for VRAM System
 * Handles dynamic memory allocation, monitoring, and optimization
 */

#ifndef MEMORY_MANAGER_H
#define MEMORY_MANAGER_H

#include <Arduino.h>

// Memory information structure
struct MemoryInfo {
  size_t totalHeap;
  size_t freeHeap;
  size_t usedHeap;
  size_t largestFreeBlock;
  int usagePercent;
  int fragmentation;
};

// Memory allocation tracking
struct MemoryBlock {
  void* ptr;
  size_t size;
  unsigned long allocTime;
  String identifier;
  MemoryBlock* next;
};

class MemoryManager {
private:
  MemoryBlock* allocatedBlocks;
  size_t totalAllocated;
  size_t peakUsage;
  unsigned long allocationCount;
  unsigned long freeCount;
  
  // Memory optimization settings
  static const size_t MIN_FREE_HEAP = 32768;  // 32KB minimum free
  static const int CRITICAL_USAGE_THRESHOLD = 90;
  static const int WARNING_USAGE_THRESHOLD = 75;
  
  void addBlock(void* ptr, size_t size, const String& identifier);
  void removeBlock(void* ptr);
  MemoryBlock* findBlock(void* ptr);
  
public:
  MemoryManager();
  ~MemoryManager();
  
  // Initialization
  void begin();
  
  // Memory allocation with tracking
  void* allocate(size_t size, const String& identifier = "");
  void* reallocate(void* ptr, size_t newSize, const String& identifier = "");
  void deallocate(void* ptr);
  
  // Memory information
  MemoryInfo getMemoryInfo();
  size_t getTotalAllocated() { return totalAllocated; }
  size_t getPeakUsage() { return peakUsage; }
  
  // Memory optimization
  bool isMemoryLow();
  bool isMemoryCritical();
  void forceGarbageCollection();
  size_t getFragmentation();
  
  // Statistics
  void printMemoryReport();
  void resetStatistics();
  
  // Emergency cleanup
  void emergencyCleanup();
};

// Global memory manager instance
extern MemoryManager memoryManager;

// Memory allocation macros with tracking
#define VRAM_MALLOC(size, id) memoryManager.allocate(size, id)
#define VRAM_REALLOC(ptr, size, id) memoryManager.reallocate(ptr, size, id)
#define VRAM_FREE(ptr) memoryManager.deallocate(ptr)

// Implementation
MemoryManager::MemoryManager() {
  allocatedBlocks = nullptr;
  totalAllocated = 0;
  peakUsage = 0;
  allocationCount = 0;
  freeCount = 0;
}

MemoryManager::~MemoryManager() {
  // Clean up all allocated blocks
  MemoryBlock* current = allocatedBlocks;
  while (current != nullptr) {
    MemoryBlock* next = current->next;
    free(current->ptr);
    delete current;
    current = next;
  }
}

void MemoryManager::begin() {
  Serial.println("MemoryManager: Initializing...");
  
  MemoryInfo info = getMemoryInfo();
  Serial.printf("Initial heap: %d bytes free, %d bytes total\n", 
                info.freeHeap, info.totalHeap);
  
  if (info.freeHeap < MIN_FREE_HEAP) {
    Serial.println("WARNING: Low initial memory!");
  }
}

void* MemoryManager::allocate(size_t size, const String& identifier) {
  // Check if allocation would cause memory issues
  MemoryInfo info = getMemoryInfo();
  if (info.freeHeap < size + MIN_FREE_HEAP) {
    Serial.printf("Allocation failed: insufficient memory (requested: %d, available: %d)\n", 
                  size, info.freeHeap);
    return nullptr;
  }
  
  void* ptr = malloc(size);
  if (ptr != nullptr) {
    addBlock(ptr, size, identifier);
    allocationCount++;
    
    // Update peak usage
    if (totalAllocated > peakUsage) {
      peakUsage = totalAllocated;
    }
    
    Serial.printf("Allocated %d bytes for '%s' at %p\n", 
                  size, identifier.c_str(), ptr);
  } else {
    Serial.printf("malloc failed for %d bytes\n", size);
  }
  
  return ptr;
}

void* MemoryManager::reallocate(void* ptr, size_t newSize, const String& identifier) {
  if (ptr == nullptr) {
    return allocate(newSize, identifier);
  }
  
  MemoryBlock* block = findBlock(ptr);
  if (block == nullptr) {
    Serial.println("realloc: pointer not found in tracking");
    return nullptr;
  }
  
  size_t oldSize = block->size;
  void* newPtr = realloc(ptr, newSize);
  
  if (newPtr != nullptr) {
    // Update tracking
    removeBlock(ptr);
    addBlock(newPtr, newSize, identifier);
    
    Serial.printf("Reallocated from %d to %d bytes for '%s'\n", 
                  oldSize, newSize, identifier.c_str());
  }
  
  return newPtr;
}

void MemoryManager::deallocate(void* ptr) {
  if (ptr == nullptr) return;
  
  MemoryBlock* block = findBlock(ptr);
  if (block != nullptr) {
    Serial.printf("Freed %d bytes for '%s'\n", 
                  block->size, block->identifier.c_str());
    removeBlock(ptr);
    freeCount++;
  } else {
    Serial.println("Free: pointer not found in tracking");
  }
  
  free(ptr);
}

void MemoryManager::addBlock(void* ptr, size_t size, const String& identifier) {
  MemoryBlock* block = new MemoryBlock();
  block->ptr = ptr;
  block->size = size;
  block->allocTime = millis();
  block->identifier = identifier;
  block->next = allocatedBlocks;
  
  allocatedBlocks = block;
  totalAllocated += size;
}

void MemoryManager::removeBlock(void* ptr) {
  MemoryBlock* current = allocatedBlocks;
  MemoryBlock* previous = nullptr;
  
  while (current != nullptr) {
    if (current->ptr == ptr) {
      if (previous != nullptr) {
        previous->next = current->next;
      } else {
        allocatedBlocks = current->next;
      }
      
      totalAllocated -= current->size;
      delete current;
      return;
    }
    
    previous = current;
    current = current->next;
  }
}

MemoryBlock* MemoryManager::findBlock(void* ptr) {
  MemoryBlock* current = allocatedBlocks;
  while (current != nullptr) {
    if (current->ptr == ptr) {
      return current;
    }
    current = current->next;
  }
  return nullptr;
}

MemoryInfo MemoryManager::getMemoryInfo() {
  MemoryInfo info;
  
  info.freeHeap = ESP.getFreeHeap();
  info.totalHeap = ESP.getHeapSize();
  info.usedHeap = info.totalHeap - info.freeHeap;
  info.largestFreeBlock = ESP.getMaxAllocHeap();
  info.usagePercent = (info.usedHeap * 100) / info.totalHeap;
  
  // Calculate fragmentation
  if (info.freeHeap > 0) {
    info.fragmentation = 100 - ((info.largestFreeBlock * 100) / info.freeHeap);
  } else {
    info.fragmentation = 100;
  }
  
  return info;
}

bool MemoryManager::isMemoryLow() {
  MemoryInfo info = getMemoryInfo();
  return info.usagePercent >= WARNING_USAGE_THRESHOLD;
}

bool MemoryManager::isMemoryCritical() {
  MemoryInfo info = getMemoryInfo();
  return info.usagePercent >= CRITICAL_USAGE_THRESHOLD || 
         info.freeHeap < MIN_FREE_HEAP;
}

void MemoryManager::forceGarbageCollection() {
  Serial.println("Forcing garbage collection...");
  
  // On ESP32, we can't force GC directly, but we can encourage it
  // by temporarily allocating and freeing small blocks
  for (int i = 0; i < 10; i++) {
    void* temp = malloc(1024);
    if (temp) {
      free(temp);
    }
    delay(1);
  }
  
  Serial.println("Garbage collection attempt completed");
}

size_t MemoryManager::getFragmentation() {
  MemoryInfo info = getMemoryInfo();
  return info.fragmentation;
}

void MemoryManager::printMemoryReport() {
  MemoryInfo info = getMemoryInfo();
  
  Serial.println("\n=== Memory Report ===");
  Serial.printf("Total Heap: %d bytes\n", info.totalHeap);
  Serial.printf("Free Heap: %d bytes\n", info.freeHeap);
  Serial.printf("Used Heap: %d bytes (%d%%)\n", info.usedHeap, info.usagePercent);
  Serial.printf("Largest Free Block: %d bytes\n", info.largestFreeBlock);
  Serial.printf("Fragmentation: %d%%\n", info.fragmentation);
  Serial.printf("Tracked Allocations: %d bytes\n", totalAllocated);
  Serial.printf("Peak Usage: %d bytes\n", peakUsage);
  Serial.printf("Allocation Count: %lu\n", allocationCount);
  Serial.printf("Free Count: %lu\n", freeCount);
  
  Serial.println("\n=== Tracked Blocks ===");
  MemoryBlock* current = allocatedBlocks;
  int blockCount = 0;
  while (current != nullptr) {
    blockCount++;
    Serial.printf("Block %d: %d bytes, '%s', age: %lums\n", 
                  blockCount, current->size, current->identifier.c_str(),
                  millis() - current->allocTime);
    current = current->next;
  }
  Serial.println("=====================\n");
}

void MemoryManager::resetStatistics() {
  allocationCount = 0;
  freeCount = 0;
  peakUsage = totalAllocated;
  Serial.println("Memory statistics reset");
}

void MemoryManager::emergencyCleanup() {
  Serial.println("EMERGENCY: Critical memory condition!");
  
  // Force garbage collection
  forceGarbageCollection();
  
  // Print emergency report
  printMemoryReport();
  
  // If still critical, we might need to restart
  if (isMemoryCritical()) {
    Serial.println("CRITICAL: Memory still low after cleanup!");
    Serial.println("System may need restart...");
  }
}

#endif // MEMORY_MANAGER_H