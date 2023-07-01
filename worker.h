#ifndef WORKER_H
#define WORKER_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "logger.h"
#include "common_structs.h"
#include "client_tree.h"

#define CONNECT_CHANNEL_FNAME "srv_conn_channel"
#define CONNECT_CHANNEL_SIZE (1024)

#define MAX_CLIENT_NAME_LEN (1024)
#define MAX_BUFFER_SIZE (1024)

#define UNREGISTER_STRING "unregister"

#define TEMP_CLIENT_SIZE (1024)

static unsigned long total_serviced_requests = 0;
static pthread_mutex_t tsr_mutex = PTHREAD_MUTEX_INITIALIZER;

int increment_service_requests()
{
    pthread_mutex_lock(&tsr_mutex);
    total_serviced_requests++;
    pthread_mutex_unlock(&tsr_mutex);

    return total_serviced_requests;
}

int get_total_serviced_requests()
{
    return total_serviced_requests;
}

typedef struct WorkerArgs
{
    char client_name[MAX_CLIENT_NAME_LEN];
    int comm_channel_block_id;
} WorkerArgs;

Response handle_arithmetic(Request req)
{
    Response res;
    switch (req.op)
    {
    case '+':
        res.result = req.n1 + req.n2;
        res.response_code = RESPONSE_SUCCESS;
        break;
    case '-':
        res.result = req.n1 - req.n2;
        res.response_code = RESPONSE_SUCCESS;
        break;
    case '*':
        res.result = req.n1 * req.n2;
        res.response_code = RESPONSE_SUCCESS;
        break;
    case '/':
        if (req.n2 == 0)
            res.response_code = RESPONSE_UNSUPPORTED;
        else
        {
            res.result = req.n1 / req.n2;
            res.response_code = RESPONSE_SUCCESS;
        }
        break;

    default:
        res.response_code = RESPONSE_UNSUPPORTED;
        break;
    }
    return res;
}

Response handle_even_or_odd(Request req)
{
    Response res;
    res.result = req.n1 % 2;
    res.response_code = RESPONSE_SUCCESS;
    return res;
}

Response handle_is_prime(Request req)
{
    Response res;
    if (req.n1 < 0)
    {
        res.response_code = RESPONSE_FAILURE;
        return res;
    }

    // 0 and 1 are not prime numbers.
    if (req.n1 == 0 || req.n1 == 1)
    {
        res.result = 0;
        res.response_code = RESPONSE_SUCCESS;
        return res;
    }

    res.result = 1;
    for (int i = 2; i * i <= req.n1; ++i)
    {
        if (req.n1 % i == 0)
        {
            res.result = 0;
            break;
        }
    }

    res.response_code = RESPONSE_SUCCESS;
    return res;
}

Response handle_is_negative(Request _req)
{
    Response res;
    res.response_code = RESPONSE_UNSUPPORTED;
    return res;
}

void *worker_function(void *args)
{
    logger("DEBUG", "Setting up communication channel for client %s",  ((WorkerArgs *)args)->client_name);
    int comm_channel_block_id = ((WorkerArgs *)args)->comm_channel_block_id;

    RequestOrResponse *comm_reqres = get_comm_channel(comm_channel_block_id);
    if (comm_reqres == NULL)
    {
        logger("ERROR", "Invalid comm channel block id provided. Could not get communication channel block.", pthread_self());

        free(args);
        args = NULL;

        pthread_exit(NULL);
    }
    logger("INFO", "Communication channel setup succesful for client %s",  ((WorkerArgs *)args)->client_name);

    unsigned long thread_tsr = 0;
    while (true)
    {
        wait_until_stage(comm_reqres, 1);
        logger("INFO", "Received request of type %d",  comm_reqres->req.request_type);

        // TODO: Error handling and logging
        if (validate_key_client(comm_reqres->req.key, ((WorkerArgs *)args)->client_name) < 0)
        {
            // TODO: Test this somehow?
            logger("INFO", "Authentication failed for client %s",  ((WorkerArgs *)args)->client_name);
            Response res;
            res.response_code = RESPONSE_UNAUTHORIZED;
            comm_reqres->res = res;
        }

        else if (comm_reqres->req.request_type == ARITHMETIC)
        {
            Response res = handle_arithmetic(comm_reqres->req);
            comm_reqres->res = res;
        }
        else if (comm_reqres->req.request_type == EVEN_OR_ODD)
        {
            Response res = handle_even_or_odd(comm_reqres->req);
            comm_reqres->res = res;
        }
        else if (comm_reqres->req.request_type == IS_PRIME)
        {
            Response res = handle_is_prime(comm_reqres->req);
            comm_reqres->res = res;
        }
        else if (comm_reqres->req.request_type == IS_NEGATIVE)
        {
            Response res = handle_is_negative(comm_reqres->req);
            comm_reqres->res = res;
        }
        else if (comm_reqres->req.request_type == UNREGISTER)
        {
            logger("INFO", "Initiating deregister of client %s",  ((WorkerArgs *)args)->client_name);
            printf("Deregistering client %s\n", ((WorkerArgs *)args)->client_name);

            // TODO: Error handling and logging
            remove_from_client_tree(comm_reqres->req.key);

            char *filename = strdup(comm_reqres->filename);

            // ? Do you need to clear mutex?.
            pthread_mutex_destroy(&comm_reqres->lock);

            detach_memory_block(comm_reqres);
            destroy_memory_block(filename);

            logger("DEBUG", "Removing file %s as a part of deregistration",  filename);
            remove_file(filename);

            logger("INFO", "Deregistration of client %s succesful",  ((WorkerArgs *)args)->client_name);
            break;
        }

        logger("INFO", "Thread Total serviced requests: %d",  ++thread_tsr);
        logger("INFO", "Total serviced requests: %d",  increment_service_requests());
        next_stage(comm_reqres);

        logger("INFO", "Response sent to client for request with response code %d",  comm_reqres->res.response_code);
        msleep(600);
    }

    free(args);
    args = NULL;

    return NULL;
}

#endif
