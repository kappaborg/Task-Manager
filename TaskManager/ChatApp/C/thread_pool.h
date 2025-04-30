#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>
#include <stdlib.h>
#include <stdbool.h>
#include "network_config.h"

// Thread pool settings
#define THREAD_POOL_MIN_SIZE 4
#define THREAD_POOL_MAX_SIZE 32
#define THREAD_POOL_QUEUE_SIZE 1000

// Task structure
typedef struct task {
    void (*function)(void *);
    void *argument;
    struct task *next;
} task_t;

// Thread pool structure
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t notify;
    pthread_t *threads;
    task_t *queue_head;
    task_t *queue_tail;
    int thread_count;
    int queue_size;
    bool shutdown;
} threadpool_t;

// Thread pool functions
threadpool_t *threadpool_create(int min_threads, int max_threads);
int threadpool_add(threadpool_t *pool, void (*function)(void *), void *argument);
int threadpool_destroy(threadpool_t *pool);

// Internal functions
static void *threadpool_worker(void *arg);
static task_t *task_create(void (*function)(void *), void *argument);
static void task_destroy(task_t *task);

// Create a new thread pool
threadpool_t *threadpool_create(int min_threads, int max_threads) {
    threadpool_t *pool;
    
    if (min_threads <= 0 || max_threads <= 0 || min_threads > max_threads) {
        return NULL;
    }
    
    // Allocate pool structure
    if ((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL) {
        return NULL;
    }
    
    // Initialize pool attributes
    pool->thread_count = min_threads;
    pool->queue_size = 0;
    pool->queue_head = NULL;
    pool->queue_tail = NULL;
    pool->shutdown = false;
    
    // Allocate thread array
    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * max_threads);
    if (pool->threads == NULL) {
        free(pool);
        return NULL;
    }
    
    // Initialize mutex and condition variable
    if (pthread_mutex_init(&(pool->lock), NULL) != 0) {
        free(pool->threads);
        free(pool);
        return NULL;
    }
    if (pthread_cond_init(&(pool->notify), NULL) != 0) {
        pthread_mutex_destroy(&(pool->lock));
        free(pool->threads);
        free(pool);
        return NULL;
    }
    
    // Start worker threads
    for (int i = 0; i < min_threads; i++) {
        if (pthread_create(&(pool->threads[i]), NULL, threadpool_worker, (void *)pool) != 0) {
            threadpool_destroy(pool);
            return NULL;
        }
    }
    
    return pool;
}

// Add task to thread pool
int threadpool_add(threadpool_t *pool, void (*function)(void *), void *argument) {
    task_t *task;
    
    if (pool == NULL || function == NULL) {
        return -1;
    }
    
    // Create task
    if ((task = task_create(function, argument)) == NULL) {
        return -1;
    }
    
    // Lock pool
    pthread_mutex_lock(&(pool->lock));
    
    // Check if queue is full
    if (pool->queue_size >= THREAD_POOL_QUEUE_SIZE) {
        pthread_mutex_unlock(&(pool->lock));
        task_destroy(task);
        return -1;
    }
    
    // Add task to queue
    if (pool->queue_size == 0) {
        pool->queue_head = task;
        pool->queue_tail = task;
    } else {
        pool->queue_tail->next = task;
        pool->queue_tail = task;
    }
    pool->queue_size++;
    
    // Signal waiting thread
    pthread_cond_signal(&(pool->notify));
    pthread_mutex_unlock(&(pool->lock));
    
    return 0;
}

// Destroy thread pool
int threadpool_destroy(threadpool_t *pool) {
    if (pool == NULL) {
        return -1;
    }
    
    // Lock pool
    pthread_mutex_lock(&(pool->lock));
    
    // Set shutdown flag
    pool->shutdown = true;
    
    // Wake up all worker threads
    pthread_cond_broadcast(&(pool->notify));
    pthread_mutex_unlock(&(pool->lock));
    
    // Wait for threads to finish
    for (int i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }
    
    // Free resources
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->notify));
    
    // Free task queue
    task_t *task;
    while (pool->queue_head != NULL) {
        task = pool->queue_head;
        pool->queue_head = task->next;
        task_destroy(task);
    }
    
    free(pool->threads);
    free(pool);
    
    return 0;
}

// Worker thread function
static void *threadpool_worker(void *arg) {
    threadpool_t *pool = (threadpool_t *)arg;
    task_t *task;
    
    while (true) {
        // Lock pool
        pthread_mutex_lock(&(pool->lock));
        
        // Wait for task
        while (pool->queue_size == 0 && !pool->shutdown) {
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }
        
        // Check shutdown flag
        if (pool->shutdown) {
            pthread_mutex_unlock(&(pool->lock));
            pthread_exit(NULL);
        }
        
        // Get task from queue
        task = pool->queue_head;
        pool->queue_size--;
        
        if (pool->queue_size == 0) {
            pool->queue_head = NULL;
            pool->queue_tail = NULL;
        } else {
            pool->queue_head = task->next;
        }
        
        // Unlock pool
        pthread_mutex_unlock(&(pool->lock));
        
        // Execute task
        (*(task->function))(task->argument);
        
        // Free task
        task_destroy(task);
    }
    
    return NULL;
}

// Create new task
static task_t *task_create(void (*function)(void *), void *argument) {
    task_t *task;
    
    if ((task = (task_t *)malloc(sizeof(task_t))) == NULL) {
        return NULL;
    }
    
    task->function = function;
    task->argument = argument;
    task->next = NULL;
    
    return task;
}

// Destroy task
static void task_destroy(task_t *task) {
    if (task) {
        free(task);
    }
}

#endif // THREAD_POOL_H 