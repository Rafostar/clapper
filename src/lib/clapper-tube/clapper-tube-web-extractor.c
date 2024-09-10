/* Clapper Tube Library
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * ClapperTubeWebExtractor:
 *
 * A base class for creating web content extractors.
 */

#include "clapper-tube-web-extractor.h"
#include "../shared/clapper-shared-utils-private.h"

#define GST_CAT_DEFAULT clapper_tube_web_extractor_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class clapper_tube_web_extractor_parent_class
G_DEFINE_TYPE (ClapperTubeWebExtractor, clapper_tube_web_extractor, CLAPPER_TUBE_TYPE_EXTRACTOR);

static inline void
_configure_msg (SoupMessage *msg)
{
  SoupMessageHeaders *headers = soup_message_get_request_headers (msg);

  /* Set some default headers if extractor did not */

  if (!soup_message_headers_get_one (headers, "User-Agent")) {
    /* User-Agent: privacy.resistFingerprining */
    soup_message_headers_replace (headers, "User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; rv:78.0) Gecko/20100101 Firefox/78.0");
  }
}

static void
clapper_tube_web_extractor_init (ClapperTubeWebExtractor *self)
{
}

static SoupSession *
clapper_tube_web_extractor_create_session (ClapperTubeWebExtractor *self)
{
  return soup_session_new_with_options ("timeout", 7, NULL);
}

static ClapperTubeFlow
clapper_tube_web_extractor_create_request (ClapperTubeWebExtractor *self,
    SoupMessage **msg, GCancellable *cancellable, GError **error)
{
  GST_ERROR_OBJECT (self, "Request creation is not implemented!");

  return CLAPPER_TUBE_FLOW_ERROR;
}

static ClapperTubeFlow
clapper_tube_web_extractor_read_response (ClapperTubeWebExtractor *self,
    SoupMessage *msg, GInputStream *stream, GCancellable *cancellable, GError **error)
{
  GST_ERROR_OBJECT (self, "Response reading is not implemented!");

  return CLAPPER_TUBE_FLOW_ERROR;
}

static ClapperTubeFlow
clapper_tube_web_extractor_extract (ClapperTubeExtractor *extractor,
    GCancellable *cancellable, GError **error)
{
  ClapperTubeWebExtractor *self = CLAPPER_TUBE_WEB_EXTRACTOR_CAST (extractor);
  ClapperTubeWebExtractorClass *web_extractor_class = CLAPPER_TUBE_WEB_EXTRACTOR_GET_CLASS (self);
  SoupSession *session;
  SoupMessage *msg = NULL;
  GInputStream *stream = NULL;
  ClapperTubeFlow flow = CLAPPER_TUBE_FLOW_ERROR;

  GST_DEBUG_OBJECT (self, "Creating session...");
  session = web_extractor_class->create_session (self);

  if (G_UNLIKELY (session == NULL)) {
    g_set_error (error, CLAPPER_TUBE_EXTRACTOR_ERROR,
        CLAPPER_TUBE_EXTRACTOR_ERROR_OTHER,
        "Soup session was not created");
    goto finish;
  }

beginning:
  GST_DEBUG_OBJECT (self, "Creating request...");
  flow = (*error == NULL && !g_cancellable_is_cancelled (cancellable))
      ? web_extractor_class->create_request (self, &msg, cancellable, error)
      : CLAPPER_TUBE_FLOW_ERROR;

  if (flow != CLAPPER_TUBE_FLOW_OK)
    goto decide_flow;

  _configure_msg (msg);

  GST_DEBUG_OBJECT (self, "Sending request...");
  stream = soup_session_send (session, msg, cancellable, error);

  GST_DEBUG_OBJECT (self, "Reading response...");
  flow = (stream && *error == NULL && !g_cancellable_is_cancelled (cancellable))
      ? web_extractor_class->read_response (self, msg, stream, cancellable, error)
      : CLAPPER_TUBE_FLOW_ERROR;

  if (stream) {
    if (g_input_stream_close (stream, NULL, NULL))
      GST_TRACE_OBJECT (self, "Input stream closed");
    else
      GST_WARNING_OBJECT (self, "Input stream could not be closed");

    g_object_unref (stream);
  }

  if (flow != CLAPPER_TUBE_FLOW_OK)
    goto decide_flow;

finish:
  g_clear_object (&msg);
  g_clear_object (&session);

  GST_DEBUG_OBJECT (self, "Web extraction finished");

  return flow;

decide_flow:
  if (flow == CLAPPER_TUBE_FLOW_RESTART) {
    g_clear_object (&msg);
    g_clear_error (error);

    goto beginning;
  }

  goto finish;
}

static void
clapper_tube_web_extractor_class_init (ClapperTubeWebExtractorClass *klass)
{
  ClapperTubeExtractorClass *extractor_class = (ClapperTubeExtractorClass *) klass;
  ClapperTubeWebExtractorClass *web_extractor_class = (ClapperTubeWebExtractorClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clappertubewebextractor", 0,
      "Clapper Tube Web Extractor");

  extractor_class->extract = clapper_tube_web_extractor_extract;

  web_extractor_class->create_session = clapper_tube_web_extractor_create_session;
  web_extractor_class->create_request = clapper_tube_web_extractor_create_request;
  web_extractor_class->read_response = clapper_tube_web_extractor_read_response;
}
