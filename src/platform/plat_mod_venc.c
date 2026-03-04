#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <pthread.h>
#include <osalog.h>
#include <osamacro.h>

//sdk
#include "hi_defines.h"
#include "hi_common.h"
#include "hi_comm_sys.h"
#include "hi_comm_vb.h"
#include "hi_comm_video.h"
#include "mpi_vb.h"
#include "mpi_sys.h"
#include "mpi_venc.h"
#include "hi_buffer.h"

#include <plat_stream.h>
#include <plat_stream_priv.h>


#ifdef __cplusplus
#if __cplusplus
extern "c"{
#endif
#endif
static OS_VENC_GETSTREAM_PARA_S gs_stPara;

static HI_S32 OS_COMM_VENC_CloseReEncode(VENC_CHN VencChn)
{
    HI_S32 s32Ret;
    VENC_RC_PARAM_S stRcParam;
    VENC_CHN_ATTR_S stChnAttr;

    s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stChnAttr);
    if (HI_SUCCESS != s32Ret) {
        OSALOG_ERROR("GetChnAttr failed!\n");
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VENC_GetRcParam(VencChn, &stRcParam);
    if (HI_SUCCESS != s32Ret) {
        OSALOG_ERROR("GetRcParam failed!\n");
        return HI_FAILURE;
    }

    if (VENC_RC_MODE_H264CBR == stChnAttr.stRcAttr.enRcMode) {
        stRcParam.stParamH264Cbr.s32MaxReEncodeTimes = 0;
    } else if (VENC_RC_MODE_H264VBR == stChnAttr.stRcAttr.enRcMode) {
        stRcParam.stParamH264Vbr.s32MaxReEncodeTimes = 0;
    } else if (VENC_RC_MODE_H265CBR == stChnAttr.stRcAttr.enRcMode) {
        stRcParam.stParamH265Cbr.s32MaxReEncodeTimes = 0;
    } else if (VENC_RC_MODE_H265VBR == stChnAttr.stRcAttr.enRcMode) {
        stRcParam.stParamH265Vbr.s32MaxReEncodeTimes = 0;
    } else {
        return HI_SUCCESS;
    }
    s32Ret = HI_MPI_VENC_SetRcParam(VencChn, &stRcParam);
    if (HI_SUCCESS != s32Ret) {
        OSALOG_ERROR("SetRcParam failed!\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

static HI_S32 OS_COMM_VENC_Creat(VENC_CHN VencChn, PAYLOAD_TYPE_E enType, OS_SIZE_E enSize, OS_RC_E enRcMode,
    HI_U32 u32Profile, HI_BOOL bRcnRefShareBuf, VENC_GOP_ATTR_S *pstGopAttr)
{
    HI_S32 s32Ret;
    SIZE_S stPicSize;
    VENC_CHN_ATTR_S stVencChnAttr;
    VENC_ATTR_JPEG_S stJpegAttr;
    HI_U32 u32FrameRate;
    HI_U32 u32StatTime;
    HI_U32 u32Gop;

	if (enSize == PIC_1080P)
	{
		stPicSize.u32Width	= 1920;
		stPicSize.u32Height = 1080;
	}
	u32FrameRate = 30;
	u32Gop = u32FrameRate * 2;

    /* *****************************************
     step 1:  Create Venc Channel
    ***************************************** */
    stVencChnAttr.stVencAttr.enType = enType;
    stVencChnAttr.stVencAttr.u32MaxPicWidth = stPicSize.u32Width;
    stVencChnAttr.stVencAttr.u32MaxPicHeight = stPicSize.u32Height;
    stVencChnAttr.stVencAttr.u32PicWidth = stPicSize.u32Width;   /* the picture width */
    stVencChnAttr.stVencAttr.u32PicHeight = stPicSize.u32Height; /* the picture height */

    if (enType == PT_MJPEG || enType == PT_JPEG) {
        stVencChnAttr.stVencAttr.u32BufSize =
            HI_ALIGN_UP(stPicSize.u32Width, 16) * HI_ALIGN_UP(stPicSize.u32Height, 16);
    } else {
        stVencChnAttr.stVencAttr.u32BufSize =
            HI_ALIGN_UP(stPicSize.u32Width * stPicSize.u32Height * 3 / 4, 64); /* stream buffer size */
    }
    stVencChnAttr.stVencAttr.u32Profile = u32Profile;
    stVencChnAttr.stVencAttr.bByFrame = HI_TRUE; /* get stream mode is slice mode or frame mode? */

    if (VENC_GOPMODE_SMARTP == pstGopAttr->enGopMode) {
        u32StatTime = pstGopAttr->stSmartP.u32BgInterval / u32Gop;
    } else {
        u32StatTime = 1;
    }

    switch (enType) {
        case PT_H265: {
            if (OS_RC_CBR == enRcMode) {
                VENC_H265_CBR_S stH265Cbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CBR;
                stH265Cbr.u32Gop = u32Gop;
                stH265Cbr.u32StatTime = u32StatTime;       /* stream rate statics time(s) */
                stH265Cbr.u32SrcFrameRate = u32FrameRate;  /* input (vi) frame rate */
                stH265Cbr.fr32DstFrameRate = u32FrameRate; /* target frame rate */
                switch (enSize) {
                    case PIC_720P:
                        stH265Cbr.u32BitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH265Cbr.u32BitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH265Cbr.u32BitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH265Cbr.u32BitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH265Cbr.u32BitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH265Cbr.u32BitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH265Cbr.u32BitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265Cbr, sizeof(VENC_H265_CBR_S), &stH265Cbr,
                    sizeof(VENC_H265_CBR_S));
            } else if (OS_RC_FIXQP == enRcMode) {
                VENC_H265_FIXQP_S stH265FixQp;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265FIXQP;
                stH265FixQp.u32Gop = u32Gop;
                stH265FixQp.u32SrcFrameRate = u32FrameRate;
                stH265FixQp.fr32DstFrameRate = u32FrameRate;
                stH265FixQp.u32IQp = 25;
                stH265FixQp.u32PQp = 30;
                stH265FixQp.u32BQp = 32;
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265FixQp, sizeof(VENC_H265_FIXQP_S), &stH265FixQp,
                    sizeof(VENC_H265_FIXQP_S));
            } else if (OS_RC_VBR == enRcMode) {
                VENC_H265_VBR_S stH265Vbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265VBR;
                stH265Vbr.u32Gop = u32Gop;
                stH265Vbr.u32StatTime = u32StatTime;
                stH265Vbr.u32SrcFrameRate = u32FrameRate;
                stH265Vbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_720P:
                        stH265Vbr.u32MaxBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH265Vbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH265Vbr.u32MaxBitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH265Vbr.u32MaxBitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH265Vbr.u32MaxBitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH265Vbr.u32MaxBitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH265Vbr.u32MaxBitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265Vbr, sizeof(VENC_H265_VBR_S), &stH265Vbr,
                    sizeof(VENC_H265_VBR_S));
            } else if (OS_RC_AVBR == enRcMode) {
                VENC_H265_AVBR_S stH265AVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265AVBR;
                stH265AVbr.u32Gop = u32Gop;
                stH265AVbr.u32StatTime = u32StatTime;
                stH265AVbr.u32SrcFrameRate = u32FrameRate;
                stH265AVbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_720P:
                        stH265AVbr.u32MaxBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH265AVbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH265AVbr.u32MaxBitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH265AVbr.u32MaxBitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH265AVbr.u32MaxBitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH265AVbr.u32MaxBitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH265AVbr.u32MaxBitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265AVbr, sizeof(VENC_H265_AVBR_S), &stH265AVbr,
                    sizeof(VENC_H265_AVBR_S));
            } else if (OS_RC_QVBR == enRcMode) {
                VENC_H265_QVBR_S stH265QVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265QVBR;
                stH265QVbr.u32Gop = u32Gop;
                stH265QVbr.u32StatTime = u32StatTime;
                stH265QVbr.u32SrcFrameRate = u32FrameRate;
                stH265QVbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_720P:
                        stH265QVbr.u32TargetBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH265QVbr.u32TargetBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH265QVbr.u32TargetBitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH265QVbr.u32TargetBitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH265QVbr.u32TargetBitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH265QVbr.u32TargetBitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH265QVbr.u32TargetBitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265QVbr, sizeof(VENC_H265_QVBR_S), &stH265QVbr,
                    sizeof(VENC_H265_QVBR_S));
            } else if (OS_RC_CVBR == enRcMode) {
                VENC_H265_CVBR_S stH265CVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265CVBR;
                stH265CVbr.u32Gop = u32Gop;
                stH265CVbr.u32StatTime = u32StatTime;
                stH265CVbr.u32SrcFrameRate = u32FrameRate;
                stH265CVbr.fr32DstFrameRate = u32FrameRate;
                stH265CVbr.u32LongTermStatTime = 1;
                stH265CVbr.u32ShortTermStatTime = u32StatTime;
                switch (enSize) {
                    case PIC_720P:
                        stH265CVbr.u32MaxBitRate = 1024 * 3 + 1024 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 512;
                        break;
                    case PIC_1080P:
                        stH265CVbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 1024;
                        break;
                    case PIC_2592x1944:
                        stH265CVbr.u32MaxBitRate = 1024 * 4 + 3072 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 1024 * 2;
                        break;
                    case PIC_3840x2160:
                        stH265CVbr.u32MaxBitRate = 1024 * 8 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 1024 * 3;
                        break;
                    case PIC_4000x3000:
                        stH265CVbr.u32MaxBitRate = 1024 * 12 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 1024 * 4;
                        break;
                    case PIC_7680x4320:
                        stH265CVbr.u32MaxBitRate = 1024 * 24 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 1024 * 6;
                        break;
                    default:
                        stH265CVbr.u32MaxBitRate = 1024 * 24 + 5120 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMaxBitrate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        stH265CVbr.u32LongTermMinBitrate = 1024 * 5;
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265CVbr, sizeof(VENC_H265_CVBR_S), &stH265CVbr,
                    sizeof(VENC_H265_CVBR_S));
            } else if (OS_RC_QPMAP == enRcMode) {
                VENC_H265_QPMAP_S stH265QpMap;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H265QPMAP;
                stH265QpMap.u32Gop = u32Gop;
                stH265QpMap.u32StatTime = u32StatTime;
                stH265QpMap.u32SrcFrameRate = u32FrameRate;
                stH265QpMap.fr32DstFrameRate = u32FrameRate;
                stH265QpMap.enQpMapMode = VENC_RC_QPMAP_MODE_MEANQP;
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH265QpMap, sizeof(VENC_H265_QPMAP_S), &stH265QpMap,
                    sizeof(VENC_H265_QPMAP_S));
            } else {
                OSALOG_ERROR("%s,%d,enRcMode(%d) not support\n", __FUNCTION__, __LINE__, enRcMode);
                return HI_FAILURE;
            }
            stVencChnAttr.stVencAttr.stAttrH265e.bRcnRefShareBuf = bRcnRefShareBuf;
        } break;
        case PT_H264: {
            if (OS_RC_CBR == enRcMode) {
                VENC_H264_CBR_S stH264Cbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
                stH264Cbr.u32Gop = u32Gop;                 /* the interval of IFrame */
                stH264Cbr.u32StatTime = u32StatTime;       /* stream rate statics time(s) */
                stH264Cbr.u32SrcFrameRate = u32FrameRate;  /* input (vi) frame rate */
                stH264Cbr.fr32DstFrameRate = u32FrameRate; /* target frame rate */
                switch (enSize) {
                    case PIC_720P:
                        stH264Cbr.u32BitRate = 1024 * 3 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH264Cbr.u32BitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH264Cbr.u32BitRate = 1024 * 4 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH264Cbr.u32BitRate = 1024 * 8 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH264Cbr.u32BitRate = 1024 * 12 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH264Cbr.u32BitRate = 1024 * 24 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH264Cbr.u32BitRate = 1024 * 24 + 5120 * u32FrameRate / 30;
                        break;
                }

                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264Cbr, sizeof(VENC_H264_CBR_S), &stH264Cbr,
                    sizeof(VENC_H264_CBR_S));
            } else if (OS_RC_FIXQP == enRcMode) {
                VENC_H264_FIXQP_S stH264FixQp;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264FIXQP;
                stH264FixQp.u32Gop = 30;
                stH264FixQp.u32SrcFrameRate = u32FrameRate;
                stH264FixQp.fr32DstFrameRate = u32FrameRate;
                stH264FixQp.u32IQp = 25;
                stH264FixQp.u32PQp = 30;
                stH264FixQp.u32BQp = 32;
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264FixQp, sizeof(VENC_H264_FIXQP_S), &stH264FixQp,
                    sizeof(VENC_H264_FIXQP_S));
            } else if (OS_RC_VBR == enRcMode) {
                VENC_H264_VBR_S stH264Vbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264VBR;
                stH264Vbr.u32Gop = u32Gop;
                stH264Vbr.u32StatTime = u32StatTime;
                stH264Vbr.u32SrcFrameRate = u32FrameRate;
                stH264Vbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_360P:
                        stH264Vbr.u32MaxBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_720P:
                        stH264Vbr.u32MaxBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH264Vbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH264Vbr.u32MaxBitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH264Vbr.u32MaxBitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH264Vbr.u32MaxBitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH264Vbr.u32MaxBitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH264Vbr.u32MaxBitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264Vbr, sizeof(VENC_H264_VBR_S), &stH264Vbr,
                    sizeof(VENC_H264_VBR_S));
            } else if (OS_RC_AVBR == enRcMode) {
                VENC_H264_VBR_S stH264AVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264AVBR;
                stH264AVbr.u32Gop = u32Gop;
                stH264AVbr.u32StatTime = u32StatTime;
                stH264AVbr.u32SrcFrameRate = u32FrameRate;
                stH264AVbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_360P:
                        stH264AVbr.u32MaxBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_720P:
                        stH264AVbr.u32MaxBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH264AVbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH264AVbr.u32MaxBitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH264AVbr.u32MaxBitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH264AVbr.u32MaxBitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH264AVbr.u32MaxBitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH264AVbr.u32MaxBitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264AVbr, sizeof(VENC_H264_AVBR_S), &stH264AVbr,
                    sizeof(VENC_H264_AVBR_S));
            } else if (OS_RC_QVBR == enRcMode) {
                VENC_H264_QVBR_S stH264QVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264QVBR;
                stH264QVbr.u32Gop = u32Gop;
                stH264QVbr.u32StatTime = u32StatTime;
                stH264QVbr.u32SrcFrameRate = u32FrameRate;
                stH264QVbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_360P:
                        stH264QVbr.u32TargetBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_720P:
                        stH264QVbr.u32TargetBitRate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stH264QVbr.u32TargetBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stH264QVbr.u32TargetBitRate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stH264QVbr.u32TargetBitRate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stH264QVbr.u32TargetBitRate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stH264QVbr.u32TargetBitRate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stH264QVbr.u32TargetBitRate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264QVbr, sizeof(VENC_H264_QVBR_S), &stH264QVbr,
                    sizeof(VENC_H264_QVBR_S));
            } else if (OS_RC_CVBR == enRcMode) {
                VENC_H264_CVBR_S stH264CVbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CVBR;
                stH264CVbr.u32Gop = u32Gop;
                stH264CVbr.u32StatTime = u32StatTime;
                stH264CVbr.u32SrcFrameRate = u32FrameRate;
                stH264CVbr.fr32DstFrameRate = u32FrameRate;
                stH264CVbr.u32LongTermStatTime = 1;
                stH264CVbr.u32ShortTermStatTime = u32StatTime;
                switch (enSize) {
                    case PIC_720P:
                        stH264CVbr.u32MaxBitRate = 1024 * 3 + 1024 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 2 + 1024 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 512;
                        break;
                    case PIC_1080P:
                        stH264CVbr.u32MaxBitRate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 2 + 2048 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 1024;
                        break;
                    case PIC_2592x1944:
                        stH264CVbr.u32MaxBitRate = 1024 * 4 + 3072 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 3 + 3072 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 1024 * 2;
                        break;
                    case PIC_3840x2160:
                        stH264CVbr.u32MaxBitRate = 1024 * 8 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 5 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 1024 * 3;
                        break;
                    case PIC_4000x3000:
                        stH264CVbr.u32MaxBitRate = 1024 * 12 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 10 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 1024 * 4;
                        break;
                    case PIC_7680x4320:
                        stH264CVbr.u32MaxBitRate = 1024 * 24 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 20 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 1024 * 6;
                        break;
                    default:
                        stH264CVbr.u32MaxBitRate = 1024 * 24 + 5120 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMaxBitrate = 1024 * 15 + 2048 * u32FrameRate / 30;
                        stH264CVbr.u32LongTermMinBitrate = 1024 * 5;
                        break;
                }
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264CVbr, sizeof(VENC_H264_CVBR_S), &stH264CVbr,
                    sizeof(VENC_H264_CVBR_S));
            } else if (OS_RC_QPMAP == enRcMode) {
                VENC_H264_QPMAP_S stH264QpMap;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264QPMAP;
                stH264QpMap.u32Gop = u32Gop;
                stH264QpMap.u32StatTime = u32StatTime;
                stH264QpMap.u32SrcFrameRate = u32FrameRate;
                stH264QpMap.fr32DstFrameRate = u32FrameRate;
                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stH264QpMap, sizeof(VENC_H264_QPMAP_S), &stH264QpMap,
                    sizeof(VENC_H264_QPMAP_S));
            } else {
                OSALOG_ERROR("%s,%d,enRcMode(%d) not support\n", __FUNCTION__, __LINE__, enRcMode);
                return HI_FAILURE;
            }
            stVencChnAttr.stVencAttr.stAttrH264e.bRcnRefShareBuf = bRcnRefShareBuf;
        } break;
        case PT_MJPEG: {
            if (OS_RC_FIXQP == enRcMode) {
                VENC_MJPEG_FIXQP_S stMjpegeFixQp;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGFIXQP;
                stMjpegeFixQp.u32Qfactor = 95;
                stMjpegeFixQp.u32SrcFrameRate = u32FrameRate;
                stMjpegeFixQp.fr32DstFrameRate = u32FrameRate;

                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stMjpegFixQp, sizeof(VENC_MJPEG_FIXQP_S), &stMjpegeFixQp,
                    sizeof(VENC_MJPEG_FIXQP_S));
            } else if (OS_RC_CBR == enRcMode) {
                VENC_MJPEG_CBR_S stMjpegeCbr;

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
                stMjpegeCbr.u32StatTime = u32StatTime;
                stMjpegeCbr.u32SrcFrameRate = u32FrameRate;
                stMjpegeCbr.fr32DstFrameRate = u32FrameRate;
                switch (enSize) {
                    case PIC_360P:
                        stMjpegeCbr.u32BitRate = 1024 * 3 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_720P:
                        stMjpegeCbr.u32BitRate = 1024 * 5 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stMjpegeCbr.u32BitRate = 1024 * 8 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stMjpegeCbr.u32BitRate = 1024 * 20 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stMjpegeCbr.u32BitRate = 1024 * 25 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stMjpegeCbr.u32BitRate = 1024 * 30 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stMjpegeCbr.u32BitRate = 1024 * 40 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stMjpegeCbr.u32BitRate = 1024 * 20 + 2048 * u32FrameRate / 30;
                        break;
                }

                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stMjpegCbr, sizeof(VENC_MJPEG_CBR_S), &stMjpegeCbr,
                    sizeof(VENC_MJPEG_CBR_S));
            } else if ((OS_RC_VBR == enRcMode) || (OS_RC_AVBR == enRcMode) || (OS_RC_QVBR == enRcMode) ||
                (OS_RC_CVBR == enRcMode)) {
                VENC_MJPEG_VBR_S stMjpegVbr;

                if (OS_RC_AVBR == enRcMode) {
                    OSALOG_ERROR("Mjpege not support AVBR, so change rcmode to VBR!\n");
                }

                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGVBR;
                stMjpegVbr.u32StatTime = u32StatTime;
                stMjpegVbr.u32SrcFrameRate = u32FrameRate;
                stMjpegVbr.fr32DstFrameRate = 5;

                switch (enSize) {
                    case PIC_360P:
                        stMjpegVbr.u32MaxBitRate = 1024 * 3 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_720P:
                        stMjpegVbr.u32MaxBitRate = 1024 * 5 + 1024 * u32FrameRate / 30;
                        break;
                    case PIC_1080P:
                        stMjpegVbr.u32MaxBitRate = 1024 * 8 + 2048 * u32FrameRate / 30;
                        break;
                    case PIC_2592x1944:
                        stMjpegVbr.u32MaxBitRate = 1024 * 20 + 3072 * u32FrameRate / 30;
                        break;
                    case PIC_3840x2160:
                        stMjpegVbr.u32MaxBitRate = 1024 * 25 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_4000x3000:
                        stMjpegVbr.u32MaxBitRate = 1024 * 30 + 5120 * u32FrameRate / 30;
                        break;
                    case PIC_7680x4320:
                        stMjpegVbr.u32MaxBitRate = 1024 * 40 + 5120 * u32FrameRate / 30;
                        break;
                    default:
                        stMjpegVbr.u32MaxBitRate = 1024 * 20 + 2048 * u32FrameRate / 30;
                        break;
                }

                (hi_void)memcpy_s(&stVencChnAttr.stRcAttr.stMjpegVbr, sizeof(VENC_MJPEG_VBR_S), &stMjpegVbr,
                    sizeof(VENC_MJPEG_VBR_S));
            } else {
                OSALOG_ERROR("cann't support other mode(%d) in this version!\n", enRcMode);
                return HI_FAILURE;
            }
        } break;

        case PT_JPEG:
            stJpegAttr.bSupportDCF = HI_FALSE;
            stJpegAttr.stMPFCfg.u8LargeThumbNailNum = 0;
            stJpegAttr.enReceiveMode = VENC_PIC_RECEIVE_SINGLE;
            (hi_void)memcpy_s(&stVencChnAttr.stVencAttr.stAttrJpege, sizeof(VENC_ATTR_JPEG_S), &stJpegAttr,
                sizeof(VENC_ATTR_JPEG_S));
            break;
        default:
            OSALOG_ERROR("cann't support this enType (%d) in this version!\n", enType);
            return HI_ERR_VENC_NOT_SUPPORT;
    }

    if (PT_MJPEG == enType || PT_JPEG == enType) {
        stVencChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;
        stVencChnAttr.stGopAttr.stNormalP.s32IPQpDelta = 0;
    } else {
        (hi_void)memcpy_s(&stVencChnAttr.stGopAttr, sizeof(VENC_GOP_ATTR_S), pstGopAttr, sizeof(VENC_GOP_ATTR_S));
        if ((VENC_GOPMODE_BIPREDB == pstGopAttr->enGopMode) && (PT_H264 == enType)) {
            if (0 == stVencChnAttr.stVencAttr.u32Profile) {
                stVencChnAttr.stVencAttr.u32Profile = 1;

                OSALOG_ERROR("H.264 base profile not support BIPREDB, so change profile to main profile!\n");
            }
        }

        if ((VENC_RC_MODE_H264QPMAP == stVencChnAttr.stRcAttr.enRcMode) ||
            (VENC_RC_MODE_H265QPMAP == stVencChnAttr.stRcAttr.enRcMode)) {
            if (VENC_GOPMODE_ADVSMARTP == pstGopAttr->enGopMode) {
                stVencChnAttr.stGopAttr.enGopMode = VENC_GOPMODE_SMARTP;

                OSALOG_ERROR("advsmartp not support QPMAP, so change gopmode to smartp!\n");
            }
        }
    }

    s32Ret = HI_MPI_VENC_CreateChn(VencChn, &stVencChnAttr);
    if (HI_SUCCESS != s32Ret) {
        OSALOG_ERROR("HI_MPI_VENC_CreateChn [%d] faild with %#x! ===\n", VencChn, s32Ret);
        return s32Ret;
    }

    s32Ret = OS_COMM_VENC_CloseReEncode(VencChn);
    if (HI_SUCCESS != s32Ret) {
        HI_MPI_VENC_DestroyChn(VencChn);
        return s32Ret;
    }

    return HI_SUCCESS;
}

static HI_S32 OS_COMM_VENC_GetGopAttr(VENC_GOP_MODE_E enGopMode, VENC_GOP_ATTR_S *pstGopAttr)
{
	switch (enGopMode) {
		case VENC_GOPMODE_NORMALP:
			pstGopAttr->enGopMode = VENC_GOPMODE_NORMALP;
			pstGopAttr->stNormalP.s32IPQpDelta = 2;
			break;
		case VENC_GOPMODE_SMARTP:
			pstGopAttr->enGopMode = VENC_GOPMODE_SMARTP;
			pstGopAttr->stSmartP.s32BgQpDelta = 4;
			pstGopAttr->stSmartP.s32ViQpDelta = 2;
			pstGopAttr->stSmartP.u32BgInterval = 90;
			break;

		case VENC_GOPMODE_DUALP:
			pstGopAttr->enGopMode = VENC_GOPMODE_DUALP;
			pstGopAttr->stDualP.s32IPQpDelta = 4;
			pstGopAttr->stDualP.s32SPQpDelta = 2;
			pstGopAttr->stDualP.u32SPInterval = 3;
			break;

		case VENC_GOPMODE_BIPREDB:
			pstGopAttr->enGopMode = VENC_GOPMODE_BIPREDB;
			pstGopAttr->stBipredB.s32BQpDelta = -2;
			pstGopAttr->stBipredB.s32IPQpDelta = 3;
			pstGopAttr->stBipredB.u32BFrmNum = 2;
			break;

		default:
			OSALOG_ERROR("not support the gop mode !\n");
			return HI_FAILURE;
			break;
	}
	return HI_SUCCESS;
}

/* *****************************************************************************
 * funciton : Start venc stream mode
 * note      : rate control parameter need adjust, according your case.
 * **************************************************************************** */
HI_S32 OS_COMM_VENC_Start(VENC_CHN VencChn, PAYLOAD_TYPE_E enType, OS_SIZE_E enSize, OS_RC_E enRcMode,
    HI_U32 u32Profile, HI_BOOL bRcnRefShareBuf, VENC_GOP_MODE_E enGopMode)
{
    HI_S32 s32Ret;
    VENC_RECV_PIC_PARAM_S stRecvParam;
	VENC_GOP_ATTR_S stGopAttr;

	OS_COMM_VENC_GetGopAttr(enGopMode, &stGopAttr);

    /* *****************************************
     step 1:  Creat Encode Chnl
    ***************************************** */
    s32Ret = OS_COMM_VENC_Creat(VencChn, enType, enSize, enRcMode, u32Profile, bRcnRefShareBuf, &stGopAttr);
    if (HI_SUCCESS != s32Ret) {
        OSALOG_ERROR("OS_COMM_VENC_Creat faild with%#x! \n", s32Ret);
        return HI_FAILURE;
    }
    /* *****************************************
     step 2:  Start Recv Venc Pictures
    ***************************************** */
    stRecvParam.s32RecvPicNum = -1;
    s32Ret = HI_MPI_VENC_StartRecvFrame(VencChn, &stRecvParam);
    if (s32Ret != HI_SUCCESS)
    {
        OSALOG_ERROR("HI_MPI_VENC_StartRecvFrame failed with %#x\n", s32Ret);
        return HI_FAILURE;
    }	

    return HI_SUCCESS;
}
	

/* *****************************************************************************
 * funciton : get file postfix according palyload_type.
 * **************************************************************************** */
static HI_S32 OS_COMM_VENC_GetFilePostfix(PAYLOAD_TYPE_E enPayload, HI_CHAR *szFilePostfix, HI_U8 len)
{
	if (PT_H264 == enPayload) {
		if (strcpy_s(szFilePostfix, len, ".h264") != EOK) {
			return HI_FAILURE;
		}
	} else if (PT_H265 == enPayload) {
		if (strcpy_s(szFilePostfix, len, ".h265") != EOK) {
			return HI_FAILURE;
		}
	} else if (PT_JPEG == enPayload) {
		if (strcpy_s(szFilePostfix, len, ".jpg") != EOK) {
			return HI_FAILURE;
		}
	} else if (PT_MJPEG == enPayload) {
		if (strcpy_s(szFilePostfix, len, ".mjp") != EOK) {
			return HI_FAILURE;
		}
	} else {
		OSALOG_ERROR("payload type err!\n");
		return HI_FAILURE;
	}
	return HI_SUCCESS;
}

/* *****************************************************************************
 * funciton : save stream
 * **************************************************************************** */
static HI_S32 OS_COMM_VENC_SaveStream(FILE *pFd, VENC_STREAM_S *pstStream)
{
	HI_S32 i;

	for (i = 0; i < pstStream->u32PackCount; i++) {
		fwrite(pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset,
			pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset, 1, pFd);

		fflush(pFd);
	}

	return HI_SUCCESS;
}

static HI_S32 OS_COMM_VENC_SendStreamToUser(VENC_STREAM_S *pstStream, fGetVideoStreamCb fCb)
{
	if (!fCb)
		return 0;

	int buflen = 0;
	for (int i = 0; i < pstStream->u32PackCount; i++) {
		// 1. 去掉 00 00 00 01 起始码
		if (pstStream->pstPack[i].pu8Addr[0] == 0x00 && pstStream->pstPack[i].pu8Addr[1] == 0x00 && pstStream->pstPack[i].pu8Addr[2] == 0x00
			&& pstStream->pstPack[i].pu8Addr[3] == 0x01)
		{
			//printf("i:%d nalue type:%#x %d %d\n", i, pstStream->pstPack[i].pu8Addr[4], pstStream->pstPack[i].u32Len, pstStream->pstPack[i].u32Offset);
			buflen = (pstStream->pstPack[i].u32Len - pstStream->pstPack[i].u32Offset - 4);
			uint8_t *buff = malloc(buflen);
			memcpy(buff, pstStream->pstPack[i].pu8Addr + pstStream->pstPack[i].u32Offset + 4, buflen);
			
			// 单次发送一个NALU，buff 由回调自动释放
			fCb(buff, buflen);
		}
	}	

	return 0;
}

/* *****************************************************************************
 * funciton : get stream from each channels and save them
 * **************************************************************************** */
static HI_VOID *OS_COMM_VENC_GetVencStreamProc(HI_VOID *p)
{
	HI_S32 i;
	HI_S32 s32ChnTotal;
	VENC_CHN_ATTR_S stVencChnAttr;
	OS_VENC_GETSTREAM_PARA_S *pstPara = HI_NULL;
	HI_S32 maxfd = 0;
	struct timeval TimeoutVal;
	fd_set read_fds;
	HI_U32 u32PictureCnt[VENC_MAX_CHN_NUM]={0};
	HI_S32 VencFd[VENC_MAX_CHN_NUM];
#if SAVE_FILE
	HI_CHAR aszFileName[VENC_MAX_CHN_NUM][FILE_NAME_LEN];
	HI_CHAR real_file_name[VENC_MAX_CHN_NUM][PATH_MAX];
	FILE* pFile[VENC_MAX_CHN_NUM] = {HI_NULL};
#endif
	HI_S32 fd[VENC_MAX_CHN_NUM] = {0};
	char szFilePostfix[10];
	VENC_CHN_STATUS_S stStat;
	VENC_STREAM_S stStream;
	HI_S32 s32Ret;
	VENC_CHN VencChn;
	PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
	VENC_STREAM_BUF_INFO_S stStreamBufInfo[VENC_MAX_CHN_NUM];

	prctl(PR_SET_NAME, "GetVencStream", 0, 0, 0);

	pstPara = (OS_VENC_GETSTREAM_PARA_S *)p;
	s32ChnTotal = pstPara->s32Cnt;
	/* *****************************************
	 step 1:  check & prepare save-file & venc-fd
	***************************************** */
	if (s32ChnTotal >= VENC_MAX_CHN_NUM) {
		OSALOG_ERROR("input count invaild\n");
		return NULL;
	}
	
	OSALOG_INFO("VencChnTotal:%d Start:%d start get streaming\n", s32ChnTotal, pstPara->VeChn[0]);
	for (i = 0; i < s32ChnTotal; i++) {
		/* decide the stream file name, and open file to save stream */
		VencChn = pstPara->VeChn[i];
		s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
		if (s32Ret != HI_SUCCESS) {
			OSALOG_ERROR("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", VencChn, s32Ret);
			return NULL;
		}
		enPayLoadType[i] = stVencChnAttr.stVencAttr.enType;

		s32Ret = OS_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix, sizeof(szFilePostfix));
		if (s32Ret != HI_SUCCESS) {
			OSALOG_ERROR("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n", stVencChnAttr.stVencAttr.enType,
				s32Ret);
			return NULL;
		}
#if SAVE_FILE
		if (PT_JPEG != enPayLoadType[i]) {
			if (snprintf_s(aszFileName[i], FILE_NAME_LEN, FILE_NAME_LEN - 1, "./") < 0) {
				return NULL;
			}
			if (realpath(aszFileName[i], real_file_name[i]) == NULL) {
				OSALOG_ERROR("chn %d stream path error\n", VencChn);
			}

			if (snprintf_s(real_file_name[i], FILE_NAME_LEN, FILE_NAME_LEN - 1, "stream_chn%d%s", i, szFilePostfix) <
				0) {
				return NULL;
			}

			fd[i] = open(real_file_name[i], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
			if (fd[i] < 0) {
				OSALOG_ERROR("open file[%s] failed!\n", real_file_name[i]);
				return NULL;
			}

			pFile[i] = fdopen(fd[i], "wb");
			if (HI_NULL == pFile[i]) {
				OSALOG_ERROR("fdopen error!\n");
				close(fd[i]);
				return NULL;
			}
		}
#endif
		/* Set Venc Fd. */
		VencFd[i] = HI_MPI_VENC_GetFd(VencChn);
		if (VencFd[i] < 0) {
			OSALOG_ERROR("HI_MPI_VENC_GetFd failed with %#x!\n", VencFd[i]);
			return NULL;
		}
		if (maxfd <= VencFd[i]) {
			maxfd = VencFd[i];
		}

		s32Ret = HI_MPI_VENC_GetStreamBufInfo(VencChn, &stStreamBufInfo[i]);
		if (HI_SUCCESS != s32Ret) {
			OSALOG_ERROR("HI_MPI_VENC_GetStreamBufInfo failed with %#x!\n", s32Ret);
			return (void *)HI_FAILURE;
		}
	}

	/* *****************************************
	 step 2:  Start to get streams of each channel.
	***************************************** */
	while (HI_TRUE == pstPara->bThreadStart) {
		FD_ZERO(&read_fds);
		for (i = 0; i < s32ChnTotal; i++) {
			FD_SET(VencFd[i], &read_fds);
		}

		TimeoutVal.tv_sec = 2;
		TimeoutVal.tv_usec = 0;
		s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
		if (s32Ret < 0) {
			OSALOG_ERROR("select failed!\n");
			break;
		} else if (s32Ret == 0) {
			OSALOG_ERROR("get venc stream time out, exit thread\n");
			continue;
		} else {
			for (i = 0; i < s32ChnTotal; i++) {
				if (FD_ISSET(VencFd[i], &read_fds)) {
					/* ******************************************************
					 step 2.1 : query how many packs in one-frame stream.
					****************************************************** */
					(HI_VOID)memset_s(&stStream, sizeof(stStream), 0, sizeof(stStream));

					VencChn = pstPara->VeChn[i];

					s32Ret = HI_MPI_VENC_QueryStatus(VencChn, &stStat);
					if (HI_SUCCESS != s32Ret) {
						OSALOG_ERROR("HI_MPI_VENC_QueryStatus chn[%d] failed with %#x!\n", VencChn, s32Ret);
						break;
					}

					/* ******************************************************
					step 2.2 :suggest to check both u32CurPacks and u32LeftStreamFrames at the same time,for example:
					 if(0 == stStat.u32CurPacks || 0 == stStat.u32LeftStreamFrames)
					 {
						SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
						continue;
					 }
					****************************************************** */
					if (0 == stStat.u32CurPacks) {
						OSALOG_ERROR("NOTE: Current  frame is NULL!\n");
						continue;
					}
					/* ******************************************************
					 step 2.3 : malloc corresponding number of pack nodes.
					****************************************************** */
					stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
					if (NULL == stStream.pstPack) {
						OSALOG_ERROR("malloc stream pack failed!\n");
						break;
					}

					/* ******************************************************
					 step 2.4 : call mpi to get one-frame stream
					****************************************************** */
					stStream.u32PackCount = stStat.u32CurPacks;
					s32Ret = HI_MPI_VENC_GetStream(VencChn, &stStream, HI_TRUE);
					if (HI_SUCCESS != s32Ret) {
						free(stStream.pstPack);
						stStream.pstPack = NULL;
						OSALOG_ERROR("HI_MPI_VENC_GetStream failed with %#x!\n", s32Ret);
						break;
					}

					/* ******************************************************
					 step 2.5 : save frame to file
					****************************************************** */
#if SAVE_FILE
					if (PT_JPEG == enPayLoadType[i]) {
						if (strcpy_s(szFilePostfix, sizeof(szFilePostfix), ".jpg") != EOK) {
							free(stStream.pstPack);
							stStream.pstPack = NULL;
							return HI_NULL;
						}
						if (snprintf_s(aszFileName[i], FILE_NAME_LEN, FILE_NAME_LEN - 1, "./") < 0) {
							free(stStream.pstPack);
							stStream.pstPack = NULL;
							return NULL;
						}
						if (realpath(aszFileName[i], real_file_name[i]) == NULL) {
							OSALOG_ERROR("chn %d stream path error\n", VencChn);
							free(stStream.pstPack);
							stStream.pstPack = NULL;
							return NULL;
						}

						if (snprintf_s(real_file_name[i], FILE_NAME_LEN, FILE_NAME_LEN - 1, "stream_chn%d_%d%s",
							VencChn, u32PictureCnt[i], szFilePostfix) < 0) {
							free(stStream.pstPack);
							stStream.pstPack = NULL;
							return NULL;
						}

						fd[i] = open(real_file_name[i], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
						if (fd[i] < 0) {
							OSALOG_ERROR("open file[%s] failed!\n", real_file_name[i]);
							free(stStream.pstPack);
							stStream.pstPack = NULL;
							return NULL;
						}

						pFile[i] = fdopen(fd[i], "wb");
						if (!pFile[i]) {
							free(stStream.pstPack);
							stStream.pstPack = NULL;
							OSALOG_ERROR("fdopen err!\n");
							close(fd[i]);
							return NULL;
						}
					}


					s32Ret = OS_COMM_VENC_SaveStream(pFile[i], &stStream);
					if (HI_SUCCESS != s32Ret) {
						free(stStream.pstPack);
						stStream.pstPack = NULL;
						OSALOG_ERROR("save stream failed!\n");
						break;
					}
#endif

					OS_COMM_VENC_SendStreamToUser(&stStream, pstPara->fUserGetframecb);

					/* ******************************************************
					 step 2.6 : release stream
					 ****************************************************** */
					s32Ret = HI_MPI_VENC_ReleaseStream(VencChn, &stStream);
					if (HI_SUCCESS != s32Ret) {
						OSALOG_ERROR("HI_MPI_VENC_ReleaseStream failed!\n");
						free(stStream.pstPack);
						stStream.pstPack = NULL;
						break;
					}

					/* ******************************************************
					 step 2.7 : free pack nodes
					****************************************************** */
					free(stStream.pstPack);
					stStream.pstPack = NULL;
					u32PictureCnt[i]++;
#if SAVE_FILE
					if (PT_JPEG == enPayLoadType[i]) {
						fclose(pFile[i]);
					}
#endif
				}
			}
		}
	}
	/* ******************************************************
	 * step 3 : close save-file
	 * ***************************************************** */
#if SAVE_FILE
	for (i = 0; i < s32ChnTotal; i++) {
		if (PT_JPEG != enPayLoadType[i]) {
			fclose(pFile[i]);
		}
	}
#endif
	return NULL;
}

/* *****************************************************************************
 * funciton : start get venc stream process thread
 * **************************************************************************** */
HI_S32 OS_COMM_VENC_StartGetStream(VENC_CHN VeChn[], HI_S32 s32Cnt, fGetVideoStreamCb cb)
{
	HI_U32 i;
	pthread_t gs_VencPid;

	gs_stPara.bThreadStart = HI_TRUE;
	gs_stPara.s32Cnt = s32Cnt;
	for (i = 0; i < s32Cnt && i < VENC_MAX_CHN_NUM; i++) {
		gs_stPara.VeChn[i] = VeChn[i];
		gs_stPara.fUserGetframecb = cb;
	}
	return pthread_create(&gs_VencPid, 0, OS_COMM_VENC_GetVencStreamProc, (HI_VOID *)&gs_stPara);
}

HI_S32 OS_COMM_VENC_RequestIDR(VENC_CHN chn, HI_BOOL bInstant)
{
	HI_S32 ret = HI_MPI_VENC_RequestIDR(chn, bInstant);
	if (ret != HI_SUCCESS)
	{
		OSALOG_ERROR("HI_MPI_VENC_RequestIDR failed!ret=%#x\n", ret)
		return ret;
	}
	return ret;
}
	

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

