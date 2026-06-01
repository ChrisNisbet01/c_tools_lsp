#pragma once

typedef struct app_state
{
    int argc;
    char ** argv;
    char ** include_paths;
    int include_paths_count;
    char * complexity_tool_path;
} app_state;
