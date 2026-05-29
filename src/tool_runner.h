#pragma once

#include <libubox/runqueue.h>
#include <libubox/uloop.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct tool_run_st tool_run_st;

typedef void (*tool_on_complete)(tool_run_st * run, void * user_data);

struct tool_run_st
{
    struct runqueue_process run_proc;
    struct uloop_fd pipe_fd;
    char * output;
    size_t output_len;
    char * temp_path;
    tool_on_complete on_complete;
    void * user_data;
    bool running;
};

void tool_run_init(tool_run_st * run);

bool tool_run_start(tool_run_st * run, struct runqueue * queue,
                    char const * tool_path, char ** argv,
                    char * temp_path,
                    tool_on_complete on_complete, void * user_data);

char const * tool_run_output(tool_run_st const * run);
size_t tool_run_output_len(tool_run_st const * run);
