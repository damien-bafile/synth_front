#include "usb_detect.h"

#ifdef _WIN32
#include <windows.h>
#include <setupapi.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

std::vector<std::string> find_serial_ports() {
    std::vector<std::string> ports;

    HKEY key;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM",
            0, KEY_READ, &key) != ERROR_SUCCESS) return ports;

    char name[256];
    char value[64];
    DWORD name_size, value_size, type;

    for (DWORD i = 0; ; i++) {
        name_size = sizeof(name);
        value_size = sizeof(value);
        if (RegEnumValueA(key, i, name, &name_size, nullptr, &type,
                reinterpret_cast<LPBYTE>(value), &value_size) != ERROR_SUCCESS) break;
        if (type == REG_SZ) ports.push_back(value);
    }

    RegCloseKey(key);
    return ports;
}

std::string find_teensy_port() {
    HDEVINFO dev_info = SetupDiGetClassDevsA(nullptr, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (dev_info == INVALID_HANDLE_VALUE) return "";

    SP_DEVINFO_DATA dev_data = {};
    dev_data.cbSize = sizeof(dev_data);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(dev_info, i, &dev_data); i++) {
        char hw_id[256] = {};
        if (!SetupDiGetDeviceRegistryPropertyA(dev_info, &dev_data,
                SPDRP_HARDWAREID, nullptr, reinterpret_cast<PBYTE>(hw_id), sizeof(hw_id), nullptr))
            continue;

        if (!strstr(hw_id, "VID_16C0"))
            continue;

        char friendly[256] = {};
        if (!SetupDiGetDeviceRegistryPropertyA(dev_info, &dev_data,
                SPDRP_FRIENDLYNAME, nullptr, reinterpret_cast<PBYTE>(friendly), sizeof(friendly), nullptr))
            continue;

        const char* open_paren = strrchr(friendly, '(');
        const char* close_paren = open_paren ? strchr(open_paren, ')') : nullptr;
        if (!open_paren || !close_paren) continue;

        std::string port(open_paren + 1, close_paren - open_paren - 1);
        SetupDiDestroyDeviceInfoList(dev_info);
        return port;
    }

    SetupDiDestroyDeviceInfoList(dev_info);
    return "";
}

#else

#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>

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
            if (strcmp(vendor, "16c0") == 0) {
                unsigned long pid = strtoul(product, nullptr, 16);
                if (pid == 0x0478 || (pid >= 0x0482 && pid <= 0x048f))
                    found = true;
            }
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

#endif
