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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstamltspvsink
 *
 * The amltspvsink element does FIXME stuff.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v fakesrc ! amltspvsink
 * ]|
 * FIXME Describe what the pipeline does.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include "gstamltspvsink.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <pthread.h>
#include "video_adaptor.h"
#include "gstamlsysctl.h"

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#define DUMP_TO_FILE 1

GST_DEBUG_CATEGORY_STATIC(gst_amltspvsink_debug_category);
#define GST_CAT_DEFAULT gst_amltspvsink_debug_category

#define gst_amltspvsink_parent_class parent_class

#define COMMON_VIDEO_CAPS          \
    "width = (int) [ 16, 1920 ], " \
    "height = (int) [ 16, 1920 ] "

#define PTS_90K 90000

/* private */
struct _GstAmltspvsinkPrivate
{
    GstAmltspvsink *sink;

    /* element status */
    gboolean paused;
    gboolean received_eos;
    gboolean eos;

    /* es dimension */
    gint32 es_w;
    gint32 es_h;

    /* es framerate */
    gdouble fr; /* frame rate */

    /* display position and dimension */
    gboolean setwindow;
    gint32 disp_x;
    gint32 disp_y;
    gint32 disp_w;
    gint32 disp_h;

    /* time */
    gint64 pts;
    gint64 duration;

    gboolean keeposd;
};

enum
{
    PROP_0,
    PROP_WINDOW_SET,
    PROP_KEEPOSD
};

/* prototypes */
static void gst_amltspvsink_set_property(GObject *object, guint property_id,
                                         const GValue *value, GParamSpec *pspec);
static void gst_amltspvsink_get_property(GObject *object, guint property_id,
                                         GValue *value, GParamSpec *pspec);
static void gst_amltspvsink_dispose(GObject *object);
static void gst_amltspvsink_finalize(GObject *object);

static GstStateChangeReturn gst_amltspvsink_change_state(GstElement *element,
                                                         GstStateChange transition);

static GstCaps *gst_amltspvsink_get_caps(GstBaseSink *sink, GstCaps *filter);
static gboolean gst_amltspvsink_set_caps(GstBaseSink *sink, GstCaps *caps);
static gboolean gst_amltspvsink_start(GstBaseSink *sink);
static gboolean gst_amltspvsink_stop(GstBaseSink *sink);
#if 0
static void gst_amltspvsink_get_times (GstBaseSink * sink, GstBuffer * buffer,
    GstClockTime * start, GstClockTime * end);
#endif
static gboolean gst_amltspvsink_query(GstBaseSink *sink, GstQuery *query);
static gboolean gst_amltspvsink_event(GstBaseSink *sink, GstEvent *event);
static GstFlowReturn gst_amltspvsink_render(GstBaseSink *sink,
                                            GstBuffer *buffer);

static void keeposd(gboolean blank);
static void dump(const char *path, const uint8_t *data, int size,
                 gboolean vp9, int frame_cnt);

/* pad templates */
/* support h264/h265/mjpeg/mpeg1/mpeg2 */
static GstStaticPadTemplate gst_amltspvsink_sink_template =
    GST_STATIC_PAD_TEMPLATE("sink",
                            GST_PAD_SINK,
                            GST_PAD_ALWAYS,
                            GST_STATIC_CAPS(
                                "video/x-h264, "
                                "stream-format={ byte-stream }, "
                                "alignment={ au }, " COMMON_VIDEO_CAPS "; "
                                "video/x-h265, "
                                "stream-format={ byte-stream }, "
                                "alignment={ au }, " COMMON_VIDEO_CAPS "; "
                                "video/mpeg, "
                                "mpegversion = (int) { 1, 2 }, "
                                "systemstream = (boolean) false, " COMMON_VIDEO_CAPS "; "
                                "image/jpeg, " COMMON_VIDEO_CAPS));

/* class initialization */
G_DEFINE_TYPE_WITH_CODE(GstAmltspvsink, gst_amltspvsink, GST_TYPE_BASE_SINK,
                        GST_DEBUG_CATEGORY_INIT(gst_amltspvsink_debug_category, "amltspvsink", 0,
                                                "debug category for amltspvsink element");
                        G_ADD_PRIVATE(GstAmltspvsink));

/* utils */
static void keeposd(gboolean blank)
{
    static int fb0_enable = -1;
    static int fb1_enable = -1;
    static gboolean first = TRUE;
    static gboolean pre_state = TRUE;

    if (TRUE == first)
    {
        fb0_enable = get_osd0_status();
        fb1_enable = get_osd1_status();
    }

    if ((TRUE == first) || (blank != pre_state))
    {
        first = FALSE;
        pre_state = blank;
        if (TRUE == blank)
        {
            if (1 == fb0_enable)
            {
                set_fb0_blank(0);
            }
            if (1 == fb1_enable)
            {
                set_fb1_blank(0);
            }
        }
        else
        {
            set_fb0_blank(1);
            set_fb1_blank(1);
        }
    }

    return;
}

#ifdef DUMP_TO_FILE
static uint8_t ivf_header[32] = {
    'D', 'K', 'I', 'F',
    0x00, 0x00, 0x20, 0x00,
    'V', 'P', '9', '0',
    0x80, 0x07, 0x38, 0x04, /* 1920 x 1080 */
    0x30, 0x76, 0x00, 0x00, /* frame rate */
    0xe8, 0x03, 0x00, 0x00, /* time scale */
    0x00, 0x00, 0xff, 0xff, /* # of frames */
    0x00, 0x00, 0x00, 0x00  /* unused */
};

static void dump(const char *path, const uint8_t *data, int size, gboolean vp9, int frame_cnt)
{
    char name[50];
    uint8_t frame_header[12] = {0};
    FILE *fd;

    sprintf(name, "%s%d.dat", path, 0);
    fd = fopen(name, "ab");

    if (!fd)
        return;
    if (vp9)
    {
        if (!frame_cnt)
            fwrite(ivf_header, 1, 32, fd);
        frame_header[0] = size & 0xff;
        frame_header[1] = (size >> 8) & 0xff;
        frame_header[2] = (size >> 16) & 0xff;
        frame_header[3] = (size >> 24) & 0xff;
        fwrite(frame_header, 1, 12, fd);
    }
    fwrite(data, 1, size, fd);
    fclose(fd);
}
#endif

/* gst-api */
static void
gst_amltspvsink_class_init(GstAmltspvsinkClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GstElementClass *element_class = GST_ELEMENT_CLASS(klass);
    GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS(klass);

    /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
    gst_element_class_add_static_pad_template(element_class,
                                              &gst_amltspvsink_sink_template);

    gst_element_class_set_static_metadata(element_class,
                                          "Video Decoder on Tsplayer",
                                          "Codec/Decoder/Video",
                                          "Video Decoder and Render on Tsplayer",
                                          "Guangjun Zhu <guangjun.zhu@amlogic.com>");

    gobject_class->set_property = gst_amltspvsink_set_property;
    gobject_class->get_property = gst_amltspvsink_get_property;
    gobject_class->dispose = gst_amltspvsink_dispose;
    gobject_class->finalize = gst_amltspvsink_finalize;

    element_class->change_state = GST_DEBUG_FUNCPTR(gst_amltspvsink_change_state);
    base_sink_class->get_caps = GST_DEBUG_FUNCPTR(gst_amltspvsink_get_caps);
    base_sink_class->set_caps = GST_DEBUG_FUNCPTR(gst_amltspvsink_set_caps);
    base_sink_class->start = GST_DEBUG_FUNCPTR(gst_amltspvsink_start);
    base_sink_class->stop = GST_DEBUG_FUNCPTR(gst_amltspvsink_stop);
    base_sink_class->query = GST_DEBUG_FUNCPTR(gst_amltspvsink_query);
    base_sink_class->event = GST_DEBUG_FUNCPTR(gst_amltspvsink_event);
    base_sink_class->render = GST_DEBUG_FUNCPTR(gst_amltspvsink_render);

    g_object_class_install_property(G_OBJECT_CLASS(klass), PROP_WINDOW_SET,
                                    g_param_spec_string("rectangle", "rectangle",
                                                        "Window Set Format: x,y,width,height",
                                                        NULL, G_PARAM_WRITABLE));
    g_object_class_install_property(G_OBJECT_CLASS(klass), PROP_KEEPOSD,
                                    g_param_spec_boolean("keeposd", "keeposd",
                                                         "Whether to keep OSD during playback",
                                                         FALSE, (GParamFlags)(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

    return;
}

static void
gst_amltspvsink_init(GstAmltspvsink *amltspvsink)
{
    GstAmltspvsinkPrivate *priv = (GstAmltspvsinkPrivate *)gst_amltspvsink_get_instance_private(amltspvsink);

    GST_FIXME_OBJECT(amltspvsink, "build time:%s,%s", __DATE__, __TIME__);

    amltspvsink->priv = priv;
    priv->sink = amltspvsink;
    priv->paused = FALSE;
    priv->received_eos = FALSE;
    priv->eos = FALSE;
    priv->setwindow = FALSE;
    priv->keeposd = FALSE;

    return;
}

void gst_amltspvsink_set_property(GObject *object, guint property_id,
                                  const GValue *value, GParamSpec *pspec)
{
    GstAmltspvsink *amltspvsink = GST_AMLTSPVSINK(object);
    GstAmltspvsinkPrivate *priv = amltspvsink->priv;

    GST_DEBUG_OBJECT(amltspvsink, "set_property");

    switch (property_id)
    {
    case PROP_WINDOW_SET:
    {
        const gchar *str = g_value_get_string(value);
        gchar **parts = g_strsplit(str, ",", 4);

        if (!parts[0] || !parts[1] || !parts[2] || !parts[3])
        {
            GST_ERROR("Bad window properties string");
        }
        else
        {
            priv->disp_x = atoi(parts[0]);
            priv->disp_y = atoi(parts[1]);
            priv->disp_w = atoi(parts[2]);
            priv->disp_h = atoi(parts[3]);
            priv->setwindow = TRUE;
            video_set_region(priv->disp_x, priv->disp_y, priv->disp_w, priv->disp_h);
            GST_INFO("set window rect (%d,%d,%d,%d)\n", priv->disp_x, priv->disp_y, priv->disp_w, priv->disp_h);
        }
        g_strfreev(parts);
        break;
    }
    case PROP_KEEPOSD:
    {
        priv->keeposd = g_value_get_boolean(value);
        keeposd(priv->keeposd);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_amltspvsink_get_property(GObject *object, guint property_id,
                                  GValue *value, GParamSpec *pspec)
{
    GstAmltspvsink *amltspvsink = GST_AMLTSPVSINK(object);
    GstAmltspvsinkPrivate *priv = amltspvsink->priv;

    GST_DEBUG_OBJECT(amltspvsink, "get_property");

    switch (property_id)
    {
    case PROP_KEEPOSD:
    {
        g_value_set_boolean(value, priv->keeposd);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

void gst_amltspvsink_dispose(GObject *object)
{
    GstAmltspvsink *amltspvsink = GST_AMLTSPVSINK(object);

    GST_DEBUG_OBJECT(amltspvsink, "dispose");

    /* clean up as possible.  may be called multiple times */

    G_OBJECT_CLASS(gst_amltspvsink_parent_class)->dispose(object);
}

void gst_amltspvsink_finalize(GObject *object)
{
    GstAmltspvsink *amltspvsink = GST_AMLTSPVSINK(object);

    GST_DEBUG_OBJECT(amltspvsink, "finalize");

    /* clean up object here */

    G_OBJECT_CLASS(gst_amltspvsink_parent_class)->finalize(object);
}

static GstStateChangeReturn
gst_amltspvsink_change_state(GstElement *element, GstStateChange transition)
{
    GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
    GstAmltspvsink *amltspvsink = GST_AMLTSPVSINK(element);
    GstAmltspvsinkPrivate *priv = amltspvsink->priv;

    GST_FIXME_OBJECT(amltspvsink, "change_state--%s!", gst_state_change_get_name(transition));
    switch (transition)
    {
    case GST_STATE_CHANGE_NULL_TO_READY:
    {
        GST_DEBUG_OBJECT(amltspvsink, "null to ready");
        GST_OBJECT_LOCK(amltspvsink);
        if (ERROR_CODE_OK != video_init())
        {
            GST_ERROR_OBJECT(amltspvsink, "video_init failed!");
            GST_OBJECT_UNLOCK(amltspvsink);
            return GST_STATE_CHANGE_FAILURE;
        }
        if (TRUE == priv->setwindow)
        {
            video_set_region(priv->disp_x, priv->disp_y, priv->disp_w, priv->disp_h);
            priv->setwindow = FALSE;
        }
        GST_OBJECT_UNLOCK(amltspvsink);
        break;
    }
    case GST_STATE_CHANGE_READY_TO_PAUSED:
    {
        GST_DEBUG_OBJECT(amltspvsink, "ready to paused");
        /* set async disable when ready to paused. */
        gst_base_sink_set_async_enabled(GST_BASE_SINK_CAST(amltspvsink), FALSE);
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
    {
        GST_DEBUG_OBJECT(amltspvsink, "paused to playing");
        GST_OBJECT_LOCK(amltspvsink);
        keeposd(priv->keeposd);
        if (TRUE == priv->paused)
        {
            video_resume();
        }
        priv->paused = FALSE;
        GST_OBJECT_UNLOCK(amltspvsink);
        break;
    }
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
    {
        GST_DEBUG_OBJECT(amltspvsink, "playing to paused");
        GST_OBJECT_LOCK(amltspvsink);
        priv->paused = TRUE;
        video_pause();
        GST_OBJECT_UNLOCK(amltspvsink);
        break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
        GST_DEBUG_OBJECT(amltspvsink, "paused to ready");
        keeposd(TRUE);
        break;
    }
    default:
        break;
    }

    ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);

    switch (transition)
    {
    case GST_STATE_CHANGE_READY_TO_NULL:
    {
        GST_DEBUG_OBJECT(amltspvsink, "ready to null");
        GST_OBJECT_LOCK(amltspvsink);
        video_stop();
        video_deinit();
        GST_OBJECT_UNLOCK(amltspvsink);
        break;
    }
    default:
        break;
    }

    return ret;
}

static GstCaps *
gst_amltspvsink_get_caps(GstBaseSink *sink, GstCaps *filter)
{
    GstAmltspvsink *amltspvsink = GST_AMLTSPVSINK(sink);
    GstCaps *caps = NULL;

    GST_DEBUG_OBJECT(amltspvsink, "get_caps");
    caps = gst_pad_get_pad_template_caps(GST_BASE_SINK_PAD(sink));
    if (filter)
    {
        GST_DEBUG_OBJECT(amltspvsink, "intersecting with filter caps %" GST_PTR_FORMAT, filter);
        GstCaps *intersection = gst_caps_intersect_full(filter, caps, GST_CAPS_INTERSECT_FIRST);
        gst_caps_unref(caps);
        caps = intersection;
    }
    GST_DEBUG_OBJECT(amltspvsink, "returning caps: %" GST_PTR_FORMAT, caps);
    return caps;
}

/* notify subclass of new caps */
static gboolean
gst_amltspvsink_set_caps(GstBaseSink *sink, GstCaps *caps)
{
    GstAmltspvsink *amltspvsink = GST_AMLTSPVSINK(sink);
    GstAmltspvsinkPrivate *priv = amltspvsink->priv;
    GstStructure *structure;
    const gchar *mime;
    gint version = 0;
    gint len = 0;
    gint num = 0;
    gint denom = 0;
    gint width = 0;
    gint height = 0;

    GST_DEBUG_OBJECT(amltspvsink, "set_caps");

    gchar *str = gst_caps_to_string(caps);
    GST_INFO_OBJECT(amltspvsink, "caps: %s", str);
    g_free(str);

    structure = gst_caps_get_structure(caps, 0);
    if (!structure)
        return FALSE;

    mime = gst_structure_get_name(structure);
    if (!mime)
        return FALSE;

    /* format */
    len = strlen(mime);
    if (len == 12 && !strncmp("video/x-h264", mime, len))
    {
        /* alignment check */
        if (gst_structure_has_field(structure, "alignment"))
        {
            const char *alignment = gst_structure_get_string(structure, "alignment");
            if (strncmp("au", alignment, strlen(alignment)))
            {
                GST_ERROR_OBJECT(amltspvsink, "aligment:%s!", alignment);
                goto error;
            }
        }
    }
    else if (len == 12 && !strncmp("video/x-h265", mime, len))
    {
        /* alignment check */
        if (gst_structure_has_field(structure, "alignment"))
        {
            const char *alignment = gst_structure_get_string(structure, "alignment");
            if (strncmp("au", alignment, strlen(alignment)))
            {
                GST_ERROR_OBJECT(amltspvsink, "aligment:%s!", alignment);
                goto error;
            }
        }
    }
    else if (len == 10 && !strncmp("video/mpeg", mime, len))
    {
        if (!gst_structure_get_int(structure, "mpegversion", &version))
        {
            goto error;
        }
    }
    else if (len == 10 && !strncmp("image/jpeg", mime, len))
    {
        /* do nothing */
    }
    else
    {
        GST_ERROR("not accepting format(%s)", mime);
        goto error;
    }

    video_set_codec(mime, version);
    video_start();

    /* frame rate */
    if (gst_structure_get_fraction(structure, "framerate", &num, &denom))
    {
        if (denom == 0)
            denom = 1;

        priv->fr = (double)num / (double)denom;
        if (priv->fr <= 0.0)
        {
            g_print("assume 60 fps\n");
            priv->fr = 60.0;
        }
    }

    /* dimension */
    if (gst_structure_get_int(structure, "width", &width))
        priv->es_w = width;
    else
        priv->es_w = -1;

    if (gst_structure_get_int(structure, "height", &height))
        priv->es_h = height;
    else
        priv->es_h = -1;

    return TRUE;

error:
    return FALSE;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean
gst_amltspvsink_start(GstBaseSink *sink)
{
    GstAmltspvsink *amltspvsink = GST_AMLTSPVSINK(sink);

    GST_DEBUG_OBJECT(amltspvsink, "start");

    return TRUE;
}

static gboolean
gst_amltspvsink_stop(GstBaseSink *sink)
{
    GstAmltspvsink *amltspvsink = GST_AMLTSPVSINK(sink);

    GST_DEBUG_OBJECT(amltspvsink, "stop");

    return TRUE;
}

/* notify subclass of query */
static gboolean
gst_amltspvsink_query(GstBaseSink *sink, GstQuery *query)
{
    gboolean res = TRUE;
    GstAmltspvsink *amltspvsink = GST_AMLTSPVSINK(sink);

    GST_DEBUG_OBJECT(amltspvsink, "query");

    res = GST_BASE_SINK_CLASS(parent_class)->query(sink, query);

    return res;
}

/* notify subclass of event */
static gboolean
gst_amltspvsink_event(GstBaseSink *sink, GstEvent *event)
{
    gboolean res = TRUE;
    GstAmltspvsink *amltspvsink = GST_AMLTSPVSINK(sink);

    GST_DEBUG_OBJECT(amltspvsink, "event");

    res = GST_BASE_SINK_CLASS(parent_class)->event(sink, event);

    return res;
}

static GstFlowReturn
gst_amltspvsink_render(GstBaseSink *sink, GstBuffer *buffer)
{
    GstAmltspvsink *amltspvsink = GST_AMLTSPVSINK(sink);
    GstClockTime time = 0;
    GstClockTime duration = 0;
    guint64 pts = 0;

    GST_DEBUG_OBJECT(amltspvsink, "render");

    time = GST_BUFFER_TIMESTAMP(buffer);
    duration = GST_BUFFER_DURATION(buffer);
    if (GST_BUFFER_PTS_IS_VALID(buffer))
    {
        GST_DEBUG_OBJECT(amltspvsink, "dts:%llu, pts:%llu, duration:%llu!", GST_BUFFER_DTS(buffer), time, duration);
        pts = time * 9 / 100000;
    }

    GST_OBJECT_LOCK(sink);
    {
        GstMapInfo map;

        gst_buffer_map(buffer, &map, (GstMapFlags)GST_MAP_READ);

        video_write_frame(map.data, (int32_t)map.size, (uint64_t)pts);

#ifdef DUMP_TO_FILE
        if (getenv("AMLTSPVSINK_ES_DUMP"))
        {
            dump("/tmp/es", map.data, map.size, FALSE, 0);
        }
#endif
        gst_buffer_unmap(buffer, &map);
    }
    GST_OBJECT_UNLOCK(sink);

    GST_DEBUG_OBJECT(amltspvsink, "render end");

    return GST_FLOW_OK;
}

static gboolean
plugin_init(GstPlugin *plugin)
{
    /* FIXME Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
    return gst_element_register(plugin, "amltspvsink", GST_RANK_PRIMARY - 1, GST_TYPE_AMLTSPVSINK);
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
#define PACKAGE_NAME "aml_media"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "http://amlogic.com"
#endif

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR,
                  GST_VERSION_MINOR,
                  amltspvsink,
                  "amltspvsink",
                  plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)
