# Plan: Move librpc2 to external library

## Part A — librpc2: Public/Private Header Split

### Public headers (`include/rpc2/`) — keep but trim

| Header | Changes needed |
|--------|---------------|
| `rpc2.h` | None (umbrella, stays as-is) |
| `rpc2_framing.h` | None (struct is a vtable interface, impl structs in .c) |
| `rpc2_process.h` | None (kept public — user allocates/embeds this struct) |
| `rpc2_timer.h` | None (kept public — user allocates/embeds this struct) |
| `rpc2_ctx.h` | Remove includes of `rpc2_framing.h` and `rpc2_request.h` (replace with forward declarations). Remove `<libubox/list.h>`, `<libubox/runqueue.h>`, `<libubox/uloop.h>`. Remove the "Internal" section (4 functions). |
| `rpc2_request.h` | Move `struct rpc_request` definition → private header, keep forward decl. Move internal function declarations → private header. Keep public accessors + response helpers. |

### New private headers (`src/`)

| File | Contents |
|------|----------|
| `src/rpc2_ctx_p.h` | `rpc_ctx_send_json()`, `rpc_ctx_send_error()`, `rpc_ctx_process_add()`, `rpc_ctx_get_queue()` |
| `src/rpc2_request_p.h` | Full `struct rpc_request` + `rpc_request_new()`, `rpc_request_free()`, `rpc_request_add_pending()`, `rpc_request_cancel_pending()` |

### Source file include updates

| File | Change |
|------|--------|
| `rpc2_ctx.c` | Add `#include "rpc2_request_p.h"` |
| `rpc2_request.c` | Change `#include "rpc2_ctx.h"` → `#include "rpc2_ctx_p.h"` |
| `rpc2_process.c` | Change `#include "rpc2_ctx.h"` → `#include "rpc2_ctx_p.h"` |
| `rpc2_timer.c` | No change |
| `rpc2_framing.c` | No change |

### CMakeLists.txt (librpc2)

Add `src/` as PRIVATE include dir:
```cmake
target_include_directories(rpc2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
```

## Part B — lsp_experiment: Use External librpc2

### Top-level CMakeLists.txt

Replace `find_package`/`find_library` with FetchContent pattern:

```cmake
include(FetchContent)
set(LOCAL_LIBRPC2_PATH "" CACHE PATH "Path to local librpc2 repository")
if(DEFINED LOCAL_LIBRPC2_PATH AND EXISTS ${LOCAL_LIBRPC2_PATH})
  add_subdirectory(${LOCAL_LIBRPC2_PATH} librpc2-build)
else()
  FetchContent_Declare(librpc2 GIT_REPOSITORY "git@github.com:ChrisNisbet01/librpc2.git" GIT_TAG "master")
  FetchContent_MakeAvailable(librpc2)
endif()
```

### src/CMakeLists.txt

Remove `LIBRPC2_SOURCES` list and `add_library(rpc2 STATIC ...)` block.
Keep `add_executable(${APP} ${APP_SOURCES})` and `target_link_libraries(${APP} PRIVATE rpc2)`.

### Files to delete from lsp_experiment

**In-tree librpc2 duplicates (11):**
- `src/rpc2.h`
- `src/rpc2_ctx.c` / `src/rpc2_ctx.h`
- `src/rpc2_request.c` / `src/rpc2_request.h`
- `src/rpc2_process.c` / `src/rpc2_process.h`
- `src/rpc2_timer.c` / `src/rpc2_timer.h`
- `src/rpc2_framing.c` / `src/rpc2_framing.h`

**Orphaned dead code (8):**
- `src/framing.c` / `src/framing.h`
- `src/rpc.c` / `src/rpc.h`
- `src/transport.c` / `src/transport.h`
- `src/tool_runner.c` / `src/tool_runner.h`

**Empty directory:**
- `includes/rpc2/`

## Execution Order

1. librpc2: Create private headers
2. librpc2: Modify public headers
3. librpc2: Update .c includes
4. librpc2: Update CMakeLists.txt
5. librpc2: Build and verify
6. lsp_experiment: Rewrite top-level CMakeLists.txt
7. lsp_experiment: Rewrite src/CMakeLists.txt
8. lsp_experiment: Delete obsolete files
9. lsp_experiment: Build and verify
