# VRAM System - Virtual RAM for M5StickC Plus2

A complete Virtual RAM (VRAM) system implementation for M5StickC Plus2 with dynamic resource management from a server.

## 🏗️ System Architecture

The VRAM system consists of three main components:

1. **Resource Server** (Python Flask) - Manages and serves resources
2. **M5StickC Plus2 Client** (ESP32 Arduino) - Dynamic memory management and resource caching
3. **Communication Protocol** - HTTP/JSON-based resource exchange

## 📁 Directory Structure

```
vram-system/
├── server/                         # Python Flask server
│   ├── app.py                     # Main server application
│   ├── resource_manager.py       # Resource storage and management
│   ├── resources/                 # Stored resource files
│   └── requirements.txt           # Python dependencies
├── m5client/                      # M5StickC Plus2 client code
│   ├── vram_client.ino           # Main Arduino sketch
│   ├── memory_manager.h          # Memory monitoring and management
│   ├── resource_cache.h          # Intelligent caching with LRU
│   └── wifi_manager.h            # WiFi connection management
└── examples/                      # Usage examples and demos
    ├── basic_usage.ino           # Basic functionality example
    └── demo_resources/           # Sample resources for testing
        ├── config.json
        ├── sensor_lib.h
        └── sample_data.txt
```

## 🚀 Quick Start

### 1. Server Setup

```bash
# Navigate to server directory
cd vram-system/server

# Install Python dependencies
pip install -r requirements.txt

# Start the Flask server
python app.py
```

The server will start on `http://localhost:5000` by default.

### 2. M5StickC Plus2 Setup

1. Open `vram-system/m5client/vram_client.ino` in Arduino IDE
2. Update WiFi credentials and server URL in the code:
   ```cpp
   #define DEFAULT_WIFI_SSID "your_wifi_network"
   #define DEFAULT_WIFI_PASSWORD "your_password"
   #define DEFAULT_SERVER_URL "http://192.168.1.100:5000"
   ```
3. Install required libraries:
   - M5StickCPlus2
   - ArduinoJson
   - WiFi
   - HTTPClient
4. Upload the sketch to your M5StickC Plus2

### 3. Basic Usage Example

See `examples/basic_usage.ino` for a complete example demonstrating:
- System initialization
- Resource loading and caching
- Memory monitoring
- Automatic cleanup

## 🔧 API Endpoints

The Flask server provides the following REST API endpoints:

### Resource Management
- `GET /api/health` - Server health check
- `GET /api/resources/<id>` - Get specific resource
- `GET /api/resources` - List available resources
- `POST /api/resources` - Upload new resource
- `DELETE /api/resources/<id>` - Delete resource

### Resource Information
- `GET /api/resources/<id>/version` - Get resource version info
- `GET /api/stats` - Get server and usage statistics

### Optimization
- `POST /api/optimize` - Trigger server-side optimization

### Example API Usage

```bash
# Get server health
curl http://localhost:5000/api/health

# List all resources
curl http://localhost:5000/api/resources

# Get specific resource
curl http://localhost:5000/api/resources/config_main

# Get resource with compression
curl "http://localhost:5000/api/resources/large_data?compress=true"

# Upload new resource
curl -X POST http://localhost:5000/api/resources \
  -H "Content-Type: application/json" \
  -d '{"resource_id": "test", "content": "Hello World", "category": "demo", "priority": 3}'
```

## 💾 Memory Management Features

### Client-Side (M5StickC Plus2)

**Memory Manager**
- Real-time memory monitoring
- Allocation tracking with identifiers
- Memory leak detection
- Emergency cleanup procedures
- Fragmentation analysis

**Resource Cache**
- LRU (Least Recently Used) algorithm
- Priority-based eviction
- Configurable cache size limits
- Hit/miss statistics
- Automatic cleanup when memory is low

**Smart Deletion Algorithm**
- Priority levels: Critical (1), Important (2), Normal (3), Low (4)
- Age-based scoring
- Access frequency consideration
- Emergency thresholds (90% memory usage triggers 30% cleanup)

### Server-Side

**Resource Storage**
- File-based storage with metadata
- Version tracking and checksums
- Category organization
- Usage analytics and logging
- Compression support for large resources

**Optimization Features**
- LRU-based server cleanup
- Storage size limits
- Access pattern analysis
- Automatic resource expiration

## 📊 Performance Goals & Metrics

| Metric | Target | Implementation |
|--------|--------|----------------|
| Response Time | < 100ms | HTTP client with 10s timeout |
| Memory Efficiency | 90%+ utilization | Smart caching with LRU eviction |
| Reliability | < 1% failures | Auto-reconnection and error handling |
| Max Resource Size | 1MB | Configurable limits with compression |

## 🎮 M5StickC Plus2 Controls

- **Button A**: Load a random demo resource
- **Button B**: Show detailed memory status
- **Power Button**: Display system statistics and perform cleanup

## 🔍 Monitoring & Debugging

### Real-time Display
The M5StickC Plus2 display shows:
- Current memory usage percentage
- Server connection status
- Number of cached resources
- System health indicators

### Serial Output
Detailed logging includes:
- Memory allocation/deallocation events
- Resource loading operations
- Cache hit/miss statistics
- Network connection status
- Error messages and debugging info

### Server Logs
The Flask server logs:
- All API requests and response times
- Resource access patterns
- Error conditions
- Performance metrics

## ⚙️ Configuration Options

### Client Configuration
```cpp
// Memory thresholds
#define MEMORY_THRESHOLD_PERCENT 90    // Trigger cleanup at 90%
#define CLEANUP_PERCENTAGE 30          // Free 30% of memory
#define MAX_CACHE_SIZE (256 * 1024)    // 256KB cache limit

// Network settings
#define SERVER_CHECK_INTERVAL 30000    // Check server every 30s
#define WIFI_CONNECT_TIMEOUT 15000     // 15s WiFi timeout
```

### Server Configuration
```python
# Resource limits
MAX_RESOURCE_SIZE = 1024 * 1024        # 1MB per resource
MAX_TOTAL_SIZE = 100 * 1024 * 1024     # 100MB total storage

# Performance settings
REQUEST_TIMEOUT = 120                   # 2 minute request timeout
CLEANUP_THRESHOLD = 0.9                # Cleanup at 90% usage
```

## 🧪 Testing

### Automated Tests
The system includes built-in testing features:
- Memory leak detection
- Cache performance validation
- Network resilience testing
- Resource integrity checks

### Load Testing
Test the system under various conditions:
```bash
# Test resource loading
for i in {1..100}; do
  curl "http://localhost:5000/api/resources/test_resource_$i"
done

# Test concurrent access
ab -n 1000 -c 10 http://localhost:5000/api/health
```

## 🔧 Troubleshooting

### Common Issues

**WiFi Connection Problems**
- Verify SSID and password
- Check WiFi signal strength
- Enable auto-reconnection

**Server Communication Errors**
- Ensure server is running and accessible
- Check firewall settings
- Verify server URL and port

**Memory Issues**
- Monitor memory usage with Button B
- Adjust cache size limits if needed
- Check for memory leaks in custom code

**Resource Loading Failures**
- Verify resource exists on server
- Check resource size limits
- Monitor network stability

### Debug Commands
```cpp
// Print detailed memory report
memoryManager.printMemoryReport();

// Show cache statistics
resourceCache.printCacheStats();

// Display WiFi connection info
wifiManager.printConnectionInfo();
```

## 🔮 Future Enhancements

Potential improvements for the VRAM system:
- Binary resource formats for better compression
- Predictive resource loading based on usage patterns
- Multi-server support with failover
- Resource synchronization and versioning
- Machine learning for optimal cache management
- Real-time resource streaming
- Encrypted resource transmission
- Peer-to-peer resource sharing between devices

## 📄 License

This project is open source and available under the MIT License.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

## 📞 Support

For support and questions:
- Check the troubleshooting section above
- Review the example code in the `examples/` directory
- Open an issue on the project repository

---

**VRAM System** - Bringing virtual memory management to embedded devices! 🚀