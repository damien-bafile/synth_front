#include "usb_detect.h"
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

#ifdef __linux__
#include <limits.h>
#endif

#ifdef __linux__
static bool is_teensy_sysfs(const std::string& dev_name) {
    std::string base = "/sys/class/tty/" + dev_name + "/device";
    char real[PATH_MAX];
    if (!realpath(base.c_str(), real)) return false;

    std::string path(real);
    for (int i = 0; i < 4; i++) {
        std::string vendor_path = path + "/idVendor";
        std::string product_path = path + "/idProduct";

        FILE* vf = fopen(vendor_path.c_str(), "r");
        FILE* pf = fopen(product_path.c_str(), "r");
        if (!vf || !pf) {
            if (vf) fclose(vf);
            if (pf) fclose(pf);
            path += "/..";
            continue;
        }

        char vendor[32] = {};
        char product[32] = {};
        bool found = false;
        if (fgets(vendor, sizeof(vendor), vf) && fgets(product, sizeof(product), pf)) {
            vendor[strcspn(vendor, "\n")] = 0;
            product[strcspn(product, "\n")] = 0;
            if (strcmp(vendor, "16c0") == 0 && strcmp(product, "0483") == 0)
                found = true;
        }

        fclose(vf);
        fclose(pf);
        return found;
    }
    return false;
}
#endif

std::vector<std::string> find_serial_ports() {
    std::vector<std::string> ports;
    DIR* dir = opendir("/dev");
    if (!dir) return ports;

    struct dirent* entry;
    while ((entry = readdir(dir))) {
        const char* name = entry->d_name;
#ifdef __APPLE__
        if (strncmp(name, "cu.", 3) == 0) {
            ports.push_back(std::string("/dev/") + name);
        }
#else
        if (strncmp(name, "ttyACM", 6) == 0 ||
            strncmp(name, "ttyUSB", 6) == 0) {
            ports.push_back(std::string("/dev/") + name);
        }
#endif
    }
    closedir(dir);
    return ports;
}

std::string find_teensy_port() {
    auto ports = find_serial_ports();
    for (const auto& p : ports) {
#ifdef __APPLE__
        if (p.find("usbmodem") != std::string::npos) return p;
        if (p.find("tty.usbmodem") != std::string::npos) return p;
#else
        std::string dev_name = p.substr(5);
        if (strncmp(dev_name.c_str(), "ttyACM", 6) == 0) {
            if (is_teensy_sysfs(dev_name)) return p;
        }
#endif
    }
    return "";
}
