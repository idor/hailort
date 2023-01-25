/**
 * Copyright (c) 2020-2022 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
 **/
/**
 * @file download_action_list_command.cpp
 * @brief Download action list command implementation
 **/

#include "download_action_list_command.hpp"
#include "common.hpp"
#include "common/file_utils.hpp"
#include "common/string_utils.hpp"

#include <iostream>
#include <iomanip>

#define MHz (1000 * 1000)
// div factor is valid only for Hailo8-B0 platform. 
// TODO - HRT-7364 - add CPU subsystem frequency into the device extended info control
// and use it for get the timer's frequency
#define NN_CORE_TO_TIMER_FREQ_FACTOR (2)
#define MERCURY_VPU_CORE_CPU_DEFAULT_FREQ_MHZ (200)

constexpr int DownloadActionListCommand::INVALID_NUMERIC_VALUE;

DownloadActionListCommand::DownloadActionListCommand(CLI::App &parent_app) :
    DeviceCommand(parent_app.add_subcommand("action-list", "Download action list data, for run time profiler"))
{
    static const char *JSON_SUFFIX = ".json";
    m_app->add_option("--output-file", m_output_file_path, "Output file path")
        ->default_val("runtime_data.json")
        ->check(FileSuffixValidator(JSON_SUFFIX));
}

hailo_status DownloadActionListCommand::execute(Device &device, const std::string &output_file_path,
    const ConfiguredNetworkGroupVector &network_groups, const std::string &hef_file_path)
{
    std::cout << "> Writing action list to '" << output_file_path << "'... ";

    auto curr_time = CliCommon::current_time_to_string();
    CHECK_EXPECTED_AS_STATUS(curr_time);

    auto chip_arch = device.get_architecture();
    CHECK_EXPECTED_AS_STATUS(chip_arch);
    unsigned int clock_cycle = 0;
    // TODO - HRT-8046 Implement extended device info for mercury
    if (HAILO_ARCH_MERCURY_VPU == chip_arch.value()) {
        clock_cycle = MERCURY_VPU_CORE_CPU_DEFAULT_FREQ_MHZ;
    } else {
        auto extended_info = device.get_extended_device_information();
        CHECK_EXPECTED_AS_STATUS(extended_info);
        clock_cycle = (extended_info->neural_network_core_clock_rate / NN_CORE_TO_TIMER_FREQ_FACTOR) / MHz;
    }

    ordered_json action_list_json = {
        {"version", ACTION_LIST_FORMAT_VERSION()},
        {"creation_time", curr_time.release()},
        {"clock_cycle_MHz", clock_cycle},
        {"hef", json({})}
    };

    if (!hef_file_path.empty()) {
        auto hef_info = parse_hef_metadata(hef_file_path);
        CHECK_EXPECTED_AS_STATUS(hef_info);
        action_list_json["hef"] = hef_info.release();
    }

    auto network_groups_list_json = parse_network_groups(device, network_groups);
    CHECK_EXPECTED_AS_STATUS(network_groups_list_json);
    action_list_json["network_groups"] = network_groups_list_json.release();

    CHECK_SUCCESS(write_json(action_list_json, output_file_path));

    std::cout << "done." << std::endl;

    return HAILO_SUCCESS;
}

hailo_status DownloadActionListCommand::set_batch_to_measure(Device &device, uint16_t batch_to_measure)
{
    return device.set_context_action_list_timestamp_batch(batch_to_measure);
}

hailo_status DownloadActionListCommand::execute_on_device(Device &device)
{
    auto status = validate_specific_device_is_given();
    CHECK_SUCCESS(status,
        "'fw-control action-list' command should get a specific device-id.");

    return execute(device, m_output_file_path);
}

Expected<ordered_json> DownloadActionListCommand::parse_hef_metadata(const std::string &hef_file_path)
{
    CHECK_AS_EXPECTED(is_valid_hef(hef_file_path), HAILO_INTERNAL_FAILURE,
        "Hef '{}' is not valid", hef_file_path);

    auto hef_md5 = calc_md5_hexdigest(hef_file_path);
    CHECK_EXPECTED(hef_md5);

    ordered_json hef_info_json = {
        {"path", hef_file_path},
        {"file_hash", hef_md5.release()}
    };
    
    return hef_info_json;
}


bool DownloadActionListCommand::is_valid_hef(const std::string &hef_file_path)
{
    // Open hef, to check that it's valid
    const auto hef = Hef::create(hef_file_path);
    return hef.has_value();
}

Expected<std::string> DownloadActionListCommand::calc_md5_hexdigest(const std::string &hef_file_path)
{
    auto hef_bin = read_binary_file(hef_file_path);
    CHECK_EXPECTED(hef_bin);

    MD5_CTX md5_ctx{};
    MD5_SUM_t md5_sum{};
    MD5_Init(&md5_ctx);
    MD5_Update(&md5_ctx, hef_bin->data(), hef_bin->size());
    MD5_Final(md5_sum, &md5_ctx);

    const bool LOWERCASE = false;
    return StringUtils::to_hex_string(md5_sum, ARRAY_ENTRIES(md5_sum), LOWERCASE);
}

hailo_status DownloadActionListCommand::write_json(const ordered_json &json_obj, const std::string &output_file_path,
    int tab_width)
{
    std::ofstream output_file(output_file_path);
    CHECK(output_file, HAILO_INTERNAL_FAILURE, "Failed opening file '{}'", output_file_path);
    
    output_file << std::setw(tab_width) << json_obj << std::endl;
    CHECK(!output_file.bad() && !output_file.fail(), HAILO_INTERNAL_FAILURE,
        "Failed writing to file '{}'", output_file_path);

    return HAILO_SUCCESS;
}

// We want to make sure that the switch-case bellow handles all of the action types in order to prevent parsing errors
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wswitch-enum"
#endif
Expected<ordered_json> DownloadActionListCommand::parse_action_data(uint32_t base_address, uint8_t *action,
    uint32_t current_buffer_offset, uint32_t *action_length, CONTEXT_SWITCH_DEFS__ACTION_TYPE_t action_type,
    uint32_t timestamp, uint8_t sub_action_index, bool sub_action_index_set, bool *is_repeated, uint8_t *num_repeated,
    CONTEXT_SWITCH_DEFS__ACTION_TYPE_t *sub_action_type)
{
    ordered_json action_json {
        {"address", base_address + current_buffer_offset},
        {"timestamp", timestamp},
        {"type", action_type}
    };

    if (sub_action_index_set) {
        action_json["sub_action_index"] = sub_action_index;
    }

    size_t action_length_local = 0;
    json data_json;
    switch (action_type) {
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_REPEATED_ACTION:
        {
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__repeated_action_header_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__repeated_action_header_t);
            const auto *repeated_header = reinterpret_cast<CONTEXT_SWITCH_DEFS__repeated_action_header_t *>(action);
            *is_repeated = true;
            *num_repeated = repeated_header->count;
            *sub_action_type = repeated_header->sub_action_type;
            break;
        }
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_LCU_INTERRUPT:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__lcu_interrupt_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__lcu_interrupt_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_SEQUENCER_DONE_INTERRUPT:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__sequencer_interrupt_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__sequencer_interrupt_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_INPUT_CHANNEL_TRANSFER_DONE_INTERRUPT:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__vdma_dataflow_interrupt_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__vdma_dataflow_interrupt_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_WAIT_FOR_DMA_IDLE_ACTION:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__wait_dma_idle_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__wait_dma_idle_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_WAIT_FOR_NMS:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__wait_nms_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__wait_nms_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_OUTPUT_CHANNEL_TRANSFER_DONE_INTERRUPT:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__vdma_dataflow_interrupt_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__vdma_dataflow_interrupt_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_MODULE_CONFIG_DONE_INTERRUPT:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__module_config_done_interrupt_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__module_config_done_interrupt_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_APPLICATION_CHANGE_INTERRUPT:
            data_json = json({});
            action_length_local = 0;
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_FETCH_CFG_CHANNEL_DESCRIPTORS:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__fetch_cfg_channel_descriptors_action_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__fetch_cfg_channel_descriptors_action_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_FETCH_CCW_BURSTS:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__fetch_ccw_bursts_action_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__fetch_ccw_bursts_action_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_TRIGGER_SEQUENCER:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__trigger_sequencer_action_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__trigger_sequencer_action_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_FETCH_DATA_FROM_VDMA_CHANNEL:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__fetch_data_action_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__fetch_data_action_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_DEACTIVATE_VDMA_CHANNEL:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__deactivate_vdma_channel_action_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__deactivate_vdma_channel_action_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_VALIDATE_VDMA_CHANNEL:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__validate_vdma_channel_action_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__validate_vdma_channel_action_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_ENABLE_LCU_DEFAULT:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__enable_lcu_action_default_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__enable_lcu_action_default_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_ENABLE_LCU_NON_DEFAULT:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__enable_lcu_action_non_default_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__enable_lcu_action_non_default_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_DISABLE_LCU:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__disable_lcu_action_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__disable_lcu_action_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_ACTIVATE_BOUNDARY_INPUT:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__activate_boundary_input_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__activate_boundary_input_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_ACTIVATE_BOUNDARY_OUTPUT:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__activate_boundary_output_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__activate_boundary_output_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_ACTIVATE_INTER_CONTEXT_INPUT:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__activate_inter_context_input_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__activate_inter_context_input_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_ACTIVATE_INTER_CONTEXT_OUTPUT:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__activate_inter_context_output_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__activate_inter_context_output_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_ACTIVATE_DDR_BUFFER_INPUT:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__activate_ddr_buffer_input_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__activate_ddr_buffer_input_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_ACTIVATE_DDR_BUFFER_OUTPUT:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__activate_ddr_buffer_output_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__activate_ddr_buffer_output_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_CHANGE_VDMA_TO_STREAM_MAPPING:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__change_vdma_to_stream_mapping_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__change_vdma_to_stream_mapping_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_ADD_DDR_PAIR_INFO:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__add_ddr_pair_info_action_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__add_ddr_pair_info_action_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_DDR_BUFFERING_START:
            data_json = json({});
            action_length_local = 0;
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_BURST_CREDITS_TASK_START:
            data_json = json({});
            action_length_local = 0;
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_ACTIVATE_CFG_CHANNEL:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__activate_cfg_channel_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__activate_cfg_channel_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_DEACTIVATE_CFG_CHANNEL:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__deactivate_cfg_channel_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__deactivate_cfg_channel_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_DDR_BUFFERING_RESET:
            data_json = json({});
            action_length_local = 0;
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_OPEN_BOUNDARY_INPUT_CHANNEL:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__open_boundary_input_channel_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__open_boundary_input_channel_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_OPEN_BOUNDARY_OUTPUT_CHANNEL:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__open_boundary_output_channel_data_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__open_boundary_output_channel_data_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_ENABLE_NMS:
            data_json = *reinterpret_cast<CONTEXT_SWITCH_DEFS__enable_nms_action_t *>(action);
            action_length_local = sizeof(CONTEXT_SWITCH_DEFS__enable_nms_action_t);
            break;
        case CONTEXT_SWITCH_DEFS__ACTION_TYPE_COUNT:
            // Fallthrough
            // Handling CONTEXT_SWITCH_DEFS__ACTION_TYPE_COUNT is needed because we compile this file with -Wswitch-enum
        default:
            std::cerr << "PARSING ERROR ! unknown action main type " << action_type << std::endl;
            return make_unexpected(HAILO_INTERNAL_FAILURE);
    }
    action_json["data"] = data_json;
    *action_length = static_cast<uint32_t>(action_length_local);
    return action_json;
}
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

Expected<ordered_json> DownloadActionListCommand::parse_single_repeated_action(uint32_t base_address,
    uint8_t *action, uint32_t current_buffer_offset, uint32_t *action_length,
    CONTEXT_SWITCH_DEFS__ACTION_TYPE_t action_type, uint32_t timestamp, uint8_t index_in_repeated_block)
{
    static const bool SET_SUB_ACTION_INDEX = true;
    return parse_action_data(base_address, action, current_buffer_offset, action_length,
        action_type, timestamp, index_in_repeated_block, SET_SUB_ACTION_INDEX);
}

Expected<ordered_json> DownloadActionListCommand::parse_single_action(uint32_t base_address,
    uint8_t *context_action_list, uint32_t current_buffer_offset, uint32_t *action_length, bool *is_repeated,
    uint8_t *num_repeated, CONTEXT_SWITCH_DEFS__ACTION_TYPE_t *sub_action_type, uint32_t *time_stamp)
{
    const auto action_length_local = sizeof(CONTEXT_SWITCH_DEFS__common_action_header_t);
    const auto *action_header = reinterpret_cast<CONTEXT_SWITCH_DEFS__common_action_header_t *>(&context_action_list[current_buffer_offset]);
    const auto time_stamp_local = CONTEXT_SWITCH_DEFS__TIMESTAMP_INIT_VALUE - action_header->time_stamp;
    current_buffer_offset += static_cast<uint32_t>(sizeof(CONTEXT_SWITCH_DEFS__common_action_header_t));

    static const bool DONT_SET_SUB_ACTION_INDEX = false;
    uint32_t action_data_length = 0;
    auto json = parse_action_data(base_address, &context_action_list[current_buffer_offset], current_buffer_offset, &action_data_length,
        action_header->action_type, time_stamp_local, 0, DONT_SET_SUB_ACTION_INDEX, is_repeated, num_repeated, sub_action_type);
    CHECK_EXPECTED(json);
    *action_length = static_cast<uint32_t>(action_length_local + action_data_length);
    *time_stamp = time_stamp_local;
    return json.release();
}

Expected<ordered_json> DownloadActionListCommand::parse_context(Device &device, uint32_t network_group_id,
    CONTROL_PROTOCOL__context_switch_context_type_t context_type, uint8_t context_index, const std::string &context_name)
{
    uint32_t action_list_base_address = 0;
    uint32_t batch_counter = 0;

    auto action_list = device.download_context_action_list(network_group_id, context_type, context_index,
        &action_list_base_address, &batch_counter);
    CHECK_EXPECTED(action_list);
    // Needs to fit in 2 bytes due to firmware limitation of action list size
    CHECK_AS_EXPECTED(IS_FIT_IN_UINT16(action_list->size()), HAILO_INTERNAL_FAILURE,
        "Action list size is expected to fit in 2B. actual size is {}", action_list->size());

    ordered_json context_json {
        {"action_list_base_address", action_list_base_address},
        {"action_list_size", action_list->size() },
        {"batch_counter", batch_counter},
        {"context_name", context_name},
    };

    ordered_json action_list_json;
    uint16_t current_buffer_offset = 0;
    while (current_buffer_offset < action_list->size()) {
        bool is_repeated = false;
        uint8_t num_repeated = 0;
        CONTEXT_SWITCH_DEFS__ACTION_TYPE_t sub_action_type = CONTEXT_SWITCH_DEFS__ACTION_TYPE_COUNT;
        uint32_t single_action_length = 0;
        uint32_t timestamp = 0;
        auto action_json = parse_single_action(action_list_base_address, action_list->data(),
            current_buffer_offset, &single_action_length, &is_repeated, &num_repeated, &sub_action_type, &timestamp);
        CHECK_EXPECTED(action_json);
        current_buffer_offset = (uint16_t)(current_buffer_offset + single_action_length);
        action_list_json.emplace_back(action_json.release());

        if (is_repeated) {
            for (uint8_t index_in_repeated_block = 0; index_in_repeated_block < num_repeated; index_in_repeated_block++) {
                uint32_t sub_action_length = 0;
                auto repeated_action_json = parse_single_repeated_action(action_list_base_address,
                    action_list->data() + current_buffer_offset, current_buffer_offset, &sub_action_length,
                    sub_action_type, timestamp, index_in_repeated_block);
                CHECK_EXPECTED(repeated_action_json);
                current_buffer_offset = (uint16_t)(current_buffer_offset + sub_action_length);
                action_list_json.emplace_back(repeated_action_json.release());
            }
        }
    }
    CHECK_AS_EXPECTED(current_buffer_offset == action_list->size(), HAILO_INTERNAL_FAILURE,
        "PARSING ERROR ! Reached forbidden memory space");

    context_json["actions"] = action_list_json;

    return context_json;
}

double DownloadActionListCommand::get_accumulator_mean_value(const AccumulatorPtr &accumulator, double default_value)
{
    auto mean_value = accumulator->mean();
    return mean_value ? mean_value.value() : default_value;
}

Expected<ordered_json> DownloadActionListCommand::parse_network_groups(Device &device, const ConfiguredNetworkGroupVector &network_groups)
{
    const auto number_of_dynamic_contexts_per_network_group = device.get_number_of_dynamic_contexts_per_network_group();
    CHECK_EXPECTED(number_of_dynamic_contexts_per_network_group);

    ordered_json network_group_list_json;
    for (uint32_t network_group_index = 0; network_group_index < number_of_dynamic_contexts_per_network_group->size(); network_group_index++) {
        // TODO: HRT-8147 use the real network_group_id instead of network_group_index
        const uint32_t network_group_id = network_group_index;

        // TODO: network_group_name via Hef::get_network_groups_names (HRT-5997)
        ordered_json network_group_json = {
            {"mean_activation_time_ms", INVALID_NUMERIC_VALUE},
            {"mean_deactivation_time_ms", INVALID_NUMERIC_VALUE},
            {"network_group_id", network_group_id},
            {"contexts", json::array()}
        };
        // We assume the the order of the network_groups in the ConfiguredNetworkGroupVector and in the action_list
        // downloaded from the fw is the same. If the received ConfiguredNetworkGroupVector is empty, we leave the 
        // mean_de/activation_time_ms with their default values (INVALID_NUMERIC_VALUE).
        if (network_groups.size() > network_group_index) {
            network_group_json["mean_activation_time_ms"] = get_accumulator_mean_value(
                network_groups[network_group_index]->get_activation_time_accumulator());
            network_group_json["mean_deactivation_time_ms"] = get_accumulator_mean_value(
                network_groups[network_group_index]->get_deactivation_time_accumulator());
        }

        auto activation_context_json = parse_context(device, network_group_id,
            CONTROL_PROTOCOL__CONTEXT_SWITCH_CONTEXT_TYPE_ACTIVATION, 0, "activation");
        CHECK_EXPECTED(activation_context_json);
        network_group_json["contexts"].emplace_back(activation_context_json.release());

        auto preliminary_context_json = parse_context(device, network_group_id,
            CONTROL_PROTOCOL__CONTEXT_SWITCH_CONTEXT_TYPE_PRELIMINARY, 0, "preliminary");
        CHECK_EXPECTED(preliminary_context_json);
        network_group_json["contexts"].emplace_back(preliminary_context_json.release());

        const auto dynamic_contexts_count = number_of_dynamic_contexts_per_network_group.value()[network_group_index];
        for (uint8_t context_index = 0; context_index < dynamic_contexts_count; context_index++) {
            auto context_json = parse_context(device, network_group_id,
                CONTROL_PROTOCOL__CONTEXT_SWITCH_CONTEXT_TYPE_DYNAMIC, context_index,
                fmt::format("dynamic_{}", context_index));
            CHECK_EXPECTED(context_json);

            network_group_json["contexts"].emplace_back(context_json.release());
        }

        auto batch_switching_context_json = parse_context(device, network_group_id,
            CONTROL_PROTOCOL__CONTEXT_SWITCH_CONTEXT_TYPE_BATCH_SWITCHING, 0, "batch_switching");
        CHECK_EXPECTED(batch_switching_context_json);
        network_group_json["contexts"].emplace_back(batch_switching_context_json.release());

        network_group_list_json.emplace_back(network_group_json);
    }

    return network_group_list_json;
}

template<typename ActionData>
static json unpack_vdma_channel_id(const ActionData &data)
{
    uint8_t engine_index = 0;
    uint8_t vdma_channel_index = 0;
    CONTEXT_SWITCH_DEFS__PACKED_VDMA_CHANNEL_ID__READ(data.packed_vdma_channel_id, engine_index, vdma_channel_index);
    return json{{"vdma_channel_index", vdma_channel_index}, {"engine_index", engine_index}};
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__deactivate_vdma_channel_action_data_t &data)
{
    j = unpack_vdma_channel_id(data);
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__validate_vdma_channel_action_data_t &data)
{
    j = unpack_vdma_channel_id(data);
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__activate_boundary_input_data_t &data)
{
    j = unpack_vdma_channel_id(data);
    j["stream_index"] = data.stream_index;
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__activate_inter_context_input_data_t &data)
{
    j = unpack_vdma_channel_id(data);
    j["stream_index"] = data.stream_index;
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__activate_ddr_buffer_input_data_t &data)
{
    j = unpack_vdma_channel_id(data);
    j["stream_index"] = data.stream_index;
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__activate_boundary_output_data_t &data)
{
    j = unpack_vdma_channel_id(data);
    j["stream_index"] = data.stream_index;
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__activate_inter_context_output_data_t &data)
{
    j = unpack_vdma_channel_id(data);
    j["stream_index"] = data.stream_index;
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__activate_ddr_buffer_output_data_t &data)
{
    j = unpack_vdma_channel_id(data);
    j["stream_index"] = data.stream_index;
}

// Needs to be backwards compatible, so we use "channel_index" instead of "vdma_channel_index".
void to_json(json& j, const CONTEXT_SWITCH_DEFS__fetch_cfg_channel_descriptors_action_data_t& data) {
    uint8_t engine_index = 0;
    uint8_t vdma_channel_index = 0;
    CONTEXT_SWITCH_DEFS__PACKED_VDMA_CHANNEL_ID__READ(data.packed_vdma_channel_id, engine_index, vdma_channel_index);
    j = json{{"descriptors_count", data.descriptors_count}, {"channel_index", vdma_channel_index},
        {"engine_index", engine_index}};
}

void to_json(json& j, const CONTEXT_SWITCH_DEFS__enable_lcu_action_non_default_data_t& data) {
    const auto cluster_index = CONTEXT_SWITCH_DEFS__PACKED_LCU_ID_CLUSTER_INDEX_READ(data.packed_lcu_id);
    const auto lcu_index = CONTEXT_SWITCH_DEFS__PACKED_LCU_ID_LCU_INDEX_READ(data.packed_lcu_id);
    j = json{{"cluster_index", cluster_index}, {"lcu_index", lcu_index}};
}

void to_json(json& j, const CONTEXT_SWITCH_DEFS__enable_lcu_action_default_data_t& data) {
    const auto cluster_index = CONTEXT_SWITCH_DEFS__PACKED_LCU_ID_CLUSTER_INDEX_READ(data.packed_lcu_id);
    const auto lcu_index = CONTEXT_SWITCH_DEFS__PACKED_LCU_ID_LCU_INDEX_READ(data.packed_lcu_id);
    j = json{{"cluster_index", cluster_index}, {"lcu_index", lcu_index}};
}

void to_json(json& j, const CONTEXT_SWITCH_DEFS__disable_lcu_action_data_t& data) {
    const auto cluster_index = CONTEXT_SWITCH_DEFS__PACKED_LCU_ID_CLUSTER_INDEX_READ(data.packed_lcu_id);
    const auto lcu_index = CONTEXT_SWITCH_DEFS__PACKED_LCU_ID_LCU_INDEX_READ(data.packed_lcu_id);
    j = json{{"cluster_index", cluster_index}, {"lcu_index", lcu_index}};
}

void to_json(json& j, const CONTEXT_SWITCH_DEFS__change_vdma_to_stream_mapping_data_t& data) {
    j = unpack_vdma_channel_id(data);
    j["stream_index"] = data.stream_index;
    j["type"] = data.is_dummy_stream ? "dummy" : "active";
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__fetch_data_action_data_t &data)
{
    j = unpack_vdma_channel_id(data);
    j["stream_index"] = data.stream_index;
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__wait_dma_idle_data_t &data)
{
    j = unpack_vdma_channel_id(data);
    j["stream_index"] = data.stream_index;
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__vdma_dataflow_interrupt_data_t &data)
{
    j = unpack_vdma_channel_id(data);
}

void to_json(json& j, const CONTEXT_SWITCH_DEFS__lcu_interrupt_data_t& data) {
    const auto cluster_index = CONTEXT_SWITCH_DEFS__PACKED_LCU_ID_CLUSTER_INDEX_READ(data.packed_lcu_id);
    const auto lcu_index = CONTEXT_SWITCH_DEFS__PACKED_LCU_ID_LCU_INDEX_READ(data.packed_lcu_id);
    j = json{{"cluster_index", cluster_index}, {"lcu_index", lcu_index}};
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__activate_cfg_channel_t &data)
{
    uint8_t engine_index = 0;
    uint8_t vdma_channel_index = 0;
    CONTEXT_SWITCH_DEFS__PACKED_VDMA_CHANNEL_ID__READ(data.packed_vdma_channel_id, engine_index, vdma_channel_index);
    j = json{{"config_stream_index", data.config_stream_index}, {"channel_index", vdma_channel_index},
        {"engine_index", engine_index}};
}
void to_json(json &j, const CONTEXT_SWITCH_DEFS__deactivate_cfg_channel_t &data)
{
    uint8_t engine_index = 0;
    uint8_t vdma_channel_index = 0;
    CONTEXT_SWITCH_DEFS__PACKED_VDMA_CHANNEL_ID__READ(data.packed_vdma_channel_id, engine_index, vdma_channel_index);
    j = json{{"config_stream_index", data.config_stream_index}, {"channel_index", vdma_channel_index},
        {"engine_index", engine_index}};
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__add_ddr_pair_info_action_data_t &data)
{
    uint8_t h2d_engine_index = 0;
    uint8_t h2d_vdma_channel_index = 0;
    uint8_t d2h_engine_index = 0;
    uint8_t d2h_vdma_channel_index = 0;

    CONTEXT_SWITCH_DEFS__PACKED_VDMA_CHANNEL_ID__READ(data.h2d_packed_vdma_channel_id, h2d_engine_index,
        h2d_vdma_channel_index);
    CONTEXT_SWITCH_DEFS__PACKED_VDMA_CHANNEL_ID__READ(data.d2h_packed_vdma_channel_id, d2h_engine_index,
        d2h_vdma_channel_index);

    j = json{{"h2d_engine_index", h2d_engine_index}, {"h2d_vdma_channel_index", h2d_vdma_channel_index},
        {"d2h_engine_index", d2h_engine_index}, {"d2h_vdma_channel_index", d2h_vdma_channel_index}};
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__open_boundary_input_channel_data_t &data)
{
    j = unpack_vdma_channel_id(data);
}

void to_json(json &j, const CONTEXT_SWITCH_DEFS__open_boundary_output_channel_data_t &data)
{
    j = unpack_vdma_channel_id(data);
}
