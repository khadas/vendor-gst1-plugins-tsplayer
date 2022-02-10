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
 *      This file implements a special audio decoder adaptor.
 *
 */

#ifndef __ADECADAPTOR_H__
#define __ADECADAPTOR_H__

#include <stdint.h>

// #ifdef __cplusplus
// extern "C" {
// #endif

int init_adec();
int deinit_adec();

int configure_adec(const char *codec);
int set_audio_rate(double rate);

int start_adec();
int pause_adec();
int resume_adec();
int flush_adec();
int stop_adec();

int decode_audio(void *data, int32_t size, uint64_t pts);
int mute_audio(int32_t mute);

int get_playing_position(int64_t *position_us);

int get_volume(int32_t *volume);
int set_volume(int32_t volume);

// #ifdef __cplusplus
// }
// #endif

#endif // __ADECADAPTOR_H__
