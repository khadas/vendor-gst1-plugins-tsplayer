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

#include "adecadaptor.h"
#include "gstamltspasink.h"

G_BEGIN_DECLS

GST_DEBUG_CATEGORY_STATIC(gst_amltspasink_debug_category);
#define GST_CAT_DEFAULT gst_amltspasink_debug_category

#define MIN_VOLUME 0
#define MAX_VOLUME 32
#define DEFAULT_VOLUME 15

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
                                "format = (string) { S16LE, S16BE }, "
                                "layout = (string) interleaved, "
                                "channels = (int) [ 1, MAX ], "
                                "rate = 48000"
                                " ;"));

/* class initialization */

G_DEFINE_TYPE_WITH_CODE(GstAmltspasink, gst_amltspasink, GST_TYPE_BASE_SINK,
                        GST_DEBUG_CATEGORY_INIT(gst_amltspasink_debug_category, "amltspasink", 0,
                                                "debug category for amltspasink element"));

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
                                    g_param_spec_int("volume", "audio volume", "Get or set volume",
                                                     MIN_VOLUME, MAX_VOLUME, DEFAULT_VOLUME, G_PARAM_READWRITE));

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
    amltspasink->priv.vol_change = FALSE;
    amltspasink->priv.in_fast = FALSE;

    return;
}

void gst_amltspasink_set_property(GObject *object, guint property_id,
                                  const GValue *value, GParamSpec *pspec)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(object);

    GST_DEBUG_OBJECT(amltspasink, "get_property");

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
        amltspasink->priv.vol_change = TRUE;
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

    GST_DEBUG_OBJECT(amltspasink, "set_property");

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
        GST_DEBUG_OBJECT(amltspasink, "get_property, volume: %d", volume);
        g_value_set_int(value, (int)volume);
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
        GST_DEBUG_OBJECT(amltspasink, "null--->ready");
        if (ERROR_CODE_OK != init_adec())
        {
            GST_ERROR_OBJECT(amltspasink, "init_adec failed!");
            return GST_STATE_CHANGE_FAILURE;
        }
        if (TRUE == amltspasink->priv.vol_change)
        {
            set_volume(amltspasink->priv.vol);
        }
        break;

    case GST_STATE_CHANGE_READY_TO_PAUSED:
        GST_DEBUG_OBJECT(amltspasink, "ready--->paused");
        gst_base_sink_set_async_enabled(GST_BASE_SINK_CAST(amltspasink), FALSE);
        break;

    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
        GST_DEBUG_OBJECT(amltspasink, "paused--->playing");
        if (TRUE == amltspasink->priv.paused)
        {
            resume_adec();
        }
        amltspasink->priv.paused = FALSE;
        break;

    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {
        GstBaseSink *bsink = GST_BASE_SINK_CAST(amltspasink);
        GST_DEBUG_OBJECT(amltspasink, "playing--->paused");
        amltspasink->priv.paused = TRUE;
        pause_adec();
        /* To complete transition to paused state in async_enabled mode,
         * we need a preroll buffer pushed to the pad.
         * This is a workaround to avoid the need for preroll buffer. */
        GST_BASE_SINK_PREROLL_LOCK(bsink);
        bsink->have_preroll = 1;
        GST_BASE_SINK_PREROLL_UNLOCK(bsink);
        break;
    }

    case GST_STATE_CHANGE_PAUSED_TO_READY:
        GST_DEBUG_OBJECT(amltspasink, "paused--->ready");
        break;

    default:
        break;
    }

    ret = GST_ELEMENT_CLASS(gst_amltspasink_parent_class)->change_state(element, transition);

    switch (transition)
    {
    case GST_STATE_CHANGE_READY_TO_NULL:
        GST_DEBUG_OBJECT(amltspasink, "ready--->null");
        amltspasink->priv.paused = FALSE;
        /* stop_adec() causes failure when audio track switch  */
        // stop_adec();
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

    return TRUE;
}

/* fixate sink caps during pull-mode negotiation */
static GstCaps *
gst_amltspasink_fixate(GstBaseSink *sink, GstCaps *caps)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_DEBUG_OBJECT(amltspasink, "fixate");
    UNUSED(caps);

    return NULL;
}

/* start or stop a pulling thread */
static gboolean
gst_amltspasink_activate_pull(GstBaseSink *sink, gboolean active)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_DEBUG_OBJECT(amltspasink, "activate_pull");
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
    GST_DEBUG_OBJECT(amltspasink, "get_times");
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

    GST_DEBUG_OBJECT(amltspasink, "propose_allocation");
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
    gboolean ret = FALSE;

    GST_FIXME_OBJECT(amltspasink, "event--%s", GST_EVENT_TYPE_NAME(event));

    switch (GST_EVENT_TYPE(event))
    {
    case GST_EVENT_FLUSH_START:
    {
        mute_audio(1);
        break;
    }

    case GST_EVENT_FLUSH_STOP:
    {
        flush_adec();
        mute_audio(0);
        break;
    }

    case GST_EVENT_SEGMENT:
    {
        GstSegment segment;

        gst_event_copy_segment(event, &segment);
        GST_FIXME_OBJECT(amltspasink, "rate--%f", segment.rate);
        amltspasink->priv.in_fast = FALSE;
        if (1.0 != segment.rate)
        {
            mute_audio(1);
            amltspasink->priv.in_fast = TRUE;
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

    GST_DEBUG_OBJECT(amltspasink, "wait_event--%s", GST_EVENT_TYPE_NAME(event));

    ret = GST_BASE_SINK_CLASS(gst_amltspasink_parent_class)->wait_event(sink, event);

    return ret;
}

/* notify subclass of buffer or list before doing sync */
static GstFlowReturn
gst_amltspasink_prepare(GstBaseSink *sink, GstBuffer *buffer)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_DEBUG_OBJECT(amltspasink, "prepare");
    UNUSED(buffer);

    return GST_FLOW_OK;
}

static GstFlowReturn
gst_amltspasink_prepare_list(GstBaseSink *sink, GstBufferList *buffer_list)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_DEBUG_OBJECT(amltspasink, "prepare_list");
    UNUSED(buffer_list);

    return GST_FLOW_OK;
}

/* notify subclass of preroll buffer or real buffer */
static GstFlowReturn
gst_amltspasink_preroll(GstBaseSink *sink, GstBuffer *buffer)
{
    GstAmltspasink *amltspasink = GST_AMLTSPASINK(sink);

    GST_DEBUG_OBJECT(amltspasink, "preroll");
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

    /* Disable decode_audio when fast forward */
    if (FALSE == amltspasink->priv.in_fast)
    {
        GstMapInfo map;
        GstClockTime pts;
        gst_buffer_map(buffer, &map, GST_MAP_READ);
        gst_get_pts_of_gstbuffer(sink, buffer, &pts);

        GST_DEBUG_OBJECT(amltspasink, "render---size: 0x%zx, pts: %lld",
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

    GST_DEBUG_OBJECT(amltspasink, "render_list");
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
