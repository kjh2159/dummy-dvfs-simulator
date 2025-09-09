# simulator: into executables
add_executable(cpu_burner cpu_burner.cpp)
add_executable(dummy_test dummy_test.cpp)
add_executable(thermo_jolt thermo_jolt.cpp)

# src header include path injection
target_link_libraries(cpu_burner PRIVATE project_headers)
target_link_libraries(dummy_test PRIVATE project_headers)
target_link_libraries(thermo_jolt PRIVATE project_headers)

# src에 .cpp가 있어 project_core를 만들었다면 링크 – 주석 해제해서 사용
# if(TARGET project_core)
#     target_link_libraries(ver1 PRIVATE project_core)
#     target_link_libraries(ver2 PRIVATE project_core)
# endif()


# limit optimization for cpu_burner
# (to prevent too long compilation time)
target_compile_options(cpu_burner PRIVATE
  $<$<CONFIG:Release>:-O0 -fno-unroll-loops -fno-lto>
)
target_compile_options(thermo_jolt PRIVATE
  $<$<CONFIG:Release>:-O0 -fno-unroll-loops -fno-lto>
)