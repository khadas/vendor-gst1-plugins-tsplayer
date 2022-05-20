/*
 * Copyright (C) 2017 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSEERROR_CODE_INVALID_OPERATION.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  DESCRIPTION
 *      This file implements a audio sink based on tsplayer.
 *
 */

#ifndef _GST_AMLTSPASINK_H_
#define _GST_AMLTSPASINK_H_

#include <gst/base/gstbasesink.h>

G_BEGIN_DECLS

#define GST_TYPE_AMLTSPASINK (gst_amltspasink_get_type())
#define GST_AMLTSPASINK(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_AMLTSPASINK, GstAmltspasink))
#define GST_AMLTSPASINK_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_AMLTSPASINK, GstAmltspasinkClass))
#define GST_IS_AMLTSPASINK(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_AMLTSPASINK))
#define GST_IS_AMLTSPASINK_CLASS(obj) (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_AMLTSPASINK))

typedef struct _GstAmltspasink GstAmltspasink;
typedef struct _GstAmltspasinkClass GstAmltspasinkClass;

typedef struct _GstAmltspasinkPrivate
{
    gboolean paused; /* is it paused.true:paused,false:non paused */
    gboolean received_eos; /* is it received eos. */
    gboolean eos; /* is it eos state */

    gint vol; /* audio volume.  */
    gboolean vol_pending; /* set volume pending flag  */

    gboolean mute; /* mute */
    gboolean mute_pending; /* set mute pending flag */
    gint vol_bak; /* backup volume before mute*/

    gboolean in_fast;
} GstAmltspasinkPrivate;

struct _GstAmltspasink
{
    GstBaseSink base_amltspasink;

    GstAmltspasinkPrivate priv;
};

struct _GstAmltspasinkClass
{
    GstBaseSinkClass base_amltspasink_class;
};

GType gst_amltspasink_get_type(void);

G_END_DECLS

#endif // _GST_AMLTSPASINK_H_
