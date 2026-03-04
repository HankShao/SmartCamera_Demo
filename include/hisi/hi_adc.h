/*
 * Copyright (c) Hisilicon Technologies Co., Ltd. 2017-2019. All rights reserved.
 * Description: Head of adc
 * Author: Hisilicon multimedia software group
 * Create: 2017-12-16
 */

#ifndef __HI_ADC_H__
#define __HI_ADC_H__

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int lsadc_init(void);
void lsadc_exit(void);

#define LSADC_IOCTL_BASE 'A'

#define LSADC_CLOCK_REG_LENGTH  0x4
#define LSADC_REG_LENGTH        0x100

typedef enum hiIOC_NR_LSADC_E {
    IOC_NR_LSADC_MODEL_SEL = 0,
    IOC_NR_LSADC_CHN_ENABLE,
    IOC_NR_LSADC_CHN_DISABLE,
    IOC_NR_LSADC_START,
    IOC_NR_LSADC_STOP,
    IOC_NR_LSADC_GET_CHNVAL,
    IOC_NR_LSADC_BUTT
} IOC_NR_LSADC_E;

#define LSADC_IOC_MODEL_SEL     _IOWR(LSADC_IOCTL_BASE, IOC_NR_LSADC_MODEL_SEL, int)
#define LSADC_IOC_CHN_ENABLE    _IOW(LSADC_IOCTL_BASE, IOC_NR_LSADC_CHN_ENABLE, int)
#define LSADC_IOC_CHN_DISABLE   _IOW(LSADC_IOCTL_BASE, IOC_NR_LSADC_CHN_DISABLE, int)
#define LSADC_IOC_START         _IO(LSADC_IOCTL_BASE, IOC_NR_LSADC_START)
#define LSADC_IOC_STOP          _IO(LSADC_IOCTL_BASE, IOC_NR_LSADC_STOP)
#define LSADC_IOC_GET_CHNVAL    _IOWR(LSADC_IOCTL_BASE, IOC_NR_LSADC_GET_CHNVAL, int)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __HI_ADC_H__ */
