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

#include "mex-queue-plugin.h"

#include <mex/mex.h>

#include <glib/gi18n-lib.h>
#include <gmodule.h>

static void model_provider_iface_init (MexModelProviderInterface *iface);
static void action_provider_iface_init (MexActionProviderInterface *iface);
G_DEFINE_TYPE_WITH_CODE (MexQueuePlugin,
                         mex_queue_plugin,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (MEX_TYPE_MODEL_PROVIDER,
                                                model_provider_iface_init)
                         G_IMPLEMENT_INTERFACE (MEX_TYPE_ACTION_PROVIDER,
                                                action_provider_iface_init))

#define GET_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), MEX_TYPE_QUEUE_PLUGIN, MexQueuePluginPrivate))

struct _MexQueuePluginPrivate {
  GList *models;
  GList *actions;
};

static void
mex_queue_plugin_dispose (GObject *object)
{
  MexQueuePlugin *self = MEX_QUEUE_PLUGIN (object);
  MexQueuePluginPrivate *priv = self->priv;

  while (priv->models)
    {
      g_object_unref (priv->models->data);
      priv->models = g_list_delete_link (priv->models, priv->models);
    }

  while (priv->actions)
    {
      MexActionInfo *info = priv->actions->data;

      g_object_unref (info->action);
      g_strfreev (info->mime_types);
      g_free (info);

      priv->actions = g_list_delete_link (priv->actions, priv->actions);
    }

  G_OBJECT_CLASS (mex_queue_plugin_parent_class)->dispose (object);
}

static void
mex_queue_plugin_finalize (GObject *object)
{
  G_OBJECT_CLASS (mex_queue_plugin_parent_class)->finalize (object);
}

static void
mex_queue_plugin_class_init (MexQueuePluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (MexQueuePluginPrivate));

  object_class->dispose = mex_queue_plugin_dispose;
  object_class->finalize = mex_queue_plugin_finalize;
}

static const gchar *all_data_mimetypes[] = { "audio/" , "x-grl/box", "video/",
    "x-mex/media", "x-mex/group", NULL };

static void
mex_queue_plugin_init (MexQueuePlugin *self)
{
  MexQueuePluginPrivate *priv;
  MexActionInfo *action_info;
  MexModel *queue_model;

  priv = self->priv = GET_PRIVATE (self);

  queue_model = mex_queue_model_dup_singleton ();

  g_object_set (queue_model,
                "icon-name", "icon-panelheader-queue",
                "category", "queue",
                NULL);

  /*
   * Attention! Fake action .. this then causes MexContentBox to put an
   * MexQueueButton in its place
   */
  action_info = g_new0 (MexActionInfo, 1);
  action_info->action = mx_action_new_full ("enqueue",
                                            _("Add to queue"),
                                            NULL,
                                            self);
  action_info->mime_types = g_strdupv ((gchar **)all_data_mimetypes);
  action_info->priority = 100;

  priv->actions = g_list_append (priv->actions, action_info);

  priv->models = g_list_append (priv->models, queue_model);
}

static const GList *
mex_queue_plugin_get_actions (MexActionProvider *action_provider)
{
  MexQueuePlugin *self = MEX_QUEUE_PLUGIN (action_provider);
  MexQueuePluginPrivate *priv = self->priv;

  return priv->actions;
}

static const GList *
mex_queue_plugin_get_models (MexModelProvider *model_provider)
{
  MexQueuePlugin *self = MEX_QUEUE_PLUGIN (model_provider);
  MexQueuePluginPrivate *priv = self->priv;

  return priv->models;
}

static void
model_provider_iface_init (MexModelProviderInterface *iface)
{
  iface->get_models = mex_queue_plugin_get_models;
}

static void
action_provider_iface_init (MexActionProviderInterface *iface)
{
  iface->get_actions = mex_queue_plugin_get_actions;
}

static GType
mex_queue_get_type (void)
{
  return MEX_TYPE_QUEUE_PLUGIN;
}

MEX_DEFINE_PLUGIN ("Queue",
		   "Provides the queue column",
		   PACKAGE_VERSION,
		   "LGPLv2.1+",
                   "Robert Bradford <rob@linux.intel.com>",
		   MEX_API_MAJOR, MEX_API_MINOR,
		   mex_queue_get_type,
		   MEX_PLUGIN_PRIORITY_CORE)
