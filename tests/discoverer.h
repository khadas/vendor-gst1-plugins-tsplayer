#ifndef __DISCOVERER_H__
#define __DISCOVERER_H__

#include <stdint.h>

int discoverer_create(void **p_hdl);

int discoverer_destroy(void *hdl);

int discoverer_run(void *hdl, char *uri);

int discoverer_has_video(void *hdl);

int discoverer_has_audio(void *hdl);

#endif
