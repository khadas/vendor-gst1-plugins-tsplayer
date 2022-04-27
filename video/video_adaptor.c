/*
 * Copyright (C) 2017 Amlogic Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *  DESCRIPTION
 *      This file implements a adaptor of video decoder and render from Amlogic.
 *
 */

#include <stdio.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

#include "AmTsPlayer.h"
#include "mediasession.h"
#include "video_adaptor.h"

#ifdef HAVE_SYSTEMD_DAEMON
#include <systemd/sd-daemon.h>
#endif

typedef int BOOL;

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

#define FALSE 0
#define TRUE 1

#define WRITE_TIME_OUT_MS 100
#define RETRY_SLEEP_TIME_US 50

#define LOG(fmt, arg...) fprintf(stdout, "[video_adaptor] %s:%d " fmt, __FUNCTION__, __LINE__, ##arg);

/* global var */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static BOOL inited = FALSE;
static BOOL in_deinit = FALSE;
static BOOL ready = FALSE;
/* tsplayer session */
static am_tsplayer_handle session = 0;

/* tsplayer callback */
void video_callback(void *user_data, am_tsplayer_event *event)
{
    UNUSED(user_data);
    LOG("video_callback type %d\n", event ? event->type : 0);
    switch (event->type)
    {
    case AM_TSPLAYER_EVENT_TYPE_VIDEO_CHANGED:
    {
        LOG("[evt] AM_TSPLAYER_EVENT_TYPE_VIDEO_CHANGED: %d x %d @%d [%d]\n",
            event->event.video_format.frame_width,
            event->event.video_format.frame_height,
            event->event.video_format.frame_rate,
            event->event.video_format.frame_aspectratio);
        break;
    }
    case AM_TSPLAYER_EVENT_TYPE_USERDATA_AFD:
    case AM_TSPLAYER_EVENT_TYPE_USERDATA_CC:
    {
        uint8_t *pbuf = event->event.mpeg_user_data.data;
        uint32_t size = event->event.mpeg_user_data.len;
        LOG("[evt] USERDATA [%d] : %x-%x-%x-%x %x-%x-%x-%x ,size %d\n",
            event->type, pbuf[0], pbuf[1], pbuf[2], pbuf[3],
            pbuf[4], pbuf[5], pbuf[6], pbuf[7], size);
        break;
    }
    case AM_TSPLAYER_EVENT_TYPE_FIRST_FRAME:
    {
        LOG("[evt] AM_TSPLAYER_EVENT_TYPE_FIRST_FRAME\n");
        #ifdef HAVE_SYSTEMD_DAEMON
        sd_notify(0, "READY=1");
        #endif
        break;
    }
    case AM_TSPLAYER_EVENT_TYPE_DECODE_FIRST_FRAME_VIDEO:
    {
        LOG("[evt] AM_TSPLAYER_EVENT_TYPE_DECODE_FIRST_FRAME_VIDEO\n");
        break;
    }
    case AM_TSPLAYER_EVENT_TYPE_DECODE_FIRST_FRAME_AUDIO:
    {
        LOG("[evt] AM_TSPLAYER_EVENT_TYPE_DECODE_FIRST_FRAME_AUDIO\n");
        break;
    }
    case AM_TSPLAYER_EVENT_TYPE_AV_SYNC_DONE:
    {
        LOG("[evt] AM_TSPLAYER_EVENT_TYPE_AV_SYNC_DONE\n");
        break;
    }
    default:
        break;
    }
}

int video_init()
{
    int ret = 0;
    int tunnelid = 0;

    pthread_mutex_lock(&lock);
    if (FALSE == inited)
    {
        // create session
        ret = create_session(&session);
        if (ERROR_CODE_OK != ret)
        {
            pthread_mutex_unlock(&lock);
            LOG("create tsplayer session failed: %d\n", ret);
            return ERROR_CODE_BASE_ERROR;
        }
        // set tsplayer param
        ret = AmTsPlayer_setSurface(session, (void *)&tunnelid);
        if (AM_TSPLAYER_OK != ret)
        {
            release_session();
            pthread_mutex_unlock(&lock);
            LOG("AmTsPlayer_setSurface failed: %d\n", ret);
            return ERROR_CODE_BASE_ERROR;
        }
        ret = AmTsPlayer_registerCb(session, video_callback, NULL);
        if (AM_TSPLAYER_OK != ret)
        {
            release_session();
            pthread_mutex_unlock(&lock);
            LOG("AmTsPlayer_registerCb failed: %d\n", ret);
            return ERROR_CODE_BASE_ERROR;
        }
        ret = AmTsPlayer_showVideo(session);
        if (AM_TSPLAYER_OK != ret)
        {
            release_session();
            pthread_mutex_unlock(&lock);
            LOG("AmTsPlayer_showVideo failed: %d\n", ret);
            return ERROR_CODE_BASE_ERROR;
        }
        ret = AmTsPlayer_setTrickMode(session, AV_VIDEO_TRICK_MODE_NONE);
        if (AM_TSPLAYER_OK != ret)
        {
            release_session();
            pthread_mutex_unlock(&lock);
            LOG("AmTsPlayer_setTrickMode failed: %d\n", ret);
            return ERROR_CODE_BASE_ERROR;
        }
        inited = TRUE;
    }
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int video_deinit()
{
    int ret = ERROR_CODE_OK;
    in_deinit = TRUE;

    pthread_mutex_lock(&lock);
    if (TRUE == inited)
    {
        ret = release_session();
        if (ret != ERROR_CODE_OK)
        {
            pthread_mutex_unlock(&lock);
            LOG("release tsplayer session failed: %d\n", ret);
            in_deinit = FALSE;
            return ERROR_CODE_BASE_ERROR;
        }
        inited = FALSE;
    }
    in_deinit = FALSE;
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

static am_tsplayer_video_codec get_vcodec_enum(const char *codec, int version)
{
    if (codec == NULL)
    {
        return AV_VIDEO_CODEC_AUTO;
    }

    if (strcasecmp(codec, "video/mpeg") == 0)
    {
        if (1 == version)
        {
            return AV_VIDEO_CODEC_MPEG1;
        }
        if (2 == version)
        {
            return AV_VIDEO_CODEC_MPEG2;
        }
        if (4 == version)
        {
            return AV_VIDEO_CODEC_MPEG4;
        }
        return AV_VIDEO_CODEC_AUTO;
    }

    if (strcasecmp(codec, "video/x-h264") == 0)
    {
        return AV_VIDEO_CODEC_H264;
    }

    if (strcasecmp(codec, "video/x-h265") == 0)
    {
        return AV_VIDEO_CODEC_H265;
    }

    if (strcasecmp(codec, "video/x-vp9") == 0)
    {
        return AV_VIDEO_CODEC_VP9;
    }

    if (strcasecmp(codec, "video/x-gst-av-avs") == 0)
    {
        return AV_VIDEO_CODEC_AVS;
    }

    if (strcasecmp(codec, "video/x-cavs") == 0)
    {
        return AV_VIDEO_CODEC_AVS2;
    }

    if (strcasecmp(codec, "image/jpeg") == 0)
    {
        return AV_VIDEO_CODEC_MJPEG;
    }

    return AV_VIDEO_CODEC_AUTO;
}

int video_set_codec(const char *codec, int version)
{
    am_tsplayer_video_codec codec_enum = get_vcodec_enum(codec, version);
    am_tsplayer_video_params param = {codec_enum, 0x100};
    am_tsplayer_result ret = AM_TSPLAYER_OK;

    LOG("codec_enum:%d!\n", codec_enum);
    pthread_mutex_lock(&lock);
    if (FALSE == inited)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_setVideoParams(session, &param);
    if (AM_TSPLAYER_OK != ret)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_setVideoParams failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int video_set_region(int32_t x, int32_t y, int32_t w, int32_t h)
{
    int ret = 0;

    pthread_mutex_lock(&lock);
    if (FALSE == inited)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_setVideoWindow(session, x, y, w, h);
    if (AM_TSPLAYER_OK != ret)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_setVideoWindow failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int video_start()
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;

    pthread_mutex_lock(&lock);
    if (FALSE == inited)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_startVideoDecoding(session);
    if (AM_TSPLAYER_OK != ret)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_startVideoDecoding failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }
    ready = TRUE;
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int video_pause()
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;

    pthread_mutex_lock(&lock);
    if (FALSE == inited)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_pauseVideoDecoding(session);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_pauseVideoDecoding failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }
    ready = FALSE;
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int video_resume()
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;

    pthread_mutex_lock(&lock);
    if (FALSE == inited)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_resumeVideoDecoding(session);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_resumeVideoDecoding failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }
    ready = TRUE;
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int video_stop()
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;

    pthread_mutex_lock(&lock);
    if (FALSE == inited)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_stopVideoDecoding(session);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_stopVideoDecoding failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }
    ready = FALSE;
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int video_write_frame(void *data, int32_t size, uint64_t pts)
{
    am_tsplayer_input_frame_buffer frame =
        {TS_INPUT_BUFFER_TYPE_NORMAL, data, size, pts, 1};
    am_tsplayer_result ret = AM_TSPLAYER_OK;
    int retry = 1000;

    if (data == NULL || size < 0)
    {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    pthread_mutex_lock(&lock);
    if ((FALSE == inited) || (FALSE == ready))
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized or not ready!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    do
    {
        ret = AmTsPlayer_writeFrameData(session, &frame, WRITE_TIME_OUT_MS);
        if ((AM_TSPLAYER_ERROR_RETRY == ret) && (FALSE == in_deinit))
        {
            usleep(RETRY_SLEEP_TIME_US);
        }
        else
        {
            break;
        }
    } while (ret || (retry-- > 0));

    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_writeFrameData failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}
