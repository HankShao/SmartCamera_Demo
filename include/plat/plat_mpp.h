#ifndef __PLAT_MPP_H__
#define __PLAT_MPP_H__

#include "osamacro.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */


typedef struct {
	uint8_t *data;
	uint32_t size;
	uint32_t height;
	uint32_t width;
	uint64_t utc;
}jpgenc_info_t;

struct frame_info_t{
	uint32_t width;
	uint32_t height;
	uint8_t*  data[3];
	uint32_t  size[3];
	uint32_t  stride[3];
};

typedef void (*FRAME_RELEASE_CB)(void *p);

typedef struct {
	struct frame_info_t frame;
	void *priv;
	FRAME_RELEASE_CB free;  //free(priv)
}vpp_frame_info_t;

int32_t mpp_algo_init(int32_t algow, int32_t algoh);

int32_t mpp_encode_jpg(void *frame, jpgenc_info_t *out);

int32_t mpp_vpp_getframe(int32_t chn, vpp_frame_info_t *frame);


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif


