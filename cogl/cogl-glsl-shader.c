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
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Authors:
 *   Robert Bragg <robert@linux.intel.com>
 *   Neil Roberts <neil@linux.intel.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "cogl-context-private.h"
#include "cogl-util-gl-private.h"
#include "cogl-glsl-shader-private.h"
#include "cogl-glsl-shader-boilerplate.h"

#include <string.h>

#include <glib.h>

void
_cogl_glsl_shader_set_source_with_boilerplate (CoglContext *ctx,
                                               const char *version_string,
                                               GLuint shader_gl_handle,
                                               GLenum shader_gl_type,
                                               GLsizei count_in,
                                               const char **strings_in,
                                               const GLint *lengths_in)
{
  const char *vertex_boilerplate;
  const char *fragment_boilerplate;

  const char **strings = g_alloca (sizeof (char *) * (count_in + 4));
  GLint *lengths = g_alloca (sizeof (GLint) * (count_in + 4));
  int count = 0;
  char *tex_coord_declarations = NULL;

  vertex_boilerplate = _COGL_VERTEX_SHADER_BOILERPLATE;
  fragment_boilerplate = _COGL_FRAGMENT_SHADER_BOILERPLATE;

  if (version_string)
    {
      strings[count] = version_string;
      lengths[count++] = -1;
    }

  if (ctx->driver == COGL_DRIVER_GLES2 &&
      cogl_has_feature (ctx, COGL_FEATURE_ID_TEXTURE_3D))
    {
      static const char texture_3d_extension[] =
        "#extension GL_OES_texture_3D : enable\n";
      strings[count] = texture_3d_extension;
      lengths[count++] = sizeof (texture_3d_extension) - 1;
    }

  if (shader_gl_type == GL_VERTEX_SHADER)
    {
      strings[count] = vertex_boilerplate;
      lengths[count++] = strlen (vertex_boilerplate);
    }
  else if (shader_gl_type == GL_FRAGMENT_SHADER)
    {
      strings[count] = fragment_boilerplate;
      lengths[count++] = strlen (fragment_boilerplate);
    }

  memcpy (strings + count, strings_in, sizeof (char *) * count_in);
  if (lengths_in)
    memcpy (lengths + count, lengths_in, sizeof (GLint) * count_in);
  else
    {
      int i;

      for (i = 0; i < count_in; i++)
        lengths[count + i] = -1; /* null terminated */
    }
  count += count_in;

  if (G_UNLIKELY (COGL_DEBUG_ENABLED (COGL_DEBUG_SHOW_SOURCE)))
    {
      GString *buf = g_string_new (NULL);
      int i;

      g_string_append_printf (buf,
                              "%s shader:\n",
                              shader_gl_type == GL_VERTEX_SHADER ?
                              "vertex" : "fragment");
      for (i = 0; i < count; i++)
        if (lengths[i] != -1)
          g_string_append_len (buf, strings[i], lengths[i]);
        else
          g_string_append (buf, strings[i]);

      g_message ("%s", buf->str);

      g_string_free (buf, TRUE);
    }

  GE( ctx, glShaderSource (shader_gl_handle, count,
                           (const char **) strings, lengths) );

  g_free (tex_coord_declarations);
}
