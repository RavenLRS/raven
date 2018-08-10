#include <hal/mutex.h>

void mutex_open(mutex_t *mutex)
{
    mutex->sema = xSemaphoreCreateBinary();
    assert(mutex->sema);
    // Semaphores are created in a taken state, we must unlock them
    mutex_unlock(mutex);
}

void mutex_lock(mutex_t *mutex)
{
    xSemaphoreTakeFromISR(mutex->sema, NULL);
}

void mutex_unlock(mutex_t *mutex)
{
    xSemaphoreGiveFromISR(mutex->sema, NULL);
}

void mutex_close(mutex_t *mutex)
{
    vSemaphoreDelete(mutex->sema);
    mutex->sema = NULL;
}