#!/usr/bin/env python3
"""
Send commands to sensor controller via Unix domain socket.

Usage:
    python send_command.py START
    python send_command.py STOP
    python send_command.py STATUS
    python send_command.py SET_RATE 10000
"""

import socket
import sys
import os

# Try multiple possible socket paths
SOCKET_PATHS = [
    "/tmp/sensor_ctrl.sock",
    "/run/sensor_ctrl.sock",
    "./sensor_ctrl.sock",
    "sensor_ctrl.sock"
]

def send_command(command):
    """Send a command to the sensor controller socket."""
    last_error = None
    
    for socket_path in SOCKET_PATHS:
        try:
            with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as client:
                client.connect(socket_path)
                client.sendall(command.encode())
                
                # Receive response
                response = client.recv(1024).decode()
                print(response, end='')
                return True
        except FileNotFoundError:
            last_error = f"Socket not found at {socket_path}"
            continue
        except ConnectionRefusedError:
            last_error = f"Connection refused to {socket_path}"
            continue
        except Exception as e:
            last_error = f"Error connecting to {socket_path}: {e}"
            continue
    
    # If we get here, all paths failed
    print(f"Error: Could not connect to sensor controller socket")
    print(f"Tried: {', '.join(SOCKET_PATHS)}")
    print("Make sure the sensor controller is running.")
    if last_error:
        print(f"Last error: {last_error}")
    return False

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python send_command.py <COMMAND> [ARGS]")
        print("Commands: START, STOP, STATUS, SET_RATE <value>")
        sys.exit(1)
    
    # Build command from arguments
    if sys.argv[1] == "SET_RATE":
        if len(sys.argv) < 3:
            print("Error: SET_RATE requires a value")
            sys.exit(1)
        command = f"SET_RATE {sys.argv[2]}"
    else:
        command = sys.argv[1]
    
    success = send_command(command + "\n")
    sys.exit(0 if success else 1)

