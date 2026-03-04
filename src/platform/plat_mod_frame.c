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
#include "mpi_vpss.h"
#include "mpi_vgs.h"
#include "hi_buffer.h"

#include "hi_common.h"
#include "hi_tde_api.h"
#include "plat_mpp.h"
#include "plat_mpp_priv.h"


#ifdef __cplusplus
#if __cplusplus
extern "c"{
#endif
#endif


static VB_POOL g_AlgoBlkPool = VB_INVALID_HANDLE;
static uint32_t g_algoWidth, g_algoHeight;
static HI_BOOL gbUsedTDE = HI_FALSE;
static HI_BOOL gbUsedVGS = HI_TRUE;

int32_t mpp_tde_scale(VIDEO_FRAME_S* input, VIDEO_FRAME_S* output);
int mpp_vgs_scale(VIDEO_FRAME_INFO_S* input, VIDEO_FRAME_INFO_S* output);

void vpp_release_frame(void *p)
{
	HI_S32 ret;
	struct vpp_frame_priv *priv = (struct vpp_frame_priv *)p;

	//OSALOG_INFO("bVGS:%d hBlk:%#x\n", priv->bUsedVGS, priv->hBlk);
	if (priv->bUsedTDE == HI_TRUE || priv->bUsedVGS == HI_TRUE)
	{
		uint32_t blksize = COMMON_GetPicBufferSize(g_algoWidth, g_algoHeight, PIXEL_FORMAT_YVU_SEMIPLANAR_420,
			DATA_BITWIDTH_8, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
		ret = HI_MPI_SYS_Munmap(priv->virAddr[0], blksize);
		if (ret != HI_SUCCESS)
		{
			OSALOG_ERROR("HI_MPI_SYS_Munmap failed! ret = %d\n", ret);
		}

		ret = HI_MPI_VB_ReleaseBlock(priv->hBlk);
		if (ret != HI_SUCCESS)
		{
			OSALOG_ERROR("HI_MPI_VB_ReleaseBlock failed! ret = %d\n", ret);
		}
		
	}else{
		if (priv->bMmap == HI_TRUE)
		{
			HI_MPI_SYS_Munmap(priv->virAddr[0], priv->planesize[0]);
			HI_MPI_SYS_Munmap(priv->virAddr[1], priv->planesize[1]);
		}
		
		ret = HI_MPI_VPSS_ReleaseChnFrame(VPSS_GROUP_ID, priv->chn, &priv->stVideoFrame);
		if (ret != HI_SUCCESS)
		{
			OSALOG_ERROR("HI_MPI_VPSS_ReleaseChnFrame failed! ret = %d\n", ret);
		}
	}

	return;
}

int32_t mpp_vpp_getframe(int32_t chn, vpp_frame_info_t *algonode)
{
	HI_S32 s32Ret;
	HI_S32 s32MilliSec = 100;
	struct vpp_frame_priv *priv = malloc(sizeof(struct vpp_frame_priv));
	memset(priv, 0, sizeof(struct vpp_frame_priv));
	
	VIDEO_FRAME_S* pVframe = &priv->stVideoFrame.stVFrame;
	VIDEO_FRAME_INFO_S stVppFrame;
	uint8_t* pVirAddrY = NULL;
	uint8_t* pVirAddrUC = NULL;
	uint32_t u32Ysize;
	uint32_t u32UvHeight;
	VIDEO_FRAME_S output;
	VB_BLK blkInput;
	if ((s32Ret = HI_MPI_VPSS_GetChnFrame(VPSS_GROUP_ID, chn, &stVppFrame, s32MilliSec)) != HI_SUCCESS)
	{
		OSALOG_ERROR("Get frame from VPSS fail(0x%x)!\n", s32Ret);
		return AV_R_EFAIL;
	}
	u32Ysize = pVframe->u32Stride[0] * pVframe->u32Height;
	u32UvHeight = pVframe->u32Height / 2;

	if (gbUsedVGS)
	{
		uint32_t blksize = COMMON_GetPicBufferSize(g_algoWidth, g_algoHeight, PIXEL_FORMAT_YVU_SEMIPLANAR_420,
			DATA_BITWIDTH_8, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
		blkInput = HI_MPI_VB_GetBlock(g_AlgoBlkPool, blksize, NULL);
		if (blkInput == VB_INVALID_HANDLE)
		{
			OSALOG_ERROR("Get Input VB Block failed!\n");
			s32Ret = -1;
			goto out;
		}
		HI_U64 physAddrInput = HI_MPI_VB_Handle2PhysAddr(blkInput);
		uint8_t *virtAddr = HI_MPI_SYS_Mmap(physAddrInput, blksize);
		if (!virtAddr)
		{
			OSALOG_ERROR("Mmap Input failed!\n");
			HI_MPI_VB_ReleaseBlock(blkInput);
			goto out;
		}

		VIDEO_FRAME_INFO_S algoFrame;
		memset(&algoFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
		algoFrame.stVFrame.enPixelFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;  // NV12
		algoFrame.stVFrame.u32Width = g_algoWidth;
		algoFrame.stVFrame.u32Height = g_algoHeight;
		algoFrame.stVFrame.u32Stride[0] = g_algoWidth;	 // Y 分量 stride
		algoFrame.stVFrame.u32Stride[1] = g_algoWidth;	 // UV 分量 stride
		algoFrame.stVFrame.u64PhyAddr[0] = physAddrInput;  // Y 平面地址
		algoFrame.stVFrame.u64PhyAddr[1] = physAddrInput + g_algoWidth * g_algoHeight;  // UV 平面地址
		algoFrame.stVFrame.u64PhyAddr[2] = algoFrame.stVFrame.u64PhyAddr[1];  // UV 平面地址
		algoFrame.stVFrame.u64VirAddr[0] = (HI_U64)(unsigned long)virtAddr;
		algoFrame.stVFrame.u64VirAddr[1] = 0;
		algoFrame.u32PoolId = g_AlgoBlkPool;
		algoFrame.enModId = HI_ID_VB;

		if ((s32Ret = mpp_vgs_scale(&stVppFrame, &algoFrame)) != HI_SUCCESS)
		{
			HI_MPI_VB_ReleaseBlock(blkInput);
			goto out;
		}
		output = algoFrame.stVFrame;

		priv->stVideoFrame = algoFrame;
	}
	else if (gbUsedTDE)
	{// Hi3516CV500 TDE缩放不支持YUV格式
		uint32_t blksize = COMMON_GetPicBufferSize(g_algoWidth, g_algoHeight, PIXEL_FORMAT_YVU_SEMIPLANAR_420,
			DATA_BITWIDTH_8, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
		VB_BLK blkInput = HI_MPI_VB_GetBlock(g_AlgoBlkPool, blksize, NULL);
		if (blkInput == VB_INVALID_HANDLE)
		{
			OSALOG_ERROR("Get Input VB Block failed!Ret=0x%x\n", s32Ret);
			goto out;
		}
		HI_U64 physAddrInput = HI_MPI_VB_Handle2PhysAddr(blkInput);
		uint8_t *virtAddr = HI_MPI_SYS_Mmap(physAddrInput, blksize);
		if (!virtAddr)
		{
			OSALOG_ERROR("Mmap Input failed!\n");
			HI_MPI_VB_ReleaseBlock(blkInput);
			goto out;
		}	
		
		output.u32Width = g_algoWidth;
		output.u32Height = g_algoHeight;
		output.u64PhyAddr[0] = physAddrInput;
		output.u64VirAddr[0] = (HI_U64)(unsigned long)virtAddr;
		output.u32Stride[0] = g_algoWidth;

		if (!mpp_tde_scale(&stVppFrame.stVFrame, &output))
		{
			HI_MPI_VB_ReleaseBlock(blkInput);
			goto out;
		}
	}
	else if (PIXEL_FORMAT_YVU_SEMIPLANAR_420 == pVframe->enPixelFormat)
    {
		if (pVframe->u64VirAddr[0] == HI_NULL)
		{
			pVirAddrY = HI_MPI_SYS_Mmap(pVframe->u64PhyAddr[0], u32Ysize);
			if (HI_NULL == pVirAddrY)
			{
				OSALOG_ERROR("HI_MPI_SYS_Mmap for pY_map fail!!\n");
				return AV_R_EFAIL;
			}
			pVirAddrUC = HI_MPI_SYS_Mmap(pVframe->u64PhyAddr[1], u32UvHeight * pVframe->u32Stride[1]);
			if (HI_NULL == pVirAddrUC)
			{
				OSALOG_ERROR("HI_MPI_SYS_Mmap for pY_map fail!!\n");
				return AV_R_EFAIL;
			}


			output.u32Width = pVframe->u32Width;
			output.u32Height = pVframe->u32Height;
			output.u64PhyAddr[0] = pVframe->u64PhyAddr[0];
			output.u64PhyAddr[1] = pVframe->u64PhyAddr[1];
			output.u64VirAddr[0] = (HI_U64)(unsigned long)pVirAddrY;
			output.u64VirAddr[1] = (HI_U64)(unsigned long)pVirAddrUC;
			output.u32Stride[0] = g_algoWidth;
			output.u32Stride[1] = g_algoWidth;
			priv->stVideoFrame = stVppFrame;
		}

    }

#if 0
	if (0 != access("./1.yuv", F_OK))
    {
		FILE *fp = fopen("./1.yuv", "w+");
		fwrite(priv->virAddr[0], 1, priv->planesize[0], fp);
		fwrite(priv->virAddr[1], 1, priv->planesize[1], fp);
		fflush(fp);
		fclose(fp);
	}
#endif

	algonode->frame.width = output.u32Width;
	algonode->frame.height = output.u32Height;
	memcpy(algonode->frame.data, output.u64VirAddr, sizeof(output.u64VirAddr));
	memcpy(algonode->frame.stride, output.u32Stride, sizeof(output.u32Stride));
	algonode->frame.size[0] = u32Ysize;
	algonode->frame.size[1] = u32UvHeight * pVframe->u32Stride[1];

	priv->chn = chn;
	memcpy(priv->virAddr, output.u64VirAddr, sizeof(output.u64VirAddr));
	priv->planesize[0] = u32Ysize;
	priv->planesize[1] = u32UvHeight * pVframe->u32Stride[1];
	priv->hBlk = blkInput;
	priv->bUsedTDE = gbUsedTDE;
	priv->bUsedVGS = gbUsedVGS;
	priv->bMmap = HI_TRUE;
	
	algonode->priv = priv;
	algonode->free = (void *)vpp_release_frame;
	
out:
	if (gbUsedVGS || gbUsedTDE){
		s32Ret = HI_MPI_VPSS_ReleaseChnFrame(VPSS_GROUP_ID, chn, &stVppFrame);
		if (s32Ret != HI_SUCCESS)
		{
			OSALOG_ERROR("HI_MPI_VPSS_ReleaseChnFrame failed! ret = %d\n", s32Ret);
		}
	}

	if(s32Ret != HI_SUCCESS)
		free(priv);
	
	return s32Ret;
}

int32_t mpp_tde_init()
{
    // 打开 TDE 设备
    int ret = HI_TDE2_Open();
    if (ret < 0)
    {
        OSALOG_ERROR("HI_TDE2_Open failed! Ret=0x%x\n", ret);
        return -1;
    }
    
    OSALOG_INFO("TDE device opened successfully!\n");
    return 0;

}

int32_t mpp_algo_vb_init(int32_t algow, int32_t algoh)
{
    // 创建输出 VB 池 (640x360 NV12)
    VB_POOL_CONFIG_S stVbPoolCfg;
    memset(&stVbPoolCfg, 0, sizeof(VB_POOL_CONFIG_S));
	stVbPoolCfg.u64BlkSize = COMMON_GetPicBufferSize(algow, algoh, PIXEL_FORMAT_YVU_SEMIPLANAR_420,
        DATA_BITWIDTH_8, COMPRESS_MODE_NONE, DEFAULT_ALIGN);
    stVbPoolCfg.u32BlkCnt = 2;
    stVbPoolCfg.enRemapMode = VB_REMAP_MODE_NONE;
    g_AlgoBlkPool = HI_MPI_VB_CreatePool(&stVbPoolCfg);
    if (g_AlgoBlkPool < 0)
    {
        OSALOG_ERROR("Create Output VB Pool failed!\n");
        return -1;
    }

	g_algoWidth = algow;
	g_algoHeight = algoh;
	
	return 0;
}

int32_t mpp_algo_init(int32_t algow, int32_t algoh)
{
	int ret = mpp_algo_vb_init(algow, algoh);

	//mpp_tde_init();

	return ret;
}

int32_t mpp_tde_scale(VIDEO_FRAME_S* input, VIDEO_FRAME_S* output)
{
    TDE2_SURFACE_S stSrc;
    TDE2_SURFACE_S stDst;
    TDE2_OPT_S stOpt;
    TDE2_RECT_S stSrcRect;
    TDE2_RECT_S stDstRect;
    HI_S32 s32Handle;

    // 1. 配置源图像属性 (NV12)
    memset(&stSrc, 0, sizeof(TDE2_SURFACE_S));
    stSrc.enColorFmt = TDE2_COLOR_FMT_MP1_YCbCr420MBP;  // NV12 格式
    stSrc.PhyAddr = input->u64PhyAddr[0];                 // Y 平面物理地址
    stSrc.u32Stride = input->u32Stride[0];               // 行 stride
    stSrc.u32Width = input->u32Width;
    stSrc.u32Height = input->u32Height;
    stSrc.bAlphaMax255 = HI_TRUE;

    // 2. 配置目标图像属性 (NV12)
    memset(&stDst, 0, sizeof(TDE2_SURFACE_S));
    stDst.enColorFmt = TDE2_COLOR_FMT_MP1_YCbCr420MBP;
    stDst.PhyAddr = output->u64PhyAddr[0];
    stDst.u32Stride = output->u32Stride[0];               // 行 stride
    stDst.u32Width = output->u32Width;
    stDst.u32Height = output->u32Height;
    stDst.bAlphaMax255 = HI_TRUE;

    // 3. 配置源图像区域 (整个图像)
    stSrcRect.s32Xpos = 0;
    stSrcRect.s32Ypos = 0;
    stSrcRect.u32Width = stSrc.u32Width;
    stSrcRect.u32Height = stSrc.u32Height;

    // 4. 配置目标图像区域 (整个图像)
    stDstRect.s32Xpos = 0;
    stDstRect.s32Ypos = 0;
    stDstRect.u32Width = stDst.u32Width;
    stDstRect.u32Height = stDst.u32Height;

    // 5. 开始 TDE 任务
    s32Handle = HI_TDE2_BeginJob();
    if (s32Handle < 0)
    {
        OSALOG_ERROR("HI_TDE2_BeginJob failed! Handle=0x%x\n", s32Handle);
        return -1;
    }
	
    // 6. 执行快速缩放
	HI_S32 s32Ret = HI_TDE2_QuickResize(s32Handle, &stSrc, &stSrcRect, &stDst, &stDstRect);
    if (s32Ret < 0)
    {
        OSALOG_ERROR("HI_TDE2_QuickResize failed! Ret=0x%x\n", s32Ret);
        HI_TDE2_CancelJob(s32Handle);
        return -1;
    }	

    // 7. 提交任务
    s32Ret = HI_TDE2_EndJob(s32Handle, HI_FALSE, HI_FALSE, 1000);
    if (s32Ret < 0)
    {
        OSALOG_ERROR("HI_TDE2_EndJob failed! Ret=0x%x\n", s32Ret);
        HI_TDE2_CancelJob(s32Handle);
        return -1;
    }

    return 0;

}

int32_t mpp_vgs_init()
{
    OSALOG_INFO("VGS initialized successfully!\n");
    return 0;
}

/**
 * 执行 VGS 缩放操作
 * @param srcPhysAddr  源图像物理地址
 * @param dstPhysAddr  目标图像物理地址
 */
int mpp_vgs_scale(VIDEO_FRAME_INFO_S* input, VIDEO_FRAME_INFO_S* output)
{
    HI_S32 s32Ret;
    VGS_HANDLE vgsHandle;
    
    // 输出图像属性
    VIDEO_FRAME_INFO_S stDstImage;
    memset(&stDstImage, 0, sizeof(VIDEO_FRAME_INFO_S));
    stDstImage.stVFrame.enPixelFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;  // NV12
    stDstImage.stVFrame.u32Width = output->stVFrame.u32Width;
    stDstImage.stVFrame.u32Height = output->stVFrame.u32Height;
    stDstImage.stVFrame.u32Stride[0] = output->stVFrame.u32Stride[0];
    stDstImage.stVFrame.u32Stride[1] = output->stVFrame.u32Stride[1];
    stDstImage.stVFrame.u64PhyAddr[0] = output->stVFrame.u64PhyAddr[0];
    stDstImage.stVFrame.u64PhyAddr[1] = output->stVFrame.u64PhyAddr[0] + output->stVFrame.u32Stride[0] * output->stVFrame.u32Height;
    stDstImage.stVFrame.u64VirAddr[0] = 0;
    stDstImage.stVFrame.u64VirAddr[1] = 0;
    stDstImage.u32PoolId =  output->u32PoolId;

    // 缩放任务参数
    VGS_TASK_ATTR_S stTask;
    memset(&stTask, 0, sizeof(VGS_TASK_ATTR_S));
    
    // 设置输入输出图像
    memcpy(&stTask.stImgIn, input, sizeof(VIDEO_FRAME_INFO_S));
    memcpy(&stTask.stImgOut, &stDstImage, sizeof(VIDEO_FRAME_INFO_S));
    
    // 缩放选项
	VGS_SCLCOEF_MODE_E enScaleCoefMode = VGS_SCLCOEF_NORMAL;

    // 开始 VGS 任务
    s32Ret = HI_MPI_VGS_BeginJob(&vgsHandle);
    if (s32Ret < 0)
    {
        OSALOG_ERROR("HI_MPI_VGS_BeginJob failed! Ret=0x%x\n", s32Ret);
        return -1;
    }

    // 添加缩放任务
    s32Ret = HI_MPI_VGS_AddScaleTask(vgsHandle, &stTask, enScaleCoefMode);
    if (s32Ret < 0)
    {
        OSALOG_ERROR("HI_MPI_VGS_AddScaleTask failed! Ret=0x%x\n", s32Ret);
        HI_MPI_VGS_CancelJob(vgsHandle);
        return -1;
    }

    // 提交任务并等待完成
    s32Ret = HI_MPI_VGS_EndJob(vgsHandle);
    if (s32Ret < 0)
    {
        OSALOG_ERROR("HI_MPI_VGS_EndJob failed! Ret=0x%x\n", s32Ret);
        HI_MPI_VGS_CancelJob(vgsHandle);
        return -1;
    }

    return 0;
}


#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif



