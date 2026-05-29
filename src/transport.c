#include "transport.h"

#include "server.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <libubox/list.h>
#include <libubox/uloop.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct write_queue_entry_st
{
    struct list_head list;
    char * buf;
    size_t len;
    size_t pos;
} write_queue_entry_st;

static void
check_exit_condition(rpc_server_st * svr)
{
    if (svr->eof_reached && list_empty(&svr->write_queue) && list_empty(&svr->tool_queue.tasks_active.list))
    {
        uloop_end();
    }
}

static void
write_queue_cb(struct uloop_fd * u, unsigned int events)
{
    UNUSED_PARAM(events);
    rpc_server_st * const svr = container_of(u, rpc_server_st, out_uloop_fd);

    while (!list_empty(&svr->write_queue))
    {
        write_queue_entry_st * entry = list_first_entry(&svr->write_queue, write_queue_entry_st, list);

        ssize_t const bytes_written = write(u->fd, entry->buf + entry->pos, entry->len - entry->pos);

        if (bytes_written < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                return;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                perror("write_queue_cb: write failed");
                uloop_end();
                return;
            }
        }

        entry->pos += bytes_written;
        if (entry->pos == entry->len)
        {
            list_del(&entry->list);
            free(entry->buf);
            free(entry);
        }
    }

    uloop_fd_delete(&svr->out_uloop_fd);
    check_exit_condition(svr);
}

static bool
append_to_buffer(rpc_server_st * svr, char const * data, size_t data_len)
{
    size_t needed = svr->buf_len + data_len;
    if (needed > svr->buf_cap)
    {
        size_t new_cap = svr->buf_cap == 0 ? 4096 : svr->buf_cap * 2;
        while (new_cap < needed)
        {
            new_cap *= 2;
        }
        char * new_buf = realloc(svr->buf, new_cap);
        if (!new_buf)
        {
            perror("realloc");
            return false;
        }
        svr->buf = new_buf;
        svr->buf_cap = new_cap;
    }
    memcpy(svr->buf + svr->buf_len, data, data_len);
    svr->buf_len = needed;
    svr->buf[svr->buf_len] = '\0';
    return true;
}

typedef enum
{
    READ_RESULT_SUCCESS,
    READ_RESULT_ERROR,
    READ_RESULT_NEED_MORE_DATA,
} read_result_t;

static read_result_t
read_header(rpc_server_st * svr)
{
    char * content_len_str = strstr(svr->buf, "Content-Length: ");

    if (content_len_str == NULL)
    {
        if (strstr(svr->buf, "\r\n\r\n"))
        {
            fprintf(stderr, "Error: missing Content-Length header\n");
            return READ_RESULT_ERROR;
        }
        return READ_RESULT_NEED_MORE_DATA;
    }

    if (sscanf(content_len_str, "Content-Length: %d", &svr->content_length) != 1 || svr->content_length < 0)
    {
        fprintf(stderr, "Error: invalid Content-Length\n");
        return READ_RESULT_ERROR;
    }

    char * header_end = strstr(svr->buf, "\r\n\r\n");
    if (header_end == NULL)
    {
        return READ_RESULT_NEED_MORE_DATA;
    }

    size_t const header_consumed = (size_t)(header_end - svr->buf) + 4;
    size_t const remaining = svr->buf_len - header_consumed;
    memmove(svr->buf, svr->buf + header_consumed, remaining);
    svr->buf_len = remaining;
    svr->buf[svr->buf_len] = '\0';
    svr->in_header = false;

    return READ_RESULT_SUCCESS;
}

static read_result_t
read_body(rpc_server_st * svr)
{
    if (svr->buf_len < (size_t)svr->content_length)
    {
        return READ_RESULT_NEED_MORE_DATA;
    }

    char saved = svr->buf[svr->content_length];
    svr->buf[svr->content_length] = '\0';

    if (svr->on_transport_msg != NULL)
    {
        svr->on_transport_msg(svr->buf, (size_t)svr->content_length, svr->on_transport_msg_data);
    }

    svr->buf[svr->content_length] = saved;

    size_t const remaining = svr->buf_len - (size_t)svr->content_length;
    memmove(svr->buf, svr->buf + svr->content_length, remaining);
    svr->buf_len = remaining;
    svr->buf[svr->buf_len] = '\0';
    svr->content_length = -1;
    svr->in_header = true;

    return READ_RESULT_SUCCESS;
}

static void
stdin_cb(struct uloop_fd * u, unsigned int events)
{
    UNUSED_PARAM(events);
    rpc_server_st * const svr = container_of(u, rpc_server_st, stdin_fd);

    if (u->eof)
    {
        svr->eof_reached = true;
        uloop_fd_delete(u);
        check_exit_condition(svr);
        return;
    }

    if (u->error)
    {
        uloop_fd_delete(u);
        uloop_end();
        return;
    }

    char tmp[4096];
    ssize_t n = read(u->fd, tmp, sizeof(tmp));

    if (n <= 0)
    {
        if (n == 0)
        {
            svr->eof_reached = true;
            uloop_fd_delete(u);
            check_exit_condition(svr);
        }
        else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        {
            uloop_fd_delete(u);
            uloop_end();
        }
        return;
    }

    if (!append_to_buffer(svr, tmp, (size_t)n))
    {
        uloop_end();
        return;
    }

    read_result_t read_result;
    do
    {
        if (svr->in_header)
        {
            read_result = read_header(svr);
        }
        else
        {
            read_result = read_body(svr);
        }

        if (read_result == READ_RESULT_ERROR)
        {
            uloop_end();
            return;
        }
        else if (read_result == READ_RESULT_NEED_MORE_DATA)
        {
            break;
        }
    } while (read_result == READ_RESULT_SUCCESS);
}

void
transport_init(rpc_server_st * svr)
{
    int flags = fcntl(svr->out_fd, F_GETFL, 0);
    fcntl(svr->out_fd, F_SETFL, flags | O_NONBLOCK);

    INIT_LIST_HEAD(&svr->write_queue);
    svr->in_header = true;
    svr->content_length = -1;

    svr->stdin_fd.fd = svr->in_fd;
    svr->stdin_fd.cb = stdin_cb;
    uloop_fd_add(&svr->stdin_fd, ULOOP_READ);

    svr->out_uloop_fd.fd = svr->out_fd;
    svr->out_uloop_fd.cb = write_queue_cb;
}

void
transport_cleanup(rpc_server_st * svr)
{
    free(svr->buf);
    svr->buf = NULL;
    svr->buf_len = 0;
    svr->buf_cap = 0;
}

void
transport_send(rpc_server_st * svr, char const * data, size_t len)
{
    write_queue_entry_st * entry = malloc(sizeof(*entry));
    entry->len = len;
    entry->buf = malloc(len);
    memcpy(entry->buf, data, len);
    entry->pos = 0;

    bool const was_empty = list_empty(&svr->write_queue);
    list_add_tail(&entry->list, &svr->write_queue);

    if (was_empty)
    {
        uloop_fd_add(&svr->out_uloop_fd, ULOOP_WRITE);
        write_queue_cb(&svr->out_uloop_fd, ULOOP_WRITE);
    }
}

void
transport_close_stdin(rpc_server_st * svr)
{
    uloop_fd_delete(&svr->stdin_fd);
    svr->eof_reached = true;
}

bool
transport_can_exit(rpc_server_st * svr)
{
    return svr->eof_reached && list_empty(&svr->write_queue) && list_empty(&svr->tool_queue.tasks_active.list);
}
