#pragma once

#include <rpc2/rpc2.h>

typedef struct app_state app_state;

void rpc_server_register_handlers(struct rpc_ctx * ctx, app_state * state);
