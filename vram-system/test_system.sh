#!/bin/bash

# VRAM System Integration Test Script
# Tests the complete VRAM system functionality

echo "=========================================="
echo "VRAM System Integration Test"
echo "=========================================="

# Configuration
SERVER_URL="http://localhost:5000"
TEST_TIMEOUT=30

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test results
TESTS_PASSED=0
TESTS_FAILED=0

# Function to run a test
run_test() {
    local test_name="$1"
    local test_command="$2"
    local expected_pattern="$3"
    
    echo -n "Testing $test_name... "
    
    # Run the test with timeout
    result=$(timeout $TEST_TIMEOUT bash -c "$test_command" 2>/dev/null)
    exit_code=$?
    
    if [ $exit_code -eq 0 ] && [[ "$result" =~ $expected_pattern ]]; then
        echo -e "${GREEN}PASS${NC}"
        ((TESTS_PASSED++))
        return 0
    else
        echo -e "${RED}FAIL${NC}"
        echo "  Expected pattern: $expected_pattern"
        echo "  Got: $result"
        ((TESTS_FAILED++))
        return 1
    fi
}

# Check if server is running
echo "Checking if server is running..."
if ! curl -s "$SERVER_URL/api/health" >/dev/null 2>&1; then
    echo -e "${YELLOW}Warning: Server not running. Starting server...${NC}"
    
    # Try to start server
    cd "$(dirname "$0")/server"
    python3 app.py &
    SERVER_PID=$!
    
    # Wait for server to start
    sleep 5
    
    if ! curl -s "$SERVER_URL/api/health" >/dev/null 2>&1; then
        echo -e "${RED}Error: Could not start server${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Server started successfully${NC}"
    STARTED_SERVER=true
else
    echo -e "${GREEN}Server is already running${NC}"
    STARTED_SERVER=false
fi

echo ""
echo "Running API tests..."
echo "===================="

# Test 1: Health check
run_test "Health Check" \
    "curl -s $SERVER_URL/api/health" \
    '"status":"healthy"'

# Test 2: List resources
run_test "List Resources" \
    "curl -s $SERVER_URL/api/resources" \
    '"resources":\['

# Test 3: Get specific resource
run_test "Get Specific Resource" \
    "curl -s $SERVER_URL/api/resources/config_main" \
    '"resource_id":"config_main"'

# Test 4: Get resource version
run_test "Get Resource Version" \
    "curl -s $SERVER_URL/api/resources/config_main/version" \
    '"version":'

# Test 5: Get statistics
run_test "Get Statistics" \
    "curl -s $SERVER_URL/api/stats" \
    '"total_resources":'

# Test 6: Create new resource
run_test "Create New Resource" \
    "curl -s -X POST $SERVER_URL/api/resources -H 'Content-Type: application/json' -d '{\"resource_id\":\"test_resource\",\"content\":\"test data\",\"category\":\"test\",\"priority\":3}'" \
    '"message":"Resource uploaded successfully"'

# Test 7: Get newly created resource
run_test "Get Created Resource" \
    "curl -s $SERVER_URL/api/resources/test_resource" \
    '"resource_id":"test_resource"'

# Test 8: Delete resource
run_test "Delete Resource" \
    "curl -s -X DELETE $SERVER_URL/api/resources/test_resource" \
    '"message":"Resource deleted successfully"'

# Test 9: Verify resource deleted
run_test "Verify Resource Deleted" \
    "curl -s $SERVER_URL/api/resources/test_resource" \
    '"error":"Resource not found"'

# Test 10: Optimization endpoint
run_test "Optimization Endpoint" \
    "curl -s -X POST $SERVER_URL/api/optimize -H 'Content-Type: application/json' -d '{}'" \
    '"message":"Optimization completed"'

echo ""
echo "Running performance tests..."
echo "============================"

# Test 11: Response time check
start_time=$(date +%s%N)
curl -s "$SERVER_URL/api/health" >/dev/null
end_time=$(date +%s%N)
response_time=$(( (end_time - start_time) / 1000000 ))  # Convert to milliseconds

if [ $response_time -lt 100 ]; then
    echo -e "Response Time (<100ms)... ${GREEN}PASS${NC} (${response_time}ms)"
    ((TESTS_PASSED++))
else
    echo -e "Response Time (<100ms)... ${RED}FAIL${NC} (${response_time}ms)"
    ((TESTS_FAILED++))
fi

# Test 12: Concurrent requests
echo -n "Concurrent Requests... "
concurrent_results=$(for i in {1..10}; do curl -s "$SERVER_URL/api/health" & done; wait)
concurrent_success=$(echo "$concurrent_results" | grep -c '"status":"healthy"')

if [ $concurrent_success -eq 10 ]; then
    echo -e "${GREEN}PASS${NC} (10/10 successful)"
    ((TESTS_PASSED++))
else
    echo -e "${RED}FAIL${NC} ($concurrent_success/10 successful)"
    ((TESTS_FAILED++))
fi

echo ""
echo "Testing file structure..."
echo "========================"

# Test 13: Check required files exist
required_files=(
    "server/app.py"
    "server/resource_manager.py"
    "server/requirements.txt"
    "server/start_server.sh"
    "m5client/vram_client.ino"
    "m5client/memory_manager.h"
    "m5client/resource_cache.h"
    "m5client/wifi_manager.h"
    "examples/basic_usage.ino"
    "README.md"
    ".gitignore"
)

missing_files=0
for file in "${required_files[@]}"; do
    if [ -f "$file" ]; then
        echo -e "File $file... ${GREEN}EXISTS${NC}"
    else
        echo -e "File $file... ${RED}MISSING${NC}"
        ((missing_files++))
    fi
done

if [ $missing_files -eq 0 ]; then
    echo -e "File Structure Check... ${GREEN}PASS${NC}"
    ((TESTS_PASSED++))
else
    echo -e "File Structure Check... ${RED}FAIL${NC} ($missing_files missing files)"
    ((TESTS_FAILED++))
fi

# Test 14: Check file sizes are reasonable
echo -n "File Size Check... "
total_size=$(du -sb . | cut -f1)
if [ $total_size -lt 1048576 ]; then  # Less than 1MB
    echo -e "${GREEN}PASS${NC} (Total size: $(du -sh . | cut -f1))"
    ((TESTS_PASSED++))
else
    echo -e "${YELLOW}WARNING${NC} (Total size: $(du -sh . | cut -f1))"
    ((TESTS_PASSED++))
fi

# Cleanup
if [ "$STARTED_SERVER" = true ] && [ -n "$SERVER_PID" ]; then
    echo ""
    echo "Stopping test server..."
    kill $SERVER_PID 2>/dev/null
    wait $SERVER_PID 2>/dev/null
fi

# Summary
echo ""
echo "=========================================="
echo "Test Summary"
echo "=========================================="
echo -e "Tests Passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests Failed: ${RED}$TESTS_FAILED${NC}"
echo -e "Total Tests: $((TESTS_PASSED + TESTS_FAILED))"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n${GREEN}All tests passed! VRAM system is working correctly.${NC}"
    exit 0
else
    echo -e "\n${RED}Some tests failed. Please check the issues above.${NC}"
    exit 1
fi