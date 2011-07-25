/*
 * Mex - a media explorer
 *
 * Copyright © 2010, 2011 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mex-gnome-dvb-plugin.h"
#include <mex/mex-grilo-feed.h>
#include <mex/mex-generic-content.h>

#include <glib/gi18n.h>

static void mex_model_provider_iface_init (MexModelProviderInterface *iface);

G_DEFINE_TYPE_WITH_CODE (MexGnomeDvbPlugin, mex_gnome_dvb_plugin,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MEX_TYPE_MODEL_PROVIDER,
                                                mex_model_provider_iface_init))

#define DVB_PLUGIN_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MEX_TYPE_GNOME_DVB_PLUGIN, MexGnomeDvbPluginPrivate))

struct _MexGnomeDvbPluginPrivate
{
  GList *models;
};


static void
mex_gnome_dvb_plugin_dispose (GObject *object)
{
  MexGnomeDvbPluginPrivate *priv = MEX_GNOME_DVB_PLUGIN (object)->priv;

  while (priv->models)
    {
      MexModelInfo *info = priv->models->data;
      mex_model_info_free (info);
      priv->models = g_list_delete_link (priv->models, priv->models);
    }

  G_OBJECT_CLASS (mex_gnome_dvb_plugin_parent_class)->dispose (object);
}

static void
mex_gnome_dvb_plugin_finalize (GObject *object)
{
  G_OBJECT_CLASS (mex_gnome_dvb_plugin_parent_class)->finalize (object);
}

static void
mex_gnome_dvb_plugin_class_init (MexGnomeDvbPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MexGnomeDvbPluginPrivate));

  object_class->dispose = mex_gnome_dvb_plugin_dispose;
  object_class->finalize = mex_gnome_dvb_plugin_finalize;
}

static const GList *
mex_gnome_dvb_plugin_get_models (MexModelProvider *self)
{
  MexGnomeDvbPluginPrivate *priv = MEX_GNOME_DVB_PLUGIN (self)->priv;

  return priv->models;
}

static void
mex_model_provider_iface_init (MexModelProviderInterface *iface)
{
  iface->get_models = mex_gnome_dvb_plugin_get_models;
}

static gboolean
_handle_error (GError **err)
{
  if (*err)
    {
      g_warning ("Error: %s", (*err)->message);
      g_clear_error (err);

      return TRUE;
    }

  return FALSE;
}

static void
get_channel_url_ready_cb (GObject      *proxy,
                          GAsyncResult *res,
                          gpointer      content)
{
  GError *err = NULL;
  GVariant *variant;
  int i, size;
  gchar *url;
  gboolean result;

  variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (proxy), res, &err);

  if (_handle_error (&err))
    return;

  g_variant_get (variant, "(sb)", &url, &result);

  mex_content_set_metadata (content, MEX_CONTENT_METADATA_STREAM, url);

  g_free (url);
}

static void
get_channel_infos_ready_cb (GObject      *proxy,
                            GAsyncResult *res,
                            gpointer      plugin)
{
  GError *err = NULL;
  GVariant *variant, *child;
  int i, size;
  gchar *object_path;
  gboolean result;
  GVariantIter iter;
  MexGnomeDvbPluginPrivate *priv = MEX_GNOME_DVB_PLUGIN (plugin)->priv;
  gchar *name, *id;
  guint32 uid;
  gboolean boolean;

  variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (proxy), res, &err);

  if (_handle_error (&err))
    return;

  g_variant_iter_init (&iter, g_variant_get_child_value (variant, 0));

  while (g_variant_iter_next (&iter, "(usb)", &uid, &name, &boolean))
    {
      MexContent *content;

      if (boolean)
        {
          g_free (name);
          continue;
        }

      id = g_strdup_printf ("%d", uid);

      content = g_object_new (MEX_TYPE_GENERIC_CONTENT, NULL);

      mex_content_set_metadata (content, MEX_CONTENT_METADATA_TITLE, name);
      mex_content_set_metadata (content, MEX_CONTENT_METADATA_ID, id);
      mex_content_set_metadata (content, MEX_CONTENT_METADATA_MIMETYPE,
                                "x-mex/tv");

      mex_model_add_content (((MexModelInfo*) priv->models->data)->model,
                             content);

      g_dbus_proxy_call (G_DBUS_PROXY (proxy), "GetChannelURL",
                         g_variant_new ("(u)", uid), G_DBUS_CALL_FLAGS_NONE,
                         -1, NULL, get_channel_url_ready_cb, content);

      g_free (name);
      g_free (id);
    }
}

static void
channel_list_proxy_ready_cb (GObject      *proxy,
                             GAsyncResult *res,
                             gpointer      plugin)
{
  GDBusProxy *new_proxy;
  GError *err = NULL;

  new_proxy = g_dbus_proxy_new_for_bus_finish (res, &err);

  if (_handle_error (&err))
    return;

  g_dbus_proxy_call (new_proxy, "GetChannelInfos", NULL, G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, get_channel_infos_ready_cb, plugin);

  g_object_unref (new_proxy);
}

static void
get_channel_list_ready_cb (GObject      *proxy,
                           GAsyncResult *res,
                           gpointer      plugin)
{
  GError *err = NULL;
  GVariant *variant;
  int i, size;
  gchar *object_path;
  gboolean result;

  variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (proxy), res, &err);

  if (_handle_error (&err))
    return;

  g_variant_get (variant, "(o)", &object_path);

  /* create a new proxy for the channel list object */
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.DVB",
                            object_path,
                            "org.gnome.DVB.ChannelList",
                            NULL,
                            channel_list_proxy_ready_cb,
                            plugin);

  g_free (object_path);
  g_variant_unref (variant);
}

static void
device_group_proxy_ready_cb (GObject      *proxy,
                             GAsyncResult *res,
                             gpointer      plugin)
{
  GDBusProxy *new_proxy;
  GError *err = NULL;

  new_proxy = g_dbus_proxy_new_for_bus_finish (res, &err);

  if (_handle_error (&err))
    return;

  g_dbus_proxy_call (new_proxy, "GetChannelList", NULL, G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, get_channel_list_ready_cb, plugin);

  g_object_unref (proxy);
}

static void
get_device_group_ready_cb (GObject      *proxy,
                           GAsyncResult *res,
                           gpointer      plugin)
{
  GError *err = NULL;
  GVariant *variant;
  int i, size;
  gchar *object_path;
  gboolean result;

  variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (proxy), res, &err);

  if (_handle_error (&err))
    return;

  g_variant_get (variant, "(ob)", &object_path, &result);

  /* create a new proxy for device group */
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.DVB",
                            object_path,
                            "org.gnome.DVB.DeviceGroup",
                            NULL,
                            device_group_proxy_ready_cb,
                            plugin);
  g_free (object_path);
}

static void
get_device_group_size_ready_cb (GObject      *proxy,
                                GAsyncResult *res,
                                gpointer      plugin)
{
  GError *err = NULL;
  GVariant *variant;
  gint32 i, size = 0;

  variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (proxy), res, &err);

  if (_handle_error (&err))
    return;

  g_variant_get (variant, "(i)", &size);

  for (i = 0; i < size; i++)
    {
      g_dbus_proxy_call (G_DBUS_PROXY (proxy), "GetDeviceGroup",
                         g_variant_new ("(u)", i+1), G_DBUS_CALL_FLAGS_NONE, -1,
                         NULL, get_device_group_ready_cb, plugin);
    }

  g_variant_unref (variant);
}

static void
proxy_ready_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      plugin)
{
  GDBusProxy *proxy;
  GError *err = NULL;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &err);

  if (_handle_error (&err))
    return;

  g_dbus_proxy_call (proxy, "GetDeviceGroupSize", NULL, G_DBUS_CALL_FLAGS_NONE,
                     -1, NULL, get_device_group_size_ready_cb, plugin);
  g_object_unref (proxy);
}


static void
mex_gnome_dvb_plugin_init (MexGnomeDvbPlugin *self)
{
  MexGnomeDvbPluginPrivate *priv = self->priv = DVB_PLUGIN_PRIVATE (self);
  GrlMediaPlugin *plugin;
  GrlPluginRegistry *registry;
  MexModel *feed;
  MexModelInfo *info;

  feed = mex_generic_model_new (_("TV"), "icon-panelheader-tv");

  info = mex_model_info_new_with_sort_funcs (feed, "live", 0);

  priv->models = g_list_append (priv->models, info);


  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.DVB",
                            "/org/gnome/DVB/Manager",
                            "org.gnome.DVB.Manager",
                            NULL,
                            proxy_ready_cb,
                            self);
}

G_MODULE_EXPORT const GType
mex_get_plugin_type (void)
{
  return MEX_TYPE_GNOME_DVB_PLUGIN;
}