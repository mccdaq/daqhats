#!/bin/bash
# Simple bash script to send commands to sensor controller

SOCKET_PATH="/run/sensor_ctrl.sock"
FALLBACK_PATH="./sensor_ctrl.sock"

# Try to find socket
if [ -S "$SOCKET_PATH" ]; then
    SOCKET="$SOCKET_PATH"
elif [ -S "$FALLBACK_PATH" ]; then
    SOCKET="$FALLBACK_PATH"
else
    echo "Error: Socket not found at $SOCKET_PATH or $FALLBACK_PATH"
    echo "Make sure sensor_controller is running."
    exit 1
fi

# Build command
if [ "$1" = "SET_RATE" ]; then
    if [ -z "$2" ]; then
        echo "Error: SET_RATE requires a value"
        exit 1
    fi
    COMMAND="SET_RATE $2"
else
    COMMAND="$1"
fi

# Send command using socat or nc (netcat)
if command -v socat &> /dev/null; then
    echo "$COMMAND" | socat - UNIX-CONNECT:"$SOCKET"
elif command -v nc &> /dev/null; then
    echo "$COMMAND" | nc -U "$SOCKET"
else
    echo "Error: Need 'socat' or 'nc' (netcat) to send commands"
    echo "Install with: sudo apt-get install socat"
    echo "Or use Python script: python3 send_command.py $*"
    exit 1
fi

