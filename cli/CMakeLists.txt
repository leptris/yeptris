# yeptris — CLI tool. Output name "yeptris"; target name kept distinct
# from the library targets.

add_executable(yeptris_cli
    main.c
    commands/version.c
    commands/registry.c
)

set_target_properties(yeptris_cli PROPERTIES OUTPUT_NAME yeptris)

target_link_libraries(yeptris_cli PRIVATE yeptris_static)
target_include_directories(yeptris_cli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
yeptris_set_warnings(yeptris_cli)
