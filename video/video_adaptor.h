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
 *      This file implements a adaptor of video decoder and render from Amlogic.
 *
 */

#ifndef __VIDEO_ADAPTOR_H__
#define __VIDEO_ADAPTOR_H__

#include <stdint.h>

#define ERROR_CODE_OK 0
#define ERROR_CODE_BAD_PARAMETER -1
#define ERROR_CODE_INVALID_OPERATION -2
#define ERROR_CODE_BASE_ERROR -3

int video_init();

int video_deinit();

int video_set_codec(const char *codec, int version);

int video_set_region(int32_t x, int32_t y, int32_t w, int32_t h);

int video_start();

int video_pause();

int video_resume();

int video_stop();

int video_write_frame(void *data, int32_t size, uint64_t pts);

#endif // __VIDEO_ADAPTOR_H__
