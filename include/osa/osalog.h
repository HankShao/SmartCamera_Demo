#ifndef __OSA_LOG_H__
#define __OSA_LOG_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif /* End of #ifdef __cplusplus */

/* printf color */
#define COLOR_NONE "\033[m"
#define RED        "\033[0;32;31m"
#define GREEN      "\033[0;32;32m"
#define BLUE       "\033[0;32;34m"
#define YELLOW     "\033[1;33m"

#define OSALOG_INFO(fmt, args ...)  printf(GREEN "(%s|%d): " fmt COLOR_NONE, __func__, __LINE__, ##args);
#define OSALOG_WARN(fmt, args ...)  printf(YELLOW "(%s|%d): " fmt COLOR_NONE, __func__, __LINE__, ##args);
#define OSALOG_ERROR(fmt, args ...) printf(RED "(%s|%d): " fmt COLOR_NONE, __func__, __LINE__, ##args);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

#endif

