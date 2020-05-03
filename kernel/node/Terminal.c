/* Copyright © 2018-2020 N. Van Bossuyt.                                      */
/* This code is licensed under the MIT License.                               */
/* See: LICENSE.md                                                            */

#include <libsystem/assert.h>
#include <libsystem/logger.h>

#include "kernel/node/Handle.h"
#include "kernel/node/Terminal.h"

#define TERMINAL_RINGBUFFER_SIZE 1024

bool terminal_FsOperationCanRead(FsTerminal *terminal, FsHandle *handle)
{
    __unused(handle);

    if (fshandle_has_flag(handle, OPEN_MASTER))
    {
        return !ringbuffer_is_empty(terminal->slave_to_master_buffer) || !terminal->node.writers;
    }
    else
    {
        return !ringbuffer_is_empty(terminal->master_to_slave_buffer) || !terminal->node.master;
    }
}

bool terminal_FsOperationCanWrite(FsTerminal *terminal, FsHandle *handle)
{
    __unused(handle);

    if (fshandle_has_flag(handle, OPEN_MASTER))
    {
        return !ringbuffer_is_full(terminal->master_to_slave_buffer) || !terminal->node.readers;
    }
    else
    {
        return !ringbuffer_is_full(terminal->slave_to_master_buffer) || !terminal->node.master;
    }
}

Result terminal_FsOperationRead(FsTerminal *terminal, FsHandle *handle, void *buffer, size_t size, size_t *read)
{
    __unused(handle);

    if (fshandle_has_flag(handle, OPEN_MASTER))
    {
        if (!terminal->node.writers)
        {
            return ERR_STREAM_CLOSED;
        }

        *read = ringbuffer_read(terminal->slave_to_master_buffer, (char *)buffer, size);
    }
    else
    {
        if (!terminal->node.master)
        {
            return ERR_STREAM_CLOSED;
        }

        *read = ringbuffer_read(terminal->master_to_slave_buffer, (char *)buffer, size);
    }

    return SUCCESS;
}

Result terminal_FsOperationWrite(FsTerminal *terminal, FsHandle *handle, const void *buffer, size_t size, size_t *written)
{
    __unused(handle);

    if (fshandle_has_flag(handle, OPEN_MASTER))
    {
        if (!terminal->node.readers)
        {
            return ERR_STREAM_CLOSED;
        }

        *written = ringbuffer_write(terminal->master_to_slave_buffer, (const char *)buffer, size);
    }
    else
    {
        if (!terminal->node.master)
        {
            return ERR_STREAM_CLOSED;
        }

        *written = ringbuffer_write(terminal->slave_to_master_buffer, (const char *)buffer, size);
    }

    return SUCCESS;
}

Result terminal_FsOperationCall(FsTerminal *terminal, FsHandle *handle, int request, void *args)
{
    __unused(handle);

    IOCallTerminalSizeArgs *size_args = (IOCallTerminalSizeArgs *)args;

    switch (request)
    {
    case IOCALL_TERMINAL_GET_SIZE:
        size_args->width = terminal->width;
        size_args->height = terminal->height;

        return SUCCESS;

    case IOCALL_TERMINAL_SET_SIZE:
        if (size_args->width < 0 || size_args->height < 0)
        {
            return ERR_INVALID_ARGUMENT;
        }

        terminal->width = size_args->width;
        terminal->height = size_args->height;

        return SUCCESS;

    default:
        return ERR_INAPPROPRIATE_CALL_FOR_DEVICE;
    }
}

size_t terminal_FsOperationSize(FsTerminal *terminal, FsHandle *handle)
{
    __unused(handle);
    __unused(terminal);

    return TERMINAL_RINGBUFFER_SIZE;
}

void terminal_FsOperationDestroy(FsTerminal *terminal)
{
    ringbuffer_destroy(terminal->master_to_slave_buffer);
    ringbuffer_destroy(terminal->slave_to_master_buffer);
}

FsNode *terminal_create(void)
{
    FsTerminal *terminal = __create(FsTerminal);

    fsnode_init(FSNODE(terminal), FILE_TYPE_TERMINAL);

    FSNODE(terminal)->can_read = (FsOperationCanRead)terminal_FsOperationCanRead;
    FSNODE(terminal)->can_write = (FsOperationCanWrite)terminal_FsOperationCanWrite;
    FSNODE(terminal)->read = (FsOperationRead)terminal_FsOperationRead;
    FSNODE(terminal)->write = (FsOperationWrite)terminal_FsOperationWrite;
    FSNODE(terminal)->call = (FsOperationCall)terminal_FsOperationCall;
    FSNODE(terminal)->size = (FsOperationSize)terminal_FsOperationSize;
    FSNODE(terminal)->destroy = (FsOperationDestroy)terminal_FsOperationDestroy;

    terminal->width = 80;
    terminal->width = 25;

    terminal->master_to_slave_buffer = ringbuffer_create(TERMINAL_RINGBUFFER_SIZE);
    terminal->slave_to_master_buffer = ringbuffer_create(TERMINAL_RINGBUFFER_SIZE);

    return FSNODE(terminal);
}
