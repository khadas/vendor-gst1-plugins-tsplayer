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

#define DEBUG

// #ifdef __cplusplus
// extern "C" {
// #endif

#ifdef DEBUG
#define LOG(fmt, arg...) fprintf(stdout, "[mediasession] %s:%d, " fmt, __FUNCTION__, __LINE__, ##arg);
#else
#define LOG(fmt, arg...)
#endif

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int refcount = 0;
static am_tsplayer_handle session = 0;

int create_session(am_tsplayer_handle *session_output)
{
    am_tsplayer_init_params param =
        {ES_MEMORY, TS_INPUT_BUFFER_TYPE_NORMAL, 0, 0};
    am_tsplayer_result ret = AM_TSPLAYER_OK;

    pthread_mutex_lock(&lock);

    if (refcount == 0)
    {
        LOG("AmTsPlayer_create now, pid: %d\n", getpid());
        ret = AmTsPlayer_create(param, &session);

        if (ret != AM_TSPLAYER_OK)
        {
            pthread_mutex_unlock(&lock);
            LOG("AmTsPlayer_create failed: %d\n", ret);
            return ERROR_CODE_BASE_ERROR;
        }

        // common config
        AmTsPlayer_setWorkMode(session, TS_PLAYER_MODE_NORMAL);
        AmTsPlayer_setSyncMode(session, TS_SYNC_AMASTER);
    }

    refcount++;

    if (session_output != NULL)
    {
        *session_output = session;
    }

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int release_session()
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;

    pthread_mutex_lock(&lock);
    if (refcount == 1)
    {
        /* ensure stop deocding */
        AmTsPlayer_stopAudioDecoding(session);
        AmTsPlayer_stopVideoDecoding(session);
        LOG("AmTsPlayer_release now, pid: %d\n", getpid());
        ret = AmTsPlayer_release(session);
        if (ret != AM_TSPLAYER_OK)
        {
            pthread_mutex_unlock(&lock);
            LOG("AmTsPlayer_release failed: %d\n", ret);
            return ERROR_CODE_BASE_ERROR;
        }
    }

    if (refcount > 0)
    {
        refcount--;
    }

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

int configure_video_region(int32_t top, int32_t left,
                           int32_t width, int32_t height)
{
    am_tsplayer_result ret = AM_TSPLAYER_OK;

    pthread_mutex_lock(&lock);

    if (refcount < 1)
    {
        pthread_mutex_unlock(&lock);
        LOG("uninitialized!\n");
        return ERROR_CODE_INVALID_OPERATION;
    }

    ret = AmTsPlayer_setVideoWindow(session, top, left, width, height);
    if (ret != AM_TSPLAYER_OK)
    {
        pthread_mutex_unlock(&lock);
        LOG("AmTsPlayer_setVideoWindow failed: %d\n", ret);
        return ERROR_CODE_BASE_ERROR;
    }

    pthread_mutex_unlock(&lock);

    return ERROR_CODE_OK;
}

// #ifdef __cplusplus
// }
// #endif
