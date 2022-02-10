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
 *      This file implements a adaptor of audio decoder from Amlogic.
 *
 */

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <gst/gst.h>
// #include <gst/audio/streamvolume.h>

#define DEBUG

#define GST_DEBUG_LEVEL "3,amltspasink:5"
#define GST_AUDIO_SINK "amltspasink"
#define GST_VIDEO_SINK "amltspvsink"

#define ERROR_CODE_OK 0
#define ERROR_CODE_BAD_PARAMETER -1
#define ERROR_CODE_INVALID_OPERATION -2
#define ERROR_CODE_BASE_ERROR -3

#ifdef DEBUG
#define LOG(fmt, arg...) fprintf(stdout, "[gst_test] %s:%d, " \
fmt, __FUNCTION__, __LINE__, ##arg);
#else
#define LOG(fmt, arg...)
#endif

#define PLAY_STATUS_NORMAL 0
#define PLAY_STATUS_STARTED 1
#define PLAY_STATUS_FINISHED 2
#define PLAY_STATUS_WARNING 3
#define PLAY_STATUS_ERROR 4

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int play_status = PLAY_STATUS_NORMAL;
int previous_play_status = play_status;


void* thread_run(void *arg) {
    GMainLoop *loop = (GMainLoop *)arg;
    if (loop == NULL) {
        pthread_exit((void *) "loop is null");
    }

    g_main_loop_run(loop);

    pthread_exit((void *) "return ok");
}

int create_main_loop(GMainLoop **loop, pthread_t *thread_id) {
    GMainLoop *local_loop = NULL;

    if (loop == NULL || thread_id == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    local_loop = g_main_loop_new (NULL, FALSE);
    if (local_loop == NULL) {
        LOG("create loop failed!\n");
        return ERROR_CODE_BASE_ERROR;
    }

    if (pthread_create(thread_id, NULL, thread_run, local_loop)) {
        LOG("create thread failed!\n");
        g_main_loop_unref(local_loop);
        return ERROR_CODE_BASE_ERROR;
    }

    *loop = local_loop;
    return ERROR_CODE_OK;
}

int release_main_loop(GMainLoop *loop, pthread_t thread_id) {
    void *thread_return = NULL;

    if (loop != NULL) {
        g_main_loop_quit(loop);
        g_main_loop_unref(loop);
    }

    if (pthread_join(thread_id, &thread_return)) {
        LOG("exit thread failed!\n");
        return ERROR_CODE_BASE_ERROR;
    }

    LOG("thread exit: %s\n", (char *)thread_return);
    return ERROR_CODE_OK;
}

int parse_file(const char *file, int32_t *has_audio,
        int32_t *has_video) {
    if (file == NULL || has_audio == NULL
        || has_video == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    *has_audio = 1;
    *has_video = 1;

    return ERROR_CODE_OK;
}

int create_pipeline(const char *file, GstElement **pipeline) {
    char cmd[256] = { 0 };
    GError *error = NULL;

    int32_t has_audio = 0;
    int32_t has_video = 0;

    if (file == NULL || pipeline == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    parse_file(file, &has_audio, & has_video);

    sprintf(cmd,
        "GST_DEBUG=\"%s\" playbin uri=file://%s %s %s",
        GST_DEBUG_LEVEL, file,
        has_audio ? "audio-sink=" GST_AUDIO_SINK : "",
        has_video ? "video-sink=" GST_VIDEO_SINK : "");

    gst_init (NULL, NULL);

    *pipeline = gst_parse_launch (cmd, &error);
    if (*pipeline == NULL) {
        LOG("create pipeline failed! cmd: %s, error: %s\n",
            cmd, error ? error->message : "unknown");
        return ERROR_CODE_BASE_ERROR;
    }

    return ERROR_CODE_OK;
}

int release_pipeline(GstElement *pipeline) {
    if (pipeline != NULL) {
        gst_element_set_state (pipeline, GST_STATE_NULL);
        gst_object_unref (pipeline);
    }
    return ERROR_CODE_OK;
}

int signal_play_status(int new_status) {
    pthread_mutex_lock(&lock);
    previous_play_status = play_status;
    play_status = new_status;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int revert_error_status() {
    pthread_mutex_lock(&lock);
    if (play_status == PLAY_STATUS_ERROR) {
        play_status = previous_play_status;
    }
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int wait_for_status(int status) {
    pthread_mutex_lock(&lock);
    while (!(play_status & status)) {
        pthread_cond_wait(&cond, &lock);
    }
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int check_is_status(int status) {
    return play_status & status;
}

gboolean handle_message(GstBus *bus, GstMessage *message, gpointer data) {
    if (message == NULL) {
        LOG("receive null message!\n");
        return TRUE;
    }

    switch (GST_MESSAGE_TYPE (message)) {
        case GST_MESSAGE_WARNING:
        {
            GError *warning = NULL;
            gchar *debug_info = NULL;

            signal_play_status(PLAY_STATUS_WARNING);

            gst_message_parse_warning (message, &warning, &debug_info);

            LOG("receive warning, %s: %s\n",
                GST_OBJECT_NAME (message->src), warning->message);
            LOG("debugging info: %s\n", debug_info ? debug_info : "none");

            g_clear_error (&warning);
            g_free (debug_info);
            break;
        }
        case GST_MESSAGE_ERROR:
        {
            GError *err = NULL;
            gchar *debug_info = NULL;

            signal_play_status(PLAY_STATUS_ERROR);

            gst_message_parse_error (message, &err, &debug_info);

            LOG("receive error, %s: %s\n",
                GST_OBJECT_NAME (message->src), err->message);
            LOG("debugging info: %s\n", debug_info ? debug_info : "none");

            g_clear_error (&err);
            g_free (debug_info);
            break;
        }
        case GST_MESSAGE_EOS:
        {
            signal_play_status(PLAY_STATUS_FINISHED);

            LOG("receive End-Of-Stream\n");
            break;
        }
        case GST_MESSAGE_STATE_CHANGED:
        {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed (message, &old_state,
                &new_state, &pending_state);

            if (old_state == GST_STATE_PAUSED
                && new_state == GST_STATE_PLAYING) {
                signal_play_status(PLAY_STATUS_STARTED);
            }

            /*
            LOG("state changed: %s ---> %s\n",
                    gst_element_state_get_name (old_state),
                    gst_element_state_get_name (new_state));
            */
            break;
        }
        case GST_MESSAGE_DURATION_CHANGED:
        {
            // GstFormat format;
            // gint64 duration_ns;
            // gst_message_parse_duration(message, &format, &duration_ns);
            // LOG("receive duration: %lld ms, type: %s\n",
            //         duration_ns, gst_format_get_name(format));
            break;
        }
        case GST_MESSAGE_BUFFERING:
        {
            gint percent = 0;
            gst_message_parse_buffering (message, &percent);
            LOG("receive bufffering: %d ms\n", percent);
            break;
        }
        case GST_MESSAGE_STREAM_START:
        {
            // GstStreamStatusType status_type;
            // gst_message_parse_stream_status(message, &status_type, NULL);
            // LOG("receive stream start, status: %d\n", status_type);
            break;
        }
        case GST_MESSAGE_STREAMS_SELECTED:
        {
            guint index = 0;
            guint streams_num = 0;
            guint collection_num = 0;
            GstStreamCollection *collection = NULL;
            GstStream *stream = NULL;
            GstStructure *caps_structure = NULL;
            GstCaps *caps = NULL;
            const gchar *name = NULL;
            gint width = 0;
            gint height = 0;

            LOG("receive stream info:\n");

            streams_num = gst_message_streams_selected_get_size(message);
            for (index = 0; index < streams_num; index++) {
                GstStream* stream = gst_message_streams_selected_get_stream(message, index);
                LOG("stream %u: %s %s\n", index,
                    gst_stream_type_get_name(gst_stream_get_stream_type(stream)),
                    gst_stream_get_stream_id(stream));
            }

            gst_message_parse_streams_selected (message, &collection);
            collection_num = gst_stream_collection_get_size (collection);

            for (index = 0; index < collection_num; index++) {
                stream = gst_stream_collection_get_stream (collection, index);
                caps = gst_stream_get_caps (stream);
                caps_structure = gst_caps_get_structure (caps, 0);

                if (gst_stream_get_stream_type (stream) == GST_STREAM_TYPE_VIDEO) {
                    gst_structure_get_int (caps_structure, "width", &width);
                    gst_structure_get_int (caps_structure, "height", &height);
                    LOG("video width: %d, height: %d\n", width, height);
                } else {
                    name = gst_structure_get_name(caps_structure);
                    LOG("name: %s\n", name);
                }
            }

            break;
        }
        case GST_MESSAGE_TAG:
        {
            GstTagList *tag_list;
            gchar *value;

            gst_message_parse_tag(message, &tag_list);

            if (gst_tag_list_get_string(tag_list, GST_TAG_TITLE, &value)) {
                LOG("receive tag-title: %s\n", value);
                g_free(value);
            } else if (gst_tag_list_get_string(tag_list, GST_TAG_AUDIO_CODEC, &value)) {
                // LOG("receive tag-audio codec: %s\n", value);
                g_free(value);
            } else if (gst_tag_list_get_string(tag_list, GST_TAG_VIDEO_CODEC, &value)) {
                // LOG("receive tag-video codec: %s\n", value);
                g_free(value);
            } else if (gst_tag_list_get_string(tag_list, GST_TAG_BITRATE, &value)) {
                LOG("receive tag-bitrate: %s\n", value);
                g_free(value);
            } else if (gst_tag_list_get_string(tag_list, GST_TAG_LANGUAGE_CODE, &value)) {
                LOG("receive tag-language: %s\n", value);
                g_free(value);
            } else {
                // LOG("receive tags from %s\n", GST_OBJECT_NAME (message->src));
            }

           gst_tag_list_unref (tag_list);

            break;
        }
        default:
        {
            // LOG("receive msg: %s\n", GST_MESSAGE_TYPE_NAME(message));
            break;
        }
    }

    return TRUE;
}

int add_message_watch(GstElement *pipeline, guint *watch_id) {
    GstBus *bus = NULL;

    if (pipeline == NULL || watch_id == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    bus = gst_element_get_bus (pipeline);
    *watch_id = gst_bus_add_watch (bus, handle_message, NULL);
    gst_object_unref (bus);

    return ERROR_CODE_OK;
}

int remove_message_watch(guint watch_id) {
    g_source_remove (watch_id);
    return ERROR_CODE_OK;
}

int create(const char *file, GMainLoop **loop,
        pthread_t *thread_id, GstElement **pipeline, guint *watch_id) {
    int ret = ERROR_CODE_OK;

    ret = create_main_loop(loop, thread_id);
    if (ret != ERROR_CODE_OK) {
        return ret;
    }

    ret = create_pipeline(file, pipeline);
    if (ret != ERROR_CODE_OK) {
        goto release_main_loop_point;
    }

    signal_play_status(PLAY_STATUS_NORMAL);

    ret = add_message_watch(*pipeline, watch_id);
    if (ret != ERROR_CODE_OK) {
        goto release_pipeline_point;
    }

    return ERROR_CODE_OK;

    release_pipeline_point:
    release_pipeline(*pipeline);
    *pipeline = NULL;

    release_main_loop_point:
    release_main_loop(*loop, *thread_id);
    *loop = NULL;
    *thread_id = 0;

    return ret;
}

int release(GMainLoop *loop, pthread_t thread_id,
        GstElement *pipeline, guint watch_id) {
    remove_message_watch(watch_id);
    release_pipeline(pipeline);
    release_main_loop(loop, thread_id);

    return ERROR_CODE_OK;
}

int start(GstElement *pipeline) {
    GstStateChangeReturn ret;

    if (pipeline == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    /* Start playing */
    ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOG("start failed!\n");
        return ERROR_CODE_BASE_ERROR;
    }

    return ERROR_CODE_OK;
}

int pause(GstElement *pipeline) {
    GstStateChangeReturn ret;

    if (pipeline == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    /* Start playing */
    ret = gst_element_set_state (pipeline, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOG("start failed!\n");
        return ERROR_CODE_BASE_ERROR;
    }

    return ERROR_CODE_OK;
}

int stop(GstElement *pipeline) {
    GstStateChangeReturn ret;

    if (pipeline == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    /* Start playing */
    ret = gst_element_set_state (pipeline, GST_STATE_NULL);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        LOG("start failed!\n");
        return ERROR_CODE_BASE_ERROR;
    }

    signal_play_status(PLAY_STATUS_FINISHED);

    return ERROR_CODE_OK;
}

int seek(GstElement *pipeline, gdouble rate, int64_t position_ns) {
    GstQuery *query;
    gboolean seekable = FALSE;
    gint64 start = position_ns;
    gint64 stop = GST_CLOCK_TIME_NONE;

    if (pipeline == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    query = gst_query_new_seeking(GST_FORMAT_TIME);
    if (!gst_element_query(pipeline, query)) {
        gst_query_unref(query);
        LOG("query failed!\n");
        return ERROR_CODE_BASE_ERROR;
    }

    gst_query_parse_seeking(query, NULL, &seekable, NULL, NULL);
    gst_query_unref(query);

    if (!seekable) {
        LOG("media is unseekable!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    if (rate < 0) {
        start = 0;
        stop = position_ns;
    }

    if (!gst_element_seek(pipeline,
        rate, GST_FORMAT_TIME,
        GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
        GST_SEEK_TYPE_SET, start,
        GST_SEEK_TYPE_SET, stop)) {
        LOG("seek failed!\n");
        return ERROR_CODE_BASE_ERROR;
    };

    /*
    if (!gst_element_seek_simple(pipeline,
        GST_FORMAT_TIME,
        GstSeekFlags(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
        position_ns)) {
        LOG("seek failed!\n");
        return ERROR_CODE_BASE_ERROR;
    };
    */

    return ERROR_CODE_OK;
}

int set_rate(GstElement *pipeline, gdouble new_rate) {
    if (pipeline == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    if (!gst_element_seek(pipeline,
        new_rate, GST_FORMAT_TIME,
        // GST_SEEK_FLAG_INSTANT_RATE_CHANGE,
        // GST_SEEK_FLAG_FLUSH,
        GST_SEEK_FLAG_NONE,
        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE,
        GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE))
    {
        pthread_mutex_unlock(&lock);
        LOG("set rate failed!\n");
        return ERROR_CODE_BASE_ERROR;
    };

    return ERROR_CODE_OK;
}

int get_duration(GstElement *pipeline, int64_t *duration_ns) {
    if (pipeline == NULL || duration_ns == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    if (!gst_element_query_duration(pipeline,
            GST_FORMAT_TIME, duration_ns)) {
        LOG("get duration failed!\n");
        return ERROR_CODE_BASE_ERROR;
    }

    return ERROR_CODE_OK;
}

int get_position(GstElement *pipeline, int64_t *position_ns) {
    if (pipeline == NULL || position_ns == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    if (!gst_element_query_position(pipeline,
        GST_FORMAT_TIME, position_ns)) {
        LOG("get position failed!\n");
        return ERROR_CODE_BASE_ERROR;
    }

    return ERROR_CODE_OK;
}

int get_tracks_num(const char *track, GstElement *pipeline,
        gint *num) {
    char name[16] = { 0 };

    if (track == NULL || pipeline == NULL || num == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    sprintf(name, "n-%s", track);
    g_object_get(pipeline, name, num, NULL);

    return ERROR_CODE_OK;
}

int get_audio_tracks_num(GstElement *pipeline, gint *num) {
    return get_tracks_num("audio", pipeline, num);
}

int set_track_index(const char *track, GstElement *pipeline,
        gint index) {
    char name[16] = { 0 };

    if (track == NULL || pipeline == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    sprintf(name, "current-%s", track);
    g_object_set(pipeline, name, index, NULL);

    return ERROR_CODE_OK;
}

int set_audio_track_index(GstElement *pipeline, gint index) {
    return set_track_index("audio", pipeline, index);
}

int set_volume(GstElement *pipeline, gdouble volume) {
    GstElement *element = NULL;

    if (pipeline == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    // g_object_set(pipeline, "volume", volume, NULL);
    // gst_stream_volume_set_volume (GST_STREAM_VOLUME (pipeline),
    //     GST_STREAM_VOLUME_FORMAT_CUBIC, volume); // gstreamer-audio-1.0

    g_object_get (pipeline, "audio-sink", &element, NULL);
    if (element == NULL) {
        LOG("can not get %s!\n", GST_AUDIO_SINK);
        return ERROR_CODE_INVALID_OPERATION;
    }

    gst_object_ref (element);
    g_object_set (element, "volume", (int)volume, NULL);
    gst_object_unref (element);

    return ERROR_CODE_OK;
}

int wait_for_start() {
    return wait_for_status(PLAY_STATUS_STARTED
        | PLAY_STATUS_ERROR | PLAY_STATUS_FINISHED);
}

int wait_for_finish() {
    return wait_for_status(PLAY_STATUS_ERROR
        | PLAY_STATUS_FINISHED);
}

int play_is_started() {
    return check_is_status(PLAY_STATUS_STARTED);
}

int play_is_error() {
    return check_is_status(PLAY_STATUS_ERROR);
}

//------------------------------------------------------
#define END NULL

#define PLAY_CMD_PLAY "play"
#define PLAY_CMD_START "start"
#define PLAY_CMD_PAUSE "pause"
#define PLAY_CMD_STOP "stop"
#define PLAY_CMD_SET_RATE "set_rate"
#define PLAY_CMD_SET_VOLUME "set_volume"
#define PLAY_CMD_SEEK_NS "seek_ns"
#define PLAY_CMD_SWITCH_AUDIO_TRACK "switch_audio_index"
#define PLAY_CMD_GET_DURATION_NS "get_duration_ns"
#define PLAY_CMD_GET_POSITION_NS "get_position_ns"
#define PLAY_CMD_SLEEP_US "sleep_us"
#define PLAY_CMD_WAIT_FOR_START "wait_for_start"
#define PLAY_CMD_WAIT_FOR_FINISH "wait_for_finish"


int check_error(int ret, int32_t enabled) {
    if (!enabled) {
        return ERROR_CODE_OK;
    }

    if (ret != ERROR_CODE_OK || play_is_error()) {
        char input = 0;

        int32_t c_char_hit_num = 0;
        int32_t c_char_hit_max_num = 3;

        int32_t e_char_hit_num = 0;
        int32_t e_char_hit_max_num = 3;

        LOG("find error! "
            "input 'c' %d times to continue, "
            "input 'e' %d times to eixt !\n",
            c_char_hit_max_num, e_char_hit_max_num);

        while (true) {
            input = getchar();

            switch (input) {
                case 'c':
                    c_char_hit_num++;
                    e_char_hit_num=0;
                    if (c_char_hit_num >= c_char_hit_max_num) {
                        revert_error_status();
                        return ERROR_CODE_OK;
                    }
                break;

                case 'e':
                    c_char_hit_num=0;
                    e_char_hit_num++;
                    if (e_char_hit_num >= e_char_hit_max_num) {
                        return ERROR_CODE_BAD_PARAMETER;
                    }
                break;

                default:
                    c_char_hit_num=0;
                    e_char_hit_num=0;
                break;
            }

            usleep(20000);
        }
    }

    return ERROR_CODE_OK;
}

int rand_int() {
    srand((unsigned)time(NULL));
    return rand();
}

int process_cmd(const char *cmds[], int32_t cmd_num, char **files,
        int32_t file_num, int32_t random_the_cmd, int32_t random_the_file,
        int32_t sleep_is_enabled, int32_t check_error_is_enabled) {
    int ret = ERROR_CODE_OK;

    GMainLoop *loop = NULL;
    pthread_t thread_id = 0;

    GstElement *pipeline = NULL;
    guint watch_id = 0;

    int32_t cmd_index = 0;
    int32_t file_index = -1;
    char *file_path = NULL;

    int32_t paused = 1;

    gdouble rate = 1.0;
    gint audio_track_num = 0;

    int64_t max_seek_time_ns = 1000000000;
    int64_t max_sleep_time_us = 1000000;

    if (cmd_num < 1 || file_num < 1) {
        LOG("no cmd or no file!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    LOG("------test begin...\n");

    while (true) {
        if (strcasecmp(cmds[cmd_index], PLAY_CMD_PLAY) == 0) {
            if (pipeline != NULL) {
                LOG("---release %s\n", file_path);

                ret = release(loop, thread_id, pipeline, watch_id);
                if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                    break;
                }

                loop = NULL;
                thread_id = 0;
                pipeline = NULL;
                watch_id = 0;
            }

            if (file_index >= file_num && !random_the_file) {
                LOG("------test end!\n");
                return ERROR_CODE_OK;
            }

            if (random_the_file) {
                file_index = rand_int() % file_num;
            } else {
                file_index++;
                if (file_index >= file_num) {
                    LOG("------test end!\n");
                    return ERROR_CODE_OK;
                }
            }

            file_path = files[file_index];
            LOG("---play %s\n", file_path);

            ret = create(file_path, &loop, &thread_id,
                &pipeline, &watch_id);
            if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                break;
            }
        } else if (strcasecmp(cmds[cmd_index], PLAY_CMD_START) == 0) {
            LOG("---start\n");

            ret = start(pipeline);
            if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                break;
            }

            if (random_the_cmd && check_error_is_enabled) {
                // to avoid calling other func before started which leads to error.
                ret = wait_for_start();
                if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                    break;
                }
            }

            paused = 0;
        } else if (strcasecmp(cmds[cmd_index], PLAY_CMD_PAUSE) == 0) {
            LOG("---pause\n");

            ret = pause(pipeline);
            if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                break;
            }

            paused = 1;
        } else if (strcasecmp(cmds[cmd_index], PLAY_CMD_STOP) == 0) {
            LOG("---stop\n");

            ret = stop(pipeline);
            if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                break;
            }

            paused = 1;
        } else if (strcasecmp(cmds[cmd_index], PLAY_CMD_SET_RATE) == 0) {
            rate = (gdouble)(rand_int() % 101) / 50.0;
            LOG("---set rate %f\n", rate);

            ret = set_rate(pipeline, rate);
            if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                break;
            }
        } else if (strcasecmp(cmds[cmd_index], PLAY_CMD_SET_VOLUME) == 0) {
            gdouble volume = (gdouble)(rand_int() % 33);
            LOG("---set volume %f\n", volume);

            ret = set_volume(pipeline, volume);
            if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                break;
            }
        } else if (strcasecmp(cmds[cmd_index], PLAY_CMD_SEEK_NS) == 0) {
            int64_t seek_time_ns = rand_int() % max_seek_time_ns;
            LOG("---seek %lld us\n", seek_time_ns);
            GST_INFO("---seek %lld us\n", seek_time_ns);

            ret = seek(pipeline, rate, seek_time_ns);
            if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                break;
            }
        } else if (strcasecmp(cmds[cmd_index], PLAY_CMD_SWITCH_AUDIO_TRACK) == 0) {
            gint audio_track_index = rand_int() % audio_track_num;
            LOG("---switch audio track %d\n", audio_track_index);

            ret = set_audio_track_index(pipeline, audio_track_index);
            if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                break;
            }
        } else if (strcasecmp(cmds[cmd_index], PLAY_CMD_GET_DURATION_NS) == 0) {
            int64_t duration_ns = -1;

            ret = get_duration(pipeline, &duration_ns);
            if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                break;
            }

            LOG("---get position: %lld ns\n", duration_ns);
        } else if (strcasecmp(cmds[cmd_index], PLAY_CMD_GET_POSITION_NS) == 0) {
            int64_t position_ns = -1;

            ret = get_position(pipeline, &position_ns);
            if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                break;
            }

            LOG("---get position: %lld ns\n", position_ns);
        } else if (strcasecmp(cmds[cmd_index], PLAY_CMD_SLEEP_US) == 0) {
            if (sleep_is_enabled) {
                int64_t sleep_time_us = rand_int() % max_sleep_time_us;
                LOG("---sleep %lld us\n", sleep_time_us);

                usleep(sleep_time_us);
                if (check_error(ERROR_CODE_OK, check_error_is_enabled) != ERROR_CODE_OK) {
                    break;
                }
            }
        } else if (strcasecmp(cmds[cmd_index], PLAY_CMD_WAIT_FOR_START) == 0) {
            int64_t duration_ns = 0;

            LOG("---wait for start, now is %s\n", paused ? "paused" : "playing");
            ret = wait_for_start();
            LOG("---wait for start done.\n");
            if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                break;
            }

            ret = get_audio_tracks_num(pipeline, &audio_track_num);
            if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                break;
            }

            ret = get_duration(pipeline, &duration_ns);
            if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                break;
            }

            max_seek_time_ns = duration_ns << 1;
            max_sleep_time_us = duration_ns >> 9;

            LOG("---audio track num: %d\n", audio_track_num);
            LOG("---max seek time: %lld ns, max sleep time: %lld us\n",
                max_seek_time_ns, max_sleep_time_us);
        } else if (strcasecmp(cmds[cmd_index], PLAY_CMD_WAIT_FOR_FINISH) == 0) {
            if (random_the_cmd & paused) {
                LOG("---wait for finish was ignored in random cmd mode.");
            } else {
                LOG("---wait for finish, now is %s\n", paused ? "paused" : "playing");

                ret = wait_for_finish();
                if (check_error(ret, check_error_is_enabled) != ERROR_CODE_OK) {
                    break;
                }

                LOG("---wait for finish done.\n");
            }
        } else {
            LOG("---unkown cmd: %s\n", cmds[cmd_index]);
        }

        if (random_the_cmd) {
            cmd_index = rand_int() % cmd_num;
        } else {
            cmd_index++;
            if (cmd_index >= cmd_num) {
                cmd_index = 0;
            }
        }
    }

    if (pipeline != NULL) {
        LOG("---release %s\n", file_path);
        release(loop, thread_id, pipeline, watch_id);
        loop = NULL;
        thread_id = 0;
        pipeline = NULL;
        watch_id = 0;
    }

    return ERROR_CODE_OK;
}

//------------------------------------------------------
#define PATH_MAX_LEN 128

int parse_argv(int argc, char *argv[],
    char *media_path,
    int32_t *random_the_cmd,
    int32_t *random_the_file,
    int32_t *sleep_is_enabled,
    int32_t *check_error_is_enabled) {
    int index = 0;
    size_t size = sizeof("media_path=");

    if (argv == NULL) {
        return ERROR_CODE_OK;
    }

    if (media_path == NULL || random_the_cmd ==NULL
            || random_the_file == NULL
            || sleep_is_enabled == NULL
            || check_error_is_enabled == NULL) {
            LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    for (index = 1; index < argc; index++) {
        if (strcasecmp(argv[index], "--help") == 0) {
            LOG("%s [media_path=/media/] [random_the_cmd=1] "
                "[random_the_file=0] [disable_sleep=1] "
                "[disable_check_error=1]\n", argv[0]);
            continue;
        }

        if (strncasecmp(argv[index], "media_path=", size) == 0) {
            size_t len = strlen(argv[index]) - size;

            if (len >= PATH_MAX_LEN) {
                LOG("bad parameter!\n");
                return ERROR_CODE_BAD_PARAMETER;
            }

            strncpy(media_path, argv[index] + size, len);
            continue;
        }

        if (strcasecmp(argv[index], "random_the_cmd=1") == 0) {
            *random_the_cmd = 1;
            continue;
        }

        if (strcasecmp(argv[index], "random_the_file=0") == 0) {
            *random_the_file = 0;
            continue;
        }

        if (strcasecmp(argv[index], "disable_sleep=1") == 0) {
            *sleep_is_enabled = 0;
            continue;
        }

        if (strcasecmp(argv[index], "disable_check_error=1") == 0) {
            *check_error_is_enabled = 0;
            continue;
        }
    }

    return ERROR_CODE_OK;
}

int path_was_filtered(char *path, const char *filter[],
        int32_t filter_len) {
    int32_t index = 0;
    size_t path_size;
    size_t filter_size;

    if (path == NULL || filter == NULL
            || filter_len < 1) {
        return ERROR_CODE_OK;
    }

    path_size = strlen(path);

    for (index = 0; index < filter_len; index++) {
        filter_size = strlen(filter[index]);

        if (path_size < filter_size) {
            continue;
        }

        if (strcasecmp(path + path_size - filter_size,
                filter[index])) {
            continue;
        }

        return ERROR_CODE_OK;
    }

    return ERROR_CODE_BAD_PARAMETER;
}

int scan_media_files(char path[], const char *filter[],
    int32_t filter_len, char ***files, int32_t *num) {
    char **dir_cache = NULL;
    size_t dir_cache_index = 0;
    size_t dir_cache_len = 300;

    char * path_cache = NULL;
    char * curr_path = NULL;
    size_t curr_path_len = 0;

    char **file_list = NULL;
    size_t file_index = 0;
    size_t max_file_num = 100;

    DIR *dir = NULL;
    struct dirent* entry = NULL;
    struct stat status;

    size_t char_size = sizeof(char);

    if (path == NULL || files == NULL || num == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    dir_cache = (char **)calloc(dir_cache_len, char_size);
    if (dir_cache == NULL) {
        LOG("calloc failed!\n");
        return ERROR_CODE_BASE_ERROR;
    }

    file_list = (char **)calloc(max_file_num, char_size);
    if (file_list == NULL) {
        LOG("calloc failed!\n");
        free(dir_cache);
        return ERROR_CODE_BASE_ERROR;
    }

    path_cache = (char *) calloc(strlen(path) + 1, char_size);
    if (path_cache == NULL) {
        LOG("calloc failed!\n");
        free(dir_cache);
        free(file_list);
        return ERROR_CODE_BASE_ERROR;
    }

    strcpy(path_cache, path);
    dir_cache[dir_cache_index++] = path_cache;

    while (dir_cache_index >= 1) {
        curr_path = dir_cache[--dir_cache_index];
        curr_path_len = strlen(curr_path);

        dir = opendir(curr_path);
        if (dir == NULL) {
            stat(curr_path, &status);

            if (!S_ISDIR(status.st_mode)) {
                if (path_was_filtered(curr_path, filter, filter_len)
                            == ERROR_CODE_OK) {
                    file_list[file_index++] = curr_path;
                } else {
                    free(curr_path);
                }
                continue;
            }

            LOG("opendir failed! path: %s\n", curr_path);
            free(curr_path);
            continue;
        }

        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_DIR) {
                if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
                    if (dir_cache_index >=dir_cache_len) {
                        LOG("dir cache is full, discard: %s/%s\n",
                            curr_path, entry->d_name);
                    } else {
                        path_cache = (char *) calloc(
                            curr_path_len + strlen(entry->d_name) + 2,
                            char_size);
                        if (path_cache == NULL) {
                            LOG("calloc failed! discard: %s/%s\n",
                                curr_path, entry->d_name);
                        } else {
                            sprintf(path_cache, "%s/%s", curr_path, entry->d_name);
                            dir_cache[dir_cache_index++] = path_cache;
                        }
                    }
                }
            } else {
                if (file_index >= max_file_num) {
                    LOG("too many files, discard: %s/%s\n",
                        curr_path, entry->d_name);
                    closedir(dir);
                    free(curr_path);
                    break;
                } else {
                    path_cache = (char *) calloc(
                        curr_path_len + strlen(entry->d_name) + 2,
                        char_size);
                    if (path_cache == NULL) {
                        LOG("calloc failed! discard: %s/%s\n",
                            curr_path, entry->d_name);
                    } else {
                        sprintf(path_cache, "%s/%s", curr_path, entry->d_name);
                        if (path_was_filtered(path_cache, filter, filter_len)
                                    == ERROR_CODE_OK) {
                            file_list[file_index++] = path_cache;
                        } else {
                            free(path_cache);
                        }
                    }
                }
            }
        }

        closedir(dir);
        free(curr_path);
    }

    while (dir_cache_index >= 1) {
        free(dir_cache[--dir_cache_index]);
    }
    free(dir_cache);

    *files = file_list;
    *num = (int32_t)file_index;

    return ERROR_CODE_OK;
}

int free_media_files(char **files, int32_t num) {
    int32_t index = 0;

    if (files != NULL) {
        for (index = 0; index < num; index++) {
            if (files[index] != NULL) {
                free(files[index]);
                files[index] = NULL;
            }
        }

        free(files);
    }

    return ERROR_CODE_OK;
}

int get_char_array_len(const char *array[], int32_t *num) {
    int32_t index = 0;

    if (array == NULL || num == NULL) {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    while (array[index] != END) {
        index++;
    }

    *num = index;

    return ERROR_CODE_OK;
}

const char *cmds[] = {
    PLAY_CMD_PLAY,
    PLAY_CMD_START,
    PLAY_CMD_WAIT_FOR_START,
    PLAY_CMD_SET_VOLUME,
    PLAY_CMD_SLEEP_US,
    END
};

const char *filter[] = {
    ".aac",
    ".ts",
    END
};

//------------------------------------------------------
int main(int argc, char *argv[]) {
    char media_path[PATH_MAX_LEN] = "/media";

    char **files = NULL;

    int32_t cmd_num = 0;
    int32_t filter_len = 0;
    int32_t file_num = 0;

    int32_t random_the_cmd = 0;
    int32_t random_the_file = 0;
    int32_t sleep_is_enabled = 1;
    int32_t check_error_is_enabled = 1;

    parse_argv(argc, argv, media_path,
        &random_the_cmd, &random_the_file,
        &sleep_is_enabled, &check_error_is_enabled);

    get_char_array_len(filter, &filter_len);
    scan_media_files(media_path, filter, filter_len, &files, &file_num);

    get_char_array_len(cmds, &cmd_num);
    process_cmd(cmds, cmd_num, files, file_num,
        random_the_cmd, random_the_file, sleep_is_enabled,
        check_error_is_enabled);

    free_media_files(files, file_num);

    return 0;
}
