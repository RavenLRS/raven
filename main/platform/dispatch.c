#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "platform/dispatch.h"

typedef struct dispatch_task_data_s
{
    dispatch_fn fn;
    void *data;
    time_millis_t delay;
} dispatch_task_data_t;

static void dispatch_task(void *arg)
{
    dispatch_task_data_t *task_data = arg;
    if (task_data->delay > 0)
    {
        vTaskDelay(task_data->delay / portTICK_PERIOD_MS);
    }
    task_data->fn(task_data->data);
    free(task_data);
    vTaskDelete(NULL);
}

void dispatch(dispatch_fn fn, void *data)
{
    dispatch_after(fn, data, 0);
}

void dispatch_after(dispatch_fn fn, void *data, time_millis_t delay)
{
    dispatch_task_data_t *task_data = malloc(sizeof(*task_data));
    task_data->fn = fn;
    task_data->data = data;
    task_data->delay = delay;
    xTaskCreatePinnedToCore(dispatch_task, "DISPATCH", 4096, task_data, tskIDLE_PRIORITY, NULL, xPortGetCoreID());
}