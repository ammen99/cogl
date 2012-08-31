/*
 * Cogl
 *
 * An object oriented GL/GLES Abstraction/Utility Layer
 *
 * Copyright (C) 2007,2008,2009 Intel Corporation.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "cogl-internal.h"
#include "cogl-context-private.h"
#include "cogl-feature-private.h"
#include "cogl-renderer-private.h"
#include "cogl-private.h"

static CoglBool
_cogl_driver_pixel_format_from_gl_internal (CoglContext *context,
                                            GLenum gl_int_format,
                                            CoglPixelFormat *out_format)
{
  return TRUE;
}

static CoglPixelFormat
_cogl_driver_pixel_format_to_gl (CoglContext *context,
                                 CoglPixelFormat  format,
                                 GLenum *out_glintformat,
                                 GLenum *out_glformat,
                                 GLenum *out_gltype)
{
  CoglPixelFormat required_format;
  GLenum glintformat;
  GLenum glformat = 0;
  GLenum gltype;

  required_format = format;

  /* Find GL equivalents */
  switch (format)
    {
    case COGL_PIXEL_FORMAT_A_8:
      glintformat = GL_ALPHA;
      glformat = GL_ALPHA;
      gltype = GL_UNSIGNED_BYTE;
      break;
    case COGL_PIXEL_FORMAT_G_8:
      glintformat = GL_LUMINANCE;
      glformat = GL_LUMINANCE;
      gltype = GL_UNSIGNED_BYTE;
      break;

    case COGL_PIXEL_FORMAT_BGRA_8888:
    case COGL_PIXEL_FORMAT_BGRA_8888_PRE:
      /* There is an extension to support this format */
      if ((context->private_feature_flags &
           COGL_PRIVATE_FEATURE_TEXTURE_FORMAT_BGRA8888))
        {
          /* For some reason the extension says you have to specify
             BGRA for the internal format too */
          glintformat = GL_BGRA_EXT;
          glformat = GL_BGRA_EXT;
          gltype = GL_UNSIGNED_BYTE;
          required_format = format;
          break;
        }
      /* flow through */

      /* Just one 24-bit ordering supported */
    case COGL_PIXEL_FORMAT_RGB_888:
    case COGL_PIXEL_FORMAT_BGR_888:
      glintformat = GL_RGB;
      glformat = GL_RGB;
      gltype = GL_UNSIGNED_BYTE;
      required_format = COGL_PIXEL_FORMAT_RGB_888;
      break;

      /* Just one 32-bit ordering supported */
    case COGL_PIXEL_FORMAT_RGBA_8888:
    case COGL_PIXEL_FORMAT_RGBA_8888_PRE:
    case COGL_PIXEL_FORMAT_ARGB_8888:
    case COGL_PIXEL_FORMAT_ARGB_8888_PRE:
    case COGL_PIXEL_FORMAT_ABGR_8888:
    case COGL_PIXEL_FORMAT_ABGR_8888_PRE:
    case COGL_PIXEL_FORMAT_RGBA_1010102:
    case COGL_PIXEL_FORMAT_RGBA_1010102_PRE:
    case COGL_PIXEL_FORMAT_BGRA_1010102:
    case COGL_PIXEL_FORMAT_BGRA_1010102_PRE:
    case COGL_PIXEL_FORMAT_ABGR_2101010:
    case COGL_PIXEL_FORMAT_ABGR_2101010_PRE:
    case COGL_PIXEL_FORMAT_ARGB_2101010:
    case COGL_PIXEL_FORMAT_ARGB_2101010_PRE:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_BYTE;
      required_format = COGL_PIXEL_FORMAT_RGBA_8888;
      required_format |= (format & COGL_PREMULT_BIT);
      break;

      /* The following three types of channel ordering
       * are always defined using system word byte
       * ordering (even according to GLES spec) */
    case COGL_PIXEL_FORMAT_RGB_565:
      glintformat = GL_RGB;
      glformat = GL_RGB;
      gltype = GL_UNSIGNED_SHORT_5_6_5;
      break;
    case COGL_PIXEL_FORMAT_RGBA_4444:
    case COGL_PIXEL_FORMAT_RGBA_4444_PRE:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_SHORT_4_4_4_4;
      break;
    case COGL_PIXEL_FORMAT_RGBA_5551:
    case COGL_PIXEL_FORMAT_RGBA_5551_PRE:
      glintformat = GL_RGBA;
      glformat = GL_RGBA;
      gltype = GL_UNSIGNED_SHORT_5_5_5_1;
      break;

    case COGL_PIXEL_FORMAT_ANY:
    case COGL_PIXEL_FORMAT_YUV:
      g_assert_not_reached ();
      break;
    }

  /* All of the pixel formats are handled above so if this hits then
     we've been given an invalid pixel format */
  g_assert (glformat != 0);

  if (out_glintformat != NULL)
    *out_glintformat = glintformat;
  if (out_glformat != NULL)
    *out_glformat = glformat;
  if (out_gltype != NULL)
    *out_gltype = gltype;

  return required_format;
}

static CoglBool
_cogl_driver_update_features (CoglContext *context,
                              CoglError **error)
{
  CoglPrivateFeatureFlags private_flags = 0;
  CoglFeatureFlags flags = 0;
  const char *gl_extensions;
  int num_stencil_bits = 0;

  /* We have to special case getting the pointer to the glGetString
     function because we need to use it to determine what functions we
     can expect */
  context->glGetString =
    (void *) _cogl_renderer_get_proc_address (context->display->renderer,
                                              "glGetString",
                                              TRUE);

  COGL_NOTE (WINSYS,
             "Checking features\n"
             "  GL_VENDOR: %s\n"
             "  GL_RENDERER: %s\n"
             "  GL_VERSION: %s\n"
             "  GL_EXTENSIONS: %s",
             context->glGetString (GL_VENDOR),
             context->glGetString (GL_RENDERER),
             _cogl_context_get_gl_version (context),
             _cogl_context_get_gl_extensions (context));

  _cogl_gpu_info_init (context, &context->gpu);

  gl_extensions = _cogl_context_get_gl_extensions (context);

  _cogl_feature_check_ext_functions (context,
                                     -1 /* GL major version */,
                                     -1 /* GL minor version */,
                                     gl_extensions);

  GE( context, glGetIntegerv (GL_STENCIL_BITS, &num_stencil_bits) );
  /* We need at least three stencil bits to combine clips */
  if (num_stencil_bits > 2)
    private_flags |= COGL_PRIVATE_FEATURE_STENCIL_BUFFER;

#ifdef HAVE_COGL_GLES
  if (context->driver == COGL_DRIVER_GLES1)
    {
      int max_clip_planes;
      GE( context, glGetIntegerv (GL_MAX_CLIP_PLANES, &max_clip_planes) );
      if (max_clip_planes >= 4)
        private_flags |= COGL_PRIVATE_FEATURE_FOUR_CLIP_PLANES;
    }
#endif

  if (context->driver == COGL_DRIVER_GLES2)
    {
      flags |= COGL_FEATURE_SHADERS_GLSL | COGL_FEATURE_OFFSCREEN;
      /* Note GLES 2 core doesn't support mipmaps for npot textures or
       * repeat modes other than CLAMP_TO_EDGE. */
      flags |= COGL_FEATURE_TEXTURE_NPOT_BASIC;
      flags |= COGL_FEATURE_DEPTH_RANGE;
      COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_GLSL, TRUE);
      COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_OFFSCREEN, TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_BASIC, TRUE);
      COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_DEPTH_RANGE, TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_MIRRORED_REPEAT, TRUE);
    }

  private_flags |= COGL_PRIVATE_FEATURE_VBOS;

  /* Both GLES 1.1 and GLES 2.0 support point sprites in core */
  flags |= COGL_FEATURE_POINT_SPRITE;
  COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_POINT_SPRITE, TRUE);

  if (context->glGenRenderbuffers)
    {
      flags |= COGL_FEATURE_OFFSCREEN;
      COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_OFFSCREEN, TRUE);
    }

  if (context->glBlitFramebuffer)
    private_flags |= COGL_PRIVATE_FEATURE_OFFSCREEN_BLIT;

  if (_cogl_check_extension ("GL_OES_element_index_uint", gl_extensions))
    {
      flags |= COGL_FEATURE_UNSIGNED_INT_INDICES;
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_UNSIGNED_INT_INDICES, TRUE);
    }

  if (_cogl_check_extension ("GL_OES_texture_npot", gl_extensions))
    {
      flags |= (COGL_FEATURE_TEXTURE_NPOT |
                COGL_FEATURE_TEXTURE_NPOT_BASIC |
                COGL_FEATURE_TEXTURE_NPOT_MIPMAP |
                COGL_FEATURE_TEXTURE_NPOT_REPEAT);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT, TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_BASIC, TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_MIPMAP, TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_REPEAT, TRUE);
    }
  else if (_cogl_check_extension ("GL_IMG_texture_npot", gl_extensions))
    {
      flags |= (COGL_FEATURE_TEXTURE_NPOT_BASIC |
                COGL_FEATURE_TEXTURE_NPOT_MIPMAP);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_BASIC, TRUE);
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_TEXTURE_NPOT_MIPMAP, TRUE);
    }

  if (context->glTexImage3D)
    {
      flags |= COGL_FEATURE_TEXTURE_3D;
      COGL_FLAGS_SET (context->features, COGL_FEATURE_ID_TEXTURE_3D, TRUE);
    }

  if (context->glMapBuffer)
    {
      /* The GL_OES_mapbuffer extension doesn't support mapping for
         read */
      flags |= COGL_FEATURE_MAP_BUFFER_FOR_WRITE;
      COGL_FLAGS_SET (context->features,
                      COGL_FEATURE_ID_MAP_BUFFER_FOR_WRITE, TRUE);
    }

  if (context->glEGLImageTargetTexture2D)
    private_flags |= COGL_PRIVATE_FEATURE_TEXTURE_2D_FROM_EGL_IMAGE;

  if (_cogl_check_extension ("GL_OES_packed_depth_stencil", gl_extensions))
    private_flags |= COGL_PRIVATE_FEATURE_OES_PACKED_DEPTH_STENCIL;

  if (_cogl_check_extension ("GL_EXT_texture_format_BGRA8888", gl_extensions))
    private_flags |= COGL_PRIVATE_FEATURE_TEXTURE_FORMAT_BGRA8888;

  if (_cogl_check_extension ("GL_EXT_unpack_subimage", gl_extensions))
    private_flags |= COGL_PRIVATE_FEATURE_UNPACK_SUBIMAGE;

  /* Cache features */
  context->private_feature_flags |= private_flags;
  context->feature_flags |= flags;

  return TRUE;
}

const CoglDriverVtable
_cogl_driver_gles =
  {
    _cogl_driver_pixel_format_from_gl_internal,
    _cogl_driver_pixel_format_to_gl,
    _cogl_driver_update_features
  };
