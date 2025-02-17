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
/*
 * SECTION:element-gstamltspasink
 *
 * The amltspasink element does audio decoder & renderer stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 filesrc location=test.aac ! aacparse ! amltspasink
 * ]|
 * will play acc file.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

#include <sys/prctl.h>
#include "adecadaptor.h"
#include "gstamltspasink.h"

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_STATIC(gst_amltspasink_debug_category);
#define GST_CAT_DEFAULT gst_amltspasink_debug_category

#define MIN_VOLUME 0
#define MAX_VOLUME 100
#define DEFAULT_VOLUME 30

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

/* prototypes */

static void gst_amltspasink_set_property(GObject *object,
                                         guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_amltspasink_get_property(GObject *object,
                                         guint property_id, GValue *value, GParamSpec *pspec);
static void gst_amltspasink_dispose(GObject *object);
static void gst_amltspasink_finalize(GObject *object);

static GstStateChangeReturn gst_amltspasink_change_state(GstElement *element,
                                                         GstStateChange transition);

static GstCaps *gst_amltspasink_get_caps(GstBaseSink *sink, GstCaps *filter);
static gboolean gst_amltspasink_set_caps(GstBaseSink *sink, GstCaps *caps);
static GstCaps *gst_amltspasink_fixate(GstBaseSink *sink, GstCaps *caps);
static gboolean gst_amltspasink_activate_pull(GstBaseSink *sink, gboolean active);
static void gst_amltspasink_get_times(GstBaseSink *sink, GstBuffer *buffer,
                                      GstClockTime *start, GstClockTime *end);
static gboolean gst_amltspasink_propose_allocation(GstBaseSink *sink, GstQuery *query);
static gboolean gst_amltspasink_start(GstBaseSink *sink);
static gboolean gst_amltspasink_stop(GstBaseSink *sink);
static gboolean gst_amltspasink_unlock(GstBaseSink *sink);
static gboolean gst_amltspasink_unlock_stop(GstBaseSink *sink);
static gboolean gst_amltspasink_query(GstBaseSink *sink, GstQuery *query);
static gboolean gst_amltspasink_event(GstBaseSink *sink, GstEvent *event);
static GstFlowReturn gst_amltspasink_wait_event(GstBaseSink *sink, GstEvent *event);
static GstFlowReturn gst_amltspasink_prepare(GstBaseSink *sink, GstBuffer *buffer);
static GstFlowReturn gst_amltspasink_prepare_list(GstBaseSink *sink,
                                                  GstBufferList *buffer_list);
static GstFlowReturn gst_amltspasink_preroll(GstBaseSink *sink, GstBuffer *buffer);
static GstFlowReturn gst_amltspasink_render(GstBaseSink *sink, GstBuffer *buffer);
static GstFlowReturn gst_amltspasink_render_list(GstBaseSink *sink,
                                                 GstBufferList *buffer_list);

enum
{
    PROP_0,
    PROP_VOLUME,
    PROP_MUTE,
};

/* pad templates */

static GstStaticPadTemplate gst_amltspasink_sink_template =
    GST_STATIC_PAD_TEMPLATE("sink",
                            GST_PAD_SINK,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS(
                                "audio/mpeg, "
                                "mpegversion = (int) 1, "
                                "layer = (int) [ 1, 3 ], "
                                "channels = (int) [ 1, MAX ], "
                                "rate = (int) [ 1, MAX ]"
                                " ;"

                                "audio/mpeg, "
                                "mpegversion = (int) {2, 4}, "
                                "framed = (boolean) true, "
                                "stream-format = { adts },"
                                "channels = (int) [ 1, MAX ], "
                                "rate = (int) [ 1, MAX ]"
                                " ;"
#if 0
                                "audio/x-ac3, "
                                "framed = (boolean) true, "
                                "channels = (int) [ 1, MAX ], "
                                "rate = (int) [ 1, MAX ]"
                                " ;"

                                "audio/x-eac3, "
                                "framed = (boolean) true, "
                                "channels = (int) [ 1, MAX ], "
                                "rate = (int) [ 1, MAX ]"
                                " ;"

                                "audio/x-dts, "
                                "framed = (boolean) true, "
                                "channels = (int) [ 1, MAX ], "
                                "rate = (int) [ 1, MAX ]"
                                " ;"
#endif
                                "audio/x-raw, "
                                "format = (string) S16LE, "
                                "layout = (string) interleaved, "
                                "channels = 2, "
                                "rate = 48000"
                                " ;"));

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstAmltspasink, gst_amltspasink, GST_TYPE_BASE_SINK,
                        GST_DEBUG_CATEGORY_INIT(gst_amltspasink_debug_category, "amltspasink", 0,
                                                "debug category for amltspasink element"));

/******************************utils start*****************************/
/* wait audio eos thread */
static gpointer audio_eos_thread(gpointer data)
{
    GstAmltspasink *amltspasink = (GstAmltspasink *)data;
    GstAmltspasinkPrivate *priv = &amltspasink->priv;
    uint32_t count = 30;    /* 3s timeout */
    uint64_t last_apts = 0; /* used to save the last vpts obtained */

    prctl(PR_SET_NAME, "amltspasink_eos_t");
    GST_INFO("enter");

    while (!priv->quit_eos_wait)
    {
        uint64_t curr_apts = 0;

        /*
         * EOS judgment basis:
         * 1.When priv->final_apts is less than or equal to curr_apts;
         * 2.When curr_apts equals last_apts, curr_apts will not change;
         */
        get_audio_pts(&curr_apts);
        GST_INFO("final_apts:%llu, curr_apts:%llu", priv->final_apts, curr_apts);
        if (priv->final_apts <= curr_apts || curr_apts == last_apts)
        {
            priv->eos = TRUE;
        }
        last_apts = curr_apts;

        /* priv->eos=TRUE or timeout */
        if (priv->eos || !count)
        {
            GstMessage *message;

            if (!count)
            {
                GST_WARNING_OBJECT(amltspasink, "EOS timeout");
            }
            GST_WARNING_OBJECT(amltspasink, "Posting EOS");
            message = gst_message_new_eos(GST_OBJECT_CAST(amltspasink));
            gst_message_set_seqnum(message, priv->seqnum);
            gst_element_post_message(GST_ELEMENT_CAST(amltspasink), message);
            break;
        }

        g_usleep(100000);
        count--;
    }

    GST_INFO("quit");
    return NULL;
}

/* start wait audio eos thread */
static int start_eos_thread(GstAmltspasink *amltspasink)
{
    GstAmltspasinkPrivate *priv = &amltspasink->priv;

    priv->eos_wait_thread = g_thread_new("audio eos thread", audio_eos_thread, amltspasink);
    if (!priv->eos_wait_thread)
    {
        GST_ERROR_OBJECT(amltspasink, "fail to create thread");
        return -1;
    }
    return 0;
}

/* stop wait audio eos thread */
static int stop_eos_thread(GstAmltspasink *amltspasink)
{
    GstAmltspasinkPrivate *priv = &amltspasink->priv;

    GST_OBJECT_LOCK(amltspasink);
    priv->quit_eos_wait = TRUE;
    if (priv->eos_wait_thread)
    {
        GST_OBJECT_UNLOCK(amltspasink);
        g_thread_join(priv->eos_wait_thread);
        priv->eos_wait_thread = NULL;
        return 0;
    }
    GST_OBJECT_UNLOCK(amltspasink);
    return 0;
}
/*******************************utils end******************************/

/* gst api */
static void
gst_amltspasink_class_init(GstAmltspasinkClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS(klass);
    GstElementClass *gstelement_class = GST_ELEMENT_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
    gst_element_class_add_static_pad_template(gstelement_class,
                                              &gst_amltspasink_sink_template);

    gst_element_class_set_static_metadata(gstelement_class,
                                          "Audio Decoder on Tsplayer",
                                          "Codec/Decoder/Audio",
                                          "Audio Decoder and Render on Tsplayer",
                                          "biao.zhang <biao.zhang@amlogic.com>");

    gobject_class->set_property = gst_amltspasink_set_property;
    gobject_class->get_property = gst_amltspasink_get_property;
    gobject_class->dispose = gst_amltspasink_dispose;
    gobject_class->finalize = gst_amltspasink_finalize;

    g_object_class_install_property(gobject_class, PROP_VOLUME,
                                    g_param_spec_int("volume", "Stream Volume", "Get or set volume",
                                                     MIN_VOLUME, MAX_VOLUME, DEFAULT_VOLUME, G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class, PROP_MUTE,
                                    g_param_spec_boolean("mute", "Mute", "Mute state of system",
                                                         FALSE, G_PARAM_READWRITE));

    gstelement_class->change_state = GST_DEBUG_FUNCPTR(gst_amltspasink_change_state);

    base_sink_class->get_caps = GST_DEBUG_FUNCPTR(gst_amltspasink_get_caps);
    base_sink_class->set_caps = GST_DEBUG_FUNCPTR(gst_amltspasink_set_caps);
    base_sink_class->fixate = GST_DEBUG_FUNCPTR(gst_amltspasink_fixate);
    base_sink_class->activate_pull = GST_DEBUG_FUNCPTR(gst_amltspasink_activate_pull);
    base_sink_class->get_times = GST_DEBUG_FUNCPTR(gst_amltspasink_get_times);
    base_sink_class->propose_allocation = GST_DEBUG_FUNCPTR(gst_amltspasink_propose_allocation);
    base_sink_class->start = GST_DEBUG_FUNCPTR(gst_amltspasink_start);
    base_sink_class->stop = GST_DEBUG_FUNCPTR(gst_amltspasink_stop);
    base_sink_class->unlock = GST_DEBUG_FUNCPTR(gst_amltspasink_unlock);
    base_sink_class->unlock_stop = GST_DEBUG_FUNCPTR(gst_amltspasink_unlock_stop);
    base_sink_class->query = GST_DEBUG_FUNCPTR(gst_amltspasink_query);
    base_sink_class->event = GST_DEBUG_FUNCPTR(gst_amltspasink_event);
    base_sink_class->wait_event = GST_DEBUG_FUNCPTR(gst_amltspasink_wait_event);
    base_sink_class->prepare = GST_DEBUG_FUNCPTR(gst_amltspasink_prepare);
    base_sink_class->prepare_list = GST_DEBUG_FUNCPTR(gst_amltspasink_prepare_list);
    base_sink_class->preroll = GST_DEBUG_FUNCPTR(gst_amltspasink_preroll);
    base_sink_class->render = GST_DEBUG_FUNCPTR(gst_amltspasink_render);
    base_sink_class->render_list = GST_DEBUG_FUNCPTR(gst_amltspasink_render_list);
}

static void
gst_amltspasink_init(GstAmltspasink *amltspasink)
{
    GST_FIXME_OBJECT(amltspasink, "build time:%s,%s", __DATE__, __TIME__);

    if (amltspasink == NULL)
    {
        GST_ERROR_OBJECT(amltspasink, "bad parameter!");
        return;
    }
    // gst_base_sink_set_sync(GST_BASE_SINK_CAST(amltspasink), FALSE);
    // gst_base_sink_set_async_enabled(GST_BASE_SINK_CAST(amltspasink), FALSE);
    amltspasink->priv.paused = FALSE;
    amltspasink->priv.received_eos = FALSE;
    amltspasink->priv.eos = FALSE;
    amltspasink->priv.vol = DEFAULT_VOLUME;
    amltspasink->priv.vol_pending = TRUE;
    amltspasink->priv.mute = FALSE;
    amltspasink->priv.mute_pending = FALSE;
    amltspasink->priv.vol_bak = DEFAULT_VOLUME;
    amltspasink->priv.in_fast = FALSE;

    return;
}

void gst_amltspasink_set_property(GObject *object, guint property_id,
                                  const GValue *value, GParamSpec *pspec)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(object);

    GST_DEBUG_OBJECT(amltspasink, "set_property, %d!", property_id);

    switch (property_id)
    {
    case PROP_VOLUME:
    {
        int32_t volume = 0;

        if (value == NULL)
        {
            GST_ERROR_OBJECT(amltspasink, "bad parameter!");
            break;
        }
        volume = (int32_t)g_value_get_int(value);
        GST_FIXME_OBJECT(amltspasink, "set_property, volume: %d", volume);
        amltspasink->priv.vol = volume;
        amltspasink->priv.vol_bak = volume;
        if (ERROR_CODE_OK != set_volume(volume))
        {
            amltspasink->priv.vol_pending = TRUE;
        }
        break;
    }
    case PROP_MUTE:
    {
        gboolean mute = FALSE;
        int volume = 0;

        if (value == NULL)
        {
            GST_ERROR_OBJECT(amltspasink, "bad parameter!");
            break;
        }
        mute = g_value_get_boolean(value);
        GST_FIXME_OBJECT(amltspasink, "set_property, mute: %d", mute);
        if (mute != amltspasink->priv.mute)
        {
            amltspasink->priv.mute = mute;
            if (amltspasink->priv.mute)
            {
                amltspasink->priv.vol_bak = amltspasink->priv.vol;
                volume = 0;
            }
            else
            {
                volume = amltspasink->priv.vol_bak;
            }
            if (ERROR_CODE_OK != set_volume(volume))
            {
                amltspasink->priv.mute_pending = TRUE;
            }
        }
        break;
    }
    default:
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
    }
}

void gst_amltspasink_get_property(GObject *object, guint property_id,
                                  GValue *value, GParamSpec *pspec)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(object);

    GST_DEBUG_OBJECT(amltspasink, "get_property, id:%d!", property_id);

    switch (property_id)
    {
    case PROP_VOLUME:
    {
        int32_t volume = 0;

        if (value == NULL)
        {
            GST_ERROR_OBJECT(amltspasink, "bad parameter!");
            break;
        }
        get_volume(&volume);
        g_value_set_int(value, (int)volume);
        break;
    }
    case PROP_MUTE:
    {
        if (value == NULL)
        {
            GST_ERROR_OBJECT(amltspasink, "bad parameter!");
            break;
        }
        g_value_set_boolean(value, amltspasink->priv.mute);
        break;
    }
    default:
    {
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
    }
}

void gst_amltspasink_dispose(GObject *object)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(object);

    GST_DEBUG_OBJECT(amltspasink, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_amltspasink_parent_class)->dispose(object);
}

void gst_amltspasink_finalize(GObject *object)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(object);

    GST_DEBUG_OBJECT(amltspasink, "finalize");

    /* clean up object here */

    G_OBJECT_CLASS(gst_amltspasink_parent_class)->finalize(object);
}

static GstStateChangeReturn
gst_amltspasink_change_state(GstElement *element, GstStateChange transition)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(element);
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;

    GST_FIXME_OBJECT(amltspasink, "change_state--%s!", gst_state_change_get_name(transition));
    switch (transition)
    {
    case GST_STATE_CHANGE_NULL_TO_READY:
        if (ERROR_CODE_OK != init_adec())
        {
            GST_ERROR_OBJECT(amltspasink, "init_adec failed!");
            return GST_STATE_CHANGE_FAILURE;
        }
        break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:
        break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        if (TRUE == amltspasink->priv.paused)
        {
            resume_adec();
        }
        amltspasink->priv.paused = FALSE;
        break;

    default:
        break;
    }

    ret = GST_ELEMENT_CLASS(gst_amltspasink_parent_class)->change_state(element, transition);

    switch (transition)
    {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {
        amltspasink->priv.paused = TRUE;
        pause_adec();
        break;
    }

    case GST_STATE_CHANGE_PAUSED_TO_READY:
        break;

    case GST_STATE_CHANGE_READY_TO_NULL:
        amltspasink->priv.paused = FALSE;
        /* stop_adec() causes failure when audio track switch  */
        stop_adec();
        deinit_adec();
        break;

    default:
        break;
    }

    return ret;
}

static GstCaps *
gst_amltspasink_get_caps(GstBaseSink *sink, GstCaps *filter)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);
    GstCaps *caps = gst_pad_get_pad_template_caps(GST_BASE_SINK_PAD(sink));

    GST_DEBUG_OBJECT(amltspasink, "get_caps, filter: %s",
                     filter ? gst_structure_get_name(gst_caps_get_structure(filter, 0)) : "null");
    if (filter != NULL)
    {
        GstCaps *intersection =
            gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);

        gst_caps_unref(caps);
        caps = intersection;
    }

    GST_DEBUG_OBJECT(amltspasink, "returning caps: %" GST_PTR_FORMAT, caps);
    return caps;
}

/* notify subclass of new caps */
static gboolean
gst_amltspasink_set_caps(GstBaseSink *sink, GstCaps *caps)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    // gint i, caps_size;
    const GstStructure *structure;
    const gchar *codec;

    if (caps == NULL)
    {
        GST_ERROR_OBJECT(amltspasink, "bad parameter!");
        return FALSE;
    }

    /*     caps_size = gst_caps_get_size(caps);
    for (i = 0; i < caps_size; i++)
    {
        codec = gst_structure_get_name(gst_caps_get_structure(caps, i));
        GST_DEBUG_OBJECT(amltspasink, "set_caps---i: %d, mime: %s",
                         i, codec);
    } */

    gchar *str = gst_caps_to_string(caps);
    GST_INFO_OBJECT(amltspasink, "caps: %s", str);
    g_free(str);

    structure = gst_caps_get_structure(caps, 0);
    codec = gst_structure_get_name(structure);
    if (strcasecmp(codec, "audio/mpeg") == 0)
    {
        gint version = 0;
        gst_structure_get_int(structure, "mpegversion", &version);

        if (version == 1)
        {
            gint layer = 0;
            gst_structure_get_int(structure, "layer", &layer);

            switch (layer)
            {
            case 2:
                codec = "audio/mp2";
                break;

            case 3:
                codec = "audio/mp3";
                break;

            default:
                GST_ERROR_OBJECT(amltspasink, "can not support the layer: %d", layer);
                return FALSE;
            }
        }
        else if (version == 2 || version == 4)
        {
            codec = "audio/aac";
        }
        else
        {
            GST_ERROR_OBJECT(amltspasink, "can not support the version: %d", version);
            return FALSE;
        }
    }

    GST_DEBUG_OBJECT(amltspasink, "set_caps, codec: %s", codec);
    configure_adec(codec);
    start_adec();
    if (TRUE == amltspasink->priv.vol_pending)
    {
        amltspasink->priv.vol_pending = FALSE;
        set_volume(amltspasink->priv.vol);
    }
    if (TRUE == amltspasink->priv.mute_pending)
    {
        amltspasink->priv.mute_pending = FALSE;
        if (TRUE == amltspasink->priv.mute)
        {
            set_volume(0);
        }
        else
        {
            set_volume(amltspasink->priv.vol_bak);
        }
    }

    return TRUE;
}

/* fixate sink caps during pull-mode negotiation */
static GstCaps *
gst_amltspasink_fixate(GstBaseSink *sink, GstCaps *caps)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_LOG_OBJECT(amltspasink, "fixate");
    UNUSED(caps);

    return NULL;
}

/* start or stop a pulling thread */
static gboolean
gst_amltspasink_activate_pull(GstBaseSink *sink, gboolean active)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_LOG_OBJECT(amltspasink, "activate_pull");
    UNUSED(active);
    return TRUE;
}

/* get timestamp of GstBuffer */
static gboolean
gst_get_timestamp_of_gstbuffer(GstBaseSink *sink, GstBuffer *buffer,
                               GstClockTime *timestamp)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    if (buffer == NULL || timestamp == NULL)
    {
        GST_ERROR_OBJECT(amltspasink, "bad parameter!");
        return FALSE;
    }

    if (GST_BUFFER_PTS_IS_VALID(buffer))
    {
        *timestamp = GST_BUFFER_PTS(buffer);
        return TRUE;
    }

    if (GST_BUFFER_DTS_IS_VALID(buffer))
    {
        *timestamp = GST_BUFFER_DTS(buffer);
        return TRUE;
    }

    GST_ERROR_OBJECT(amltspasink, "can not find timestamp!");
    return FALSE;
}

/* get the start and end times for syncing on this buffer */
static void
gst_amltspasink_get_times(GstBaseSink *sink, GstBuffer *buffer,
                          GstClockTime *start, GstClockTime *end)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);
    GST_LOG_OBJECT(amltspasink, "get_times");
    UNUSED(buffer);
    *start = GST_CLOCK_TIME_NONE;
    *end = GST_CLOCK_TIME_NONE;

    return;
}

/* propose allocation parameters for upstream */
static gboolean
gst_amltspasink_propose_allocation(GstBaseSink *sink, GstQuery *query)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_LOG_OBJECT(amltspasink, "propose_allocation");
    UNUSED(query);

    return TRUE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_amltspasink_start(GstBaseSink *sink)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_DEBUG_OBJECT(amltspasink, "start");

    return TRUE;
}

static gboolean
gst_amltspasink_stop(GstBaseSink *sink)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_DEBUG_OBJECT(amltspasink, "stop");

    return TRUE;
}

/* unlock any pending access to the resource. subclasses should unlock
 * any function ASAP. */
static gboolean
gst_amltspasink_unlock(GstBaseSink *sink)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_DEBUG_OBJECT(amltspasink, "unlock");

    return TRUE;
}

/* Clear a previously indicated unlock request not that unlocking is
 * complete. Sub-classes should clear any command queue or indicator they
 * set during unlock */
static gboolean
gst_amltspasink_unlock_stop(GstBaseSink *sink)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_DEBUG_OBJECT(amltspasink, "unlock_stop");

    return TRUE;
}

/* notify subclass of query */
static gboolean
gst_amltspasink_query(GstBaseSink *sink, GstQuery *query)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);
    gboolean ret = FALSE;

    GST_FIXME_OBJECT(amltspasink, "query--%s", GST_QUERY_TYPE_NAME(query));

    switch (GST_QUERY_TYPE(query))
    {
    case GST_QUERY_POSITION:
    {
        GstFormat format;
        int64_t position_us;

        gst_query_parse_position(query, &format, NULL);
        if (format == GST_FORMAT_TIME)
        {
            get_playing_position(&position_us);
            gst_query_set_position(query, format, (gint64)position_us);

            GST_DEBUG_OBJECT(amltspasink, "query, position: %lld us", position_us);
            return TRUE;
        }

        break;
    }

    default:
        break;
    }

    ret = GST_BASE_SINK_CLASS(gst_amltspasink_parent_class)->query(sink, query);

    return ret;
}

/* notify subclass of event */
static gboolean
gst_amltspasink_event(GstBaseSink *sink, GstEvent *event)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);
    GstAmltspasinkPrivate *priv = &amltspasink->priv;
    gboolean ret = FALSE;

    GST_FIXME_OBJECT(amltspasink, "event--%s", GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_EOS:
        GST_OBJECT_LOCK(sink);
        priv->received_eos = TRUE;
        priv->eos = FALSE;
        priv->seqnum = gst_event_get_seqnum(event);
        GST_WARNING_OBJECT(amltspasink, "EOS received seqnum %d", priv->seqnum);
        start_eos_thread(amltspasink);
        GST_OBJECT_UNLOCK(sink);
        return TRUE;
    case GST_EVENT_FLUSH_START:
    {
        set_volume(0);
        break;
    }

    case GST_EVENT_FLUSH_STOP:
    {
        set_volume(amltspasink->priv.vol_bak);
        flush_adec();
        break;
    }

    case GST_EVENT_SEGMENT:
    {
        GstSegment segment;

        gst_event_copy_segment(event, &segment);
        GST_FIXME_OBJECT(amltspasink, "rate--%f", segment.rate);
        if (1.0 != segment.rate)
        {
            set_volume(0);
            amltspasink->priv.in_fast = TRUE;
        }
        else
        {
            set_volume(amltspasink->priv.vol_bak);
            amltspasink->priv.in_fast = FALSE;
        }
        break;
    }

        /*     case GST_EVENT_INSTANT_RATE_CHANGE:
    {
        gdouble rate = 1.0;

        gst_event_parse_instant_rate_change(event, &rate, NULL);
        GST_DEBUG_OBJECT(amltspasink, "event--rate: %f", rate);

        // TODO: call flush of tsplayer.
        // TODO: call set_rate of tsplayer.
        break;
    } */

    default:
        break;
    }

    ret = GST_BASE_SINK_CLASS(gst_amltspasink_parent_class)->event(sink, event);

    return ret;
}

/* wait for eos or gap, subclasses should chain up to parent first */
static GstFlowReturn
gst_amltspasink_wait_event(GstBaseSink *sink, GstEvent *event)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);
    GstFlowReturn ret = GST_FLOW_OK;

    GST_LOG_OBJECT(amltspasink, "wait_event--%s", GST_EVENT_TYPE_NAME(event));

    ret = GST_BASE_SINK_CLASS(gst_amltspasink_parent_class)->wait_event(sink, event);

    return ret;
}

/* notify subclass of buffer or list before doing sync */
static GstFlowReturn
gst_amltspasink_prepare(GstBaseSink *sink, GstBuffer *buffer)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_LOG_OBJECT(amltspasink, "prepare");
    UNUSED(buffer);

    return GST_FLOW_OK;
}

static GstFlowReturn
gst_amltspasink_prepare_list(GstBaseSink *sink, GstBufferList *buffer_list)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_LOG_OBJECT(amltspasink, "prepare_list");
    UNUSED(buffer_list);

    return GST_FLOW_OK;
}

/* notify subclass of preroll buffer or real buffer */
static GstFlowReturn
gst_amltspasink_preroll(GstBaseSink *sink, GstBuffer *buffer)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_LOG_OBJECT(amltspasink, "preroll");
    UNUSED(buffer);

    return GST_FLOW_OK;
}

/* get pts of GstBuffer */
static gboolean
gst_get_pts_of_gstbuffer(GstBaseSink *sink, GstBuffer *buffer,
                         GstClockTime *pts)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GstClockTime timestamp;
    gboolean ret = TRUE;

    if (pts == NULL)
    {
        GST_ERROR_OBJECT(amltspasink, "bad parameter!");
        return FALSE;
    }

    ret = gst_get_timestamp_of_gstbuffer(sink, buffer, &timestamp);
    if (ret == TRUE)
    {
        *pts = timestamp * 9LL / 100000LL;
    }

    return ret;
}

static GstFlowReturn
gst_amltspasink_render(GstBaseSink *sink, GstBuffer *buffer)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);
    GstAmltspasinkPrivate *priv = &(amltspasink->priv);

    /* Disable decode_audio when fast forward */
    if (FALSE == priv->in_fast)
    {
        GstMapInfo map;
        GstClockTime pts;
        gst_buffer_map(buffer, &map, GST_MAP_READ);
        gst_get_pts_of_gstbuffer(sink, buffer, &pts);
        priv->final_apts = pts;

        GST_DEBUG_OBJECT(amltspasink, "render---size: 0x%zx, apts: %lld",
                         map.size, pts);
        decode_audio(map.data, map.size, pts);

        gst_buffer_unmap(buffer, &map);
    }

    return GST_FLOW_OK;
}

/* Render a BufferList */
static GstFlowReturn
gst_amltspasink_render_list(GstBaseSink *sink, GstBufferList *buffer_list)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_LOG_OBJECT(amltspasink, "render_list");
    UNUSED(buffer_list);

    return GST_FLOW_OK;
}

static gboolean
plugin_init(GstPlugin *plugin)
{

    /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
    return gst_element_register(plugin, "amltspasink", GST_RANK_NONE, GST_TYPE_AMLTSPASINK);
}

/* FIXME: these are normally defined by the GStreamer build system.
   If you are creating an element to be included in gst-plugins-*,
   remove these, as they're always defined.  Otherwise, edit as
   appropriate for your external plugin package. */
#ifndef VERSION
#define VERSION "1.0.0"
#endif
#ifndef PACKAGE
#define PACKAGE "aml_package"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "aml_package"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://amlogic.com/"
#endif

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  amltspasink,
                  "amltspasink",
                  plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

G_END_DECLS
