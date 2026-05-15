#pragma once
#include <string>
#include <vector>

// Return paths to all /dev/cu.* serial devices on macOS.
std::vector<std::string> find_serial_ports();
// Find the first Teensy /dev/cu.usbmodem* port, or empty string if not found.
std::string find_teensy_port();
