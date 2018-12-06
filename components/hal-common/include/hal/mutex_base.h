#pragma once

typedef struct mutex_s mutex_t;

void mutex_open(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);
void mutex_close(mutex_t *mutex);