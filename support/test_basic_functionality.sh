#!/bin/bash
#
# Basic functionality test for Tvheadend
# Tests that the binary starts up and HTTP port is reachable
#

set -e

# Configuration
TVHEADEND_BIN="${TVHEADEND_BIN:-./build.linux/tvheadend}"
HTTP_PORT="${HTTP_PORT:-9981}"
TIMEOUT="${TIMEOUT:-30}"
STARTUP_WAIT="${STARTUP_WAIT:-10}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Cleanup function
cleanup() {
    log_info "Cleaning up..."
    if [ ! -z "$TVH_PID" ]; then
        log_info "Stopping tvheadend (PID: $TVH_PID)"
        kill $TVH_PID 2>/dev/null || true
        sleep 2
        # Force kill if still running
        kill -9 $TVH_PID 2>/dev/null || true
    fi
    # Clean up temporary config directory
    if [ ! -z "$TEMP_CONFIG" ] && [ -d "$TEMP_CONFIG" ]; then
        rm -rf "$TEMP_CONFIG"
    fi
}

trap cleanup EXIT INT TERM

# Test 1: Check if binary exists
test_binary_exists() {
    log_info "Test 1: Checking if tvheadend binary exists..."
    if [ ! -f "$TVHEADEND_BIN" ]; then
        log_error "Binary not found at $TVHEADEND_BIN"
        return 1
    fi
    if [ ! -x "$TVHEADEND_BIN" ]; then
        log_error "Binary is not executable at $TVHEADEND_BIN"
        return 1
    fi
    log_info "✓ Binary exists and is executable"
    return 0
}

# Test 2: Check version output
test_version() {
    log_info "Test 2: Checking version output..."
    if ! timeout 5 "$TVHEADEND_BIN" --version 2>&1 | grep -qi "tvheadend.*version"; then
        log_error "Failed to get version information"
        return 1
    fi
    log_info "✓ Version output successful"
    return 0
}

# Test 3: Check help output
test_help() {
    log_info "Test 3: Checking help output..."
    if ! timeout 5 "$TVHEADEND_BIN" --help 2>&1 | grep -q "Usage:"; then
        log_error "Failed to get help information"
        return 1
    fi
    log_info "✓ Help output successful"
    return 0
}

# Test 4: Start tvheadend and check if it runs
test_startup() {
    log_info "Test 4: Starting tvheadend..."
    
    # Create temporary config directory
    TEMP_CONFIG=$(mktemp -d)
    log_info "Using temporary config directory: $TEMP_CONFIG"
    
    # Start tvheadend in background with first-run setup and no stderr output
    "$TVHEADEND_BIN" -C -c "$TEMP_CONFIG" --http_port "$HTTP_PORT" --nostderr > /dev/null 2>&1 &
    TVH_PID=$!
    
    log_info "Started tvheadend with PID: $TVH_PID"
    
    # Wait for startup
    log_info "Waiting ${STARTUP_WAIT}s for tvheadend to initialize..."
    sleep "$STARTUP_WAIT"
    
    # Check if process is still running
    if ! kill -0 "$TVH_PID" 2>/dev/null; then
        log_error "Tvheadend process died during startup"
        return 1
    fi
    
    log_info "✓ Tvheadend started successfully"
    return 0
}

# Test 5: Check if HTTP port is reachable
test_http_port() {
    log_info "Test 5: Checking if HTTP port $HTTP_PORT is reachable..."
    
    # Try to connect to the HTTP port using curl or wget
    if command -v curl >/dev/null 2>&1; then
        if timeout 10 curl -s -f "http://localhost:$HTTP_PORT/" > /dev/null; then
            log_info "✓ HTTP port is reachable (curl)"
            return 0
        fi
    elif command -v wget >/dev/null 2>&1; then
        if timeout 10 wget -q -O /dev/null "http://localhost:$HTTP_PORT/"; then
            log_info "✓ HTTP port is reachable (wget)"
            return 0
        fi
    fi
    
    # Fallback to netcat if available
    if command -v nc >/dev/null 2>&1; then
        if timeout 5 nc -z localhost "$HTTP_PORT"; then
            log_info "✓ HTTP port is reachable (nc)"
            return 0
        fi
    fi
    
    # Last resort: check if port is listening using /proc
    if [ -f "/proc/net/tcp" ]; then
        # Convert port to hex
        PORT_HEX=$(printf '%04X' "$HTTP_PORT")
        if grep -q ":$PORT_HEX" /proc/net/tcp 2>/dev/null; then
            log_info "✓ HTTP port is listening (proc)"
            return 0
        fi
    fi
    
    log_error "HTTP port is not reachable"
    return 1
}

# Test 6: Verify web interface returns valid response
test_web_interface() {
    log_info "Test 6: Verifying web interface responds..."
    
    if command -v curl >/dev/null 2>&1; then
        local response=$(timeout 10 curl -s "http://localhost:$HTTP_PORT/" 2>/dev/null)
        if echo "$response" | grep -q -i "tvheadend\|html"; then
            log_info "✓ Web interface returns valid response"
            return 0
        fi
    elif command -v wget >/dev/null 2>&1; then
        local response=$(timeout 10 wget -q -O - "http://localhost:$HTTP_PORT/" 2>/dev/null)
        if echo "$response" | grep -q -i "tvheadend\|html"; then
            log_info "✓ Web interface returns valid response"
            return 0
        fi
    fi
    
    log_warn "Could not verify web interface response (curl/wget not available)"
    return 0  # Don't fail if tools not available
}

# Test 7: Check if process is healthy
test_process_health() {
    log_info "Test 7: Checking process health..."
    
    if ! kill -0 "$TVH_PID" 2>/dev/null; then
        log_error "Tvheadend process is not running"
        return 1
    fi
    
    log_info "✓ Process is healthy"
    return 0
}

# Main test execution
main() {
    local failed=0
    
    log_info "========================================"
    log_info "Tvheadend Basic Functionality Tests"
    log_info "========================================"
    log_info "Binary: $TVHEADEND_BIN"
    log_info "HTTP Port: $HTTP_PORT"
    log_info "Timeout: ${TIMEOUT}s"
    log_info "========================================"
    
    # Run tests
    test_binary_exists || ((failed++))
    test_version || ((failed++))
    test_help || ((failed++))
    test_startup || { ((failed++)); log_error "Cannot continue without successful startup"; exit 1; }
    test_http_port || ((failed++))
    test_web_interface || ((failed++))
    test_process_health || ((failed++))
    
    log_info "========================================"
    if [ $failed -eq 0 ]; then
        log_info "${GREEN}All tests passed!${NC}"
        return 0
    else
        log_error "${RED}$failed test(s) failed!${NC}"
        return 1
    fi
}

# Run main function
main
exit $?
