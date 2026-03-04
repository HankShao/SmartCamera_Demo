#ifndef __PLAT_STREAM_H__
#define __PLAT_STREAM_H__

#include "osamacro.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

typedef void (* fGetVideoStreamCb)(uint8_t*, int32_t);

typedef struct soccam_ops{
	fGetVideoStreamCb getStreamCb;

}soccam_ops_s;

typedef struct{
	int width;
	int height;
	int fps;
	soccam_ops_s ops;
}os_strparam;

typedef enum hiSAMPLE_RC_E {
    OS_RC_CBR = 0,
    OS_RC_VBR,
    OS_RC_AVBR,
    OS_RC_QVBR,
    OS_RC_CVBR,
    OS_RC_QPMAP,
    OS_RC_FIXQP
} OS_RC_E;

/*******************************************************
    enum define
*******************************************************/
typedef enum  {
    PIC_CIF,
    PIC_360P,    /* 640 * 360 */
    PIC_D1_PAL,  /* 720 * 576 */
    PIC_D1_NTSC, /* 720 * 480 */
    PIC_720P,    /* 1280 * 720 */
    PIC_1080P,   /* 1920 * 1080 */
    PIC_480P,
    PIC_576P,
    PIC_800x600,
    PIC_1024x768,
    PIC_1280x1024,
    PIC_1366x768,
    PIC_1440x900,
    PIC_1280x800,
    PIC_1600x1200,
    PIC_1680x1050,
    PIC_1920x1200,
    PIC_640x480,
    PIC_1920x2160,
    PIC_2560x1440,
    PIC_2560x1600,
    PIC_2592x1520,
    PIC_2592x1536,
    PIC_2592x1944,
    PIC_2688x1536,
    PIC_2716x1524,
    PIC_3840x2160,
    PIC_4096x2160,
    PIC_3000x3000,
    PIC_4000x3000,
    PIC_7680x4320,
    PIC_3840x8640,
    PIC_BUTT
} OS_SIZE_E;

HANDLE os_stream_create(int chn, os_strparam *param);
int os_stream_set_param(HANDLE hnd, os_strparam *param);
int os_stream_get_param(HANDLE hnd, os_strparam *param);
int os_stream_get_frame(HANDLE hnd);
int os_stream_release_frame(HANDLE hnd);
int os_stream_requestIDR(int chn, int bInstant);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif

