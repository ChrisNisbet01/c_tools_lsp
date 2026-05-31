#include "handlers.h"

#include "app_state.h"
#include "documents.h"
#include "rpc2.h"
#include "utils.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CYCLOMATIC_COMPLEXITY_PATH                                                                                     \
    "/home/chris/projects/c_tools/cyclomatic_complexity/"                                                              \
    "build/cyclomatic_complexity"

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

static bool
handle_include_paths(app_state * state, struct json_object * init_opts)
{
    struct json_object * include_paths_arr = NULL;

    if (!json_object_object_get_ex(init_opts, "includePaths", &include_paths_arr)
        || !json_object_is_type(include_paths_arr, json_type_array))
    {
        fprintf(stderr, "[LSP] initialize: no includePaths in initializationOptions\n");
        return true;
    }

    int const count = json_object_array_length(include_paths_arr);

    fprintf(stderr, "[LSP] initialize: received %d include paths\n", count);

    if (count == 0)
    {
        return true;
    }

    state->include_paths = calloc((size_t)count, sizeof(*state->include_paths));
    if (!state->include_paths)
    {
        return true;
    }

    state->include_paths_count = count;
    for (int i = 0; i < count; i++)
    {
        struct json_object * elem = json_object_array_get_idx(include_paths_arr, i);

        if (json_object_is_type(elem, json_type_string))
        {
            state->include_paths[i] = strdup(json_object_get_string(elem));
            fprintf(stderr, "[LSP] initialize: include_paths[%d] = \"%s\"\n", i, state->include_paths[i]);
        }
    }

    return true;
}

static bool
handle_initialization_options(app_state * state, struct json_object * params)
{
    struct json_object * init_opts = NULL;

    if (json_object_object_get_ex(params, "initializationOptions", &init_opts))
    {
        handle_include_paths(state, init_opts);
    }
    else
    {
        fprintf(stderr, "[LSP] initialize: no initializationOptions\n");
    }

    return true;
}

static bool
handle_initialize(struct rpc_request * req)
{
    struct json_object * params = rpc_params(req);
    app_state * state = rpc_handler_data(req);

    if (params && state)
    {
        handle_initialization_options(state, params);
    }

    struct json_object * result = json_object_new_object();
    struct json_object * capabilities = json_object_new_object();

    json_object_object_add(capabilities, "textDocumentSync", json_object_new_int(1));
    json_object_object_add(result, "capabilities", capabilities);

    rpc_ok(req, result);

    return true;
}

static bool
handle_shutdown(struct rpc_request * req)
{
    rpc_ok(req, NULL);
    return true;
}

static bool
handle_exit(struct rpc_request * req)
{
    rpc_ctx * ctx = rpc_request_ctx(req);

    rpc_ctx_close_stdin(ctx);
    return true;
}

/* ── Document synchronization ──────────────────────────────────────────── */

static bool
handle_text_document_did_open(struct rpc_request * req)
{
    struct json_object * params = rpc_params(req);
    struct json_object * text_document = NULL;

    if (!json_object_object_get_ex(params, "textDocument", &text_document))
    {
        fprintf(stderr, "[LSP] Error: 'textDocument' field missing in params\n");
        return false;
    }

    struct json_object * uri_obj = NULL;
    struct json_object * text_obj = NULL;

    if (!json_object_object_get_ex(text_document, "uri", &uri_obj)
        || !json_object_object_get_ex(text_document, "text", &text_obj))
    {
        fprintf(stderr, "[LSP] Error: missing required fields in 'textDocument'\n");
        return false;
    }

    documents_update(json_object_get_string(uri_obj), json_object_get_string(text_obj));

    return true;
}

static bool
handle_text_document_did_change(struct rpc_request * req)
{
    struct json_object * params = rpc_params(req);
    struct json_object * text_document = NULL;

    if (!json_object_object_get_ex(params, "textDocument", &text_document))
    {
        return false;
    }

    struct json_object * uri_obj = NULL;

    if (!json_object_object_get_ex(text_document, "uri", &uri_obj))
    {
        return false;
    }

    struct json_object * content_changes = NULL;

    if (!json_object_object_get_ex(params, "contentChanges", &content_changes)
        || json_object_array_length(content_changes) < 1)
    {
        return false;
    }

    struct json_object * change = json_object_array_get_idx(content_changes, 0);
    struct json_object * text_obj = NULL;

    if (!json_object_object_get_ex(change, "text", &text_obj))
    {
        return false;
    }

    documents_update(json_object_get_string(uri_obj), json_object_get_string(text_obj));

    return true;
}

static bool
handle_text_document_did_close(struct rpc_request * req)
{
    struct json_object * params = rpc_params(req);
    struct json_object * text_document = NULL;

    if (!json_object_object_get_ex(params, "textDocument", &text_document))
    {
        return false;
    }

    struct json_object * uri_obj = NULL;

    if (!json_object_object_get_ex(text_document, "uri", &uri_obj))
    {
        return false;
    }

    documents_remove(json_object_get_string(uri_obj));

    return true;
}

/* ── Function Complexity feature ────────────────────────────────────────── */

typedef struct complexity_ctx_st
{
    struct rpc_request * req;
    rpc_process process;
} complexity_ctx_st;

static void
complexity_on_complete(rpc_ctx * ctx, rpc_process * proc, void * user_data)
{
    UNUSED_PARAM(ctx);
    UNUSED_PARAM(proc);
    complexity_ctx_st * comp_ctx = user_data;
    struct rpc_request * req = comp_ctx->req;
    int complexity_value = -1;
    char func_name_buf[256] = "";
    char const * output = rpc_process_output(proc);

    if (output != NULL && output[0] != '\0')
    {
        struct json_object * arr = json_tokener_parse(output);

        if (arr && json_object_is_type(arr, json_type_array))
        {
            int const len = json_object_array_length(arr);

            if (len > 0)
            {
                struct json_object * item = json_object_array_get_idx(arr, 0);
                struct json_object * comp_obj = NULL;
                struct json_object * name_obj = NULL;

                if (json_object_object_get_ex(item, "complexity", &comp_obj))
                {
                    complexity_value = json_object_get_int(comp_obj);
                }
                if (json_object_object_get_ex(item, "function_name", &name_obj))
                {
                    snprintf(func_name_buf, sizeof(func_name_buf), "%s", json_object_get_string(name_obj));
                }
            }
        }
        json_object_put(arr);
    }

    struct json_object * result = json_object_new_object();

    json_object_object_add(result, "complexity", json_object_new_int(complexity_value));
    if (func_name_buf[0])
    {
        json_object_object_add(result, "function_name", json_object_new_string(func_name_buf));
    }

    rpc_ok(req, result);

    free(proc->output);
    free(comp_ctx);
}

static bool
handle_function_complexity(struct rpc_request * req)
{
    struct json_object * params = rpc_params(req);
    struct json_object * text_document_obj = NULL;
    struct json_object * func_name_obj = NULL;
    app_state * state = rpc_handler_data(req);
    rpc_ctx * ctx = rpc_request_ctx(req);

    if (!json_object_object_get_ex(params, "textDocument", &text_document_obj)
        || !json_object_object_get_ex(params, "functionName", &func_name_obj))
    {
        fprintf(stderr, "[LSP] complexity request missing textDocument or functionName\n");
        return false;
    }

    if (!json_object_is_type(text_document_obj, json_type_object)
        || !json_object_is_type(func_name_obj, json_type_string))
    {
        fprintf(stderr, "[LSP] complexity request: invalid parameter types\n");
        return false;
    }

    struct json_object * uri_obj = NULL;

    json_object_object_get_ex(text_document_obj, "uri", &uri_obj);
    char const * uri = json_object_get_string(uri_obj);
    char const * function_name = json_object_get_string(func_name_obj);

    fprintf(
        stderr,
        "[LSP] complexity: uri=%s, function=%s\n",
        uri ? uri : "(null)",
        function_name ? function_name : "(null)"
    );

    if (!uri || !function_name)
    {
        return false;
    }

    document_st * doc = documents_lookup(uri);

    if (!doc)
    {
        fprintf(stderr, "[LSP] complexity: document not found: %s\n", uri);
        return false;
    }

    /* Write document text to a temporary file. */
    char tmp_pattern[] = "/tmp/c_tools_XXXXXX.c";
    int tmp_fd = mkstemps(tmp_pattern, 2);

    if (tmp_fd < 0)
    {
        perror("mkstemps");
        return false;
    }

    size_t text_len = strlen(doc->text);
    ssize_t written = write(tmp_fd, doc->text, text_len);

    if (written < 0 || (size_t)written != text_len)
    {
        perror("write");
        close(tmp_fd);
        unlink(tmp_pattern);
        return false;
    }
    close(tmp_fd);

    /* Build argv with include paths from LSP initialization options. */
    int const num_inc_paths = state ? state->include_paths_count : 0;
    int const argc = 1 + 2 + 1 + (num_inc_paths * 2) + 1 + 1;
    char ** argv = malloc((size_t)argc * sizeof(*argv));

    if (argv == NULL)
    {
        unlink(tmp_pattern);
        return false;
    }

    int arg_idx = 0;

    argv[arg_idx++] = "cyclomatic_complexity";
    argv[arg_idx++] = "-j";
    argv[arg_idx++] = "-f";
    argv[arg_idx++] = (char *)function_name;
    for (int i = 0; i < num_inc_paths; i++)
    {
        if (state->include_paths[i])
        {
            argv[arg_idx++] = "-I";
            argv[arg_idx++] = state->include_paths[i];
        }
    }
    argv[arg_idx++] = tmp_pattern;
    argv[arg_idx] = NULL;

    fprintf(stderr, "[LSP] complexity: command: %s", CYCLOMATIC_COMPLEXITY_PATH);
    for (int i = 1; i < argc; i++)
    {
        fprintf(stderr, " %s", argv[i]);
    }
    fprintf(stderr, "\n");

    /* Allocate context and start the tool. */
    complexity_ctx_st * comp_ctx = calloc(1, sizeof(*comp_ctx));

    if (comp_ctx == NULL)
    {
        free(argv);
        unlink(tmp_pattern);
        return false;
    }

    comp_ctx->req = req;

    rpc_process_init(&comp_ctx->process);

    if (!rpc_process_start(ctx, &comp_ctx->process, CYCLOMATIC_COMPLEXITY_PATH, argv, complexity_on_complete, comp_ctx))
    {
        free(argv);
        unlink(tmp_pattern);
        free(comp_ctx);
        return false;
    }

    free(argv);
    return true;
}

/* ── No-op handlers ─────────────────────────────────────────────────────── */

static bool
handle_initialized(struct rpc_request * req)
{
    UNUSED_PARAM(req);
    return true;
}

/* ── Registration ───────────────────────────────────────────────────────── */

void
rpc_server_register_handlers(rpc_ctx * ctx, app_state * state)
{
    rpc_add_handler(ctx, "initialize", handle_initialize, 0, NULL, state);
    rpc_add_handler(ctx, "initialized", handle_initialized, 0, NULL, NULL);
    rpc_add_handler(ctx, "shutdown", handle_shutdown, 0, NULL, NULL);
    rpc_add_handler(ctx, "exit", handle_exit, 0, NULL, NULL);
    rpc_add_handler(ctx, "textDocument/didOpen", handle_text_document_did_open, 0, NULL, NULL);
    rpc_add_handler(ctx, "textDocument/didChange", handle_text_document_did_change, 0, NULL, NULL);
    rpc_add_handler(ctx, "textDocument/didClose", handle_text_document_did_close, 0, NULL, NULL);
    rpc_add_handler(ctx, "textDocument/functionComplexity", handle_function_complexity, 0, NULL, state);
}
