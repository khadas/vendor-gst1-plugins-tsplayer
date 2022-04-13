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
#include <string.h>

#include <pthread.h>
#include <unistd.h>

#include "AmTsPlayer.h"
#include "mediasession.h"
#include "adecadaptor.h"

#define DEBUG

// #ifdef __cplusplus
// extern "C" {
// #endif

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int initialized = 0;
static int in_deinit = 0;
static int ready = 0;
static am_tsplayer_handle session = 0;

static uint64_t timeout_ms = 10;
static uint32_t sleep_us = 1000;

#ifdef DEBUG
#define LOG(fmt, arg...) fprintf(stdout, "[adecadaptor] %s:%d, " fmt, __FUNCTION__, __LINE__, ##arg);
#else
#define LOG(fmt, arg...)
#endif

int init_adec()
{
    int ret = ERROR_CODE_OK;

    LOG("enter!\n");
    pthread_mutex_lock(&lock);

    if (initialized == 0)
    {
        ret = create_session(&session);
        if (ret != ERROR_CODE_OK)
        {
            pthread_mutex_unlock(&lock);
            LOG("create_session failed: %d\n", ret);
            return ret;
        }
        AmTsPlayer_setSyncMode(session, TS_SYNC_AMASTER);
        initialized = 1;
    }

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int deinit_adec()
{
    in_deinit = 1;
    int ret = ERROR_CODE_OK;
    LOG("enter!\n");
    pthread_mutex_lock(&lock);

    if (initialized != 0)
    {
        AmTsPlayer_setSyncMode(session, TS_SYNC_VMASTER);
        ret = release_session();
        if (ret != ERROR_CODE_OK)
        {
            in_deinit = 0;
            pthread_mutex_unlock(&lock);
            LOG("release_session failed: %d\n", ret);
            return ret;
        }

        initialized = 0;
    }

    in_deinit = 0;
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

static am_tsplayer_audio_codec codec_char_to_enum(const char *codec)
{
    if (codec == NULL)
    {
        return AV_AUDIO_CODEC_AUTO;
    }

    if (strcasecmp(codec, "audio/mp2") == 0)
    {
        return AV_AUDIO_CODEC_MP2;
    }

    if (strcasecmp(codec, "audio/mp3") == 0)
    {
        return AV_AUDIO_CODEC_MP3;
    }

    if (strcasecmp(codec, "audio/x-ac3") == 0)
    {
        return AV_AUDIO_CODEC_AC3;
    }

    if (strcasecmp(codec, "audio/x-eac3") == 0)
    {
        return AV_AUDIO_CODEC_EAC3;
    }

    if (strcasecmp(codec, "audio/x-dts") == 0)
    {
        return AV_AUDIO_CODEC_DTS;
    }

    if (strcasecmp(codec, "audio/x-private1-dts") == 0)
    {
        return AV_AUDIO_CODEC_DTS;
    }

    if (strcasecmp(codec, "audio/aac") == 0)
    {
        return AV_AUDIO_CODEC_AAC;
    }

    if (strcasecmp(codec, "audio/x-latm") == 0)
    {
        return AV_AUDIO_CODEC_LATM;
    }

    if (strcasecmp(codec, "audio/x-raw") == 0)
    {
        return AV_AUDIO_CODEC_PCM;
    }

    if (strcasecmp(codec, "audio/ac4") == 0)
    {
        return AV_AUDIO_CODEC_AC4;
    }

    if (strcasecmp(codec, "audio/x-dra") == 0)
    {
        return AV_AUDIO_CODEC_DRA;
    }

    if (strcasecmp(codec, "audio/x-flac") == 0)
    {
        return AV_AUDIO_CODEC_FLAC;
    }

    if (strcasecmp(codec, "audio/x-vorbis") == 0)
    {
        return AV_AUDIO_CODEC_VORBIS;
    }

    if (strcasecmp(codec, "audio/x-opus") == 0)
    {
        return AV_AUDIO_CODEC_OPUS;
    }

    if (strcasecmp(codec, "audio/x-max") == 0)
    {
        return AV_AUDIO_CODEC_MAX;
    }

    return AV_AUDIO_CODEC_AUTO;
}

int configure_adec(const char *codec)
{
    am_tsplayer_audio_codec codec_enum = codec_char_to_enum(codec);
    am_tsplayer_audio_params param = {codec_enum, 0x101, 0};
    am_tsplayer_result ret = AM_TSPLAYER_OK;

    LOG("enter!\n");
    pthread_mutex_lock(&lock);

    if (initialized == 0)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_setAudioParams(session, &param);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_setAudioParams failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int set_audio_rate(double rate)
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;
    LOG("enter!\n");
    pthread_mutex_lock(&lock);

    if (initialized == 0)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_startFast(session, (float)rate);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_startFast failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int start_adec()
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;
    LOG("enter!\n");
    pthread_mutex_lock(&lock);

    if (initialized == 0)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_startAudioDecoding(session);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_startAudioDecoding failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }
    ready = 1;
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int pause_adec()
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;
    LOG("enter!\n");
    pthread_mutex_lock(&lock);

    if (initialized == 0)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_pauseAudioDecoding(session);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_pauseAudioDecoding failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }
    ready = 0;

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int resume_adec()
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;
    LOG("enter!\n");
    pthread_mutex_lock(&lock);

    if (initialized == 0)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_resumeAudioDecoding(session);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_resumeAudioDecoding failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }
    ready = 1;

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int flush_adec()
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;

    LOG("enter!\n");
    pthread_mutex_lock(&lock);

    if (initialized == 0)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    // TODO, api not ok
    // ret = AmTsPlayer_stopAudioDecoding(session);
    // ret |= AmTsPlayer_startAudioDecoding(session);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer stop&start failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }
    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int stop_adec()
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;
    LOG("enter!\n");
    pthread_mutex_lock(&lock);

    if (initialized == 0)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_stopAudioDecoding(session);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_stopAudioDecoding failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }
    ready = 0;

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int decode_audio(void *data, int32_t size, uint64_t pts)
{
    am_tsplayer_input_frame_buffer frame =
        {TS_INPUT_BUFFER_TYPE_NORMAL, data, size, pts, 0};
    am_tsplayer_result ret = AM_TSPLAYER_OK;
    int retry = 100;

    if (data == NULL || size < 0)
    {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    pthread_mutex_lock(&lock);
    if (initialized == 0 || ready == 0)
    {
        pthread_mutex_unlock(&lock);
        LOG("---uninitialized or not ready!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    do
    {
        ret = AmTsPlayer_writeFrameData(session, &frame, timeout_ms);
        if ((AM_TSPLAYER_ERROR_RETRY == ret) && (0 == in_deinit))
        {
            usleep(sleep_us);
        }
        else
        {
            break;
        }
    } while ((retry-- > 0) && ret);

    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_writeFrameData failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int mute_audio(int32_t mute)
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;
    LOG("enter!\n");
    pthread_mutex_lock(&lock);

    if (initialized == 0)
    {
        pthread_mutex_unlock(&lock);
        LOG("---uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_setAudioMute(session, mute, mute);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_setAudioMute(%d) failed: %d\n",
            mute, ret);
        return ERROR_CODE_BASE_ERROR;
    }

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int get_playing_position(int64_t *position_us)
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;

    if (position_us == NULL)
    {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }
    LOG("enter!\n");
    pthread_mutex_lock(&lock);

    if (initialized == 0)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_getCurrentTime(session, position_us);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_getCurrentTime failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int get_volume(int32_t *volume)
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;

    if (volume == NULL)
    {
        LOG("bad parameter!\n");
        return ERROR_CODE_BAD_PARAMETER;
    }

    pthread_mutex_lock(&lock);
    if (initialized == 0)
    {
        pthread_mutex_unlock(&lock);
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_getAudioVolume(session, volume);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_getAudioVolume failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int set_volume(int32_t volume)
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;
    LOG("enter!\n");
    pthread_mutex_lock(&lock);

    if (initialized == 0)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_setAudioVolume(session, volume);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_setAudioVolume failed: %d, "
            "volume: %d\n",
            ret, volume);
        return ERROR_CODE_BASE_ERROR;
    }

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

// #ifdef __cplusplus
// }
// #endif
