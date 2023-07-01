#ifndef COMMON_STRUCTS_H
#define COMMON_STRUCTS_H

#include <pthread.h>
#include <semaphore.h>

#include "logger.h"
#include "utils.h"

#define CONNECT_CHANNEL_FNAME "srv_conn_channel"
#define CONNECT_CHANNEL_SIZE (1024)

#define MAX_CLIENT_NAME_LEN (1024)
#define MAX_BUFFER_SIZE (1024)

/*!< Defining the type of requests */
typedef enum RequestType
{
    ARITHMETIC,
    EVEN_OR_ODD,
    IS_PRIME,
    IS_NEGATIVE,
    UNREGISTER
} RequestType;

/*!< Initializing response codes */
typedef enum ResponseCode
{
    RESPONSE_SUCCESS = 200,
    RESPONSE_UNAUTHORIZED = 401,
    RESPONSE_UNSUPPORTED = 422,
    RESPONSE_FAILURE = 500
} ResponseCode;

/*!< Defining the parameter of a request along with Request type. */
typedef struct Request
{
    RequestType request_type;
    int n1, n2;
    char op;
    int key;
} Request;

/*!< Defining the response parameters */
typedef struct Response
{
    ResponseCode response_code;
    int result;
} Response;

typedef struct RequestOrResponse
{
    /* Synchronization structures */
    pthread_mutex_t lock;
    int stage;

    /* Utility variables */
    char client_name[MAX_CLIENT_NAME_LEN];
    char filename[MAX_CLIENT_NAME_LEN];

    /* Request Object */
    Request req;

    /* Response Object */
    Response res;
} RequestOrResponse;

/*!< This function attaches the shared block to the communication channel */
RequestOrResponse *get_comm_channel(int comm_channel_block_id)
{
    RequestOrResponse *comm_channel = (RequestOrResponse *)attach_with_shared_block_id(comm_channel_block_id);
    if (comm_channel == NULL)
    {
        logger("ERROR", "Could not create shared RequestOrResponse object.");
        return NULL;
    }

    return comm_channel;
}

/*!< Creating comm channel unique to every client */
int create_comm_channel(const char *client_name)
{
    create_file_if_does_not_exist(client_name);
    int comm_channel_block_id = get_shared_block(client_name, sizeof(RequestOrResponse));
    if (comm_channel_block_id == IPC_RESULT_ERROR)
    {
        logger("ERROR", "get_shared_block failed. Could not get comm_channel_block_id.");
        return IPC_RESULT_ERROR;
    }

    RequestOrResponse *comm_channel = get_comm_channel(comm_channel_block_id);
    if (comm_channel == NULL)
        return IPC_RESULT_ERROR;

    comm_channel->stage = 0;
    strncpy(comm_channel->filename, client_name, MAX_CLIENT_NAME_LEN);

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&comm_channel->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    return comm_channel_block_id;
}

/** This function puts the process in a wait state 
 *  until the desired stage is reached.
 */
void wait_until_stage(RequestOrResponse *req_or_res, int stage)
{
    bool reached_stage = false;
    if(stage == -1){
        logger("ERROR", "Server does not exist anymore, client is exiting..");
        exit(EXIT_FAILURE);
    }
    while (!reached_stage)
    {
        pthread_mutex_lock(&req_or_res->lock);
        logger("DEBUG", "On stage: %d waiting for stage: %d",  req_or_res->stage, stage);
        reached_stage = (req_or_res->stage == stage);
        pthread_mutex_unlock(&req_or_res->lock);
        if (!reached_stage)
            msleep(500);
    }
}

void next_stage(RequestOrResponse *req_or_res)
{
    pthread_mutex_lock(&req_or_res->lock);
    req_or_res->stage = (req_or_res->stage + 1) % 3;
    logger("DEBUG", "Set stage to: %d",  req_or_res->stage);
    pthread_mutex_unlock(&req_or_res->lock);
}

/** This function sets the stage that the process 
 *  is in currently.
 */
void set_stage(RequestOrResponse *req_or_res, int stage)
{
    pthread_mutex_lock(&req_or_res->lock);
    req_or_res->stage = stage;
    logger("DEBUG", "Set stage to: %d",  req_or_res->stage);
    pthread_mutex_unlock(&req_or_res->lock);
}

RequestOrResponse *get_req_or_res(const char *client_name)
{
    create_file_if_does_not_exist(client_name);
    RequestOrResponse *req_or_res =
        (RequestOrResponse *)attach_memory_block(client_name, sizeof(req_or_res));

    return req_or_res;
}

void destroy_req_or_res(RequestOrResponse *req_or_res)
{
    pthread_mutex_destroy(&req_or_res->lock);

    detach_memory_block(req_or_res);
    req_or_res = NULL;
    // destroy_memory_block(req_or_res);
}

#endif