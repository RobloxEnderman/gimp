/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpwidgets.c
 * Copyright (C) 2000 Michael Natterer <mitch@gimp.org>
 *
 * This library is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <lcms2.h>

#include <gegl.h>
#include <gtk/gtk.h>

#ifdef G_OS_WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <icm.h>
#endif

#ifdef GDK_WINDOWING_QUARTZ
#include <Carbon/Carbon.h>
#include <ApplicationServices/ApplicationServices.h>
#include <CoreServices/CoreServices.h>
#endif

#include "libgimpcolor/gimpcolor.h"
#include "libgimpconfig/gimpconfig.h"

#include "gimpwidgetstypes.h"

#include "gimpsizeentry.h"
#include "gimpwidgetsutils.h"

#include "libgimp/libgimp-intl.h"


/**
 * SECTION: gimpwidgetsutils
 * @title: GimpWidgetsUtils
 * @short_description: A collection of helper functions.
 *
 * A collection of helper functions.
 **/


static GtkWidget *
find_mnemonic_widget (GtkWidget *widget,
                      gint       level)
{
  gboolean can_focus;

  g_object_get (widget, "can-focus", &can_focus, NULL);

  if (GTK_WIDGET_GET_CLASS (widget)->activate_signal ||
      can_focus                                      ||
      GTK_WIDGET_GET_CLASS (widget)->mnemonic_activate !=
      GTK_WIDGET_CLASS (g_type_class_peek (GTK_TYPE_WIDGET))->mnemonic_activate)
    {
      return widget;
    }

  if (GIMP_IS_SIZE_ENTRY (widget))
    {
      GimpSizeEntry *entry    = GIMP_SIZE_ENTRY (widget);
      gint           n_fields = gimp_size_entry_get_n_fields (entry);

      return gimp_size_entry_get_help_widget (entry, n_fields - 1);
    }
  else if (GTK_IS_CONTAINER (widget))
    {
      GtkWidget *mnemonic_widget = NULL;
      GList     *children;
      GList     *list;

      children = gtk_container_get_children (GTK_CONTAINER (widget));

      for (list = children; list; list = g_list_next (list))
        {
          mnemonic_widget = find_mnemonic_widget (list->data, level + 1);

          if (mnemonic_widget)
            break;
        }

      g_list_free (children);

      return mnemonic_widget;
    }

  return NULL;
}

/**
 * gimp_event_triggers_context_menu:
 * @event:      The #GdkEvent to verify.
 * @on_release: Whether a menu is triggered on a button release event
 *              instead of a press event.
 *
 * Alternative of gdk_event_triggers_context_menu() with the additional
 * feature of allowing a menu triggering to happen on a button release
 * event. All the other rules on whether @event should trigger a
 * contextual menu are exactly the same. Only the swapping to release
 * state as additional feature is different.
 *
 * Returns: %TRUE if the event should trigger a context menu.
 *
 * Since: 3.0
 **/
gboolean
gimp_event_triggers_context_menu (const GdkEvent *event,
                                  gboolean        on_release)
{
  GdkEvent *copy_event;
  gboolean  trigger = FALSE;

  g_return_val_if_fail (event != NULL, FALSE);

  copy_event = gdk_event_copy (event);

  /* It's an ugly trick because GDK only considers press events as
   * menu-triggering events. We want to use the same per-platform
   * conventions as set in GTK/GDK, except for this small point.
   * So when we want a menu-triggering on release events, we just
   * temporary simulate the event to be a PRESS event and pass it to
   * the usual function.
   */
  if (on_release)
    {
      if (event->type == GDK_BUTTON_RELEASE)
        copy_event->type = GDK_BUTTON_PRESS;
      else if (event->type == GDK_BUTTON_PRESS)
        copy_event->type = GDK_BUTTON_RELEASE;
    }
  trigger = gdk_event_triggers_context_menu (copy_event);

  gdk_event_free (copy_event);

  return trigger;
}

/**
 * gimp_grid_attach_aligned:
 * @grid:           The #GtkGrid the widgets will be attached to.
 * @left:           The column to start with.
 * @top:            The row to attach the widgets.
 * @label_text:     The text for the #GtkLabel which will be attached left of
 *                  the widget.
 * @label_xalign:   The horizontal alignment of the #GtkLabel.
 * @label_yalign:   The vertical alignment of the #GtkLabel.
 * @widget:         The #GtkWidget to attach right of the label.
 * @widget_columns: The number of columns the widget will use.
 *
 * Note that the @label_text can be %NULL and that the widget will be
 * attached starting at (@column + 1) in this case, too.
 *
 * Returns: (transfer none): The created #GtkLabel.
 **/
GtkWidget *
gimp_grid_attach_aligned (GtkGrid     *grid,
                          gint         left,
                          gint         top,
                          const gchar *label_text,
                          gfloat       label_xalign,
                          gfloat       label_yalign,
                          GtkWidget   *widget,
                          gint         widget_columns)
{
  GtkWidget *label = NULL;

  if (label_text)
    {
      GtkWidget *mnemonic_widget;

      label = gtk_label_new_with_mnemonic (label_text);
      gtk_label_set_xalign (GTK_LABEL (label), label_xalign);
      gtk_label_set_yalign (GTK_LABEL (label), label_yalign);
      gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
      gtk_grid_attach (grid, label, left, top, 1, 1);
      gtk_widget_show (label);

      mnemonic_widget = find_mnemonic_widget (widget, 0);

      if (mnemonic_widget)
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), mnemonic_widget);
    }

  gtk_widget_set_hexpand (widget, TRUE);
  gtk_grid_attach (grid, widget, left + 1, top, widget_columns, 1);
  gtk_widget_show (widget);

  return label;
}

/**
 * gimp_label_set_attributes: (skip)
 * @label: a #GtkLabel
 * @...:   a list of PangoAttrType and value pairs terminated by -1.
 *
 * Sets Pango attributes on a #GtkLabel in a more convenient way than
 * gtk_label_set_attributes().
 *
 * This function is useful if you want to change the font attributes
 * of a #GtkLabel. This is an alternative to using PangoMarkup which
 * is slow to parse and awkward to handle in an i18n-friendly way.
 *
 * The attributes are set on the complete label, from start to end. If
 * you need to set attributes on part of the label, you will have to
 * use the PangoAttributes API directly.
 *
 * Since: 2.2
 **/
void
gimp_label_set_attributes (GtkLabel *label,
                           ...)
{
  PangoAttribute *attr  = NULL;
  PangoAttrList  *attrs;
  va_list         args;

  g_return_if_fail (GTK_IS_LABEL (label));

  attrs = pango_attr_list_new ();

  va_start (args, label);

  do
    {
      PangoAttrType attr_type = va_arg (args, PangoAttrType);

      if (attr_type == -1)
        attr_type = PANGO_ATTR_INVALID;

      switch (attr_type)
        {
        case PANGO_ATTR_LANGUAGE:
          attr = pango_attr_language_new (va_arg (args, PangoLanguage *));
          break;

        case PANGO_ATTR_FAMILY:
          attr = pango_attr_family_new (va_arg (args, const gchar *));
          break;

        case PANGO_ATTR_STYLE:
          attr = pango_attr_style_new (va_arg (args, PangoStyle));
          break;

        case PANGO_ATTR_WEIGHT:
          attr = pango_attr_weight_new (va_arg (args, PangoWeight));
          break;

        case PANGO_ATTR_VARIANT:
          attr = pango_attr_variant_new (va_arg (args, PangoVariant));
          break;

        case PANGO_ATTR_STRETCH:
          attr = pango_attr_stretch_new (va_arg (args, PangoStretch));
          break;

        case PANGO_ATTR_SIZE:
          attr = pango_attr_size_new (va_arg (args, gint));
          break;

        case PANGO_ATTR_FONT_DESC:
          attr = pango_attr_font_desc_new (va_arg (args,
                                                   const PangoFontDescription *));
          break;

        case PANGO_ATTR_FOREGROUND:
          {
            const PangoColor *color = va_arg (args, const PangoColor *);

            attr = pango_attr_foreground_new (color->red,
                                              color->green,
                                              color->blue);
          }
          break;

        case PANGO_ATTR_BACKGROUND:
          {
            const PangoColor *color = va_arg (args, const PangoColor *);

            attr = pango_attr_background_new (color->red,
                                              color->green,
                                              color->blue);
          }
          break;

        case PANGO_ATTR_UNDERLINE:
          attr = pango_attr_underline_new (va_arg (args, PangoUnderline));
          break;

        case PANGO_ATTR_STRIKETHROUGH:
          attr = pango_attr_strikethrough_new (va_arg (args, gboolean));
          break;

        case PANGO_ATTR_RISE:
          attr = pango_attr_rise_new (va_arg (args, gint));
          break;

        case PANGO_ATTR_SCALE:
          attr = pango_attr_scale_new (va_arg (args, gdouble));
          break;

        default:
          g_warning ("%s: invalid PangoAttribute type %d",
                     G_STRFUNC, attr_type);
        case PANGO_ATTR_INVALID:
          attr = NULL;
          break;
        }

      if (attr)
        {
          attr->start_index = 0;
          attr->end_index   = -1;
          pango_attr_list_insert (attrs, attr);
        }
    }
  while (attr);

  va_end (args);

  gtk_label_set_attributes (label, attrs);
  pango_attr_list_unref (attrs);
}

/**
 * gimp_widget_get_monitor:
 * @widget: a #GtkWidget.
 *
 * Returns: (transfer none): the #GdkMonitor where @widget is current displayed on.
 */
GdkMonitor *
gimp_widget_get_monitor (GtkWidget *widget)
{
  GdkWindow     *window;
  GtkAllocation  allocation;
  gint           x, y;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), 0);

  window = gtk_widget_get_window (widget);

  if (! window)
    return gimp_get_monitor_at_pointer ();

  gdk_window_get_origin (window, &x, &y);
  gtk_widget_get_allocation (widget, &allocation);

  if (! gtk_widget_get_has_window (widget))
    {
      x += allocation.x;
      y += allocation.y;
    }

  x += allocation.width  / 2;
  y += allocation.height / 2;

  return gdk_display_get_monitor_at_point (gdk_display_get_default (), x, y);
}

/**
 * gimp_get_monitor_at_pointer:
 *
 * Returns: (transfer none): the #GdkMonitor where the pointer is.
 */
GdkMonitor *
gimp_get_monitor_at_pointer (void)
{
  GdkDisplay *display;
  GdkSeat    *seat;
  gint        x, y;

  display = gdk_display_get_default ();
  seat = gdk_display_get_default_seat (display);

  gdk_device_get_position (gdk_seat_get_pointer (seat),
                           NULL, &x, &y);

  return gdk_display_get_monitor_at_point (display, x, y);
}

typedef void (* MonitorChangedCallback) (GtkWidget *, gpointer);

typedef struct
{
  GtkWidget  *widget;
  GdkMonitor *monitor;

  MonitorChangedCallback callback;
  gpointer               user_data;
} TrackMonitorData;

static gboolean
track_monitor_configure_event (GtkWidget        *toplevel,
                               GdkEvent         *event,
                               TrackMonitorData *track_data)
{
  GdkMonitor *monitor = gimp_widget_get_monitor (toplevel);

  if (monitor != track_data->monitor)
    {
      track_data->monitor = monitor;

      track_data->callback (track_data->widget, track_data->user_data);
    }

  return FALSE;
}

static void
track_monitor_hierarchy_changed (GtkWidget        *widget,
                                 GtkWidget        *previous_toplevel,
                                 TrackMonitorData *track_data)
{
  GtkWidget *toplevel;

  if (previous_toplevel)
    {
      g_signal_handlers_disconnect_by_func (previous_toplevel,
                                            track_monitor_configure_event,
                                            track_data);
    }

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    {
      GClosure   *closure;
      GdkMonitor *monitor;

      closure = g_cclosure_new (G_CALLBACK (track_monitor_configure_event),
                                track_data, NULL);
      g_object_watch_closure (G_OBJECT (widget), closure);
      g_signal_connect_closure (toplevel, "configure-event", closure, FALSE);

      monitor = gimp_widget_get_monitor (toplevel);

      if (monitor != track_data->monitor)
        {
          track_data->monitor = monitor;

          track_data->callback (track_data->widget, track_data->user_data);
        }
    }
}

/**
 * gimp_widget_track_monitor:
 * @widget:                   a #GtkWidget
 * @monitor_changed_callback: the callback when @widget's monitor changes
 * @user_data:                data passed to @monitor_changed_callback
 * @user_data_destroy:        destroy function for @user_data.
 *
 * This function behaves as if #GtkWidget had a signal
 *
 * GtkWidget::monitor_changed(GtkWidget *widget, gpointer user_data)
 *
 * That is emitted whenever @widget's toplevel window is moved from
 * one monitor to another. This function automatically connects to
 * the right toplevel #GtkWindow, even across moving @widget between
 * toplevel windows.
 *
 * Note that this function tracks the toplevel, not @widget itself, so
 * all a window's widgets are always considered to be on the same
 * monitor. This is because this function is mainly used for fetching
 * the new monitor's color profile, and it makes little sense to use
 * different profiles for the widgets of one window.
 *
 * Since: 2.10
 **/
void
gimp_widget_track_monitor (GtkWidget      *widget,
                           GCallback       monitor_changed_callback,
                           gpointer        user_data,
                           GDestroyNotify  user_data_destroy)
{
  TrackMonitorData *track_data;
  GtkWidget        *toplevel;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (monitor_changed_callback != NULL);

  track_data = g_new0 (TrackMonitorData, 1);

  track_data->widget    = widget;
  track_data->callback  = (MonitorChangedCallback) monitor_changed_callback;
  track_data->user_data = user_data;

  g_object_weak_ref (G_OBJECT (widget), (GWeakNotify) g_free,
                     track_data);

  if (user_data_destroy)
    g_object_weak_ref (G_OBJECT (widget), (GWeakNotify) user_data_destroy,
                       user_data);

  g_signal_connect (widget, "hierarchy-changed",
                    G_CALLBACK (track_monitor_hierarchy_changed),
                    track_data);

  toplevel = gtk_widget_get_toplevel (widget);

  if (GTK_IS_WINDOW (toplevel))
    track_monitor_hierarchy_changed (widget, NULL, track_data);
}

static gint
monitor_number (GdkMonitor *monitor)
{
  GdkDisplay *display    = gdk_monitor_get_display (monitor);
  gint        n_monitors = gdk_display_get_n_monitors (display);
  gint        i;

  for (i = 0; i < n_monitors; i++)
    if (gdk_display_get_monitor (display, i) == monitor)
      return i;

  return 0;
}

/**
 * gimp_monitor_get_color_profile:
 * @monitor: a #GdkMonitor
 *
 * This function returns the #GimpColorProfile of @monitor
 * or %NULL if there is no profile configured.
 *
 * Since: 3.0
 *
 * Returns: (nullable) (transfer full): the monitor's #GimpColorProfile,
 *          or %NULL.
 **/
GimpColorProfile *
gimp_monitor_get_color_profile (GdkMonitor *monitor)
{
  GimpColorProfile *profile = NULL;

  g_return_val_if_fail (GDK_IS_MONITOR (monitor), NULL);

#if defined GDK_WINDOWING_X11
  {
    GdkDisplay *display;
    GdkScreen  *screen;
    GdkAtom     type    = GDK_NONE;
    gint        format  = 0;
    gint        nitems  = 0;
    gint        number;
    gchar      *atom_name;
    guchar     *data    = NULL;

    display = gdk_monitor_get_display (monitor);
    number  = monitor_number (monitor);

    if (number > 0)
      atom_name = g_strdup_printf ("_ICC_PROFILE_%d", number);
    else
      atom_name = g_strdup ("_ICC_PROFILE");

    screen = gdk_display_get_default_screen (display);

    if (gdk_property_get (gdk_screen_get_root_window (screen),
                          gdk_atom_intern (atom_name, FALSE),
                          GDK_NONE,
                          0, 64 * 1024 * 1024, FALSE,
                          &type, &format, &nitems, &data) && nitems > 0)
      {
        profile = gimp_color_profile_new_from_icc_profile (data, nitems,
                                                           NULL);
        g_free (data);
      }

    g_free (atom_name);
  }
#elif defined GDK_WINDOWING_QUARTZ
  {
    CGColorSpaceRef space = NULL;

    space = CGDisplayCopyColorSpace (monitor_number (monitor));

    if (space)
      {
        CFDataRef data;

        data = CGColorSpaceCopyICCData (space);

        if (data)
          {
            UInt8 *buffer = g_malloc (CFDataGetLength (data));

            /* We cannot use CFDataGetBytesPtr(), because that returns
             * a const pointer where cmsOpenProfileFromMem wants a
             * non-const pointer.
             */
            CFDataGetBytes (data, CFRangeMake (0, CFDataGetLength (data)),
                            buffer);

            profile = gimp_color_profile_new_from_icc_profile (buffer,
                                                               CFDataGetLength (data),
                                                               NULL);

            g_free (buffer);
            CFRelease (data);
          }

        CFRelease (space);
      }
  }
#elif defined G_OS_WIN32
  {
    GdkRectangle    monitor_geometry;
    gint            scale_factor;
    POINT           point;
    gint            offsetx = GetSystemMetrics (SM_XVIRTUALSCREEN);
    gint            offsety = GetSystemMetrics (SM_YVIRTUALSCREEN);
    HMONITOR        monitor_handle;
    MONITORINFOEXW  info;
    DISPLAY_DEVICEW display_device;

    info.cbSize       = sizeof (info);
    display_device.cb = sizeof (display_device);

    /* If the first monitor is not set as the main monitor,
     * monitor_number(monitor) may not match the index used in
     * EnumDisplayDevices(devicename, index, displaydevice, flags).
     */
    gdk_monitor_get_geometry (monitor, &monitor_geometry);
    scale_factor = gdk_monitor_get_scale_factor (monitor);
    point.x = monitor_geometry.x * scale_factor + offsetx;
    point.y = monitor_geometry.y * scale_factor + offsety;
    monitor_handle = MonitorFromPoint (point, MONITOR_DEFAULTTONEAREST);

    if (GetMonitorInfoW (monitor_handle, (LPMONITORINFO)&info))
      {
        if (EnumDisplayDevicesW (info.szDevice, 0, &display_device, 0))
          {
            wchar_t                      *device_key = display_device.DeviceKey;
            wchar_t                      *filename_utf16 = NULL;
            char                         *filename       = NULL;
            wchar_t                      *dir_utf16      = NULL;
            char                         *dir            = NULL;
            char                         *fullpath       = NULL;
            GFile                        *file           = NULL;
            DWORD                         len            = 0;
            gboolean                      per_user;
            WCS_PROFILE_MANAGEMENT_SCOPE  scope;

            WcsGetUsePerUserProfiles (device_key, CLASS_MONITOR, &per_user);
            scope = per_user ? WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER :
                               WCS_PROFILE_MANAGEMENT_SCOPE_SYSTEM_WIDE;

            if (WcsGetDefaultColorProfileSize (scope,
                                               device_key,
                                               CPT_ICC,
                                               CPST_NONE,
                                               0,
                                               &len))
              {
                filename_utf16 = (wchar_t*) g_malloc0 (len);

                WcsGetDefaultColorProfile (scope,
                                           device_key,
                                           CPT_ICC,
                                           CPST_NONE,
                                           0,
                                           len,
                                           filename_utf16);
              }
            else
              {
                /* Due to a bug in Windows, the meanings of LCS_sRGB and
                 * LCS_WINDOWS_COLOR_SPACE are swapped.
                 */
                if (GetStandardColorSpaceProfileW (NULL, LCS_sRGB, NULL, &len) != 0 && len > 0)
                  {
                    filename_utf16 = (wchar_t*) g_malloc0 (len);
                    GetStandardColorSpaceProfileW (NULL, LCS_sRGB, filename_utf16, &len);
                  }
              }

            filename = g_utf16_to_utf8 (filename_utf16, -1, NULL, NULL, NULL);

            GetColorDirectoryW (NULL, NULL, &len);
            dir_utf16 = g_malloc0 (len);
            GetColorDirectoryW (NULL, dir_utf16, &len);

            dir = g_utf16_to_utf8 (dir_utf16, -1, NULL, NULL, NULL);

            fullpath = g_build_filename (dir, filename, NULL);
            file = g_file_new_for_path (fullpath);

            profile = gimp_color_profile_new_from_file (file, NULL);
            g_object_unref (file);

            g_free (fullpath);
            g_free (dir);
            g_free (dir_utf16);
            g_free (filename);
            g_free (filename_utf16);
            g_free (device_key);
          }
      }
  }
#endif

  return profile;
}

/**
 * gimp_widget_get_color_profile:
 * @widget: a #GtkWidget
 *
 * This function returns the #GimpColorProfile of the monitor @widget is
 * currently displayed on, or %NULL if there is no profile configured.
 *
 * Since: 3.0
 *
 * Returns: (nullable) (transfer full): @widget's monitor's #GimpColorProfile,
 *          or %NULL.
 **/
GimpColorProfile *
gimp_widget_get_color_profile (GtkWidget *widget)
{
  GdkMonitor *monitor;

  g_return_val_if_fail (widget == NULL || GTK_IS_WIDGET (widget), NULL);

  if (widget)
    {
      monitor = gimp_widget_get_monitor (widget);
    }
  else
    {
      monitor = gdk_display_get_monitor (gdk_display_get_default (), 0);
    }

  return gimp_monitor_get_color_profile (monitor);
}

static GimpColorProfile *
get_display_profile (GtkWidget       *widget,
                     GimpColorConfig *config)
{
  GimpColorProfile *profile = NULL;

  if (gimp_color_config_get_display_profile_from_gdk (config))
    /* get the toplevel's profile so all a window's colors look the same */
    profile = gimp_widget_get_color_profile (gtk_widget_get_toplevel (widget));

  if (! profile)
    profile = gimp_color_config_get_display_color_profile (config, NULL);

  if (! profile)
    profile = gimp_color_profile_new_rgb_srgb ();

  return profile;
}

typedef struct _TransformCache TransformCache;

struct _TransformCache
{
  GimpColorTransform *transform;

  GimpColorConfig         *config;
  GimpColorProfile        *src_profile;
  const Babl              *src_format;
  GimpColorProfile        *dest_profile;
  const Babl              *dest_format;
  GimpColorProfile        *proof_profile;
  GimpColorRenderingIntent proof_intent;
  gboolean                 proof_bpc;

  gulong              notify_id;
};

static GList    *transform_caches = NULL;
static gboolean  debug_cache      = FALSE;

static gboolean
profiles_equal (GimpColorProfile *profile1,
                GimpColorProfile *profile2)
{
  return ((profile1 == NULL && profile2 == NULL) ||
          (profile1 != NULL && profile2 != NULL &&
           gimp_color_profile_is_equal (profile1, profile2)));
}

static TransformCache *
transform_cache_get (GimpColorConfig         *config,
                     GimpColorProfile        *src_profile,
                     const Babl              *src_format,
                     GimpColorProfile        *dest_profile,
                     const Babl              *dest_format,
                     GimpColorProfile        *proof_profile,
                     GimpColorRenderingIntent proof_intent,
                     gboolean                 proof_bpc)
{
  GList *list;

  for (list = transform_caches; list; list = g_list_next (list))
    {
      TransformCache *cache = list->data;

      if (config       == cache->config                        &&
          src_format   == cache->src_format                    &&
          dest_format  == cache->dest_format                   &&
          profiles_equal (src_profile,   cache->src_profile)   &&
          profiles_equal (dest_profile,  cache->dest_profile)  &&
          profiles_equal (proof_profile, cache->proof_profile) &&
          proof_intent == cache->proof_intent                  &&
          proof_bpc    == cache->proof_bpc)
        {
          if (debug_cache)
            g_printerr ("found cache %p\n", cache);

          return cache;
        }
    }

  return NULL;
}

static void
transform_cache_config_notify (GObject          *config,
                               const GParamSpec *pspec,
                               TransformCache   *cache)
{
  transform_caches = g_list_remove (transform_caches, cache);

  g_signal_handler_disconnect (config, cache->notify_id);

  if (cache->transform)
    g_object_unref (cache->transform);

  g_object_unref (cache->src_profile);
  g_object_unref (cache->dest_profile);

  if (cache->proof_profile)
    g_object_unref (cache->proof_profile);

  if (debug_cache)
    g_printerr ("deleted cache %p\n", cache);

  g_free (cache);
}

/**
 * gimp_widget_get_color_transform:
 * @widget:      a #GtkWidget
 * @config:      a #GimpColorConfig
 * @src_profile: a #GimpColorProfile
 * @src_format:  Babl format for the transform's source pixels
 * @dest_format: Babl format for the transforms's destination pixels
 *
 * This function returns the #GimpColorTransform that transforms pixels
 * from @src_profile to the profile of the #GdkMonitor the @widget is
 * displayed on.
 *
 * Returns: (nullable) (transfer full): the #GimpColorTransform.
 *
 * Since: 2.10
 **/
GimpColorTransform *
gimp_widget_get_color_transform (GtkWidget               *widget,
                                 GimpColorConfig         *config,
                                 GimpColorProfile        *src_profile,
                                 const Babl              *src_format,
                                 const Babl              *dest_format,
                                 GimpColorProfile        *softproof_profile,
                                 GimpColorRenderingIntent proof_intent,
                                 gboolean                 proof_bpc)
{
  static gboolean     initialized   = FALSE;
  GimpColorProfile   *proof_profile = NULL;
  GimpColorProfile   *dest_profile  = NULL;
  TransformCache     *cache;

  g_return_val_if_fail (widget == NULL || GTK_IS_WIDGET (widget), NULL);
  g_return_val_if_fail (GIMP_IS_COLOR_CONFIG (config), NULL);
  g_return_val_if_fail (GIMP_IS_COLOR_PROFILE (src_profile), NULL);
  g_return_val_if_fail (src_format != NULL, NULL);
  g_return_val_if_fail (dest_format != NULL, NULL);

  if (G_UNLIKELY (! initialized))
    {
      initialized = TRUE;

      debug_cache = g_getenv ("GIMP_DEBUG_TRANSFORM_CACHE") != NULL;
    }

  switch (gimp_color_config_get_mode (config))
    {
    case GIMP_COLOR_MANAGEMENT_OFF:
      return NULL;

    case GIMP_COLOR_MANAGEMENT_SOFTPROOF:
      if (! softproof_profile)
        proof_profile = gimp_color_config_get_simulation_color_profile (config,
                                                                        NULL);
      else
        proof_profile = g_object_ref (softproof_profile);
      /*  fallthru  */

    case GIMP_COLOR_MANAGEMENT_DISPLAY:
      dest_profile = get_display_profile (widget, config);
      break;
    }

  cache = transform_cache_get (config,
                               src_profile,
                               src_format,
                               dest_profile,
                               dest_format,
                               proof_profile,
                               proof_intent,
                               proof_bpc);

  if (cache)
    {
      g_object_unref (dest_profile);

      if (proof_profile)
        g_object_unref (proof_profile);

      if (cache->transform)
        return g_object_ref (cache->transform);

      return NULL;
    }

  if (! proof_profile &&
      gimp_color_profile_is_equal (src_profile, dest_profile))
    {
      g_object_unref (dest_profile);

      return NULL;
    }

  cache = g_new0 (TransformCache, 1);

  if (debug_cache)
    g_printerr ("creating cache %p\n", cache);

  cache->config        = g_object_ref (config);
  cache->src_profile   = g_object_ref (src_profile);
  cache->src_format    = src_format;
  cache->dest_profile  = dest_profile;
  cache->dest_format   = dest_format;
  cache->proof_profile = proof_profile;
  cache->proof_intent  = proof_intent;
  cache->proof_bpc     = proof_bpc;

  cache->notify_id =
    g_signal_connect (cache->config, "notify",
                      G_CALLBACK (transform_cache_config_notify),
                      cache);

  transform_caches = g_list_prepend (transform_caches, cache);

  if (cache->proof_profile)
    {
      GimpColorTransformFlags flags = 0;

      if (proof_bpc)
        flags |= GIMP_COLOR_TRANSFORM_FLAGS_BLACK_POINT_COMPENSATION;

      if (! gimp_color_config_get_simulation_optimize (config))
        flags |= GIMP_COLOR_TRANSFORM_FLAGS_NOOPTIMIZE;

      if (gimp_color_config_get_simulation_gamut_check (config))
        {
          GimpRGB         color;
          cmsUInt16Number alarmCodes[cmsMAXCHANNELS] = { 0, };
          guchar          r, g, b;

          flags |= GIMP_COLOR_TRANSFORM_FLAGS_GAMUT_CHECK;

          gimp_color_config_get_out_of_gamut_color (config, &color);
          gimp_rgb_get_uchar (&color, &r, &g, &b);

          alarmCodes[0] = (cmsUInt16Number) r * 256;
          alarmCodes[1] = (cmsUInt16Number) g * 256;
          alarmCodes[2] = (cmsUInt16Number) b * 256;

          cmsSetAlarmCodes (alarmCodes);
        }

      cache->transform =
        gimp_color_transform_new_proofing (cache->src_profile,
                                           cache->src_format,
                                           cache->dest_profile,
                                           cache->dest_format,
                                           cache->proof_profile,
                                           cache->proof_intent,
                                           gimp_color_config_get_display_intent (config),
                                           flags);
    }
  else
    {
      GimpColorTransformFlags flags = 0;

      if (gimp_color_config_get_display_bpc (config))
        flags |= GIMP_COLOR_TRANSFORM_FLAGS_BLACK_POINT_COMPENSATION;

      if (! gimp_color_config_get_display_optimize (config))
        flags |= GIMP_COLOR_TRANSFORM_FLAGS_NOOPTIMIZE;

      cache->transform =
        gimp_color_transform_new (cache->src_profile,
                                  cache->src_format,
                                  cache->dest_profile,
                                  cache->dest_format,
                                  gimp_color_config_get_display_intent (config),
                                  flags);
    }

  if (cache->transform)
    return g_object_ref (cache->transform);

  return NULL;
}
