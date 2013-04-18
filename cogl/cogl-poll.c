/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2012 Intel Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *  Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-poll.h"
#include "cogl-poll-private.h"
#include "cogl-winsys-private.h"
#include "cogl-renderer-private.h"
#include "cogl-context-private.h"

typedef struct _CoglPollSource
{
  int fd;
  CoglPollCheckCallback check;
  CoglPollDispatchCallback dispatch;
  void *user_data;
} CoglPollSource;

int
cogl_poll_renderer_get_info (CoglRenderer *renderer,
                             CoglPollFD **poll_fds,
                             int *n_poll_fds,
                             int64_t *timeout)
{
  GList *l;

  _COGL_RETURN_VAL_IF_FAIL (cogl_is_renderer (renderer), 0);
  _COGL_RETURN_VAL_IF_FAIL (poll_fds != NULL, 0);
  _COGL_RETURN_VAL_IF_FAIL (n_poll_fds != NULL, 0);
  _COGL_RETURN_VAL_IF_FAIL (timeout != NULL, 0);

  *poll_fds = (void *)renderer->poll_fds->data;
  *n_poll_fds = renderer->poll_fds->len;

  if (!COGL_LIST_EMPTY (&renderer->idle_closures))
    {
      *timeout = 0;
      return renderer->poll_fds_age;
    }

  for (l = renderer->poll_sources; l; l = l->next)
    {
      CoglPollSource *source = l->data;
      if (source->check && source->check (source->user_data))
        {
          *timeout = 0;
          return renderer->poll_fds_age;
        }
    }

  *timeout = -1;
  return renderer->poll_fds_age;
}

void
cogl_poll_renderer_dispatch (CoglRenderer *renderer,
                             const CoglPollFD *poll_fds,
                             int n_poll_fds)
{
  GList *l;

  _COGL_RETURN_IF_FAIL (cogl_is_renderer (renderer));

  _cogl_closure_list_invoke_no_args (&renderer->idle_closures);

  for (l = renderer->poll_sources; l; l = l->next)
    {
      CoglPollSource *source = l->data;
      int i;

      for (i = 0; i < n_poll_fds; i++)
        {
          const CoglPollFD *pollfd = &poll_fds[i];

          if (pollfd->fd == source->fd)
            {
              source->dispatch (source->user_data);
              break;
            }
        }
    }
}

static int
find_pollfd (CoglRenderer *renderer, int fd)
{
  int i;

  for (i = 0; i < renderer->poll_fds->len; i++)
    {
      CoglPollFD *pollfd = &g_array_index (renderer->poll_fds, CoglPollFD, i);

      if (pollfd->fd == fd)
        return i;
    }

  return -1;
}

void
_cogl_poll_renderer_remove_fd (CoglRenderer *renderer, int fd)
{
  int i = find_pollfd (renderer, fd);
  GList *l;

  if (i < 0)
    return;

  g_array_remove_index_fast (renderer->poll_fds, i);
  renderer->poll_fds_age++;

  for (l = renderer->poll_sources; l; l = l->next)
    {
      CoglPollSource *source = l->data;
      if (source->fd == fd)
        {
          renderer->poll_sources =
            g_list_delete_link (renderer->poll_sources, l);
          g_slice_free (CoglPollSource, source);
          break;
        }
    }
}

void
_cogl_poll_renderer_add_fd (CoglRenderer *renderer,
                            int fd,
                            CoglPollFDEvent events,
                            CoglPollCheckCallback check,
                            CoglPollDispatchCallback dispatch,
                            void *user_data)
{
  CoglPollFD pollfd = {
    fd,
    events
  };
  CoglPollSource *source;

  _cogl_poll_renderer_remove_fd (renderer, fd);

  source = g_slice_new0 (CoglPollSource);
  source->fd = fd;
  source->check = check;
  source->dispatch = dispatch;
  source->user_data = user_data;

  renderer->poll_sources = g_list_prepend (renderer->poll_sources, source);

  g_array_append_val (renderer->poll_fds, pollfd);
  renderer->poll_fds_age++;
}

CoglClosure *
_cogl_poll_renderer_add_idle (CoglRenderer *renderer,
                              CoglIdleCallback idle_cb,
                              void *user_data,
                              CoglUserDataDestroyCallback destroy_cb)
{
  return _cogl_closure_list_add (&renderer->idle_closures,
                                idle_cb,
                                user_data,
                                destroy_cb);
}
