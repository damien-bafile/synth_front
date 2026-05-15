#include "usb_detect.h"
#include <dirent.h>
#include <cstring>

std::vector<std::string> find_serial_ports() {
  std::vector<std::string> ports;
  DIR* dir = opendir("/dev");
  if (!dir) return ports;

  struct dirent* entry;
  while ((entry = readdir(dir))) {
    if (strncmp(entry->d_name, "cu.", 3) == 0) {
      ports.push_back(std::string("/dev/") + entry->d_name);
    }
  }
  closedir(dir);
  return ports;
}

std::string find_teensy_port() {
  auto ports = find_serial_ports();
  for (const auto& p : ports) {
    if (p.find("usbmodem") != std::string::npos) {
      return p;
    }
  }
  return "";
}
