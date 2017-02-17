/* gtd-plugin-todo-txt.c
 *
 * Copyright (C) 2016 Rohit Kaushik <kaushikrohit325@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "Todo Txt"

#include "gtd-plugin-todo-txt.h"
#include "gtd-provider-todo-txt.h"

#include <glib/gi18n.h>
#include <glib-object.h>

/**
 * The #GtdPluginTodoTxt is a class that loads Todo.txt
 * provider of GNOME To Do.
 */

struct _GtdPluginTodoTxt
{
  PeasExtensionBase   parent;

  gchar              *source;

  GFile              *source_file;
  GFileMonitor       *monitor;

  GSettings          *settings;

  GtkWidget          *preferences_box;
  GtkWidget          *preferences;

  /* Providers */
  GList              *providers;
};

enum
{
  PROP_0,
  PROP_PREFERENCES_PANEL,
  LAST_PROP
};

static void          gtd_activatable_iface_init                  (GtdActivatableInterface  *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (GtdPluginTodoTxt, gtd_plugin_todo_txt, PEAS_TYPE_EXTENSION_BASE,
                                0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (GTD_TYPE_ACTIVATABLE,
                                                               gtd_activatable_iface_init))

void
gtd_plugin_todo_txt_monitor_source (GFileMonitor      *monitor,
                                    GFile             *first,
                                    GFile             *second,
                                    GFileMonitorEvent  event,
                                    gpointer           data)
{
  GtdProviderTodoTxt *provider;
  GtdPluginTodoTxt *self;

  self = data;

  provider = self->providers->data;

  g_list_free_full (self->providers, g_object_unref);
  self->providers = NULL;

  g_signal_emit_by_name (self, "provider-removed", provider);

  provider = gtd_provider_todo_txt_new (self->source);
  self->source_file = g_file_new_for_uri (self->source);

  self->providers = g_list_append (self->providers, provider);
  g_signal_emit_by_name (self, "provider-added", provider);
}

static void
gtd_plugin_todo_txt_load_source_monitor (GtdPluginTodoTxt *self)
{
  GError *file_monitor = NULL;

  self->monitor = g_file_monitor_file (self->source_file,
                                       G_FILE_MONITOR_WATCH_MOVES,
                                       NULL,
                                       &file_monitor);

  if (file_monitor)
    {
      gtd_manager_emit_error_message (gtd_manager_get_default (),
                                      _("Error while opening the file monitor. Todo.txt will not be monitored"),
                                      file_monitor->message);
      g_clear_error (&file_monitor);
    }
  else
    {
      gtd_provider_todo_txt_set_monitor (self->providers->data, self->monitor);
      g_signal_connect (self->monitor, "changed", G_CALLBACK (gtd_plugin_todo_txt_monitor_source), self);
    }
}

static gboolean
gtd_plugin_todo_txt_set_default_source (GtdPluginTodoTxt *self)
{
  gchar *default_file;
  GError *error;

  default_file = g_strconcat (g_get_user_special_dir (G_USER_DIRECTORY_DOCUMENTS), "todo.txt", NULL);
  error = NULL;

  self->source = g_filename_to_uri (default_file, NULL, &error);
  self->source_file = g_file_new_for_uri (default_file);

  if (error)
    {
      gtd_manager_emit_error_message (gtd_manager_get_default (),
                                      _("Error while converting the default Todo.txt path to an URI"),
                                      error->message);

      g_clear_error (&error);
      return FALSE;
    }

  if (!g_file_query_exists (self->source_file, NULL))
    {
      g_file_create (self->source_file,
                     G_FILE_CREATE_NONE,
                     NULL,
                     &error);

      if (error)
        {
          gtd_manager_emit_error_message (gtd_manager_get_default (),
                                          _("Cannot create Todo.txt file"),
                                          error->message);

          g_clear_error (&error);
          return FALSE;
        }
    }
  return TRUE;
}

/*
 * GtdActivatable interface implementation
 */
static void
gtd_plugin_todo_txt_activate (GtdActivatable *activatable)
{
  GtdPluginTodoTxt *self;
  GtdProviderTodoTxt *provider;

  self = GTD_PLUGIN_TODO_TXT (activatable);

  if (!self->source || self->source[0] == '\0')
    {
      gboolean set;
      set = gtd_plugin_todo_txt_set_default_source (self);

      if (!set)
        return;
    }


  provider = gtd_provider_todo_txt_new (self->source);
  self->source_file = g_file_new_for_uri (self->source);

  self->providers = g_list_append (self->providers, provider);
  g_signal_emit_by_name (self, "provider-added", provider);

  gtd_plugin_todo_txt_load_source_monitor (self);
}

static void
gtd_plugin_todo_txt_deactivate (GtdActivatable *activatable)
{
  ;
}

static GList*
gtd_plugin_todo_txt_get_header_widgets (GtdActivatable *activatable)
{
  return NULL;
}

static GtkWidget*
gtd_plugin_todo_txt_get_preferences_panel (GtdActivatable *activatable)
{
  GtdPluginTodoTxt *plugin = GTD_PLUGIN_TODO_TXT (activatable);

  return plugin->preferences_box;

}

static GList*
gtd_plugin_todo_txt_get_panels (GtdActivatable *activatable)
{
  return NULL;
}

static GList*
gtd_plugin_todo_txt_get_providers (GtdActivatable *activatable)
{
  GtdPluginTodoTxt *plugin = GTD_PLUGIN_TODO_TXT (activatable);

  return plugin->providers;
}

static void
gtd_activatable_iface_init (GtdActivatableInterface *iface)
{
  iface->activate = gtd_plugin_todo_txt_activate;
  iface->deactivate = gtd_plugin_todo_txt_deactivate;
  iface->get_header_widgets = gtd_plugin_todo_txt_get_header_widgets;
  iface->get_preferences_panel = gtd_plugin_todo_txt_get_preferences_panel;
  iface->get_panels = gtd_plugin_todo_txt_get_panels;
  iface->get_providers = gtd_plugin_todo_txt_get_providers;
}

/*
 * Init
 */

static void
gtd_plugin_todo_txt_source_changed_finished_cb (GtdPluginTodoTxt *self)
{
  GtdProviderTodoTxt *provider;

  self->source = g_settings_get_string (self->settings, "file");

  if (!self->source || self->source[0] == '\0')
    {
      gboolean set;
      set = gtd_plugin_todo_txt_set_default_source (self);

      if(!set)
        return;
    }

  self->source_file = g_file_new_for_uri (self->source);

  provider = gtd_provider_todo_txt_new (self->source);
  self->providers = g_list_append (self->providers, provider);

  gtd_plugin_todo_txt_load_source_monitor (self);

  g_signal_emit_by_name (self, "provider-added", provider);
}

static void
gtd_plugin_todo_txt_source_changed_cb (GtkWidget *preference_panel,
                                       gpointer   user_data)
{
  GtdPluginTodoTxt *self;
  GtdProviderTodoTxt *provider;

  self = GTD_PLUGIN_TODO_TXT (user_data);

  g_clear_object (&self->monitor);
  g_free (self->source);
  g_clear_object (&self->source_file);

  g_settings_set_string (self->settings,
                        "file",
                         gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (self->preferences)));

  if (self->providers)
    {
      provider = self->providers->data;

      g_list_free_full (self->providers, g_object_unref);
      self->providers = NULL;

      g_signal_emit_by_name (self, "provider-removed", provider);
    }

  gtd_plugin_todo_txt_source_changed_finished_cb (self);
}


static void
gtd_plugin_todo_txt_finalize (GObject *object)
{
  GtdPluginTodoTxt *self = (GtdPluginTodoTxt *) object;

  g_clear_object (&self->monitor);
  g_free (self->source);
  g_clear_object (&self->source_file);
  g_list_free_full (self->providers, g_object_unref);
  self->providers = NULL;

  G_OBJECT_CLASS (gtd_plugin_todo_txt_parent_class)->finalize (object);
}

static void
gtd_plugin_todo_txt_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtdPluginTodoTxt *self = GTD_PLUGIN_TODO_TXT (object);
  switch (prop_id)
    {
    case PROP_PREFERENCES_PANEL:
      g_value_set_object (value, self->preferences);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_plugin_todo_txt_class_init (GtdPluginTodoTxtClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize     = gtd_plugin_todo_txt_finalize;
  object_class->get_property = gtd_plugin_todo_txt_get_property;

  g_object_class_override_property (object_class,
                                    PROP_PREFERENCES_PANEL,
                                    "preferences-panel");
}

static void
gtd_plugin_todo_txt_init (GtdPluginTodoTxt *self)
{
  GtkWidget *label, *frame;

  self->settings = g_settings_new ("org.gnome.todo.plugins.todo-txt");
  self->source = g_settings_get_string (self->settings, "file");
  self->providers = NULL;

  /* Preferences */
  self->preferences_box = g_object_new (GTK_TYPE_BOX,
                                        "margin", 18,
                                        "spacing", 12,
                                        "expand", TRUE,
                                        "orientation", GTK_ORIENTATION_VERTICAL,
                                        NULL);
  label = gtk_label_new (_("Select a Todo.txt-formatted file:"));
  frame = gtk_frame_new (NULL);
  self->preferences = gtk_file_chooser_widget_new (GTK_FILE_CHOOSER_ACTION_SAVE);

  gtk_container_add (GTK_CONTAINER (self->preferences_box), label);
  gtk_container_add (GTK_CONTAINER (self->preferences_box), frame);
  gtk_container_add (GTK_CONTAINER (frame), self->preferences);

  gtk_widget_show_all (self->preferences_box);

  g_signal_connect (self->preferences,
                    "file-activated",
                    G_CALLBACK (gtd_plugin_todo_txt_source_changed_cb),
                    self);
}

/* Empty class_finalize method */
static void
gtd_plugin_todo_txt_class_finalize (GtdPluginTodoTxtClass *klass)
{

}

G_MODULE_EXPORT void
gtd_plugin_todo_txt_register_types (PeasObjectModule *module)
{
  gtd_plugin_todo_txt_register_type (G_TYPE_MODULE (module));

  peas_object_module_register_extension_type (module,
                                              GTD_TYPE_ACTIVATABLE,
                                              GTD_TYPE_PLUGIN_TODO_TXT);
}
