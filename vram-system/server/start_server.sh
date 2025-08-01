#!/bin/bash

# VRAM System Server Startup Script

echo "Starting VRAM System Server..."
echo "================================"

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo "Error: Python 3 is not installed"
    exit 1
fi

# Check if required packages are installed
echo "Checking dependencies..."
python3 -c "import flask, json, os, logging" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "Installing required packages..."
    pip3 install -r requirements.txt
fi

# Create resources directory if it doesn't exist
mkdir -p resources

# Start the server
echo "Starting Flask server on http://0.0.0.0:5000"
echo "Press Ctrl+C to stop the server"
echo "================================"

python3 app.py