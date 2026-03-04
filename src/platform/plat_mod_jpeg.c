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

#include "plat_mpp.h"
#include "plat_mpp_priv.h"


#ifdef __cplusplus
#if __cplusplus
extern "c"{
#endif
#endif

/*===================================================================
 * 函数名称:  mpp_jpg_open
 * 功能描述:  创建JPEG编码通道（通过VENC模块）
===================================================================*/
static int32_t mpp_jpg_open(uint32_t chn_num, uint32_t width, uint32_t height)
{
	HI_S32 ret;
	VENC_CHN_ATTR_S stChnAttr;
 
	/* 检查通道号范围 */
	if (chn_num < 0 || chn_num >= VENC_MAX_CHN_NUM) {
		printf("[ERROR] Invalid channel number %d\n", chn_num);
		return -1;
	}
 
	/* 配置VENC通道属性 - JPEG编码 */
	memset(&stChnAttr, 0, sizeof(stChnAttr));
	
	/* 编码类型设为JPEG */
	stChnAttr.stVencAttr.enType = PT_JPEG;
	
	/* 最大分辨率 */
	stChnAttr.stVencAttr.u32MaxPicWidth = width;
	stChnAttr.stVencAttr.u32MaxPicHeight = height;
	
	/* 实际编码分辨率 */
	stChnAttr.stVencAttr.u32PicWidth = width;
	stChnAttr.stVencAttr.u32PicHeight = height;
	
	/* Stride设置（必须与输入匹配）*/
	stChnAttr.stVencAttr.u32Profile = 0;
	stChnAttr.stVencAttr.u32BufSize = width*height*3/2;
	
	/* 图像质量配置 */
	stChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGFIXQP;  // 质量因子 1-100
	stChnAttr.stRcAttr.stMjpegFixQp.u32SrcFrameRate = 25;
	stChnAttr.stRcAttr.stMjpegFixQp.fr32DstFrameRate = 8;
	stChnAttr.stRcAttr.stMjpegFixQp.u32Qfactor = 90;

	stChnAttr.stVencAttr.bByFrame = HI_TRUE;
	
	/* 创建VENC通道 */
	ret = HI_MPI_VENC_CreateChn(chn_num, &stChnAttr);
	if (ret != HI_SUCCESS) {
		OSALOG_ERROR("[ERROR] HI_MPI_VENC_CreateChn failed with 0x%x\n", ret);
		return ret;
	}
 
	/* 启动接收帧 */
	VENC_RECV_PIC_PARAM_S stRecParam;
	stRecParam.s32RecvPicNum = -1;
	ret = HI_MPI_VENC_StartRecvFrame(chn_num, &stRecParam);
	if (ret != HI_SUCCESS) {
		OSALOG_ERROR("[ERROR] HI_MPI_VENC_StartRecvFrame failed with 0x%x\n", ret);
		HI_MPI_VENC_DestroyChn(chn_num);
		return ret;
	}
 
//	OSALOG_INFO("[INFO] VENC channel %d created, resolution: %d * %d \n", 
//		   chn_num, width, height);
 
	return HI_SUCCESS;
}


/*===================================================================
 * 函数名称:  mpp_jpg_close
 * 功能描述:  销毁JPEG编码通道
===================================================================*/
static HI_S32 mpp_jpg_close(HI_S32 chn_num)
{
    HI_S32 ret;

    /* 停止接收帧 */
    ret = HI_MPI_VENC_StopRecvFrame(chn_num);
    if (ret != HI_SUCCESS) {
        OSALOG_ERROR("[ERROR] HI_MPI_VENC_StopRecvFrame failed with 0x%x\n", ret);
    }

    /* 销毁通道 */
    ret = HI_MPI_VENC_DestroyChn(chn_num);
    if (ret != HI_SUCCESS) {
        OSALOG_ERROR("[ERROR] HI_MPI_VENC_DestroyChn failed with 0x%x\n", ret);
        return ret;
    }

    return HI_SUCCESS;
}

/*===================================================================
 * 函数名称:  sample_jpeg_encode
 * 功能描述:  执行JPEG编码并保存文件
===================================================================*/
static HI_S32 mpp_jpeg_encoding(uint32_t chn_num, VIDEO_FRAME_INFO_S *data, jpgenc_info_t* out)
{
    HI_S32 ret;
    HI_U32 i;
    
    VENC_STREAM_S stStream;
    VENC_CHN_STATUS_S stStat;
    VIDEO_FRAME_INFO_S stFrameInfo;
	
    /* 1. 构建帧信息 */
    memset(&stFrameInfo, 0, sizeof(stFrameInfo));
	stFrameInfo.stVFrame.enField = VIDEO_FIELD_FRAME;
    stFrameInfo.stVFrame.u32Width = data->stVFrame.u32Width;
    stFrameInfo.stVFrame.u32Height = data->stVFrame.u32Height;
    stFrameInfo.stVFrame.enPixelFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
    stFrameInfo.stVFrame.enCompressMode = COMPRESS_MODE_NONE;
    stFrameInfo.stVFrame.u64PhyAddr[0] = data->stVFrame.u64PhyAddr[0];
    stFrameInfo.stVFrame.u64PhyAddr[1] = data->stVFrame.u64PhyAddr[1];
    stFrameInfo.stVFrame.u32Stride[0] = data->stVFrame.u32Stride[0];
    stFrameInfo.stVFrame.u32Stride[1] = data->stVFrame.u32Stride[1];
	stFrameInfo.enModId = data->enModId;
	stFrameInfo.u32PoolId = data->u32PoolId;

    /* 2. 发送帧到编码器 */
    ret = HI_MPI_VENC_SendFrame(chn_num, &stFrameInfo, -1);
    if (ret != HI_SUCCESS) {
        OSALOG_ERROR("[ERROR] HI_MPI_VENC_SendFrame failed with 0x%x\n", ret);
        return ret;
    }

    /* 3. 获取编码码流 */
    memset(&stStream, 0, sizeof(stStream)); 
    /* 轮询获取码流 */
	while(1)
	{
		ret = HI_MPI_VENC_QueryStatus(chn_num, &stStat);
		if (HI_SUCCESS != ret) {
			OSALOG_ERROR("HI_MPI_VENC_QueryStatus chn[%d] failed with %#x!\n", chn_num, ret);
			return ret;
		}
		
		if (0 == stStat.u32CurPacks) {
			OSALOG_WARN("NOTE: Current  frame is NULL!\n");
			usleep(10000);
			continue;
		}
		
		stStream.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
		stStream.u32PackCount = stStat.u32CurPacks;
		break;
	}
	
    ret = HI_MPI_VENC_GetStream(chn_num, &stStream, 100);
    if (ret != HI_SUCCESS) {
		OSALOG_ERROR("[ERROR] HI_MPI_VENC_GetStream failed with 0x%x\n", ret);
		free(stStream.pstPack);
        return ret;
    }

//   OSALOG_INFO("[INFO] Encode output: %d bytes, pack count: %d\n", stStream.u32PackCount <= 1 ? stStream.pstPack[0].u32Len : 0,
//           stStream.u32PackCount);
    
    /* 保存码流 */
	uint32_t jpglen = 0, buflen = 0;
    for (i = 0; i < stStream.u32PackCount; i++) {
		buflen = stStream.pstPack[i].u32Len - stStream.pstPack[i].u32Offset;
		jpglen += buflen;
    }

	memset(out, 0, sizeof(jpgenc_info_t));
	out->data = malloc(jpglen);
	for (i = 0; i < stStream.u32PackCount; i++) {
		buflen = stStream.pstPack[i].u32Len - stStream.pstPack[i].u32Offset;
		memcpy(out->data + out->size, stStream.pstPack[i].pu8Addr + stStream.pstPack[i].u32Offset, buflen);
		out->size += buflen;
	}
	out->width = stFrameInfo.stVFrame.u32Width;
	out->height = stFrameInfo.stVFrame.u32Height;
	out->utc = stStream.pstPack[i].u64PTS;

    /* 4. 释放码流 */
    ret = HI_MPI_VENC_ReleaseStream(chn_num, &stStream);
    if (ret != HI_SUCCESS) {
        OSALOG_ERROR("[ERROR] HI_MPI_VENC_ReleaseStream failed with 0x%x\n", ret);
    }

	free(stStream.pstPack);

    return HI_SUCCESS;
}

int32_t mpp_jpg_releaseCB(void *priv)
{
	if (priv)
		free(priv);
}

int32_t mpp_encode_jpg(void *data, jpgenc_info_t *out)
{
	
	struct vpp_frame_priv *priv = (struct vpp_frame_priv *)data;
	VIDEO_FRAME_INFO_S *frame = &priv->stVideoFrame;
	if (HI_SUCCESS != mpp_jpg_open(JPEG_CHN_ID, frame->stVFrame.u32Width, frame->stVFrame.u32Height))
	{
		return AV_R_EFAIL;
	}

	if (HI_SUCCESS != mpp_jpeg_encoding(JPEG_CHN_ID, frame, out))
	{
		mpp_jpg_close(JPEG_CHN_ID);
		return AV_R_EFAIL;
	}
	
	out->priv = out->data;
	out->free = (void *)mpp_jpg_releaseCB;

	mpp_jpg_close(JPEG_CHN_ID);
	
	return AV_R_SOK;
}

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif


