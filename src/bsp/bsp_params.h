#ifndef __BSP_PARAMS_H__
#define __BSP_PARAMS_H__
#include "cmsis_os.h"
typedef struct
{
    osSemaphoreId semId;
    osMutexId print_Mutex;
}task_params_t;


#endif
