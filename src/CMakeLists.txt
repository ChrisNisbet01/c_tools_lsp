set(APP c_tools_lsp)
set(CMAKE_C_FLAGS "-Wall -Wextra -Werror")

set(APP_SOURCES
  main.c
  server.c
  handlers.c
  documents.c
  utils.c
)

add_compile_definitions(_GNU_SOURCE)

add_executable(${APP} ${APP_SOURCES})

target_link_libraries(${APP}
  PRIVATE
    rpc2
)

target_compile_features(${APP} PRIVATE c_std_11)

set(CYCLOMATIC_COMPLEXITY_PATH_DEFAULT "/usr/local/bin/cyclomatic_complexity"
    CACHE STRING "Default path to the cyclomatic_complexity executable")
target_compile_definitions(${APP} PRIVATE
    CYCLOMATIC_COMPLEXITY_PATH_DEFAULT="${CYCLOMATIC_COMPLEXITY_PATH_DEFAULT}")

install(TARGETS ${APP} RUNTIME DESTINATION bin)
