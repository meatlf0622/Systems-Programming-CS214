#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include "mcached.h"

#define _POSIX_C_SOURCE 200809L

#define MAX_CONN_QUEUE 128
#define HASH_SIZE 1024
#define BUFFER_SIZE 4096
#define MAX_KEY_LEN 256
#define MAX_VAL_LEN (1024 * 1024) // 1 MB

typedef struct entry {
    char *key;
    uint16_t key_len;
    char *value;
    uint32_t val_len;
    pthread_mutex_t lock;
    struct entry *next;
} entry_t;

entry_t *hash_table[HASH_SIZE];
pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;

unsigned int hash(const char *key, uint16_t keylen) {
    unsigned int h = 5381;
    for (int i = 0; i < keylen; i++) {
        h = ((h << 5) + h) + key[i];
    }
    return h % HASH_SIZE;
}

entry_t *find_entry(const char *key, uint16_t keylen) {
    unsigned int idx = hash(key, keylen);
    entry_t *cur = hash_table[idx];
    while (cur) {
        if (cur->key_len == keylen && memcmp(cur->key, key, keylen) == 0)
            return cur;
        cur = cur->next;
    }
    return NULL;
}

void send_response(int client_fd, uint8_t opcode, uint16_t status, uint16_t keylen, uint32_t vallen, char *key, char *val) {
    memcache_req_header_t hdr = {0};
    hdr.magic = 0x81;
    hdr.opcode = opcode;
    hdr.vbucket_id = htons(status);
    hdr.key_length = htons(keylen);
    hdr.total_body_length = htonl(keylen + vallen);
    write(client_fd, &hdr, sizeof(hdr));
    if (keylen) write(client_fd, key, keylen);
    if (vallen) write(client_fd, val, vallen);
}

void* worker_thread(void *arg) {
    int server_fd = *(int *)arg;
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd < 0) continue;

        memcache_req_header_t hdr;
        ssize_t n = read(client_fd, &hdr, sizeof(hdr));
        if (n != sizeof(hdr) || hdr.magic != 0x80) {
            close(client_fd);
            continue;
        }

        uint16_t keylen = ntohs(hdr.key_length);
        uint32_t bodylen = ntohl(hdr.total_body_length);
        uint32_t vallen = bodylen - keylen;
        char *body = malloc(bodylen);
        read(client_fd, body, bodylen);
        char *key = body;
        char *val = body + keylen;

        unsigned int idx = hash(key, keylen);

        switch (hdr.opcode) {
            case CMD_GET: {
                pthread_mutex_lock(&table_lock);
                entry_t *entry = find_entry(key, keylen);
                pthread_mutex_unlock(&table_lock);
                if (entry) {
                    pthread_mutex_lock(&entry->lock);
                    send_response(client_fd, CMD_GET, RES_OK, 0, entry->val_len, NULL, entry->value);
                    pthread_mutex_unlock(&entry->lock);
                } else {
                    send_response(client_fd, CMD_GET, RES_NOT_FOUND, 0, 0, NULL, NULL);
                }
                break;
            }
            case CMD_SET:
            case CMD_ADD: {
                pthread_mutex_lock(&table_lock);
                entry_t *entry = find_entry(key, keylen);
                if (hdr.opcode == CMD_ADD && entry) {
                    pthread_mutex_unlock(&table_lock);
                    send_response(client_fd, CMD_ADD, RES_EXISTS, 0, 0, NULL, NULL);
                    break;
                }
                if (!entry) {
                    entry = malloc(sizeof(entry_t));
                    entry->key = malloc(keylen);
                    memcpy(entry->key, key, keylen);
                    entry->key_len = keylen;
                    pthread_mutex_init(&entry->lock, NULL);
                    entry->next = hash_table[idx];
                    hash_table[idx] = entry;
                }
                pthread_mutex_lock(&entry->lock);
                pthread_mutex_unlock(&table_lock);

                free(entry->value);
                entry->value = malloc(vallen);
                memcpy(entry->value, val, vallen);
                entry->val_len = vallen;
                pthread_mutex_unlock(&entry->lock);
                send_response(client_fd, hdr.opcode, RES_OK, 0, 0, NULL, NULL);
                break;
            }
            case CMD_DELETE: {
                pthread_mutex_lock(&table_lock);
                entry_t **ptr = &hash_table[idx];
                while (*ptr) {
                    entry_t *cur = *ptr;
                    if (cur->key_len == keylen && memcmp(cur->key, key, keylen) == 0) {
                        pthread_mutex_lock(&cur->lock);
                        *ptr = cur->next;
                        pthread_mutex_unlock(&cur->lock);
                        pthread_mutex_destroy(&cur->lock);
                        free(cur->key);
                        free(cur->value);
                        free(cur);
                        pthread_mutex_unlock(&table_lock);
                        send_response(client_fd, CMD_DELETE, RES_OK, 0, 0, NULL, NULL);
                        goto done;
                    }
                    ptr = &((*ptr)->next);
                }
                pthread_mutex_unlock(&table_lock);
                send_response(client_fd, CMD_DELETE, RES_NOT_FOUND, 0, 0, NULL, NULL);
                break;
            }
            case CMD_VERSION: {
                const char *ver = "C-Memcached 1.0";
                send_response(client_fd, CMD_VERSION, RES_OK, 0, strlen(ver), NULL, (char *)ver);
                break;
            }
            case CMD_OUTPUT: {
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                pthread_mutex_lock(&table_lock);
                for (int i = 0; i < HASH_SIZE; i++) {
                    entry_t *cur = hash_table[i];
                    while (cur) {
                        pthread_mutex_lock(&cur->lock);
                        printf("%08lx:%08lx:", ts.tv_sec, ts.tv_nsec);  // Timestamp
                        for (int j = 0; j < cur->key_len; j++)
                            printf("%02x", (unsigned char)cur->key[j]); // Key
                        printf(":");
                        for (int j = 0; j < cur->val_len; j++)
                            printf("%02x", (unsigned char)cur->value[j]); // Value
                        printf("\n");
                        fflush(stdout);  // <- MAKE SURE THIS IS HERE
                        pthread_mutex_unlock(&cur->lock);
                        cur = cur->next;
                    }
                }
                pthread_mutex_unlock(&table_lock);
                send_response(client_fd, CMD_OUTPUT, RES_OK, 0, 0, NULL, NULL);
                break;
            }
            
            default:
                send_response(client_fd, hdr.opcode, RES_ERROR, 0, 0, NULL, NULL);
        }

done:
        close(client_fd);
        free(body);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <port> <num_threads>\n", argv[0]);
        return -1;
    }

    int port = atoi(argv[1]);
    int num_threads = atoi(argv[2]);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, MAX_CONN_QUEUE);

    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, worker_thread, &server_fd);
    }
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    close(server_fd);
    return 0;
}
