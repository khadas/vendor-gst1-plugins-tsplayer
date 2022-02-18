/* GStreamer
 * Copyright (C) 2021 zgj <fixme@example.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_AMLTSPVSINK_H_
#define _GST_AMLTSPVSINK_H_

#include <gst/base/gstbasesink.h>

G_BEGIN_DECLS

#define GST_TYPE_AMLTSPVSINK (gst_amltspvsink_get_type())
#define GST_AMLTSPVSINK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AMLTSPVSINK, GstAmltspvsink))
#define GST_AMLTSPVSINK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AMLTSPVSINK, GstAmltspvsinkClass))
#define GST_IS_AMLTSPVSINK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AMLTSPVSINK))
#define GST_IS_AMLTSPVSINK_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AMLTSPVSINK))

typedef struct _GstAmltspvsink GstAmltspvsink;
typedef struct _GstAmltspvsinkClass GstAmltspvsinkClass;
typedef struct _GstAmltspvsinkPrivate GstAmltspvsinkPrivate;

struct _GstAmltspvsink
{
    GstBaseSink base_amltspvsink;

    /*< private >*/
    GstAmltspvsinkPrivate *priv;
};

struct _GstAmltspvsinkClass
{
    GstBaseSinkClass base_amltspvsink_class;
};

GType gst_amltspvsink_get_type(void);

G_END_DECLS

#endif
