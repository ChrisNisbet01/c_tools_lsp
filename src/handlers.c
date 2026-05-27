#include "handlers.h"

#include "server.h"
#include "utils.h"

#include <libubox/uloop.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
queue_success_response(rpc_server_st * svr, struct json_object * id, struct json_object * result)
{
    struct json_object * res = json_object_new_object();
    json_object_object_add(res, "jsonrpc", json_object_new_string("2.0"));
    json_object_object_add(res, "result", result);
    if (id)
    {
        json_object_object_add(res, "id", json_object_get(id));
    }
    else
    {
        json_object_object_add(res, "id", NULL);
    }
    rpc_server_queue_response(svr, res);
    json_object_put(res);
}

static bool
handle_initialize(rpc_server_st * svr, struct json_object * params, struct json_object * id)
{
    (void)params;

    struct json_object * result = json_object_new_object();

    struct json_object * capabilities = json_object_new_object();
    json_object_object_add(capabilities, "textDocumentSync", json_object_new_int(1));
    json_object_object_add(result, "capabilities", capabilities);

    queue_success_response(svr, id, result);

    return true;
}

static bool
handle_shutdown(rpc_server_st * svr, struct json_object * params, struct json_object * id)
{
    (void)params;

    svr->shutdown_requested = true;

    queue_success_response(svr, id, NULL);

    return true;
}

static bool
handle_exit(rpc_server_st * svr, struct json_object * params, struct json_object * id)
{
    UNUSED_PARAM(svr);
    (void)params;
    (void)id;

    uloop_end();

    return true;
}

void
rpc_server_register_handlers(rpc_server_st * svr)
{
    rpc_server_register_method(svr, "initialize", handle_initialize);
    rpc_server_register_method(svr, "shutdown", handle_shutdown);
    rpc_server_register_method(svr, "exit", handle_exit);
}
