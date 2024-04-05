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

#include <glib/gi18n.h>
#include <gst/gst.h>
#include <clapper/clapper.h>

#include "clapper-app-preferences-window.h"
#include "clapper-app-application.h"
#include "clapper-app-utils.h"

#define GST_CAT_DEFAULT clapper_app_preferences_window_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

struct _ClapperAppPreferencesWindow
{
  AdwPreferencesWindow parent;

  AdwComboRow *seek_method_combo_row;
  AdwComboRow *seek_unit_combo_row;
  AdwSpinRow *seek_value_spin_row;
  AdwSwitchRow *server_switch_row;

  AdwSpinRow *audio_offset_spin_row;
  AdwSpinRow *subtitle_offset_spin_row;
  GtkFontDialogButton *font_dialog_button;

  AdwNavigationPage *plugins_subpage;
  AdwComboRow *plugins_combo_row;
  AdwComboRow *features_combo_row;
  AdwPreferencesGroup *overrides_group;

  GSettings *settings;

  GList *features;
  GtkStringList *plugins_list;

  GPtrArray *rank_rows;
  gulong ranks_setting_changed_id;

  gboolean ranking_has_plugins_model;
};

#define parent_class clapper_app_preferences_window_parent_class
G_DEFINE_TYPE (ClapperAppPreferencesWindow, clapper_app_preferences_window, ADW_TYPE_PREFERENCES_WINDOW);

typedef struct
{
  ClapperAppPreferencesWindow *prefs;
  GHashTable *parsed_overrides;
  gboolean updated;
} ClapperAppPreferencesIterRanksData;

enum
{
  PROP_0,
  PROP_RANK_ROWS,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

/* Sort by plugin name and if the same, sort by element name */
static gint
_compare_plugins_cb (gconstpointer ptr_a, gconstpointer ptr_b)
{
  GstPluginFeature *feature_a = GST_PLUGIN_FEATURE_CAST (ptr_a);
  GstPluginFeature *feature_b = GST_PLUGIN_FEATURE_CAST (ptr_b);
  gint result;

  result = strcmp (
      gst_plugin_feature_get_plugin_name (feature_a),
      gst_plugin_feature_get_plugin_name (feature_b));

  if (result == 0) {
    result = strcmp (
        gst_plugin_feature_get_name (feature_a),
        gst_plugin_feature_get_name (feature_b));
  }

  return result;
}

static gint
_compare_names_cb (gconstpointer ptr_a, gconstpointer ptr_b)
{
  GstPluginFeature *feature = GST_PLUGIN_FEATURE_CAST (ptr_a);
  const gchar *plugin_name = (const gchar *) ptr_b;

  return strcmp (gst_plugin_feature_get_plugin_name (feature), plugin_name);
}

static gboolean
_prefs_rows_compare_func (gconstpointer ptr_a, gconstpointer ptr_b)
{
  AdwPreferencesRow *row = (AdwPreferencesRow *) ptr_a;
  const gchar *name = (const gchar *) ptr_b;

  return (strcmp (adw_preferences_row_get_title (row), name) == 0);
}

static gboolean
_find_rank_overide_for_name (ClapperAppPreferencesWindow *self,
    const gchar *plugin_feature, guint *index)
{
  return g_ptr_array_find_with_equal_func (self->rank_rows,
      plugin_feature, (GEqualFunc) _prefs_rows_compare_func, index);
}

static gboolean
_plugin_feature_filter_cb (GstPluginFeature *feature, gpointer user_data G_GNUC_UNUSED)
{
  return GST_IS_ELEMENT_FACTORY (feature);
}

static void
remove_rank_override_button_clicked_cb (GtkButton *button, ClapperAppPreferencesWindow *self)
{
  GtkWidget *spin_row;
  const gchar *feature_name;

  spin_row = gtk_widget_get_ancestor (GTK_WIDGET (button), ADW_TYPE_SPIN_ROW);
  feature_name = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (spin_row));

  GST_DEBUG ("Removing rank override for: %s", feature_name);

  g_ptr_array_remove (self->rank_rows, spin_row);
  adw_preferences_group_remove (self->overrides_group, GTK_WIDGET (spin_row));

  gtk_widget_set_visible (GTK_WIDGET (self->overrides_group), self->rank_rows->len > 0);

  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_RANK_ROWS]);
}

static void
_add_rank_override (ClapperAppPreferencesWindow *self,
    const gchar *feature_name, GstRank rank, gboolean from_env)
{
  GtkWidget *spin_row, *remove_button;

  spin_row = adw_spin_row_new_with_range (0, G_MAXINT, 1);
  remove_button = gtk_button_new_from_icon_name ("user-trash-symbolic");

  gtk_widget_set_halign (remove_button, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (remove_button, GTK_ALIGN_CENTER);
  gtk_widget_add_css_class (remove_button, "circular");

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (spin_row), feature_name);
  adw_action_row_add_prefix (ADW_ACTION_ROW (spin_row), remove_button);
  adw_spin_row_set_numeric (ADW_SPIN_ROW (spin_row), TRUE);
  adw_spin_row_set_value (ADW_SPIN_ROW (spin_row), rank);
  gtk_widget_set_sensitive (spin_row, !from_env);

  if (!from_env) {
    g_signal_connect (remove_button, "clicked",
        G_CALLBACK (remove_rank_override_button_clicked_cb), self);
  }

  adw_preferences_group_add (self->overrides_group, spin_row);
  g_ptr_array_add (self->rank_rows, spin_row);
}

static void
_iter_ranks_func (const gchar *feature_name, GstRank rank,
    gboolean from_env, ClapperAppPreferencesIterRanksData *data)
{
  ClapperAppPreferencesWindow *self = data->prefs;
  guint index = 0;

  if (_find_rank_overide_for_name (self, feature_name, &index)) {
    GtkWidget *spin_row = g_ptr_array_index (self->rank_rows, index);

    if (rank != adw_spin_row_get_value (ADW_SPIN_ROW (spin_row))) {
      adw_spin_row_set_value (ADW_SPIN_ROW (spin_row), rank);
      data->updated = TRUE;
    }
    if (from_env == gtk_widget_get_sensitive (spin_row)) {
      gtk_widget_set_sensitive (spin_row, !from_env);
      data->updated = TRUE;
    }
  } else {
    _add_rank_override (self, feature_name, rank, from_env);
    data->updated = TRUE;
  }
  g_hash_table_insert (data->parsed_overrides,
      g_strdup (feature_name), GINT_TO_POINTER (rank));
}

static void
_update_rank_overrides (ClapperAppPreferencesWindow *self)
{
  ClapperAppPreferencesIterRanksData *data;
  gint i;

  data = g_new (ClapperAppPreferencesIterRanksData, 1);
  data->prefs = self;
  data->parsed_overrides = g_hash_table_new (g_str_hash, g_str_equal);
  data->updated = FALSE;

  GST_DEBUG ("Updating rank overrides");

  clapper_app_utils_iterate_plugin_feature_ranks (self->settings,
      (ClapperAppUtilsIterRanks) _iter_ranks_func, data);

  for (i = self->rank_rows->len - 1; i >= 0; --i) {
    AdwPreferencesRow *prefs_row = ADW_PREFERENCES_ROW (g_ptr_array_index (self->rank_rows, i));
    const gchar *feature_name = adw_preferences_row_get_title (prefs_row);

    if (!g_hash_table_contains (data->parsed_overrides, feature_name)) {
      g_ptr_array_remove_index (self->rank_rows, i);
      adw_preferences_group_remove (self->overrides_group, GTK_WIDGET (prefs_row));
      data->updated = TRUE;
    }
  }

  if (data->updated) {
    gtk_widget_set_visible (GTK_WIDGET (self->overrides_group), self->rank_rows->len > 0);
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_RANK_ROWS]);
  }

  g_hash_table_unref (data->parsed_overrides);
  g_free (data);
}

static void
add_override_button_clicked_cb (GtkButton *button, ClapperAppPreferencesWindow *self)
{
  GstPluginFeature *plugin_feature;
  GstRank rank;
  GtkStringObject *string_obj;
  const gchar *feature_name;

  string_obj = GTK_STRING_OBJECT (adw_combo_row_get_selected_item (self->features_combo_row));

  /* Should never happen, as button is insensitive when no selection */
  if (G_UNLIKELY (string_obj == NULL))
    return;

  feature_name = gtk_string_object_get_string (string_obj);

  GST_DEBUG ("Adding rank override for: %s", feature_name);

  plugin_feature = gst_registry_lookup_feature (gst_registry_get (), feature_name);
  rank = gst_plugin_feature_get_rank (plugin_feature);

  _add_rank_override (self, feature_name, rank, FALSE);
  gtk_widget_set_visible (GTK_WIDGET (self->overrides_group), self->rank_rows->len > 0);

  g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_RANK_ROWS]);

  gst_object_unref (plugin_feature);
}

static GtkStringList *
_make_plugin_features_string_list (ClapperAppPreferencesWindow *self, const gchar *plugin_name)
{
  GList *features, *feat;
  GtkStringList *features_list;
  GStrvBuilder *builder;
  gchar **features_names;

  GST_DEBUG ("Reading plugin features for plugin: %s", plugin_name);

  features = g_list_find_custom (self->features, plugin_name, (GCompareFunc) _compare_names_cb);
  builder = g_strv_builder_new ();

  for (feat = features; feat != NULL; feat = feat->next) {
    GstPluginFeature *feature = GST_PLUGIN_FEATURE_CAST (feat->data);
    const gchar *feature_name = gst_plugin_feature_get_name (feature);

    if (strcmp (gst_plugin_feature_get_plugin_name (feature), plugin_name) != 0)
      break;

    g_strv_builder_add (builder, feature_name);
  }

  features_names = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);

  features_list = gtk_string_list_new ((const gchar *const *) features_names);
  g_strfreev (features_names);

  GST_DEBUG ("Found plugin features: %u", g_list_model_get_n_items (G_LIST_MODEL (features_list)));

  return features_list;
}

static GtkStringList *
ranking_features_model_closure (ClapperAppPreferencesWindow *self, GtkStringObject *string_obj)
{
  if (!string_obj || !self->ranking_has_plugins_model)
    return NULL;

  return _make_plugin_features_string_list (self, gtk_string_object_get_string (string_obj));
}

static gboolean
add_override_button_sensitive_closure (ClapperAppPreferencesWindow *self,
    GtkStringObject *string_obj, GPtrArray *rank_rows)
{
  return (string_obj && !_find_rank_overide_for_name (self,
      gtk_string_object_get_string (string_obj), NULL));
}

static void
plugin_feature_ranks_settings_changed_cb (GSettings *settings,
    gchar *key G_GNUC_UNUSED, ClapperAppPreferencesWindow *self)
{
  GST_DEBUG ("Plugin feature ranks stored setting changed");
  _update_rank_overrides (self);
}

static void
_ensure_plugins_and_features_lists (ClapperAppPreferencesWindow *self)
{
  GList *feat;
  GStrvBuilder *builder;
  gchar **plugin_names;
  const gchar *last_plugin_name = NULL;

  /* Return if we were here already. Features can be NULL
   * when no plugins are found at specified directory */
  if (self->features || self->plugins_list)
    return;

  GST_DEBUG ("Reading available plugin features...");

  self->features = gst_registry_feature_filter (gst_registry_get (),
      (GstPluginFeatureFilter) _plugin_feature_filter_cb,
      FALSE, NULL);
  self->features = g_list_sort (self->features, (GCompareFunc) _compare_plugins_cb);

  builder = g_strv_builder_new ();

  for (feat = self->features; feat != NULL; feat = feat->next) {
    GstPluginFeature *feature = GST_PLUGIN_FEATURE_CAST (feat->data);
    const gchar *plugin_name = gst_plugin_feature_get_plugin_name (feature);

    if (g_strcmp0 (plugin_name, last_plugin_name) != 0) {
      g_strv_builder_add (builder, plugin_name);
      last_plugin_name = plugin_name;
    }
  }

  plugin_names = g_strv_builder_end (builder);
  g_strv_builder_unref (builder);

  GST_DEBUG ("Read all available plugin features");

  self->plugins_list = gtk_string_list_new ((const gchar *const *) plugin_names);
  g_strfreev (plugin_names);
}

static void
plugin_ranking_activated_cb (AdwActionRow *action_row, ClapperAppPreferencesWindow *self)
{
  _ensure_plugins_and_features_lists (self);

  if (!self->ranking_has_plugins_model) {
    adw_combo_row_set_model (self->plugins_combo_row, G_LIST_MODEL (self->plugins_list));
    adw_combo_row_set_selected (self->plugins_combo_row, GTK_INVALID_LIST_POSITION);

    GST_DEBUG ("Populated plugins combo row in ranking subpage");

    /* This is needed here so we will not populate plugin features row after setting
     * model and unset it again after changing back to GTK_INVALID_LIST_POSITION */
    self->ranking_has_plugins_model = TRUE;
  }

  if (self->ranks_setting_changed_id == 0) {
    self->ranks_setting_changed_id = g_signal_connect (self->settings,
        "changed::plugin-feature-ranks",
        G_CALLBACK (plugin_feature_ranks_settings_changed_cb), self);
  }
  _update_rank_overrides (self);

  adw_preferences_window_push_subpage (ADW_PREFERENCES_WINDOW (self), self->plugins_subpage);
}

static void
plugin_ranking_unrealize_cb (GtkWidget *widget, ClapperAppPreferencesWindow *self)
{
  GString *string;
  gchar *ranks_str;
  guint i;

  /* Since we are closing ranking subpage, disconnect this
   * signal as we do not need to update widgets immediately */
  if (self->ranks_setting_changed_id != 0) {
    g_signal_handler_disconnect (self->settings, self->ranks_setting_changed_id);
    self->ranks_setting_changed_id = 0;
  }

  GST_DEBUG ("Saving current rank overrides");
  string = g_string_new (NULL);

  for (i = 0; i < self->rank_rows->len; ++i) {
    GtkWidget *spin_row = g_ptr_array_index (self->rank_rows, i);
    GstRank rank;
    const gchar *feature_name;

    /* Insensitive are from env, we do not want to save these */
    if (!gtk_widget_get_sensitive (spin_row))
      continue;

    rank = adw_spin_row_get_value (ADW_SPIN_ROW (spin_row));
    feature_name = adw_preferences_row_get_title (ADW_PREFERENCES_ROW (spin_row));

    if (string->len == 0)
      g_string_append_printf (string, "%s:%i", feature_name, rank);
    else
      g_string_append_printf (string, ",%s:%i", feature_name, rank);
  }

  ranks_str = g_string_free_and_steal (string);
  g_settings_set_string (self->settings, "plugin-feature-ranks", ranks_str);
  g_free (ranks_str);
}

static gchar *
seek_method_name_closure (AdwEnumListItem *list_item, gpointer *user_data G_GNUC_UNUSED)
{
  switch (adw_enum_list_item_get_value (list_item)) {
    case CLAPPER_PLAYER_SEEK_METHOD_ACCURATE:
      return g_strdup (_("Accurate"));
    case CLAPPER_PLAYER_SEEK_METHOD_NORMAL:
      return g_strdup (_("Normal"));
    case CLAPPER_PLAYER_SEEK_METHOD_FAST:
      return g_strdup (_("Fast"));
    default:
      return NULL;
  }
}

static gboolean
_get_font_mapping (GValue *value,
    GVariant *variant, gpointer user_data G_GNUC_UNUSED)
{
  PangoFontDescription *desc;
  const gchar *desc_str = g_variant_get_string (variant, NULL);

  desc = pango_font_description_from_string (desc_str);
  g_value_set_boxed (value, desc);

  pango_font_description_free (desc);

  return TRUE;
}

static GVariant *
_set_font_mapping (const GValue *value,
    const GVariantType *expected_type, gpointer user_data G_GNUC_UNUSED)
{
  PangoFontDescription *desc;
  gchar *desc_str;

  desc = (PangoFontDescription *) g_value_get_boxed (value);
  desc_str = pango_font_description_to_string (desc);

  return g_variant_new_take_string (desc_str);
}

GtkWidget *
clapper_app_preferences_window_new (GtkApplication *gtk_app)
{
  ClapperAppPreferencesWindow *window;

  window = g_object_new (CLAPPER_APP_TYPE_PREFERENCES_WINDOW,
      "application", gtk_app,
      "transient-for", gtk_application_get_active_window (gtk_app),
      NULL);

  return GTK_WIDGET (window);
}

static void
clapper_app_preferences_window_init (ClapperAppPreferencesWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->rank_rows = g_ptr_array_new ();

  self->settings = g_settings_new (CLAPPER_APP_ID);

  g_settings_bind (self->settings, "seek-method",
      self->seek_method_combo_row, "selected", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "seek-unit",
      self->seek_unit_combo_row, "selected", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "seek-value",
      self->seek_value_spin_row, "value", G_SETTINGS_BIND_DEFAULT);

#if CLAPPER_HAVE_SERVER
  g_settings_bind (self->settings, "server-enabled",
      self->server_switch_row, "active", G_SETTINGS_BIND_DEFAULT);
#else
  gtk_widget_set_sensitive (GTK_WIDGET (self->server_switch_row), FALSE);
#endif

  g_settings_bind (self->settings, "audio-offset",
      self->audio_offset_spin_row, "value", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "subtitle-offset",
      self->subtitle_offset_spin_row, "value", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind_with_mapping (self->settings, "subtitle-font-desc",
      self->font_dialog_button, "font-desc", G_SETTINGS_BIND_DEFAULT,
      (GSettingsBindGetMapping) _get_font_mapping,
      (GSettingsBindSetMapping) _set_font_mapping,
      NULL, NULL);
}

static void
clapper_app_preferences_window_dispose (GObject *object)
{
  ClapperAppPreferencesWindow *self = CLAPPER_APP_PREFERENCES_WINDOW_CAST (object);

  g_clear_pointer (&self->rank_rows, g_ptr_array_unref);
  gtk_widget_dispose_template (GTK_WIDGET (self), CLAPPER_APP_TYPE_PREFERENCES_WINDOW);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
clapper_app_preferences_window_finalize (GObject *object)
{
  ClapperAppPreferencesWindow *self = CLAPPER_APP_PREFERENCES_WINDOW_CAST (object);

  GST_TRACE_OBJECT (self, "Finalize");

  g_object_unref (self->settings);

  g_clear_object (&self->plugins_list);

  if (self->features)
    gst_plugin_feature_list_free (self->features);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
clapper_app_preferences_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  ClapperAppPreferencesWindow *self = CLAPPER_APP_PREFERENCES_WINDOW_CAST (object);

  switch (prop_id) {
    case PROP_RANK_ROWS:
      g_value_set_boxed (value, self->rank_rows);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
clapper_app_preferences_window_class_init (ClapperAppPreferencesWindowClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "clapperapppreferenceswindow", 0,
      "Clapper App Preferences Window");

  gobject_class->get_property = clapper_app_preferences_get_property;
  gobject_class->dispose = clapper_app_preferences_window_dispose;
  gobject_class->finalize = clapper_app_preferences_window_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
      CLAPPER_APP_RESOURCE_PREFIX "/ui/clapper-app-preferences-window.ui");

  param_specs[PROP_RANK_ROWS] = g_param_spec_boxed ("rank-rows",
      NULL, NULL, G_TYPE_PTR_ARRAY,
      G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gtk_widget_class_bind_template_child (widget_class, ClapperAppPreferencesWindow, seek_method_combo_row);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppPreferencesWindow, seek_unit_combo_row);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppPreferencesWindow, seek_value_spin_row);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppPreferencesWindow, server_switch_row);

  gtk_widget_class_bind_template_child (widget_class, ClapperAppPreferencesWindow, audio_offset_spin_row);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppPreferencesWindow, subtitle_offset_spin_row);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppPreferencesWindow, font_dialog_button);

  gtk_widget_class_bind_template_child (widget_class, ClapperAppPreferencesWindow, plugins_subpage);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppPreferencesWindow, plugins_combo_row);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppPreferencesWindow, features_combo_row);
  gtk_widget_class_bind_template_child (widget_class, ClapperAppPreferencesWindow, overrides_group);

  gtk_widget_class_bind_template_callback (widget_class, seek_method_name_closure);

  gtk_widget_class_bind_template_callback (widget_class, plugin_ranking_activated_cb);
  gtk_widget_class_bind_template_callback (widget_class, plugin_ranking_unrealize_cb);

  gtk_widget_class_bind_template_callback (widget_class, ranking_features_model_closure);
  gtk_widget_class_bind_template_callback (widget_class, add_override_button_sensitive_closure);
  gtk_widget_class_bind_template_callback (widget_class, add_override_button_clicked_cb);
}
