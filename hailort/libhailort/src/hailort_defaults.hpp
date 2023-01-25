/**
 * Copyright (c) 2020-2022 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
 **/
/**
 * @file hailort_defaults.hpp
 * @brief 
 **/
#ifndef HAILORT_DEFAULTS_HPP_
#define HAILORT_DEFAULTS_HPP_

#include <hailo/hailort.h>
#include "hailo/expected.hpp"
#include "hailo/network_group.hpp"
#include "hailo/hef.hpp"
#include "common/logger_macros.hpp"
#include "common/utils.hpp"

namespace hailort
{

constexpr hailo_format_order_t DEFAULT_FORMAT_ORDER_MAP[] = {
    // Key is device_format_order, value is default user_format_order
    HAILO_FORMAT_ORDER_AUTO,                // HAILO_FORMAT_ORDER_AUTO, - Should not be used!
    HAILO_FORMAT_ORDER_NHWC,                // HAILO_FORMAT_ORDER_NHWC,
    HAILO_FORMAT_ORDER_NHWC,                // HAILO_FORMAT_ORDER_NHCW,
    HAILO_FORMAT_ORDER_FCR,                 // HAILO_FORMAT_ORDER_FCR,
    HAILO_FORMAT_ORDER_F8CR,                // HAILO_FORMAT_ORDER_F8CR,
    HAILO_FORMAT_ORDER_NHW,                 // HAILO_FORMAT_ORDER_NHW,
    HAILO_FORMAT_ORDER_NC,                  // HAILO_FORMAT_ORDER_NC,
    HAILO_FORMAT_ORDER_BAYER_RGB,           // HAILO_FORMAT_ORDER_BAYER_RGB,
    HAILO_FORMAT_ORDER_12_BIT_BAYER_RGB,    // HAILO_FORMAT_ORDER_12_BIT_BAYER_RGB
    HAILO_FORMAT_ORDER_HAILO_NMS,           // HAILO_FORMAT_ORDER_HAILO_NMS,
    HAILO_FORMAT_ORDER_NHWC,                // HAILO_FORMAT_ORDER_RGB888,
    HAILO_FORMAT_ORDER_NCHW,                // HAILO_FORMAT_ORDER_NCHW,
    HAILO_FORMAT_ORDER_YUY2,                // HAILO_FORMAT_ORDER_YUY2,
    HAILO_FORMAT_ORDER_MAX_ENUM,            // Not used in device side - HAILO_FORMAT_ORDER_NV12,
    HAILO_FORMAT_ORDER_MAX_ENUM,            // Not used in device side - HAILO_FORMAT_ORDER_NV21,
    HAILO_FORMAT_ORDER_NV12,                // HAILO_FORMAT_ORDER_HAILO_YYUV,
    HAILO_FORMAT_ORDER_NV21,                // HAILO_FORMAT_ORDER_HAILO_YYVU,
    HAILO_FORMAT_ORDER_MAX_ENUM             // Not used in device side - HAILO_FORMAT_ORDER_RGB4
    };

constexpr hailo_format_order_t DEFAULT_FORMAT_ARGMAX_ORDER_MAP[] = {
    // Key is device_format_order, value is default user_format_order
    HAILO_FORMAT_ORDER_AUTO,                // HAILO_FORMAT_ORDER_AUTO, - Should not be used!
    HAILO_FORMAT_ORDER_NHW,                 // HAILO_FORMAT_ORDER_NHWC,
    HAILO_FORMAT_ORDER_NHW,                 // HAILO_FORMAT_ORDER_NHCW,
    HAILO_FORMAT_ORDER_NHW,                 // HAILO_FORMAT_ORDER_FCR,
    HAILO_FORMAT_ORDER_NHW,                 // HAILO_FORMAT_ORDER_F8CR,
    HAILO_FORMAT_ORDER_NHW,                 // HAILO_FORMAT_ORDER_NHW,
    HAILO_FORMAT_ORDER_NC,                  // HAILO_FORMAT_ORDER_NC,
    HAILO_FORMAT_ORDER_NHW,                 // HAILO_FORMAT_ORDER_BAYER_RGB,
    HAILO_FORMAT_ORDER_NHW,                 // HAILO_FORMAT_ORDER_12_BIT_BAYER_RGB
    HAILO_FORMAT_ORDER_HAILO_NMS,           // HAILO_FORMAT_ORDER_HAILO_NMS,
    HAILO_FORMAT_ORDER_NHW,                 // HAILO_FORMAT_ORDER_RGB888,
    HAILO_FORMAT_ORDER_NHW,                 // HAILO_FORMAT_ORDER_NCHW,
    HAILO_FORMAT_ORDER_YUY2,                // HAILO_FORMAT_ORDER_YUY2,
    HAILO_FORMAT_ORDER_MAX_ENUM,            // Not used in device side - HAILO_FORMAT_ORDER_NV12,
    HAILO_FORMAT_ORDER_MAX_ENUM,            // Not used in device side - HAILO_FORMAT_ORDER_NV21,
    HAILO_FORMAT_ORDER_NV12,                // HAILO_FORMAT_ORDER_HAILO_YYUV,
    HAILO_FORMAT_ORDER_NV21,                // HAILO_FORMAT_ORDER_HAILO_YYVU,
    HAILO_FORMAT_ORDER_MAX_ENUM             // Not used in device side - HAILO_FORMAT_ORDER_RGB4
};


#define HAILO_DEFAULT_NETWORK_NAME_QUALIFIER (std::string("/"))


class HailoRTDefaults
{
public:
    HailoRTDefaults() = delete;

    // This func must be aligned to SDK!
    static Expected<hailo_format_order_t> get_device_format_order(uint32_t compiler_format_order)
    {
        switch (compiler_format_order) {
        case 0:
            return std::move(HAILO_FORMAT_ORDER_NHWC);
            break;
        case 1:
            return std::move(HAILO_FORMAT_ORDER_NHCW);
            break;
        case 2:
            return std::move(HAILO_FORMAT_ORDER_NC);
            break;
        case 3:
            return std::move(HAILO_FORMAT_ORDER_FCR);
            break;
        case 4:
            return std::move(HAILO_FORMAT_ORDER_BAYER_RGB);
            break;
        case 5:
            return std::move(HAILO_FORMAT_ORDER_NHW);
            break;
        case 6:
            return std::move(HAILO_FORMAT_ORDER_HAILO_NMS);
            break;
        case 7:
            return std::move(HAILO_FORMAT_ORDER_F8CR);
            break;
        case 8:
            return std::move(HAILO_FORMAT_ORDER_RGB888);
            break;
        case 11:
            return std::move(HAILO_FORMAT_ORDER_YUY2);
            break;
        case 14:
            return std::move(HAILO_FORMAT_ORDER_HAILO_YYUV);
            break;
        case 15:
            return std::move(HAILO_FORMAT_ORDER_HAILO_YYVU);
            break;
        default:
            LOGGER__ERROR("Invalid compiler_format_order ({})", compiler_format_order);
            return make_unexpected(HAILO_INTERNAL_FAILURE);
        }
    }

    static constexpr hailo_format_order_t get_default_host_format_order(const hailo_format_t &device_format)
    {
        const bool is_argmax = (0 != (device_format.flags & HAILO_FORMAT_FLAGS_HOST_ARGMAX));
        if (!is_argmax) {
            return DEFAULT_FORMAT_ORDER_MAP[device_format.order];
        } else {
            return DEFAULT_FORMAT_ARGMAX_ORDER_MAP[device_format.order];
        }
    }

    static constexpr struct sockaddr_in get_sockaddr() {
        struct sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = 0;
        address.sin_addr.s_addr = INADDR_ANY;
        // sin_zero is already zeroed
        return address;
    }

    static constexpr hailo_format_t get_user_buffer_format()
    {
        return get_user_buffer_format(true, HAILO_FORMAT_TYPE_AUTO);
    }

    static constexpr hailo_format_t get_user_buffer_format(bool quantized, hailo_format_type_t format_type)
    {
        hailo_format_t user_buffer_format{};
        user_buffer_format.type = format_type;
        user_buffer_format.order = HAILO_FORMAT_ORDER_AUTO;

        hailo_format_flags_t flags = HAILO_FORMAT_FLAGS_NONE;
        if (quantized) {
            flags = static_cast<hailo_format_flags_t>(flags | HAILO_FORMAT_FLAGS_QUANTIZED);
        }

        user_buffer_format.flags = flags;
        return user_buffer_format;
    }

    static constexpr hailo_transform_params_t get_transform_params(bool quantized, hailo_format_type_t format_type)
    {
        hailo_transform_params_t params{};
        params.transform_mode = HAILO_STREAM_TRANSFORM_COPY;
        params.user_buffer_format = get_user_buffer_format(quantized, format_type);
        return params;
    }

    static constexpr hailo_transform_params_t get_transform_params()
    {
        return get_transform_params(true, HAILO_FORMAT_TYPE_AUTO);
    }

    static constexpr hailo_vstream_params_t get_vstreams_params()
    {
        return get_vstreams_params(true, HAILO_FORMAT_TYPE_AUTO);
    }

    static constexpr hailo_vstream_params_t get_vstreams_params(bool quantized, hailo_format_type_t format_type)
    {
        hailo_vstream_params_t params{};
        params.user_buffer_format = get_user_buffer_format(quantized, format_type);
        params.queue_size = HAILO_DEFAULT_VSTREAM_QUEUE_SIZE;
        params.timeout_ms = HAILO_DEFAULT_VSTREAM_TIMEOUT_MS;
        params.vstream_stats_flags = HAILO_VSTREAM_STATS_NONE;
        params.pipeline_elements_stats_flags = HAILO_PIPELINE_ELEM_STATS_NONE;
        return params;
    }

    static constexpr hailo_transform_params_t get_transform_params(const hailo_stream_info_t &stream_info)
    {
        hailo_transform_params_t params{};
        params.transform_mode = HAILO_STREAM_TRANSFORM_COPY;
        params.user_buffer_format.type = stream_info.format.type;
        params.user_buffer_format.order = get_default_host_format_order(stream_info.format);
        params.user_buffer_format.flags = static_cast<hailo_format_flags_t>(
            HAILO_FORMAT_FLAGS_QUANTIZED &
            ~HAILO_FORMAT_FLAGS_TRANSPOSED);
        return params;
    }

    static constexpr hailo_eth_input_stream_params_t get_eth_input_stream_params() {
        hailo_eth_input_stream_params_t params{};
        params.host_address = get_sockaddr();
        params.device_port = HAILO_DEFAULT_ETH_DEVICE_PORT;
        params.max_payload_size = HAILO_DEFAULT_ETH_MAX_PAYLOAD_SIZE;
        params.is_sync_enabled = false;
        params.frames_per_sync = 0;
        params.rate_limit_bytes_per_sec = 0;
        params.buffers_threshold = HAILO_DEFAULT_BUFFERS_THRESHOLD;
        return params;
    }

    static constexpr hailo_eth_output_stream_params_t get_eth_output_stream_params() {
        hailo_eth_output_stream_params_t params{};
        params.host_address = get_sockaddr();
        params.device_port = HAILO_DEFAULT_ETH_DEVICE_PORT;
        params.max_payload_size = HAILO_DEFAULT_ETH_MAX_PAYLOAD_SIZE;
        params.is_sync_enabled = true;
        return params;
    }


    static constexpr hailo_pcie_input_stream_params_t get_pcie_input_stream_params() {
        hailo_pcie_input_stream_params_t params{};
        return params;
    }

    static constexpr hailo_pcie_output_stream_params_t get_pcie_output_stream_params() {
        hailo_pcie_output_stream_params_t params{};
        return params;
    }

    static constexpr hailo_core_input_stream_params_t get_core_input_stream_params() {
        hailo_core_input_stream_params_t params{};
        return params;
    }

    static constexpr hailo_core_output_stream_params_t get_core_output_stream_params() {
        hailo_core_output_stream_params_t params{};
        return params;
    }

    static constexpr hailo_mipi_input_stream_params_t get_mipi_input_stream_params()
    {
        hailo_mipi_input_stream_params_t params = {};

        params.mipi_rx_id = 0;
        params.data_type = HAILO_MIPI_RX_TYPE_RAW_8;

        params.mipi_common_params.img_width_pixels = 1920;
        params.mipi_common_params.img_height_pixels = 1080;
        params.mipi_common_params.pixels_per_clock = HAILO_MIPI_PIXELS_PER_CLOCK_4;
        params.mipi_common_params.number_of_lanes = 2;
        params.mipi_common_params.clock_selection = HAILO_MIPI_CLOCK_SELECTION_AUTOMATIC;
        params.mipi_common_params.data_rate = 260;
        params.mipi_common_params.virtual_channel_index = 0;

        params.isp_enable = false;
        params.isp_params.isp_img_in_order = HAILO_MIPI_ISP_IMG_IN_ORDER_GR_FIRST;
        params.isp_params.isp_img_out_data_type = HAILO_MIPI_IMG_OUT_DATA_TYPE_RGB_888;
        params.isp_params.isp_crop_enable = false;
        params.isp_params.isp_crop_output_width_pixels = 1920;
        params.isp_params.isp_crop_output_height_pixels = 1080;
        params.isp_params.isp_crop_output_width_start_offset_pixels = 0;
        params.isp_params.isp_crop_output_height_start_offset_pixels = 0;
        params.isp_params.isp_test_pattern_enable = true;
        params.isp_params.isp_configuration_bypass = false;
        params.isp_params.isp_run_time_ae_enable = true;
        params.isp_params.isp_run_time_awb_enable = true;
        params.isp_params.isp_run_time_adt_enable = true;
        params.isp_params.isp_run_time_af_enable = false;
        params.isp_params.isp_run_time_calculations_interval_ms = 0;
        params.isp_params.isp_light_frequency = HAILO_MIPI_ISP_LIGHT_FREQUENCY_50HZ;

        return params;
    }

    static Expected<hailo_stream_parameters_t> get_stream_parameters(hailo_stream_interface_t interface,
            hailo_stream_direction_t direction)
    {
        hailo_stream_parameters_t params = {};
        params.stream_interface = interface;
        params.direction = direction;
        switch (params.stream_interface) {
        case HAILO_STREAM_INTERFACE_PCIE:
            if (HAILO_H2D_STREAM == direction) {
                params.pcie_input_params = get_pcie_input_stream_params();
            } else {
                params.pcie_output_params = get_pcie_output_stream_params();
            }
            break;
        case HAILO_STREAM_INTERFACE_CORE:
            if (HAILO_H2D_STREAM == direction) {
                params.core_input_params = get_core_input_stream_params();
            } else {
                params.core_output_params = get_core_output_stream_params();
            }
            break;
        case HAILO_STREAM_INTERFACE_ETH:
            if (HAILO_H2D_STREAM == direction) {
                params.eth_input_params = get_eth_input_stream_params();
            } else {
                params.eth_output_params = get_eth_output_stream_params();
            }
            break;
        default:
            LOGGER__ERROR("Invalid stream interface");
            return make_unexpected(HAILO_INVALID_ARGUMENT);
        }
        return params;
    }

    static hailo_activate_network_group_params_t get_network_group_params()
    {
        hailo_activate_network_group_params_t params = {};
        return params;
    }

    static ConfigureNetworkParams get_configure_params(uint16_t batch_size = HAILO_DEFAULT_BATCH_SIZE,
        hailo_power_mode_t power_mode = HAILO_POWER_MODE_PERFORMANCE)
    {
        ConfigureNetworkParams params = {};
        params.batch_size = batch_size;
        if (std::getenv("FORCE_POWER_MODE_ULTRA_PERFORMANCE") != nullptr) {
            power_mode = HAILO_POWER_MODE_ULTRA_PERFORMANCE;
        }
        params.power_mode = power_mode;
        params.latency = HAILO_LATENCY_NONE;
        return params;
    }

    static hailo_network_parameters_t get_network_parameters(uint16_t batch_size = HAILO_DEFAULT_BATCH_SIZE)
    {
        hailo_network_parameters_t params = {};
        params.batch_size = batch_size;

        return params;
    }    

    static std::string get_network_name(const std::string &net_group_name)
    {
        std::string default_network_name = net_group_name + 
            HAILO_DEFAULT_NETWORK_NAME_QUALIFIER + 
            net_group_name;

        return default_network_name;
    }

    static hailo_format_t expand_auto_format(const hailo_format_t &host_format, const hailo_format_t &hw_format)
    {
        auto host_format_copy = host_format;
        if (HAILO_FORMAT_TYPE_AUTO == host_format_copy.type) {
            host_format_copy.type = hw_format.type;
        }
        if (HAILO_FORMAT_ORDER_AUTO == host_format_copy.order) {
            host_format_copy.order = get_default_host_format_order(hw_format);
        }
        return host_format_copy;
    }

    static hailo_vdevice_params_t get_vdevice_params()
    {
        hailo_vdevice_params_t params = {};
        params.device_count = HAILO_DEFAULT_DEVICE_COUNT;
        params.scheduling_algorithm = HAILO_SCHEDULING_ALGORITHM_ROUND_ROBIN;
        params.device_ids = nullptr;
        params.group_id = HAILO_DEFAULT_VDEVICE_GROUP_ID;
        params.multi_process_service = false;
        return params;
    }
};

} /* namespace hailort */

#endif /* HAILORT_DEFAULTS_HPP_ */
