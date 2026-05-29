#include "tool_runner.h"

#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
pipe_read_cb(struct uloop_fd * u, unsigned int events)
{
    UNUSED_PARAM(events);
    tool_run_st * run = container_of(u, tool_run_st, pipe_fd);
    char buf[4096];
    ssize_t n;

    while ((n = read(u->fd, buf, sizeof(buf))) > 0)
    {
        char * new_output = realloc(run->output, run->output_len + (size_t)n + 1);
        if (!new_output)
        {
            perror("realloc");
            return;
        }
        run->output = new_output;
        memcpy(run->output + run->output_len, buf, (size_t)n);
        run->output_len += (size_t)n;
        run->output[run->output_len] = '\0';
    }

    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK))
    {
        uloop_fd_delete(u);
        close(u->fd);
        u->fd = -1;
    }
}

static void
task_complete_cb(struct runqueue * q, struct runqueue_task * t)
{
    UNUSED_PARAM(q);
    tool_run_st * run = container_of(t, tool_run_st, run_proc.task);

    /* Final drain of any remaining pipe data. */
    if (run->pipe_fd.fd != -1)
    {
        pipe_read_cb(&run->pipe_fd, ULOOP_READ);
        if (run->pipe_fd.fd != -1)
        {
            uloop_fd_delete(&run->pipe_fd);
            close(run->pipe_fd.fd);
            run->pipe_fd.fd = -1;
        }
    }

    run->running = false;

    if (run->on_complete)
    {
        run->on_complete(run, run->user_data);
    }
}

static const struct runqueue_task_type tool_task_type = {
    .run = NULL,
};

void
tool_run_init(tool_run_st * run)
{
    run->pipe_fd.fd = -1;
    run->running = false;
}

bool
tool_run_start(tool_run_st * run, struct runqueue * queue,
               char const * tool_path, char ** argv,
               char * temp_path,
               tool_on_complete on_complete, void * user_data)
{
    /* Create a pipe to capture child stdout. */
    int pipefds[2];
    if (pipe(pipefds) < 0)
    {
        perror("pipe");
        return false;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        close(pipefds[0]);
        close(pipefds[1]);
        return false;
    }

    if (pid == 0)
    {
        /* Child: redirect stdout to pipe and exec. */
        close(pipefds[0]);
        if (dup2(pipefds[1], STDOUT_FILENO) < 0)
        {
            _exit(127);
        }
        close(pipefds[1]);
        execv(tool_path, argv);
        perror("execv failed");
        _exit(127);
    }

    /* Parent: close write end, set up pipe reading. */
    close(pipefds[1]);

    int flags = fcntl(pipefds[0], F_GETFL, 0);
    fcntl(pipefds[0], F_SETFL, flags | O_NONBLOCK);

    run->pipe_fd.fd = pipefds[0];
    run->pipe_fd.cb = pipe_read_cb;
    uloop_fd_add(&run->pipe_fd, ULOOP_READ);

    run->temp_path = temp_path;
    run->on_complete = on_complete;
    run->user_data = user_data;
    run->running = true;
    run->run_proc.task.type = &tool_task_type;
    run->run_proc.task.complete = task_complete_cb;
    runqueue_process_add(queue, &run->run_proc, pid);

    return true;
}

char const *
tool_run_output(tool_run_st const * run)
{
    return run->output;
}

size_t
tool_run_output_len(tool_run_st const * run)
{
    return run->output_len;
}


