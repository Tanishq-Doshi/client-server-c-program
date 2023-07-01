#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "shared_memory.h"
#include "common_structs.h"
#include "utils.h"
#include "conn_chanel.h"
#include "logger.h"

/*!< This function establishes a connection to the server via connection channel. */
int connect_to_server(const char *client_name)
{
    queue_t *conn_q = get_queue(); /*!< Joining the conn channel */
    if (conn_q == NULL)
    {
        logger("ERROR", "Could not get connection queue.");
        return -1;
    }

    logger("INFO", "Sending register request to server with name %s", client_name);
    RequestOrResponse *conn_reqres = post(conn_q, client_name);
    if (conn_reqres == NULL)
    {
        logger("ERROR", "Could not get personal connection channel to connect to server. Registration failed.");
        return -1;
    }

    wait_until_stage(conn_reqres, 1); 

    if (conn_reqres->res.response_code != RESPONSE_SUCCESS)
    {
        logger("ERROR", "Registering to server failed with response code %d", conn_reqres->res.response_code);
        return -1;
    }

    int key = conn_reqres->res.result;
    logger("DEBUG", "Succesfully connected to the server and received key %d", key);

    logger("INFO", "Cleaning up the personal connection channel");
    if (destroy_node(conn_reqres) == -1)
        logger("WARN", "Personal connection channel could not be cleaned up succesfully.");

    return key;
}

/*!< This function establishes the communication channel functionalities and handles request/response. */
int communicate(const char *client_name, int key)
{
    RequestOrResponse *comm_reqres = get_req_or_res(client_name);
    if (comm_reqres == NULL)
    {
        logger("ERROR", "Communication channel couldn't be established with response code %d", comm_reqres->res.response_code);
        return -1;
    }

    // We don't set key here, but while making request,
    // since we can never be sure if the server tampered with the key

    logger("DEBUG", "Obtained communication channel succesfully");

    int n1, n2;
    char op;

    int current_choice = 0;
    int client_requests = 0;
    while (true)
    {
        wait_until_stage(comm_reqres, 0);
#ifndef DEBUGGER
        printf(
            "Options:\nArithmetic Operations: %d\nCheck even or odd: %d\nCheck prime?: %d\nCheck negative: %d\nUnregister: %d\nEnter your choice: ",
            ARITHMETIC, EVEN_OR_ODD, IS_PRIME, IS_NEGATIVE, UNREGISTER);
        scanf("%d", &current_choice);
#else
        current_choice = EVEN_OR_ODD;
#endif

        logger("DEBUG", "Sending request of type %d to server", current_choice);
        if (current_choice == ARITHMETIC)
        {
#ifndef DEBUGGER

            printf("Enter operation with format <num1> <op> <num2>: ");
            scanf("%d %c %d", &n1, &op, &n2);
#else
            n1 = 4, n2 = 5, op = '*';
#endif

            comm_reqres->req.key = key;
            comm_reqres->req.request_type = ARITHMETIC;
            comm_reqres->req.n1 = n1;
            comm_reqres->req.n2 = n2;
            comm_reqres->req.op = op;

            logger("DEBUG", "Sending request of type %d to server with params: n1: %d op: %c n2: %d", current_choice, n1, op, n2);

            next_stage(comm_reqres);
        }

        else if (current_choice == EVEN_OR_ODD)
        {
#ifndef DEBUGGER
            printf("Enter number to check evenness: ");
            scanf("%d", &n1);
#else
            n1 = 43;
#endif

            comm_reqres->req.key = key;
            comm_reqres->req.request_type = EVEN_OR_ODD;
            comm_reqres->req.n1 = n1;

            logger("DEBUG", "Sending request of type %d to server with params: n1: %d", current_choice, n1);

            next_stage(comm_reqres);
        }

        else if (current_choice == IS_PRIME)
        {
#ifndef DEBUGGER

            printf("Enter number to check primality: ");
            scanf("%d", &n1);
#else
            n1 = 43;
#endif
            comm_reqres->req.key = key;
            comm_reqres->req.request_type = IS_PRIME;
            comm_reqres->req.n1 = n1;

            logger("DEBUG", "Sending request of type %d to server with params: n1: %d", current_choice, n1);

            next_stage(comm_reqres);
        }

        else if (current_choice == IS_NEGATIVE)
        {
#ifndef DEBUGGER

            printf("Enter number to check sign: ");
            scanf("%d", &n1);
#else
            n1 = -5;
#endif
            comm_reqres->req.key = key;
            comm_reqres->req.request_type = IS_NEGATIVE;
            comm_reqres->req.n1 = n1;

            logger("DEBUG", "Sending request of type %d to server with params: n1: %d", current_choice, n1);

            next_stage(comm_reqres);
        }

        else if (current_choice == UNREGISTER)
        {
            printf("Unregistering...\n");
            logger("INFO", "Initiating unregister");
            comm_reqres->req.key = key;
            comm_reqres->req.request_type = UNREGISTER;

            logger("DEBUG", "Sending request of type %d to server", current_choice);

            next_stage(comm_reqres);
            break;
        }

        else // DON'T DO ANYTHING AND EXIT
        {
            logger("WARN", "Input was invalid. Exiting without cleaning up or deregistering");
            next_stage(comm_reqres); // ? Fix this
            break;
        }

        wait_until_stage(comm_reqres, 2); // TODO: Use a timed wait here as well.

        logger("INFO", "Received response from server with status code: %d", comm_reqres->res.response_code);
        client_requests = client_requests + 1;
        logger("INFO", "Number of requests served for %s are: %d", client_name, client_requests);

        if (comm_reqres->res.response_code == RESPONSE_SUCCESS)
            printf("Result: %d\n", comm_reqres->res.result);

        else if (comm_reqres->res.response_code == RESPONSE_UNSUPPORTED)
            logger("ERROR", "Unsupported request. Try another.");

        else if (comm_reqres->res.response_code == RESPONSE_FAILURE)
            logger("ERROR", "Unknown failure");

        else
            logger("ERROR", "Unsupported response. Something is wrong with the server.");
        next_stage(comm_reqres);
    }

    return 0;
}

int main(int argc, char **argv)
{
#ifndef DEBUGGER
    if (argc < 2)
    {
        printf("Usage: ./client <unique_name_for_client>\n");
        exit(EXIT_FAILURE);
    }
    char client_name[MAX_CLIENT_NAME_LEN];
    memcpy(client_name, argv[1], MAX_CLIENT_NAME_LEN);
#else
    char client_name[MAX_CLIENT_NAME_LEN];
    sprintf(client_name, "xyz_%lu", (unsigned long)time(NULL));
#endif

    if (init_logger(client_name) == EXIT_FAILURE)
        return EXIT_FAILURE;

    // If the connection file does not exist, then the server is probably not running.
    int connect_channel_exists = does_file_exist(CONNECT_CHANNEL_FNAME);
    if (connect_channel_exists != 1)
    {
        logger("ERROR", "Server likely not running. Please start the server and try again or debug with other messages.");
        return EXIT_FAILURE;
    }

    /*!< Returns unique key after establishing connection with the server. */
    int key = connect_to_server(client_name);
    if (key < 0)
    {
        logger("ERROR", "Could not connect to server. Ending the process.");
        return EXIT_FAILURE;
    }

    communicate(client_name, key); /*!< Communication with the server begins. */

    close_logger(); 

    return 0;
}