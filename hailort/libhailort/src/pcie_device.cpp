/**
 * Copyright (c) 2020-2022 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
 **/
/**
 * @file pcie_device.cpp
 * @brief TODO: brief
 *
 * TODO: doc
 **/

#include "pcie_device.hpp"
#include "hailo/hailort.h"
#include "common/utils.hpp"
#include "hailo/device.hpp"
#include "hailo/hef.hpp"
#include "control.hpp"
#include "common/compiler_extensions_compat.hpp"
#include "os/hailort_driver.hpp"
#include "context_switch/multi_context/resource_manager.hpp"
#include "context_switch/multi_context/vdma_config_manager.hpp"

#include <new>
#include <algorithm>

namespace hailort
{

Expected<std::vector<hailo_pcie_device_info_t>> PcieDevice::scan()
{
    auto scan_results = HailoRTDriver::scan_devices();
    CHECK_EXPECTED(scan_results);

    std::vector<hailo_pcie_device_info_t> out_results;
    out_results.reserve(scan_results->size());
    for (const auto &scan_result : scan_results.value()) {
        const bool DONT_LOG_ON_FAILURE = true;
        auto device_info = parse_pcie_device_info(scan_result.device_id, DONT_LOG_ON_FAILURE);
        if (device_info) {
            out_results.emplace_back(device_info.release());
        }
    }

    return out_results;
}

Expected<std::unique_ptr<PcieDevice>> PcieDevice::create()
{
    // Take the first device
    auto scan_result = scan();
    CHECK_EXPECTED(scan_result, "Failed scanning pcie devices");
    CHECK_AS_EXPECTED(scan_result->size() == 1, HAILO_INVALID_OPERATION,
        "Expected only 1 PCIe device. Pass `hailo_pcie_device_info_t` to create a specific PCIe device");
    return create(scan_result->at(0));
}

Expected<std::unique_ptr<PcieDevice>> PcieDevice::create(const hailo_pcie_device_info_t &pcie_device_info)
{
    auto device_info = find_device_info(pcie_device_info);
    CHECK_EXPECTED(device_info);

    auto pcie_device_info_str = pcie_device_info_to_string(pcie_device_info);
    CHECK_EXPECTED(pcie_device_info_str);

    auto driver = HailoRTDriver::create(device_info->dev_path);
    CHECK_EXPECTED(driver);

    hailo_status status = HAILO_UNINITIALIZED;
    auto device = std::unique_ptr<PcieDevice>(new (std::nothrow) PcieDevice(driver.release(), pcie_device_info, status,
        pcie_device_info_str.release()));
    CHECK_AS_EXPECTED((nullptr != device), HAILO_OUT_OF_HOST_MEMORY);
    CHECK_SUCCESS_AS_EXPECTED(status, "Failed creating PcieDevice");
    return device;
}

// same format as in lspci - [<domain>].<bus>.<device>.<func> 
// domain (0 to ffff) bus (0 to ff), device (0 to 1f) and function (0 to 7).
static const char *DEVICE_ID_STRING_FMT_SHORT = "%02x:%02x.%d";
static constexpr int DEVICE_ID_ELEMENTS_COUNT_SHORT = 3;
static constexpr int DEVICE_ID_STRING_LENGTH_SHORT = 7; // Length without null terminator

static const char *DEVICE_ID_STRING_FMT_LONG = "%04x:%02x:%02x.%d";
static constexpr int DEVICE_ID_ELEMENTS_COUNT_LONG = 4;
static constexpr int DEVICE_ID_STRING_LENGTH_LONG = 12; // Length without null terminator

static constexpr int DEVICE_ID_MAX_STRING_LENGTH = std::max(DEVICE_ID_STRING_LENGTH_SHORT, DEVICE_ID_STRING_LENGTH_LONG);

Expected<hailo_pcie_device_info_t> PcieDevice::parse_pcie_device_info(const std::string &device_info_str,
    bool log_on_failure)
{
    hailo_pcie_device_info_t device_info{};
    int scanf_res = sscanf(device_info_str.c_str(), DEVICE_ID_STRING_FMT_LONG,
        &device_info.domain, &device_info.bus, &device_info.device, &device_info.func);
    if (DEVICE_ID_ELEMENTS_COUNT_LONG != scanf_res) {
        // Domain not included, trying short
        device_info.domain = HAILO_PCIE_ANY_DOMAIN;
        scanf_res = sscanf(device_info_str.c_str(), DEVICE_ID_STRING_FMT_SHORT,
            &device_info.bus, &device_info.device, &device_info.func);
        if (DEVICE_ID_ELEMENTS_COUNT_SHORT != scanf_res) {
            if (log_on_failure) {
                LOGGER__ERROR("Invalid device info string (format is [<domain>].<bus>.<device>.<func>) {}", device_info_str);
            }
            return make_unexpected(HAILO_INVALID_ARGUMENT);
        }
    }

    return device_info;
}

Expected<std::string> PcieDevice::pcie_device_info_to_string(const hailo_pcie_device_info_t &device_info)
{
    char device_string[DEVICE_ID_MAX_STRING_LENGTH + 1] = { 0 };

    if (HAILO_PCIE_ANY_DOMAIN != device_info.domain) {
        int res = snprintf(device_string, DEVICE_ID_STRING_LENGTH_LONG + 1, DEVICE_ID_STRING_FMT_LONG, 
            device_info.domain, device_info.bus, device_info.device, device_info.func);
        // If the users give invalid device_info on release, they will get an invalid string.
        CHECK_AS_EXPECTED((DEVICE_ID_STRING_LENGTH_LONG) == res, HAILO_INVALID_ARGUMENT, "Invalid device info");
    }
    else {
        int res = snprintf(device_string, DEVICE_ID_STRING_LENGTH_SHORT + 1, DEVICE_ID_STRING_FMT_SHORT, 
            device_info.bus, device_info.device, device_info.func);
        // If the users gives invalid device_info on release, they will get an invalid string.
        CHECK_AS_EXPECTED((DEVICE_ID_STRING_LENGTH_SHORT) == res, HAILO_INVALID_ARGUMENT, "Invalid device info");
    }

    return std::string(device_string);
}

PcieDevice::PcieDevice(HailoRTDriver &&driver, const hailo_pcie_device_info_t &device_info, hailo_status &status,
    const std::string &device_id) :
        VdmaDevice::VdmaDevice(std::move(driver), Device::Type::PCIE, device_id),
        m_device_info(device_info)
{
    if (driver.is_fw_loaded()) {
        status = update_fw_state();
        if (HAILO_SUCCESS != status) {
            LOGGER__ERROR("update_fw_state() failed with status {}", status);
            return;
        }
    } else {
        LOGGER__WARNING("FW is not loaded to the device. Please load FW before using the device.");
        m_is_control_version_supported = false;
    }

    m_device_id = device_id;

    status = HAILO_SUCCESS;
}

void PcieDevice::set_is_control_version_supported(bool value)
{
    m_is_control_version_supported = value;
}

Expected<hailo_device_architecture_t> PcieDevice::get_architecture() const
{
    if (!m_driver.is_fw_loaded()) {
        LOGGER__WARNING("FW is not loaded to the device. Please load FW before using the device.");
        return make_unexpected(HAILO_INVALID_OPERATION);
    }

    return Expected<hailo_device_architecture_t>(m_device_architecture);
}

hailo_status PcieDevice::direct_write_memory(uint32_t address, const void *buffer, uint32_t size)
{
    return m_driver.write_memory(HailoRTDriver::MemoryType::DIRECT_MEMORY, address, buffer, size);
}

hailo_status PcieDevice::direct_read_memory(uint32_t address, void *buffer, uint32_t size)
{
    return m_driver.read_memory(HailoRTDriver::MemoryType::DIRECT_MEMORY, address, buffer, size);
}

const char *PcieDevice::get_dev_id() const
{
    return m_device_id.c_str();
}

hailo_status PcieDevice::close_all_vdma_channels()
{
    auto status = HAILO_UNINITIALIZED;

    // TODO: Add one icotl to stop all channels at once (HRT-6097)
    constexpr uint8_t PCIE_DEFAULT_ENGINE_INDEX = 0;
    for (uint8_t channel_index = 0; channel_index <= MAX_H2D_CHANNEL_INDEX; channel_index++) {
        const vdma::ChannelId channel_id = { PCIE_DEFAULT_ENGINE_INDEX, channel_index };
        auto host_registers = VdmaChannelRegs(m_driver, channel_id, HailoRTDriver::DmaDirection::H2D);
        status = host_registers.stop_channel();
        CHECK_SUCCESS(status);

        auto device_registers = VdmaChannelRegs(m_driver, channel_id, HailoRTDriver::DmaDirection::D2H);
        status = device_registers.stop_channel();
        CHECK_SUCCESS(status);
    }

    for (uint8_t channel_index = MIN_D2H_CHANNEL_INDEX; channel_index <= MAX_D2H_CHANNEL_INDEX; channel_index++) {
        const vdma::ChannelId channel_id = { PCIE_DEFAULT_ENGINE_INDEX, channel_index };
        auto host_registers = VdmaChannelRegs(m_driver, channel_id, HailoRTDriver::DmaDirection::D2H);
        status = host_registers.stop_channel();
        CHECK_SUCCESS(status);

        auto device_registers = VdmaChannelRegs(m_driver, channel_id, HailoRTDriver::DmaDirection::H2D);
        status = device_registers.stop_channel();
        CHECK_SUCCESS(status);
    }

    return HAILO_SUCCESS;
}

hailo_status PcieDevice::reset_impl(CONTROL_PROTOCOL__reset_type_t reset_type)
{
    hailo_status status = HAILO_UNINITIALIZED;
    HAILO_COMMON_STATUS_t common_status = HAILO_COMMON_STATUS__UNINITIALIZED;
    CONTROL_PROTOCOL__request_t request = {};
    size_t request_size = 0;
    uint8_t response_buffer[RESPONSE_MAX_BUFFER_SIZE] = {};
    size_t response_size = RESPONSE_MAX_BUFFER_SIZE;
    CONTROL_PROTOCOL__response_header_t *header = NULL;
    CONTROL_PROTOCOL__payload_t *payload = NULL;
    bool is_expecting_response = true;

    CHECK(CONTROL_PROTOCOL__RESET_TYPE__CHIP != reset_type, HAILO_INVALID_OPERATION,
        "Chip reset is not supported for PCIe device.");

    if ((CONTROL_PROTOCOL__RESET_TYPE__FORCED_SOFT == reset_type) || (CONTROL_PROTOCOL__RESET_TYPE__SOFT == reset_type)) {
        is_expecting_response = false; // TODO: Check boot source, set is_expecting_response = (boot_source != pcie)
        status = close_all_vdma_channels();
        CHECK_SUCCESS(status);
    }

    common_status = CONTROL_PROTOCOL__pack_reset_request(&request, &request_size, m_control_sequence, reset_type);
    status = (HAILO_COMMON_STATUS__SUCCESS == common_status) ? HAILO_SUCCESS : HAILO_INTERNAL_FAILURE;
    CHECK_SUCCESS(status);

    LOGGER__DEBUG("Sending reset request");
    status = this->fw_interact((uint8_t*)(&request), request_size, (uint8_t*)&response_buffer, &response_size);
    // fw_interact should return failure if response is not expected
    // TODO: fix logic with respect to is_expecting_response, implement wait_for_wakeup();
    if (HAILO_SUCCESS == status) {
        status = Control::parse_and_validate_response(response_buffer, (uint32_t)(response_size), &header,
            &payload, &request);
        CHECK_SUCCESS(status);
        CHECK(is_expecting_response, HAILO_INTERNAL_FAILURE, "Recived valid response from FW for control who is not expecting one.");
    } else if ((HAILO_FW_CONTROL_FAILURE == status) && (!is_expecting_response)){
        status = HAILO_SUCCESS;
    } else {
        return status;
    }

    LOGGER__DEBUG("Board has been reset successfully");
    return HAILO_SUCCESS;
}

Expected<HailoRTDriver::DeviceInfo> PcieDevice::find_device_info(const hailo_pcie_device_info_t &pcie_device_info)
{
    auto scan_results = HailoRTDriver::scan_devices();
    CHECK_EXPECTED(scan_results);

    // Find device index based on the information from "device_info"
    for (const auto &scan_result : scan_results.value()) {
        const bool DONT_LOG_ON_FAILURE = false;
        auto scanned_info = parse_pcie_device_info(scan_result.device_id, DONT_LOG_ON_FAILURE);
        if (!scanned_info) {
            continue;
        }

        const bool match = (pcie_device_info.bus == scanned_info->bus) && 
           (pcie_device_info.device == scanned_info->device) &&
           (pcie_device_info.func == scanned_info->func) &&
           ((HAILO_PCIE_ANY_DOMAIN == pcie_device_info.domain) || (pcie_device_info.domain == scanned_info->domain));
        if (match) {
            return HailoRTDriver::DeviceInfo(scan_result);
        }
    }

    LOGGER__ERROR("Requested device not found");
    return make_unexpected(HAILO_INVALID_ARGUMENT);
}

} /* namespace hailort */
