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
 *      This file implements a special EQ  from Amlogic.
 *
 */

#ifndef __MEDIASESSION_H__
#define __MEDIASESSION_H__

#include <stdint.h>
#include "AmTsPlayer.h"

#define ERROR_CODE_OK 0
#define ERROR_CODE_BAD_PARAMETER -1
#define ERROR_CODE_INVALID_OPERATION -2
#define ERROR_CODE_BASE_ERROR -3

int create_session(am_tsplayer_handle *session_output);
int release_session();

int configure_video_region(int32_t top, int32_t left,
        int32_t width,int32_t height);

#endif // __MEDIASESSION_H__
