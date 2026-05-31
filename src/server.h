#pragma once

typedef struct app_state app_state;

void run_server(int in_fd, int out_fd, app_state * state);
