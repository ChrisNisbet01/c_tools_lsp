#include "server.h"

#include "documents.h"
#include "handlers.h"
#include "rpc2.h"

#include <stdio.h>
#include <stdlib.h>

void
run_server(int const in_fd, int const out_fd)
{
    fprintf(stderr, "[LSP] Server starting on in_fd=%d, out_fd=%d\n", in_fd, out_fd);

    documents_init();

    rpc_ctx * ctx = rpc_ctx_new();

    if (!ctx)
    {
        fprintf(stderr, "[LSP] Error: failed to create rpc_ctx\n");
        return;
    }

    rpc_ctx_set_framing(ctx, framing_content_length_create());
    rpc_ctx_set_fds(ctx, in_fd, out_fd);

    rpc_server_register_handlers(ctx);

    rpc_ctx_run(ctx);

    rpc_ctx_destroy(ctx);
    documents_cleanup();

    if (in_fd != STDIN_FILENO)
    {
        close(in_fd);
    }
}
