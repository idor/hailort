cmake_minimum_required(VERSION 3.0.0)

add_subdirectory(vstreams_example)
add_subdirectory(infer_pipeline_example)
add_subdirectory(raw_streams_example)
add_subdirectory(multi_network_vstream_example)
add_subdirectory(switch_network_groups_example)
add_subdirectory(switch_network_groups_manually_example)
add_subdirectory(multi_device_example)
add_subdirectory(power_measurement_example)
add_subdirectory(multi_process_example)

add_custom_target(cpp_hailort_examples)
add_dependencies(cpp_hailort_examples
    cpp_vstreams_example
    cpp_infer_pipeline_example
    cpp_raw_streams_example
    cpp_multi_network_vstream_example
    cpp_switch_network_groups_example
    cpp_switch_network_groups_manually_example
    cpp_multi_device_example
    cpp_power_measurement_example
    cpp_multi_process_example)