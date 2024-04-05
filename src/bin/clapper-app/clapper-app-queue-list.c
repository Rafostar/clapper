/* Clapper Application
 * Copyright (C) 2024 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gdk/gdk.h>

#include "clapper-app-queue-list.h"
#include "clapper-app-queue-selection.h"
#include "clapper-app-media-item-box.h"
#include "clapper-app-utils.h"

#define GST_CAT_DEFAULT clapper_app_queue_list_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppQueueList
{
  GtkBox parent;

  GtkWidget *progression_drop_down;
  GtkWidget *list_view;

  GtkWidget *stack;
  GtkWidget *stack_default_page;
  GtkWidget *stack_trash_page;

  GtkDropTarget *trash_drop_target;
  GtkDropTarget *drop_target;

  GBinding *queue_progression_binding;

  GtkWidget *list_target; // store last target
  gboolean drop_after; // if should drop below list_target
};

#define parent_class clapper_app_queue_list_parent_class
G_DEFINE_TYPE (ClapperAppQueueList, clapper_app_queue_list, GTK_TYPE_BOX);

typedef struct
{
  ClapperMediaItem *item;
  GtkWidget *widget;
  GdkPaintable *paintable;
  gdouble x, y;
} ClapperAppQueueListDragData;

static GdkContentProvider *
drag_item_prepare_cb (GtkDragSource *drag_source, gdouble x, gdouble y, ClapperAppQueueList *self)
{
  GtkWidget *list_view, *pickup, *list_widget;
  GdkPaintable *paintable;
  ClapperMediaItem *item;
  ClapperAppQueueListDragData *drag_data;
  graphene_point_t p;

  /* Ensure no target yet */
  self->list_target = NULL;

  list_view = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drag_source));
  pickup = gtk_widget_pick (list_view, x, y, GTK_PICK_DEFAULT);

  if (G_UNLIKELY (pickup == NULL) || !CLAPPER_APP_IS_MEDIA_ITEM_BOX (pickup))
    return NULL;

  list_widget = gtk_widget_get_parent (pickup);
  item = clapper_app_media_item_box_get_media_item (CLAPPER_APP_MEDIA_ITEM_BOX_CAST (pickup));

  if (G_UNLIKELY (item == NULL || list_widget == NULL))
    return NULL;

  GST_DEBUG_OBJECT (self, "Preparing drag for: %" GST_PTR_FORMAT, item);

  if (!gtk_widget_compute_point (list_view, list_widget, &GRAPHENE_POINT_INIT (x, y), &p))
    graphene_point_init (&p, x, y);

  paintable = gtk_widget_paintable_new (list_widget);

  drag_data = g_new0 (ClapperAppQueueListDragData, 1);
  drag_data->item = gst_object_ref (item);
  drag_data->widget = g_object_ref_sink (pickup);
  drag_data->paintable = gdk_paintable_get_current_image (paintable);
  drag_data->x = p.x;
  drag_data->y = p.y;

  g_object_set_data (G_OBJECT (self), "drag-data", drag_data);

  g_object_unref (paintable);

  return gdk_content_provider_new_typed (GTK_TYPE_WIDGET, pickup);
}

static void
drag_item_drag_begin_cb (GtkDragSource *drag_source, GdkDrag *drag, ClapperAppQueueList *self)
{
  ClapperAppQueueListDragData *drag_data;
  GtkWidget *list_view;

  drag_data = (ClapperAppQueueListDragData *) g_object_get_data (G_OBJECT (self), "drag-data");

  gtk_drag_source_set_icon (drag_source, drag_data->paintable, drag_data->x, drag_data->y);
  gtk_widget_set_opacity (drag_data->widget, 0.3);

  list_view = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drag_source));
  gtk_widget_add_css_class (list_view, "dnd");

  gtk_stack_set_visible_child (GTK_STACK (self->stack), self->stack_trash_page);
}

static void
drag_item_drag_end_cb (GtkDragSource *drag_source, GdkDrag *drag,
    gboolean delete_data, ClapperAppQueueList *self)
{
  ClapperAppQueueListDragData *drag_data;
  GtkWidget *list_view;

  drag_data = (ClapperAppQueueListDragData *) g_object_get_data (G_OBJECT (self), "drag-data");
  g_object_set_data (G_OBJECT (self), "drag-data", NULL);

  list_view = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drag_source));
  gtk_widget_remove_css_class (list_view, "dnd");

  gtk_widget_set_opacity (drag_data->widget, 1.0);
  gtk_stack_set_visible_child (GTK_STACK (self->stack), self->stack_default_page);

  gst_object_unref (drag_data->item);
  g_object_unref (drag_data->widget);
  g_object_unref (drag_data->paintable);
  g_free (drag_data);

  gtk_event_controller_reset (GTK_EVENT_CONTROLLER (drag_source));
}

static void
queue_drop_value_notify_cb (GtkDropTarget *drop_target,
    GParamSpec *pspec G_GNUC_UNUSED, ClapperAppQueueList *self)
{
  const GValue *value = gtk_drop_target_get_value (drop_target);

  if (value && !clapper_app_utils_value_for_item_is_valid (value))
    gtk_drop_target_reject (drop_target);
}

static GdkDragAction
queue_drop_motion_cb (GtkDropTarget *drop_target,
    gdouble x, gdouble y, ClapperAppQueueList *self)
{
  GtkWidget *list_view, *pickup;
  GdkDrop *drop;

  list_view = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (drop_target));
  pickup = gtk_widget_pick (list_view, x, y, GTK_PICK_DEFAULT);

  if (pickup && CLAPPER_APP_IS_MEDIA_ITEM_BOX (pickup)) {
    ClapperAppQueueListDragData *drag_data;
    GtkWidget *list_widget;
    graphene_point_t point;
    gint height, margin_top = 0, margin_bottom = 0;

    drag_data = (ClapperAppQueueListDragData *) g_object_get_data (G_OBJECT (self), "drag-data");

    list_widget = gtk_widget_get_parent (pickup);
    height = gtk_widget_get_height (list_widget);

    if ((!drag_data || pickup != drag_data->widget)
        && gtk_widget_compute_point (list_view, list_widget, &GRAPHENE_POINT_INIT (x, y), &point)) {
      GtkWidget *sibling = NULL;

      if (point.y < (gfloat) height / 2) {
        if (drag_data)
          sibling = gtk_widget_get_prev_sibling (list_widget);

        if (!sibling || gtk_widget_get_parent (drag_data->widget) != sibling)
          margin_top = height;
      } else {
        if (drag_data)
          sibling = gtk_widget_get_next_sibling (list_widget);

        if (!sibling || gtk_widget_get_parent (drag_data->widget) != sibling)
          margin_bottom = height;
      }
    }

    if (self->list_target && self->list_target != list_widget) {
      gtk_widget_set_margin_top (self->list_target, 0);
      gtk_widget_set_margin_bottom (self->list_target, 0);
    }

    gtk_widget_set_margin_top (list_widget, margin_top);
    gtk_widget_set_margin_bottom (list_widget, margin_bottom);

    self->list_target = list_widget;
    self->drop_after = (margin_bottom > margin_top);
  }

  if ((drop = gtk_drop_target_get_current_drop (drop_target))) {
    GdkContentFormats *formats = gdk_drop_get_formats (drop);

    /* If it is a widget we move it from one place to another */
    if (gdk_content_formats_contain_gtype (formats, GTK_TYPE_WIDGET))
      return GDK_ACTION_MOVE;
  }

  return GDK_ACTION_COPY;
}

static void
queue_drop_leave_cb (GtkDropTarget *drop_target, ClapperAppQueueList *self)
{
  if (self->list_target) {
    gtk_widget_set_margin_top (self->list_target, 0);
    gtk_widget_set_margin_bottom (self->list_target, 0);
  }
}

static gboolean
queue_drop_cb (GtkDropTarget *drop_target, const GValue *value,
    gdouble x, gdouble y, ClapperAppQueueList *self)
{
  ClapperQueue *queue;
  ClapperMediaItem *item;
  GtkWidget *pickup;
  guint drop_index = 0;
  gboolean success = FALSE;

  if (G_UNLIKELY (self->list_target == NULL))
    return FALSE;

  pickup = gtk_widget_get_first_child (self->list_target);

  /* Reset margins on drop */
  gtk_widget_set_margin_top (self->list_target, 0);
  gtk_widget_set_margin_bottom (self->list_target, 0);
  self->list_target = NULL;

  if (G_UNLIKELY (pickup == NULL) || !CLAPPER_APP_IS_MEDIA_ITEM_BOX (pickup))
    return FALSE;

  item = clapper_app_media_item_box_get_media_item (CLAPPER_APP_MEDIA_ITEM_BOX_CAST (pickup));
  queue = CLAPPER_QUEUE (gst_object_get_parent (GST_OBJECT (item)));

  if (G_UNLIKELY (queue == NULL))
    return FALSE;

  if (!clapper_queue_find_item (queue, item, &drop_index)) {
    gst_object_unref (queue);
    return FALSE;
  }

  if (self->drop_after)
    drop_index++;

  /* Moving item with widget */
  if (G_VALUE_HOLDS (value, GTK_TYPE_WIDGET)) {
    ClapperAppQueueListDragData *drag_data;

    drag_data = (ClapperAppQueueListDragData *) g_object_get_data (G_OBJECT (self), "drag-data");

    /* Insert at different place */
    if (item != drag_data->item) {
      guint index = 0;

      if (clapper_queue_find_item (queue, drag_data->item, &index)) {
        if (drop_index > index)
          drop_index--;

        clapper_queue_reposition_item (queue, drag_data->item, drop_index);
        success = TRUE;
      }
    }
  } else {
    GFile **files = NULL;
    gint n_files = 0;

    if (clapper_app_utils_files_from_value (value, &files, &n_files)) {
      gint i;

      for (i = 0; i < n_files; ++i) {
        ClapperMediaItem *new_item = clapper_media_item_new_from_file (files[i]);

        clapper_queue_insert_item (queue, new_item, drop_index + i);
        gst_object_unref (new_item);
      }

      clapper_app_utils_files_free (files);
      success = TRUE;
    }
  }

  gst_object_unref (queue);

  return success;
}

static gboolean
trash_drop_cb (GtkDropTarget *drop_target, const GValue *value,
    gdouble x, gdouble y, ClapperAppQueueList *self)
{
  ClapperAppQueueListDragData *drag_data;
  ClapperQueue *queue;

  drag_data = (ClapperAppQueueListDragData *) g_object_get_data (G_OBJECT (self), "drag-data");

  if ((queue = CLAPPER_QUEUE (gst_object_get_parent (GST_OBJECT (drag_data->item))))) {
    clapper_queue_remove_item (queue, drag_data->item);
    gst_object_unref (queue);
  }

  return TRUE;
}

static void
_item_selected_cb (ClapperAppQueueSelection *selection, guint index, ClapperAppQueueList *self)
{
  GtkWidget *list_revealer;

  /* Auto hide queue list after selection */
  list_revealer = gtk_widget_get_ancestor (self->list_view, GTK_TYPE_REVEALER);
  if (G_LIKELY (list_revealer != NULL))
    gtk_revealer_set_reveal_child (GTK_REVEALER (list_revealer), FALSE);
}

static gboolean
_queue_progression_mode_transform_to_func (GBinding *binding, const GValue *from_value,
    GValue *to_value, ClapperAppQueueList *self)
{
  ClapperQueueProgressionMode mode = g_value_get_enum (from_value);

  g_value_set_uint (to_value, (guint) mode);
  return TRUE;
}

static gboolean
_queue_progression_mode_transform_from_func (GBinding *binding, const GValue *from_value,
    GValue *to_value, ClapperAppQueueList *self)
{
  guint mode = g_value_get_uint (from_value);

  if (mode == GTK_INVALID_LIST_POSITION)
    return FALSE;

  g_value_set_enum (to_value, (ClapperQueueProgressionMode) mode);
  return TRUE;
}

static void
clapper_app_queue_list_realize (GtkWidget *widget)
{
  ClapperAppQueueList *self = CLAPPER_APP_QUEUE_LIST_CAST (widget);
  ClapperPlayer *player;

  GTK_WIDGET_CLASS (parent_class)->realize (widget);

  GST_TRACE_OBJECT (self, "Realize");

  if ((player = clapper_gtk_get_player_from_ancestor (widget))) {
    ClapperQueue *queue = clapper_player_get_queue (player);
    ClapperAppQueueSelection *selection = clapper_app_queue_selection_new (queue);

    g_signal_connect (selection, "item-selected",
        G_CALLBACK (_item_selected_cb), self);

    self->queue_progression_binding = g_object_bind_property_full (queue, "progression-mode",
        self->progression_drop_down, "selected", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
        (GBindingTransformFunc) _queue_progression_mode_transform_to_func,
        (GBindingTransformFunc) _queue_progression_mode_transform_from_func,
        self, NULL);

    gtk_list_view_set_model (GTK_LIST_VIEW (self->list_view),
        GTK_SELECTION_MODEL (selection));
    g_object_unref (selection);
  }
}

static void
clapper_app_queue_list_unrealize (GtkWidget *widget)
{
  ClapperAppQueueList *self = CLAPPER_APP_QUEUE_LIST_CAST (widget);

  GST_TRACE_OBJECT (self, "Unrealize");

  g_clear_pointer (&self->queue_progression_binding, g_binding_unbind);
  gtk_list_view_set_model (GTK_LIST_VIEW (self->list_view), NULL);

  GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}

static void
clapper_app_queue_list_init (ClapperAppQueueList *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  /* Does not work correctly with OSD */
  gtk_widget_remove_css_class (self->list_view, "view");

  gtk_drop_target_set_gtypes (self->trash_drop_target,
      (GType[1]) { GTK_TYPE_WIDGET }, 1);
  gtk_drop_target_set_gtypes (self->drop_target,
      (GType[4]) { GTK_TYPE_WIDGET, GDK_TYPE_FILE_LIST, G_TYPE_FILE, G_TYPE_STRING }, 4);
}

static void
clapper_app_queue_list_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CLAPPER_APP_TYPE_QUEUE_LIST);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_app_queue_list_finalize (GObject *object)
{
  ClapperAppQueueList *self = CLAPPER_APP_QUEUE_LIST_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_queue_list_class_init (ClapperAppQueueListClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperappqueuelist", 0,
      "Clapper App Queue List");

  gobject_class->dispose = clapper_app_queue_list_dispose;
  gobject_class->finalize = clapper_app_queue_list_finalize;

  widget_class->realize = clapper_app_queue_list_realize;
  widget_class->unrealize = clapper_app_queue_list_unrealize;

  gtk_widget_class_set_template_from_resource (widget_class,
      CLAPPER_APP_RESOURCE_PREFIX "/ui/clapper-app-queue-list.ui");

  gtk_widget_class_bind_template_child (widget_class, ClapperAppQueueList, progression_drop_down);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppQueueList, list_view);

  gtk_widget_class_bind_template_child (widget_class, ClapperAppQueueList, stack);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppQueueList, stack_default_page);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppQueueList, stack_trash_page);

  gtk_widget_class_bind_template_child (widget_class, ClapperAppQueueList, trash_drop_target);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppQueueList, drop_target);

  gtk_widget_class_bind_template_callback (widget_class, drag_item_prepare_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_item_drag_begin_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_item_drag_end_cb);

  gtk_widget_class_bind_template_callback (widget_class, queue_drop_value_notify_cb);
  gtk_widget_class_bind_template_callback (widget_class, queue_drop_motion_cb);
  gtk_widget_class_bind_template_callback (widget_class, queue_drop_leave_cb);
  gtk_widget_class_bind_template_callback (widget_class, queue_drop_cb);
  gtk_widget_class_bind_template_callback (widget_class, trash_drop_cb);

  gtk_widget_class_set_css_name (widget_class, "clapper-app-queue-list");
}
