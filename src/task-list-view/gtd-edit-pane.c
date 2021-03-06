/* gtd-edit-pane.c
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
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

#define G_LOG_DOMAIN "GtdEditPane"

#include "gtd-debug.h"
#include "gtd-edit-pane.h"
#include "gtd-manager.h"
#include "gtd-markdown-renderer.h"
#include "gtd-task.h"
#include "gtd-task-list.h"

#include <glib/gi18n.h>

struct _GtdEditPane
{
  GtkBox             parent;

  GtkCalendar       *calendar;
  GtkLabel          *date_label;
  GtkTextView       *notes_textview;

  /* task bindings */
  GBinding          *notes_binding;

  GtdTask           *task;
};

G_DEFINE_TYPE (GtdEditPane, gtd_edit_pane, GTK_TYPE_BOX)

enum
{
  PROP_0,
  PROP_TASK,
  LAST_PROP
};

enum
{
  CHANGED,
  REMOVE_TASK,
  NUM_SIGNALS
};


static guint signals[NUM_SIGNALS] = { 0, };


static void          on_date_selected_cb                         (GtkCalendar        *calendar,
                                                                  GtdEditPane        *self);

/*
 * Auxiliary methods
 */

static void
update_date_widgets (GtdEditPane *self)
{
  GDateTime *dt;
  gchar *text;

  g_return_if_fail (GTD_IS_EDIT_PANE (self));

  dt = self->task ? gtd_task_get_due_date (self->task) : NULL;
  text = dt ? g_date_time_format (dt, "%x") : NULL;

  g_signal_handlers_block_by_func (self->calendar, on_date_selected_cb, self);

  if (dt)
    {
      gtk_calendar_select_month (self->calendar,
                                 g_date_time_get_month (dt) - 1,
                                 g_date_time_get_year (dt));

      gtk_calendar_select_day (self->calendar, g_date_time_get_day_of_month (dt));

    }
  else
    {
      GDateTime *today;

      today = g_date_time_new_now_local ();

      gtk_calendar_select_month (self->calendar,
                                 g_date_time_get_month (today) - 1,
                                 g_date_time_get_year (today));
      gtk_calendar_select_day (self->calendar,
                               g_date_time_get_day_of_month (today));

      g_clear_pointer (&today, g_date_time_unref);
    }

  g_signal_handlers_unblock_by_func (self->calendar, on_date_selected_cb, self);

  gtk_label_set_label (self->date_label, text ? text : _("No date set"));

  g_free (text);
}


/*
 * Callbacks
 */

static void
on_date_selected_cb (GtkCalendar *calendar,
                     GtdEditPane *self)
{
  g_autoptr (GDateTime) new_dt = NULL;
  g_autofree gchar *text = NULL;
  guint year;
  guint month;
  guint day;

  GTD_ENTRY;

  gtk_calendar_get_date (calendar,
                         &year,
                         &month,
                         &day);

  new_dt = g_date_time_new_local (year,
                                  month + 1,
                                  day,
                                  0,
                                  0,
                                  0);

  text = g_date_time_format (new_dt, "%x");

  gtd_task_set_due_date (self->task, new_dt);
  gtk_label_set_label (self->date_label, text);

  g_signal_emit (self, signals[CHANGED], 0);

  GTD_EXIT;
}

static void
on_delete_button_clicked_cb (GtkButton   *button,
                             GtdEditPane *self)
{
  g_signal_emit (self, signals[REMOVE_TASK], 0, self->task);
}

static void
on_no_date_button_clicked_cb (GtkButton   *button,
                              GtdEditPane *self)
{
  GTD_ENTRY;

  gtd_task_set_due_date (self->task, NULL);
  gtk_calendar_clear_marks (GTK_CALENDAR (self->calendar));
  update_date_widgets (self);

  g_signal_emit (self, signals[CHANGED], 0);

  GTD_EXIT;
}

static void
on_today_button_clicked_cb (GtkButton   *button,
                            GtdEditPane *self)
{
  g_autoptr (GDateTime) new_dt = g_date_time_new_now_local ();

  GTD_ENTRY;

  gtd_task_set_due_date (self->task, new_dt);
  update_date_widgets (self);

  g_signal_emit (self, signals[CHANGED], 0);

  GTD_EXIT;
}

static void
on_priority_changed_cb (GtkComboBox *combobox,
                        GtdEditPane *self)
{
  GTD_ENTRY;

  g_signal_emit (self, signals[CHANGED], 0);

  GTD_EXIT;
}

static void
on_tomorrow_button_clicked_cb (GtkButton   *button,
                               GtdEditPane *self)
{
  g_autoptr (GDateTime) current_date = NULL;
  g_autoptr (GDateTime) new_dt = NULL;

  GTD_ENTRY;

  current_date = g_date_time_new_now_local ();
  new_dt = g_date_time_add_days (current_date, 1);

  gtd_task_set_due_date (self->task, new_dt);
  update_date_widgets (self);

  g_signal_emit (self, signals[CHANGED], 0);

  GTD_EXIT;
}

static void
on_text_buffer_changed_cb (GtkTextBuffer *buffer,
                           GParamSpec    *pspec,
                           GtdEditPane   *self)
{
  GTD_ENTRY;

  g_signal_emit (self, signals[CHANGED], 0);

  GTD_EXIT;
}

static void
on_hyperlink_hover_cb (GtkEventControllerMotion *controller,
                       gdouble                   x,
                       gdouble                   y,
                       GtdEditPane              *self)
{
  GtkTextView *text_view;
  GtkTextIter iter;
  gboolean hovering;
  gint ex, ey;

  text_view = GTK_TEXT_VIEW (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (controller)));

  gtk_text_view_window_to_buffer_coords (text_view, GTK_TEXT_WINDOW_WIDGET, x, y, &ex, &ey);

  hovering = FALSE;

  if (gtk_text_view_get_iter_at_location (text_view, &iter, ex, ey))
    {
      GSList *tags = NULL;
      GSList *l = NULL;

      tags = gtk_text_iter_get_tags (&iter);

      for (l = tags; l; l = l->next)
        {
          g_autofree gchar *tag_name = NULL;
          GtkTextTag *tag;

          tag = l->data;

          g_object_get (tag, "name", &tag_name, NULL);

          if (g_strcmp0 (tag_name, "url") == 0)
            {
              hovering = TRUE;
              break;
            }
        }
    }

  gtk_widget_set_cursor_from_name (GTK_WIDGET (text_view), hovering ? "pointer" : "text");
}

static void
on_hyperlink_clicked_cb (GtkGestureMultiPress *gesture,
                         gint                  n_press,
                         gdouble               x,
                         gdouble               y,
                         GtdEditPane          *self)
{
  GdkEventSequence *sequence;
  GtkTextBuffer *buffer;
  GtkTextView *text_view;
  GtkTextIter end_iter;
  GtkTextIter iter;
  GSList *tags = NULL;
  GSList *l = NULL;
  gint ex = 0;
  gint ey = 0;

  GTD_ENTRY;

  /* Claim the sequence to prevent propagating to GtkListBox and closing the row */
  sequence = gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  gtk_gesture_set_sequence_state (GTK_GESTURE (gesture), sequence, GTK_EVENT_SEQUENCE_CLAIMED);

  text_view = GTK_TEXT_VIEW (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)));
  buffer = gtk_text_view_get_buffer (text_view);

  /* We shouldn't follow a link if the user has selected something */
  if (gtk_text_buffer_get_has_selection (buffer))
    return;

  gtk_text_view_window_to_buffer_coords (text_view, GTK_TEXT_WINDOW_WIDGET, x, y, &ex, &ey);

  if (!gtk_text_view_get_iter_at_location (text_view, &iter, ex, ey))
    return;

  tags = gtk_text_iter_get_tags (&iter);

  for (l = tags; l; l = l->next)
    {
      g_autoptr (GError) error = NULL;
      g_autofree gchar *tag_name = NULL;
      g_autofree gchar *url = NULL;
      GtkTextIter url_start;
      GtkTextIter url_end;
      GtkTextTag *tag;
      GtkWindow *window;

      tag = l->data;

      g_object_get (tag, "name", &tag_name, NULL);

      if (g_strcmp0 (tag_name, "url") != 0)
        continue;

      gtk_text_buffer_get_iter_at_line (buffer, &iter, gtk_text_iter_get_line (&iter));
      end_iter = iter;
      gtk_text_iter_forward_to_line_end (&end_iter);

      /* Find the beginning... */
      if (!gtk_text_iter_forward_search (&iter, "(", GTK_TEXT_SEARCH_TEXT_ONLY, NULL, &url_start, NULL))
        continue;

      /* ... and the end of the URL */
      if (!gtk_text_iter_forward_search (&iter, ")", GTK_TEXT_SEARCH_TEXT_ONLY, &url_end, NULL, &end_iter))
        continue;

      url = gtk_text_iter_get_text (&url_start, &url_end);
      window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (text_view)));

      gtk_show_uri_on_window (window, url, GDK_CURRENT_TIME, &error);

      if (error)
        {
          g_warning ("%s", error->message);
          return;
        }
    }
}


/*
 * GObject overrides
 */

static void
gtd_edit_pane_finalize (GObject *object)
{
  GtdEditPane *self = (GtdEditPane *) object;

  g_clear_object (&self->task);

  G_OBJECT_CLASS (gtd_edit_pane_parent_class)->finalize (object);
}

static void
gtd_edit_pane_dispose (GObject *object)
{
  GtdEditPane *self = (GtdEditPane *) object;

  g_clear_object (&self->task);

  G_OBJECT_CLASS (gtd_edit_pane_parent_class)->dispose (object);
}

static void
gtd_edit_pane_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtdEditPane *self = GTD_EDIT_PANE (object);

  switch (prop_id)
    {
    case PROP_TASK:
      g_value_set_object (value, self->task);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_edit_pane_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtdEditPane *self = GTD_EDIT_PANE (object);

  switch (prop_id)
    {
    case PROP_TASK:
      self->task = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtd_edit_pane_class_init (GtdEditPaneClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtd_edit_pane_finalize;
  object_class->dispose = gtd_edit_pane_dispose;
  object_class->get_property = gtd_edit_pane_get_property;
  object_class->set_property = gtd_edit_pane_set_property;

  /**
   * GtdEditPane::task:
   *
   * The task that is actually being edited.
   */
  g_object_class_install_property (
        object_class,
        PROP_TASK,
        g_param_spec_object ("task",
                             "Task being edited",
                             "The task that is actually being edited",
                             GTD_TYPE_TASK,
                             G_PARAM_READWRITE));

  /**
   * GtdEditPane::changed:
   *
   * Emitted when the task was changed.
   */
  signals[CHANGED] = g_signal_new ("changed",
                                   GTD_TYPE_EDIT_PANE,
                                   G_SIGNAL_RUN_LAST,
                                   0,
                                   NULL,
                                   NULL,
                                   NULL,
                                   G_TYPE_NONE,
                                   0);

  /**
   * GtdEditPane::task-removed:
   *
   * Emitted when the user wants to remove the task.
   */
  signals[REMOVE_TASK] = g_signal_new ("remove-task",
                                       GTD_TYPE_EDIT_PANE,
                                       G_SIGNAL_RUN_LAST,
                                       0,
                                       NULL,
                                       NULL,
                                       NULL,
                                       G_TYPE_NONE,
                                       1,
                                       GTD_TYPE_TASK);

  /* template class */
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/todo/ui/edit-pane.ui");

  gtk_widget_class_bind_template_child (widget_class, GtdEditPane, calendar);
  gtk_widget_class_bind_template_child (widget_class, GtdEditPane, date_label);
  gtk_widget_class_bind_template_child (widget_class, GtdEditPane, notes_textview);

  gtk_widget_class_bind_template_callback (widget_class, on_date_selected_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_delete_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_hyperlink_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_hyperlink_hover_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_no_date_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_priority_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_text_buffer_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_today_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_tomorrow_button_clicked_cb);

  gtk_widget_class_set_css_name (widget_class, "editpane");
}

static void
gtd_edit_pane_init (GtdEditPane *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gtd_edit_pane_new (void)
{
  return g_object_new (GTD_TYPE_EDIT_PANE, NULL);
}

/**
 * gtd_edit_pane_get_task:
 * @self: a #GtdEditPane
 *
 * Retrieves the currently edited #GtdTask of %pane, or %NULL if none is set.
 *
 * Returns: (transfer none): the current #GtdTask being edited from %pane.
 */
GtdTask*
gtd_edit_pane_get_task (GtdEditPane *self)
{
  g_return_val_if_fail (GTD_IS_EDIT_PANE (self), NULL);

  return self->task;
}

/**
 * gtd_edit_pane_set_task:
 * @pane: a #GtdEditPane
 * @task: a #GtdTask or %NULL
 *
 * Sets %task as the currently editing task of %pane.
 */
void
gtd_edit_pane_set_task (GtdEditPane *self,
                        GtdTask     *task)
{
  g_return_if_fail (GTD_IS_EDIT_PANE (self));

  if (!g_set_object (&self->task, task))
    return;

  if (task)
    {
      GtkTextBuffer *buffer;

      buffer = gtk_text_view_get_buffer (self->notes_textview);

      g_signal_handlers_block_by_func (buffer, on_text_buffer_changed_cb, self);

      /* due date */
      update_date_widgets (self);

      /* description */
      gtk_text_buffer_set_text (buffer, gtd_task_get_description (task), -1);

      self->notes_binding = g_object_bind_property (buffer,
                                                    "text",
                                                    task,
                                                    "description",
                                                    G_BINDING_BIDIRECTIONAL);

      g_signal_handlers_unblock_by_func (buffer, on_text_buffer_changed_cb, self);

    }

  g_object_notify (G_OBJECT (self), "task");
}

void
gtd_edit_pane_set_markdown_renderer (GtdEditPane         *self,
                                     GtdMarkdownRenderer *renderer)
{
  GtkTextBuffer *buffer;

  g_assert (GTD_IS_EDIT_PANE (self));
  g_assert (GTD_IS_MARKDOWN_RENDERER (renderer));

  buffer = gtk_text_view_get_buffer (self->notes_textview);

  gtd_markdown_renderer_add_buffer (renderer, buffer);
}
