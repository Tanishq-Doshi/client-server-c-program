#ifndef CONN_CHANNEL_H
#define CONN_CHANNEL_H

#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#include "common_structs.h"
#include "shared_memory.h"
#include "logger.h"

#define MAX_QUEUE_LEN 100

typedef struct node_t
{
    int req_or_res_block_id;
} node_t;

typedef struct queue_t
{
    pthread_mutex_t lock;
    int head;
    int tail;
    size_t size;
    node_t nodes[MAX_QUEUE_LEN];
} queue_t;

/** Connection channel queue is established 
 *  and relevant actions are logged.
 */
queue_t *create_queue()
{
    logger("DEBUG", "Initialising connection channel queue");
    create_file_if_does_not_exist(CONNECT_CHANNEL_FNAME);
    queue_t *q = (queue_t *)attach_memory_block(CONNECT_CHANNEL_FNAME, sizeof(queue_t));

    if (q == NULL)
    {
        logger("ERROR", "Could not create shared memory block for queue.");
        return NULL;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&q->lock, &attr);
    pthread_mutexattr_destroy(&attr);
    q->head = 0;
    q->tail = 0;
    q->size = 0;

    logger("INFO", "Connection channel queue creation successful");
    return q;
}

/*!< Client joins in the connection channel to connect to the server. */
queue_t *get_queue()
{
    queue_t *q = (queue_t *)attach_memory_block(CONNECT_CHANNEL_FNAME, sizeof(queue_t));
    if (q == NULL)
    {
        logger("ERROR", "Could not create shared memory block for queue.");
        return NULL;
    }

    return q;
}

RequestOrResponse *post(queue_t *q, const char *client_name)
{
    pthread_mutex_lock(&q->lock);
    if (q->size == MAX_QUEUE_LEN)
    {
        logger("ERROR", "Could not enqueue to connection queue. Connection queue is full.");
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    char shm_reqres_fname[MAX_CLIENT_NAME_LEN];
    sprintf(shm_reqres_fname, "queue_%d", q->tail);
    create_file_if_does_not_exist(shm_reqres_fname);

    int req_or_res_block_id = get_shared_block(shm_reqres_fname, sizeof(RequestOrResponse));
    RequestOrResponse *shm_req_or_res = (RequestOrResponse *)attach_with_shared_block_id(req_or_res_block_id);
    if (shm_req_or_res == NULL)
    {
        logger("ERROR", "Could not create shared memory block %s for the personal connection channel.", shm_reqres_fname);
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shm_req_or_res->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    strncpy(shm_req_or_res->client_name, client_name, MAX_CLIENT_NAME_LEN);
    strncpy(shm_req_or_res->filename, shm_reqres_fname, MAX_CLIENT_NAME_LEN);
    shm_req_or_res->stage = 0;

    q->nodes[q->tail].req_or_res_block_id = req_or_res_block_id;

    q->tail = (q->tail + 1) % MAX_QUEUE_LEN;
    q->size += 1;
    pthread_mutex_unlock(&q->lock);

    return shm_req_or_res;
}

RequestOrResponse *dequeue(queue_t *q)
{
    pthread_mutex_lock(&q->lock);

    if (q->size == 0)
    {
        logger("INFO", "Connection queue empty. Nothing to dequeue.");
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    int req_or_res_block_id = q->nodes[q->head].req_or_res_block_id;

    // Whether we are able to delete the node or not, the connection request no longer remains valid
    // if a shared memory block can not be attached to it. Ideally, this should not be possible.
    // But it seems, that it is infact, very possible.
    q->head = (q->head + 1) % MAX_QUEUE_LEN;
    q->size -= 1;

    RequestOrResponse *req_or_res = attach_with_shared_block_id(req_or_res_block_id);
    if (req_or_res == NULL)
    {
        logger("ERROR", "Could not dequeue and get shared block.");
        return (void *)(-1);
    }

    pthread_mutex_unlock(&q->lock);

    return req_or_res;
}

RequestOrResponse *fetch(queue_t *q)
{
    pthread_mutex_lock(&q->lock);

    if (q->size == 0)
    {
        logger("INFO", "Connection queue empty. Nothing to fetch.");
        pthread_mutex_unlock(&q->lock);

        return NULL;
    }

    int req_or_res_block_id = q->nodes[q->head].req_or_res_block_id;

    RequestOrResponse *req_or_res = attach_with_shared_block_id(req_or_res_block_id);
    pthread_mutex_unlock(&q->lock);

    return req_or_res;
}

int empty(queue_t *q)
{
    pthread_mutex_lock(&q->lock);

    q->size = 0;
    pthread_mutex_unlock(&q->lock);

    return 0;
}

int destroy_node(RequestOrResponse *reqres)
{

    logger("INFO", "Starting cleanup of node");
    if (reqres == NULL)
    {
        logger("ERROR", "Failed to destroy connection memory block. Null pointer was passed");
        return -1;
    }

    char filename[MAX_CLIENT_NAME_LEN];
    strncpy(filename, reqres->filename, MAX_CLIENT_NAME_LEN);

    if (pthread_mutex_destroy(&reqres->lock) != 0)
    {
        if (errno == EINVAL)
            logger("ERROR", "Failed to destroy mutex on connection block %s used by client %s. The value specified by the mutex is invalid", filename, reqres->client_name);

        else if (errno == EBUSY)
            logger("ERROR", "Failed to destroy mutex on connection block %s used by client %s. The mutex is locked", filename, reqres->client_name);

        else
            logger("ERROR", "Failed to destroy mutex on connection block %s used by client %s. Unknown error occured", filename, reqres->client_name);

        return -1;
    }

    if (detach_memory_block(reqres) == IPC_RESULT_ERROR)
    {
        logger("ERROR", "Failed to detach connection memory block %s used by client %s", filename, reqres->client_name);
        return -1;
    }

    if (destroy_memory_block(filename) == IPC_RESULT_ERROR)
    {
        logger("ERROR", "Failed to destroy connection memory block %s used by client %s", filename, reqres->client_name);
        return -1;
    }

    logger("INFO", "Deleting file %s", filename);
    return remove_file(filename);
}

/** This function deletes all the requests on the  
 *  connection channel and further deletes the queue.
 */
int destroy_queue(queue_t *q)
{
    logger("INFO", "Starting queue cleanup");
    while (q->size)
    {
        logger("DEBUG", "Cleaning node at index %d", q->head);

        RequestOrResponse *reqres = dequeue(q); // ! What if dequeue returns NULL or -1. > Null occurs only when queue is empty. Should not occur in this case.
        if (reqres == (void *)(-1))
        {
            break;
        }

        destroy_node(reqres);
    }

    logger("DEBUG", "Detaching memory block for queue.");
    detach_memory_block(q);

    logger("DEBUG", "Destroying file of memory block for queue.");
    destroy_memory_block(CONNECT_CHANNEL_FNAME);

    logger("INFO", "Deleting file %s", CONNECT_CHANNEL_FNAME);
    remove_file(CONNECT_CHANNEL_FNAME);

    logger("INFO", "Completed queue cleanup");

    return 0;
}

#endif