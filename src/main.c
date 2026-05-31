#include "app_state.h"
#include "server.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void
usage(FILE * const out_fp, char const * const exe_name)
{
    fprintf(out_fp, "Usage: %s\n", exe_name);
}

int
main(int argc, char ** argv)
{
    int opt;

    while ((opt = getopt(argc, argv, "")) != -1)
    {
        switch (opt)
        {
        case '?':
            usage(stderr, argv[0]);
            return EXIT_FAILURE;
        }
    }

    app_state state = {
        .argc = argc,
        .argv = argv,
        .include_paths = NULL,
        .include_paths_count = 0,
    };

    run_server(STDIN_FILENO, STDOUT_FILENO, &state);

    for (int i = 0; i < state.include_paths_count; i++)
    {
        free(state.include_paths[i]);
    }
    free(state.include_paths);

    return EXIT_SUCCESS;
}
