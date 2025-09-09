# perfetto tester: into executables
add_executable(perfetto_async perfetto_async.cpp)

# src 헤더 include 경로 주입
target_link_libraries(perfetto_async PRIVATE project_headers)

# src에 .cpp가 있어 project_core를 만들었다면 링크 – 주석 해제해서 사용
# if(TARGET project_core)
#     target_link_libraries(ver1 PRIVATE project_core)
#     target_link_libraries(ver2 PRIVATE project_core)
# endif()

# exclude_from_all for non-Debug builds
set_property(TARGET perfetto_async PROPERTY EXCLUDE_FROM_ALL $<NOT:$<CONFIG:Debug>>)