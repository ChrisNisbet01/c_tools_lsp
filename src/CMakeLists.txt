set(APP c_tools_lsp)
set(CMAKE_C_FLAGS "-Wall -Wextra -Werror")

set(LIBRPC2_SOURCES
  rpc2_ctx.c
  rpc2_request.c
  rpc2_timer.c
  rpc2_process.c
  rpc2_framing.c
)

set(APP_SOURCES
  main.c
  server.c
  handlers.c
  documents.c
  utils.c
)

add_compile_definitions(_GNU_SOURCE)

add_library(rpc2 STATIC ${LIBRPC2_SOURCES})

target_link_libraries(rpc2
  PUBLIC
    PkgConfig::JSON_C
    ${UBOX}
)

target_compile_features(rpc2 PRIVATE c_std_11)

add_executable(${APP} ${APP_SOURCES})

target_link_libraries(${APP}
  PRIVATE
    rpc2
)

target_compile_features(${APP} PRIVATE c_std_11)
