#include "app_state.h"
#include "server.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
usage(FILE * const out_fp, char const * const exe_name)
{
    fprintf(
        out_fp,
        "Usage: %s [--complexity-tool <path>]\n"
        "  --complexity-tool <path>  Path to the cyclomatic_complexity executable\n"
        "                            (default: compiled-in CMake default)\n",
        exe_name
    );
}

int
main(int argc, char ** argv)
{
    int opt;
    int opt_index = 0;

    static struct option const long_opts[] = { { "complexity-tool", required_argument, NULL, 'c' }, { 0, 0, 0, 0 } };

    app_state state = {
        .argc = argc,
        .argv = argv,
        .include_paths = NULL,
        .include_paths_count = 0,
        .complexity_tool_path = NULL,
    };

    while ((opt = getopt_long(argc, argv, "", long_opts, &opt_index)) != -1)
    {
        switch (opt)
        {
        case 'c':
            state.complexity_tool_path = strdup(optarg);
            break;
        default:
            usage(stderr, argv[0]);
            return EXIT_FAILURE;
        }
    }

    run_server(STDIN_FILENO, STDOUT_FILENO, &state);

    free(state.complexity_tool_path);
    for (int i = 0; i < state.include_paths_count; i++)
    {
        free(state.include_paths[i]);
    }
    free(state.include_paths);

    return EXIT_SUCCESS;
}
