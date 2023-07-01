#ifndef CLIENT_TREE_H
#define CLIENT_TREE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "common_structs.h"
#include "logger.h"

typedef struct tree_node_t
{
    int key;
    char *client_name;
    struct tree_node_t *left;
    struct tree_node_t *right;
} tree_node_t;

tree_node_t *create_node(int key, char *client_name)
{
    logger("DEBUG", "Creating new node for client %s with key %d", client_name, key);
    tree_node_t *new_node = (tree_node_t *)malloc(sizeof(tree_node_t));
    new_node->key = key;
    new_node->client_name = strdup(client_name);
    new_node->left = new_node->right = NULL;
    logger("INFO", "Successfully created new node for client %s with key %d", client_name, key);

    return new_node;
}

typedef struct client_tree_t
{
    tree_node_t *root;
    size_t size;
} client_tree_t;

static client_tree_t *tree;
static pthread_mutex_t tree_mutex = PTHREAD_MUTEX_INITIALIZER;

const size_t get_num_connected_clients()
{
    return tree->size;
}

client_tree_t *init_client_tree()
{
    logger("DEBUG", "Initialising client tree");
    if (tree)
    {
        logger("WARN", "client tree is being reinitialized. Possibly leaking the pointer.");
    }

    tree = (client_tree_t *)malloc(sizeof(client_tree_t));
    tree->root = NULL;
    tree->size = 0;

    logger("INFO", "Tree initialized succesfully");

    return tree;
}

int insert_to_client_tree(int key, char *client_name)
{
    pthread_mutex_lock(&tree_mutex);

    if (tree->root == NULL)
    {
        tree->root = create_node(key, client_name);
        tree->size++;
        pthread_mutex_unlock(&tree_mutex);
        return 0;
    }

    tree_node_t *current = tree->root;
    while (true)
    {
        if (key < current->key)
        {
            if (current->left == NULL)
            {
                current->left = create_node(key, client_name);
                break;
            }
            current = current->left;
        }
        else if (key > current->key)
        {
            if (current->right == NULL)
            {
                current->right = create_node(key, client_name);
                break;
            }
            current = current->right;
        }
        else
        {
            logger("ERROR", "Duplicate key found. Invalid state.");
            pthread_mutex_unlock(&tree_mutex);
            return -1;
        }
    }

    tree->size++;
    pthread_mutex_unlock(&tree_mutex);
    return 0;
}

int validate_key_client(int key, const char *client_name)
{
    pthread_mutex_lock(&tree_mutex);

    tree_node_t *current = tree->root;
    while (current != NULL)
    {
        if (key < current->key)
            current = current->left;
        else if (key > current->key)
            current = current->right;
        else
            break;
    }

    if (current == NULL)
    {
        logger("ERROR", "Key %d not found in the client tree.", key);
        pthread_mutex_unlock(&tree_mutex);
        return -1;
    }

    if (strcmp(client_name, current->client_name))
    {
        logger("ERROR", "key-value mismatch: Client name in tree (%s) did not match passed client name (%s)", current->client_name, client_name);
        return -1;
    }

    pthread_mutex_unlock(&tree_mutex);
    return 0;
}

int remove_from_client_tree(int key)
{
    pthread_mutex_lock(&tree_mutex);

    if (tree->root == NULL)
    {
        logger("ERROR", "Tree is empty. Nothing to delete here.");
        pthread_mutex_unlock(&tree_mutex);
        return -1;
    }

    tree_node_t *parent = NULL;
    tree_node_t *current = tree->root;

    while (current != NULL && current->key != key)
    {
        parent = current;
        if (key < current->key)
            current = current->left;

        else
            current = current->right;
    }

    if (current == NULL)
    {
        logger("ERROR", "Node with key %d not found", key);
        pthread_mutex_unlock(&tree_mutex);
        return -1;
    }

    if (current->left == NULL && current->right == NULL)
    {
        if (parent == NULL)
        {
            free(current->client_name);
            free(current);
            tree->size--;
            pthread_mutex_unlock(&tree_mutex);
            return 0;
        }

        if (current == parent->left)
            parent->left = NULL;
        else
            parent->right = NULL;

        free(current->client_name);
        free(current);
    }

    else if (current->left == NULL)
    {
        if (parent == NULL)
            tree->root = current->right;
        else if (current == parent->left)
            parent->left = current->right;
        else
            parent->right = current->right;
    }

    else if (current->right == NULL)
    {
        if (parent == NULL)
            tree->root = current->left;
        else if (current == parent->left)
            parent->left = current->left;
        else
            parent->right = current->left;

        free(current->client_name);
        free(current);
    }

    else
    {
        tree_node_t *successor_parent = current;
        tree_node_t *successor = current->right;

        while (successor->left != NULL)
        {
            successor_parent = successor;
            successor = successor->left;
        }

        if (successor_parent != current)
        {
            successor_parent->left = successor->right;
            successor->right = current->right;
        }

        if (parent == NULL)
            tree->root = successor;

        else if (current == parent->left)
            parent->left = successor;

        else
            parent->right = successor;

        successor->left = current->left;
        free(current->client_name);
        free(current);
    }

    tree->size--;
    pthread_mutex_unlock(&tree_mutex);
    return 0;
}

// TODO: Cleanup tree function.

#endif