#pragma once

typedef struct app_state
{
    int argc;
    char ** argv;
    char ** include_paths;
    int include_paths_count;
} app_state;
