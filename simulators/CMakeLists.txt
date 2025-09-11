# simulator/CMakeLists.txt

# helper function to make simulator executables
function(make_sim exe_name)
    add_executable(${exe_name} ${exe_name}.cpp)

    # header directories
    target_link_libraries(${exe_name} PRIVATE project_headers)

    # link to core libraries written in src/*.cpp
    if(TARGET project_core)
        target_link_libraries(${exe_name} PRIVATE project_core)
    endif()

    target_compile_features(${exe_name} PRIVATE cxx_std_17)
endfunction()

# make simulator executables
make_sim(cpu_burner)
make_sim(dummy_test)
make_sim(thermo_jolt)


# limit optimization for cpu_burner
# (to prevent too long compilation time)
target_compile_options(cpu_burner PRIVATE
  $<$<CONFIG:Release>:-O0 -fno-unroll-loops -fno-lto>
)
target_compile_options(thermo_jolt PRIVATE
  $<$<CONFIG:Release>:-O0 -fno-unroll-loops -fno-lto>
)