/*
 * Copyright (c) Hisilicon Technologies Co., Ltd. 2019-2020. All rights reserved.
 * Description: calibration for mcf algorithm
 * Author: ISP SW
 * Create: 2019-6-18
 */

#ifndef __HI_MCF_CAL_H__
#define __HI_MCF_CAL_H__

#include "hi_errno.h"
#include "hi_comm_video.h"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* __cplusplus */

#define EN_ERR_FEATURE_OVERFLOW   30
#define EN_ERR_FEATURE_LACK       31
#define EN_ERR_RANSAC_FAIL        32
#define EN_ERR_ILLEGAL_MATRIX     33
#define EN_ERR_ILLEGAL_MOTION     34
#define EN_ERR_FAIL               35

#define HI_ERR_MCF_CALIBRATION_NULL_PTR      HI_DEF_ERR(HI_ID_MCF_CALIBRATION, EN_ERR_LEVEL_ERROR, EN_ERR_NULL_PTR)
#define HI_ERR_MCF_CALIBRATION_NOT_SUPPORT   HI_DEF_ERR(HI_ID_MCF_CALIBRATION, EN_ERR_LEVEL_ERROR, EN_ERR_NOT_SUPPORT)
#define HI_ERR_MCF_CALIBRATION_NOT_PERM      HI_DEF_ERR(HI_ID_MCF_CALIBRATION, EN_ERR_LEVEL_ERROR, EN_ERR_NOT_PERM)
#define HI_ERR_MCF_CALIBRATION_NOMEM         HI_DEF_ERR(HI_ID_MCF_CALIBRATION, EN_ERR_LEVEL_ERROR, EN_ERR_NOMEM)
#define HI_ERR_MCF_CALIBRATION_NOBUF         HI_DEF_ERR(HI_ID_MCF_CALIBRATION, EN_ERR_LEVEL_ERROR, EN_ERR_NOBUF)
#define HI_ERR_MCF_CALIBRATION_ILLEGAL_PARAM HI_DEF_ERR(HI_ID_MCF_CALIBRATION, EN_ERR_LEVEL_ERROR, EN_ERR_ILLEGAL_PARAM)
#define HI_ERR_MCF_CALIBRATION_FEATURE_LACK  HI_DEF_ERR(HI_ID_MCF_CALIBRATION, EN_ERR_LEVEL_ERROR, EN_ERR_FEATURE_LACK)
#define HI_ERR_MCF_CALIBRATION_RANSAC_FAIL   HI_DEF_ERR(HI_ID_MCF_CALIBRATION, EN_ERR_LEVEL_ERROR, EN_ERR_RANSAC_FAIL)
#define HI_ERR_MCF_CALIBRATION_FAIL          HI_DEF_ERR(HI_ID_MCF_CALIBRATION, EN_ERR_LEVEL_ERROR, EN_ERR_FAIL)
#define HI_ERR_MCF_CALIBRATION_ILLEGAL_MATRIX \
    HI_DEF_ERR(HI_ID_MCF_CALIBRATION, EN_ERR_LEVEL_ERROR, EN_ERR_ILLEGAL_MATRIX)
#define HI_ERR_MCF_CALIBRATION_ILLEGAL_MOTION \
    HI_DEF_ERR(HI_ID_MCF_CALIBRATION, EN_ERR_LEVEL_ERROR, EN_ERR_ILLEGAL_MOTION)
#define HI_ERR_MCF_CALIBRATION_FEATURE_OVERFLOW \
    HI_DEF_ERR(HI_ID_MCF_CALIBRATION, EN_ERR_LEVEL_ERROR, EN_ERR_FEATURE_OVERFLOW)

#define MCF_COEF_NUM              9

typedef enum {
    HI_MCF_CALIBRATION_AFFINE = 0, /* affine Transform mode */
    HI_MCF_CALIBRATION_PROJECTIVE, /* projective mode */
    HI_MCF_CALIBRATION_MODE_BUTT
} hi_mcf_calibration_mode;

typedef struct {
    hi_s32 refer_feature_num;    /* SURF feature number of reference image */
    hi_s32 register_feature_num; /* SURF feature number of image to be registered */
    hi_s32 match_feature_num;    /* match feature number between two image after RANSAC process */
} hi_mcf_feature_info;

typedef struct {
    hi_s64 correct_coef[MCF_COEF_NUM]; /* matrix for match the two */
    RECT_S region;                     /* ROI region */
} hi_mcf_cal;

hi_s32 hi_mcf_calibration(VIDEO_FRAME_S *pic_in_match, VIDEO_FRAME_S *pic_in_refer, hi_mcf_calibration_mode mode,
    hi_mcf_feature_info *feature_info, hi_mcf_cal *mcf_cal_info);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* end of __cplusplus */

#endif /* hi_mcf_cal.h */
