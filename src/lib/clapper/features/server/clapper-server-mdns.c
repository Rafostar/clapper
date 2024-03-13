/*
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

#include <gst/gst.h>

#include "clapper-server-mdns-private.h"

#define CLAPPER_SERVER_MDNS_SERVICE "_clapper._tcp.local"

#define N_RESP 4

#define PTR_INDEX(i) (i * N_RESP)
#define TXT_INDEX(i) (PTR_INDEX(i) + 1)
#define SRV_INDEX(i) (PTR_INDEX(i) + 2)
#define A_AAAA_INDEX(i) (PTR_INDEX(i) + 3)

typedef struct
{
  gchar *name;
  gchar *service_link;
  guint port;
} ClapperServerMdnsEntry;

typedef struct
{
  GPtrArray *entries;
  GPtrArray *pending_entries;
} ClapperServerMdns;

#define GST_CAT_DEFAULT clapper_server_mdns_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static ClapperServerMdns *mdns = NULL;
static GCond mdns_cond;
static GMutex mdns_lock;

void
clapper_server_mdns_debug_init (void)
{
  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperservermdns",
      GST_DEBUG_FG_RED, "Clapper Server MDNS");
}

static void
clapper_server_mdns_entry_free (ClapperServerMdnsEntry *entry)
{
  GST_TRACE ("Freeing MDNS entry: %p", entry);

  g_free (entry->name);
  g_free (entry->service_link);
  g_free (entry);
}

static void
clapper_server_mdns_remove_port (GPtrArray *entries, guint port)
{
  guint i;

  for (i = 0; i < entries->len; ++i) {
    ClapperServerMdnsEntry *entry = g_ptr_array_index (entries, i);

    if (entry->port == port) {
      GST_TRACE ("Removing entry with port: %u", port);
      g_ptr_array_remove_index (entries, i);
      break;
    }
  }
}

static inline void
_send_entries (struct mdns_ctx *ctx, const struct sockaddr *addr,
    enum mdns_announce_type type, GPtrArray *entries)
{
  const guint n_answers = N_RESP * entries->len;
  gchar domain_name[32];
  guint i;

  struct rr_entry *answers = g_alloca0 (sizeof (struct rr_entry) * n_answers);
  struct mdns_hdr hdr = { 0, };

  hdr.flags |= FLAG_QR;
  hdr.flags |= FLAG_AA;
  hdr.num_ans_rr = n_answers;

  g_snprintf (domain_name, sizeof (domain_name), "%s.local", g_get_host_name ());

  for (i = 0; i < entries->len; ++i) {
    ClapperServerMdnsEntry *entry = g_ptr_array_index (entries, i);

    GST_LOG ("Preparing answers for MDNS query, service: \"%s\""
        ", domain: \"%s\", link: \"%s\"",
        CLAPPER_SERVER_MDNS_SERVICE, domain_name, entry->service_link);

    answers[PTR_INDEX(i)] = (struct rr_entry) {
      .type = RR_PTR,
      .name = (char *) CLAPPER_SERVER_MDNS_SERVICE,
      .data.PTR.domain = entry->service_link,
      .rr_class = RR_IN,
      .msbit = 1,
      .ttl = (type == MDNS_ANNOUNCE_GOODBYE) ? 0 : 120,
      .next = &answers[TXT_INDEX(i)]
    };

    answers[TXT_INDEX(i)] = (struct rr_entry) {
      .type = RR_TXT,
      .name = entry->service_link,
      .rr_class = RR_IN,
      .msbit = 1,
      .ttl = (type == MDNS_ANNOUNCE_GOODBYE) ? 0 : 120,
      .next = &answers[SRV_INDEX(i)]
    };

    answers[SRV_INDEX(i)] = (struct rr_entry) {
      .type = RR_SRV,
      .name = entry->service_link,
      .data.SRV.port = entry->port,
      .data.SRV.priority = 0,
      .data.SRV.weight = 0,
      .data.SRV.target = domain_name,
      .rr_class = RR_IN,
      .msbit = 1,
      .ttl = (type == MDNS_ANNOUNCE_GOODBYE) ? 0 : 120,
      .next = &answers[A_AAAA_INDEX(i)]
    };

    answers[A_AAAA_INDEX(i)] = (struct rr_entry) {
      .name = domain_name,
      .rr_class = RR_IN,
      .msbit = 1,
      .ttl = (type == MDNS_ANNOUNCE_GOODBYE) ? 0 : 120,
      .next = (i + 1 < entries->len) ? &answers[PTR_INDEX(i + 1)] : NULL
    };

    if (addr->sa_family == AF_INET) {
      answers[A_AAAA_INDEX(i)].type = RR_A;
      memcpy (&answers[A_AAAA_INDEX(i)].data.A.addr,
          &((struct sockaddr_in *) addr)->sin_addr,
          sizeof (answers[A_AAAA_INDEX(i)].data.A.addr));
    } else {
      answers[A_AAAA_INDEX(i)].type = RR_AAAA;
      memcpy(&answers[A_AAAA_INDEX(i)].data.AAAA.addr,
          &((struct sockaddr_in6 *) addr)->sin6_addr,
          sizeof (answers[A_AAAA_INDEX(i)].data.AAAA.addr));
    }

    GST_LOG ("Prepared %u/%u bunches of answers", i + 1, entries->len);
  }

  /* Needs to still have lock here, as pointers
   * in answers are simply assigned from entries */
  GST_LOG ("Sending all answers");
  mdns_entries_send (ctx, &hdr, answers);
}

static void
_mdns_cb (struct mdns_ctx *ctx, const struct sockaddr *addr,
    const char* service, enum mdns_announce_type type)
{
  if (service && strcmp (service, CLAPPER_SERVER_MDNS_SERVICE) != 0)
    return;

  g_mutex_lock (&mdns_lock);

  switch (type) {
    case MDNS_ANNOUNCE_INITIAL:
      if (mdns->pending_entries->len > 0) {
        GST_LOG ("Handling announcement type: INITIAL");
        _send_entries (ctx, addr, type, mdns->pending_entries);

        /* Move to entries after initial announcement */
        while (mdns->pending_entries->len > 0) {
          ClapperServerMdnsEntry *entry;

          /* MDNS advertises entries in reverse order */
          entry = g_ptr_array_steal_index (mdns->pending_entries, 0);
          g_ptr_array_insert (mdns->entries, 0, entry);
        }
      }
      break;
    case MDNS_ANNOUNCE_RESPONSE:
    case MDNS_ANNOUNCE_GOODBYE:
      if (mdns->entries->len > 0) {
        GST_LOG ("Handling announcement type: %s",
            (type == MDNS_ANNOUNCE_RESPONSE) ? "RESPONSE" : "GOODBYE");
        _send_entries (ctx, addr, type, mdns->entries);
      }
      break;
    default:
      break;
  }

  g_mutex_unlock (&mdns_lock);
}

static gboolean
mdns_stop_cb (struct mdns_ctx *ctx)
{
  gboolean announce;

  g_mutex_lock (&mdns_lock);

  if (mdns->entries->len == 0
      && mdns->pending_entries->len == 0) {
    g_mutex_unlock (&mdns_lock);
    return TRUE;
  }

  announce = (mdns->pending_entries->len > 0);
  g_mutex_unlock (&mdns_lock);

  if (announce)
    mdns_request_initial_announce (ctx, NULL);

  return FALSE;
}

static gpointer
mdns_thread_func (gpointer user_data)
{
  struct mdns_ctx *ctx = NULL;
  int resp;
  char err_str[128];

  GST_TRACE ("MDNS init");

  if ((resp = mdns_init (&ctx, MDNS_ADDR_IPV4, MDNS_PORT)) < 0) {
    mdns_strerror(resp, err_str, sizeof (err_str));
    GST_ERROR ("Could not initialize MDNS, reason: %s", err_str);
    return NULL;
  }

  mdns_announce (ctx, RR_PTR, (mdns_announce_callback) _mdns_cb, ctx);

  GST_DEBUG ("MDNS start");
serve:
  if ((resp = mdns_serve (ctx, (mdns_stop_func) mdns_stop_cb, ctx)) < 0) {
    mdns_strerror(resp, err_str, sizeof (err_str));
    GST_ERROR ("Could start MDNS, reason: %s", err_str);
  }

  g_mutex_lock (&mdns_lock);

  /* Can happen when stopped due to lack of entries,
   * but entries were added afterwards */
  if (resp >= 0 && (mdns->entries->len > 0
      || mdns->pending_entries->len > 0)) {
    g_mutex_unlock (&mdns_lock);
    goto serve;
  }

  /* No more going back now */
  GST_DEBUG ("MDNS stop");

  /* Destroy with a lock, this ensures unbind
   * of MDNS_PORT before doing "mdns_init" again */
  GST_TRACE ("MDNS destroy");
  mdns_destroy (ctx);

  GST_TRACE ("Freeing MDNS entries storage: %p", mdns);
  g_ptr_array_unref (mdns->entries);
  g_ptr_array_unref (mdns->pending_entries);
  g_clear_pointer (&mdns, g_free);

  g_cond_broadcast (&mdns_cond);

  g_mutex_unlock (&mdns_lock);

  return NULL;
}

void
clapper_server_mdns_serve (gchar *name, guint port)
{
  ClapperServerMdnsEntry *entry;
  const gchar *prgname = g_get_prgname ();
  gboolean stopped;

  entry = g_new (ClapperServerMdnsEntry, 1);
  entry->name = name;
  entry->service_link = g_strdup_printf ("%s %s %s.%s",
      g_get_host_name (), (prgname) ? prgname : "clapperplayer",
      entry->name, CLAPPER_SERVER_MDNS_SERVICE);
  entry->port = port;
  GST_TRACE ("Created MDNS entry: %p", entry);

  g_mutex_lock (&mdns_lock);

  if ((stopped = mdns == NULL)) {
    mdns = g_new (ClapperServerMdns, 1);
    mdns->entries = g_ptr_array_new_with_free_func (
        (GDestroyNotify) clapper_server_mdns_entry_free);
    mdns->pending_entries = g_ptr_array_new_with_free_func (
        (GDestroyNotify) clapper_server_mdns_entry_free);
    GST_TRACE ("Created MDNS entries storage: %p", mdns);
  }
  g_ptr_array_add (mdns->pending_entries, entry);

  g_mutex_unlock (&mdns_lock);

  if (stopped) {
    GThread *thread;
    GError *error = NULL;

    GST_DEBUG ("Starting MDNS service");
    thread = g_thread_try_new ("clapper-server-mdns",
        (GThreadFunc) mdns_thread_func, NULL, &error);

    if (error) {
      GST_ERROR ("Could not create MDNS thread, reason: %s", error->message);
      g_error_free (error);
    } else {
      g_thread_unref (thread);
    }
  }
}

void
clapper_server_mdns_remove (guint port)
{
  g_mutex_lock (&mdns_lock);

  clapper_server_mdns_remove_port (mdns->entries, port);
  clapper_server_mdns_remove_port (mdns->pending_entries, port);

  if (mdns
      && mdns->entries->len == 0
      && mdns->pending_entries->len == 0) {
    GST_DEBUG ("MDNS is going to stop");

    while (mdns != NULL)
      g_cond_wait (&mdns_cond, &mdns_lock);
  }

  g_mutex_unlock (&mdns_lock);
}
