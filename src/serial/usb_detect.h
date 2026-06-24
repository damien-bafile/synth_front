/// @file usb_detect.h
/// @brief Serial port enumeration and Teensy USB auto-detection.
///
/// The Teensy is identified by its USB vendor/product IDs: VID 0x16C0 with
/// PID 0x0478 or PIDs in the 0x0482-0x048F range.

#pragma once
#include <string>
#include <vector>

/// Enumerate candidate serial ports on this machine.
/// @return A list of device paths (e.g. "/dev/ttyACM0", "COM3").
std::vector<std::string> find_serial_ports();

/// Find the first attached Teensy serial port.
/// @return The device path, or an empty string if no Teensy was detected.
std::string find_teensy_port();
