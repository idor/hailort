/**
 * Copyright (c) 2020-2022 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
 **/
/**
 * @file driver_scan.cpp
 * @brief Parse pcie driver sysfs
 **/

#include "os/driver_scan.hpp"
#include <stdarg.h>
#include <dirent.h>
#include <fstream>

namespace hailort
{

#define HAILO_CLASS_PATH ("/sys/class/hailo_chardev")
#define HAILO_BOARD_LOCATION_FILENAME ("board_location")


Expected<std::vector<std::string>> list_devices()
{
    DIR *dir_iter = opendir(HAILO_CLASS_PATH);
    if (!dir_iter) {
        if (ENOENT == errno) {
            LOGGER__ERROR("Can't find hailo pcie class, this may happen if the driver is not installed (this may happen"
            " if the kernel was updated), or if there is no connected Hailo board");
            return make_unexpected(HAILO_PCIE_DRIVER_NOT_INSTALLED);
        }
        else {
            LOGGER__ERROR("Failed to open hailo pcie class ({}), errno {}", HAILO_CLASS_PATH, errno);
            return make_unexpected(HAILO_PCIE_DRIVER_FAIL);
        }
    }

    std::vector<std::string> devices;
    struct dirent *dir = nullptr;
    while ((dir = readdir(dir_iter)) != nullptr) {
        std::string device_name(dir->d_name);
        if (device_name == "." || device_name == "..") {
            continue;
        }
        devices.push_back(device_name);
    }

    closedir(dir_iter);
    return devices;
}

Expected<HailoRTDriver::DeviceInfo> query_device_info(const std::string &device_name)
{
    const std::string device_id_path = std::string(HAILO_CLASS_PATH) + "/" +
        device_name + "/" + HAILO_BOARD_LOCATION_FILENAME;
    std::ifstream device_id_file(device_id_path);
    CHECK_AS_EXPECTED(device_id_file.good(), HAILO_PCIE_DRIVER_FAIL, "Failed open {}", device_id_path);

    std::string device_id;
    std::getline(device_id_file, device_id);
    CHECK_AS_EXPECTED(device_id_file.eof(), HAILO_PCIE_DRIVER_FAIL, "Failed read {}", device_id_path);

    HailoRTDriver::DeviceInfo device_info = {};
    device_info.dev_path = std::string("/dev/") + device_name;
    device_info.device_id = device_id;

    return device_info;
}

} /* namespace hailort */
