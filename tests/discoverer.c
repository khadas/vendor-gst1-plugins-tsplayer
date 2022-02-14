#include <string.h>
#include <gst/gst.h>
#include <gst/pbutils/pbutils.h>

/* Structure to contain all our information, so we can pass it around */
typedef struct _CustomData
{
    GstDiscoverer *discoverer;
    GMainLoop *loop;

    gboolean has_video;
    gboolean has_audio;
    ///< ...
} CustomData;

///< static api
/* Print a tag in a human-readable format (name: value) */
static void print_tag_foreach(const GstTagList *tags, const gchar *tag, gpointer user_data)
{
    GValue val = {
        0,
    };
    gchar *str;
    gint depth = GPOINTER_TO_INT(user_data);

    gst_tag_list_copy_value(&val, tags, tag);

    if (G_VALUE_HOLDS_STRING(&val))
        str = g_value_dup_string(&val);
    else
        str = gst_value_serialize(&val);

    GST_DEBUG("%*s%s: %s\n", 2 * depth, " ", gst_tag_get_nick(tag), str);
    g_free(str);

    g_value_unset(&val);
}

/* Print information regarding a stream */
static void print_stream_info(GstDiscovererStreamInfo *info, gint depth, CustomData *data)
{
    gchar *desc = NULL;
    GstCaps *caps;
    const GstTagList *tags;
    const gchar *stream_info = NULL;

    caps = gst_discoverer_stream_info_get_caps(info);

    if (caps)
    {
        if (gst_caps_is_fixed(caps))
            desc = gst_pb_utils_get_codec_description(caps);
        else
            desc = gst_caps_to_string(caps);
        gst_caps_unref(caps);
    }

    stream_info = gst_discoverer_stream_info_get_stream_type_nick(info);
    if (strstr(stream_info, "audio:"))
    {
        data->has_audio = TRUE;
    }
    if (strstr(stream_info, "video:"))
    {
        data->has_video = TRUE;
    }
    GST_DEBUG("%*s%s: %s\n", 2 * depth, " ", stream_info, (desc ? desc : ""));

    if (desc)
    {
        g_free(desc);
        desc = NULL;
    }

    tags = gst_discoverer_stream_info_get_tags(info);
    if (tags)
    {
        GST_DEBUG("%*sTags:\n", 2 * (depth + 1), " ");
        gst_tag_list_foreach(tags, print_tag_foreach, GINT_TO_POINTER(depth + 2));
    }
}

/* Print information regarding a stream and its substreams, if any */
static void print_topology(GstDiscovererStreamInfo *info, gint depth, CustomData *data)
{
    GstDiscovererStreamInfo *next;

    if (!info)
        return;

    print_stream_info(info, depth, data);

    next = gst_discoverer_stream_info_get_next(info);
    if (next)
    {
        print_topology(next, depth + 1, data);
        gst_discoverer_stream_info_unref(next);
    }
    else if (GST_IS_DISCOVERER_CONTAINER_INFO(info))
    {
        GList *tmp, *streams;

        streams = gst_discoverer_container_info_get_streams(GST_DISCOVERER_CONTAINER_INFO(info));
        for (tmp = streams; tmp; tmp = tmp->next)
        {
            GstDiscovererStreamInfo *tmpinf = (GstDiscovererStreamInfo *)tmp->data;
            print_topology(tmpinf, depth + 1, data);
        }
        gst_discoverer_stream_info_list_free(streams);
    }
}

/* This function is called every time the discoverer has information regarding
 * one of the URIs we provided.*/
static void on_discovered_cb(GstDiscoverer *discoverer, GstDiscovererInfo *info, GError *err, CustomData *data)
{
    GstDiscovererResult result;
    const gchar *uri;
    const GstTagList *tags;
    GstDiscovererStreamInfo *sinfo;

    uri = gst_discoverer_info_get_uri(info);
    result = gst_discoverer_info_get_result(info);
    switch (result)
    {
    case GST_DISCOVERER_URI_INVALID:
        GST_ERROR("Invalid URI '%s'\n", uri);
        break;
    case GST_DISCOVERER_ERROR:
        GST_ERROR("Discoverer error: %s\n", err->message);
        break;
    case GST_DISCOVERER_TIMEOUT:
        GST_ERROR("Timeout\n");
        break;
    case GST_DISCOVERER_BUSY:
        GST_ERROR("Busy\n");
        break;
    case GST_DISCOVERER_MISSING_PLUGINS:
    {
        const GstStructure *s;
        gchar *str;

        s = gst_discoverer_info_get_misc(info);
        str = gst_structure_to_string(s);

        GST_ERROR("Missing plugins: %s\n", str);
        g_free(str);
        break;
    }
    case GST_DISCOVERER_OK:
        GST_DEBUG("Discovered '%s'\n", uri);
        break;
    }

    if (result != GST_DISCOVERER_OK)
    {
        g_printerr("This URI cannot be played\n");
        return;
    }

    /* If we got no error, show the retrieved information */

    GST_DEBUG("\nDuration: %" GST_TIME_FORMAT "\n", GST_TIME_ARGS(gst_discoverer_info_get_duration(info)));

    tags = gst_discoverer_info_get_tags(info);
    if (tags)
    {
        GST_DEBUG("Tags:\n");
        gst_tag_list_foreach(tags, print_tag_foreach, GINT_TO_POINTER(1));
    }

    GST_DEBUG("Seekable: %s\n", (gst_discoverer_info_get_seekable(info) ? "yes" : "no"));

    GST_DEBUG("\n");

    sinfo = gst_discoverer_info_get_stream_info(info);
    if (!sinfo)
        return;

    GST_DEBUG("Stream information:\n");

    print_topology(sinfo, 1, data);

    gst_discoverer_stream_info_unref(sinfo);

    GST_DEBUG("\n");
}

/* This function is called when the discoverer has finished examining
 * all the URIs we provided.*/
static void on_finished_cb(GstDiscoverer *discoverer, CustomData *data)
{
    GST_DEBUG("Finished discovering\n");
    g_main_loop_quit(data->loop);
}

///< extern api
int discoverer_create(void **p_hdl)
{
    /* Check input param */
    if (NULL == p_hdl)
    {
        GST_ERROR("invalid param!\n");
        return -1;
    }

    GError *err = NULL;
    CustomData *data = (CustomData *)malloc(sizeof(CustomData));
    if (NULL == data)
    {
        return -1;
    }
    memset(data, 0, sizeof(CustomData));

    /* Initialize GStreamer */
    gst_init(NULL, NULL);

    /* Instantiate the Discoverer */
    data->discoverer = gst_discoverer_new(5 * GST_SECOND, &err);
    if (!data->discoverer)
    {
        GST_ERROR("Error creating discoverer instance: %s\n", err->message);
        g_clear_error(&err);
        return -1;
    }

    /* Connect to the interesting signals */
    g_signal_connect(data->discoverer, "discovered", G_CALLBACK(on_discovered_cb), data);
    g_signal_connect(data->discoverer, "finished", G_CALLBACK(on_finished_cb), data);

    /* Create a GLib Main Loop */
    data->loop = g_main_loop_new(NULL, FALSE);
    *p_hdl = data;

    return 0;
}

int discoverer_destroy(void *hdl)
{
    /* Check input param */
    if (NULL == hdl)
    {
        GST_ERROR("invalid param!\n");
        return -1;
    }
    CustomData *data = (CustomData *)hdl;

    /* Stop the discoverer process */
    gst_discoverer_stop(data->discoverer);

    /* Free resources */
    g_object_unref(data->discoverer);
    g_main_loop_unref(data->loop);
    free(data);

    return 0;
}

int discoverer_run(void *hdl, char *uri)
{
    /* Check input param */
    if (NULL == hdl || NULL == uri)
    {
        GST_ERROR("invalid param!\n");
        return -1;
    }
    CustomData *data = (CustomData *)hdl;

    /* Start the discoverer process (nothing to do yet) */
    gst_discoverer_start(data->discoverer);

    /* Add a request to process asynchronously the URI passed through the command line */
    if (!gst_discoverer_discover_uri_async(data->discoverer, uri))
    {
        GST_ERROR("Failed to start discovering URI '%s'\n", uri);
        g_object_unref(data->discoverer);
        return -1;
    }
    g_main_loop_run(data->loop);

    return 0;
}

int discoverer_has_video(void *hdl)
{
    /* Check input param */
    if (NULL == hdl)
    {
        GST_ERROR("invalid param!\n");
        return -1;
    }
    CustomData *data = (CustomData *)hdl;
    return data->has_video ? 0 : 1;
}

int discoverer_has_audio(void *hdl)
{
    /* Check input param */
    if (NULL == hdl)
    {
        GST_ERROR("invalid param!\n");
        return -1;
    }
    CustomData *data = (CustomData *)hdl;
    return data->has_audio ? 0 : 1;
}