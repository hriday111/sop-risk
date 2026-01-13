#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "risk.h"

typedef struct {
    char id;          /* 'A' or 'B' */
    int points;
    int gave_up;
} player_t;

typedef struct {
    region_t *regions;
    pthread_mutex_t *region_mutexes;
    int num_regions;
    player_t *A;
    player_t *B;
    int terminate;
} shared_t;


void usage(char **argv) {
    fprintf(stderr, "USAGE: %s map.risk\n", argv[0]);
    exit(EXIT_FAILURE);
}

void print_board(region_t *regions, int n) {
    for (int i = 0; i < n; i++) {
        printf("%d [%c] : ", i, regions[i].owner);
        for (int j = 0; j < regions[i].num_neighbors; j++) {
            printf("%d", regions[i].neighbors[j]);
            if (j + 1 < regions[i].num_neighbors)
                printf(";");
        }
        printf("\n");
    }
}

/* Lock region set in ascending order (deadlock-free) */
void lock_regions(pthread_mutex_t *m, int *ids, int cnt) {
    for (int i = 0; i < cnt; i++)
        pthread_mutex_lock(&m[ids[i]]);
}

void unlock_regions(pthread_mutex_t *m, int *ids, int cnt) {
    for (int i = cnt - 1; i >= 0; i--)
        pthread_mutex_unlock(&m[ids[i]]);
}

void *player_thread(void *arg) {
    shared_t *shared = arg;
    player_t *me = (pthread_equal(pthread_self(), pthread_self()) ? NULL : NULL);
    /* player pointer is passed via shared struct below */
    me = shared->A == NULL ? shared->B : shared->A;

    int illegal = 0;

    while (illegal < FRUSTRATION_LIMIT) {

        if (shared->terminate)
            pthread_exit(NULL);

        int r = rand() % shared->num_regions;

        /* Build lock set: r + neighbors */
        int count = 1 + shared->regions[r].num_neighbors;
        int ids[count];
        ids[0] = r;
        for (int i = 0; i < shared->regions[r].num_neighbors; i++)
            ids[i + 1] = shared->regions[r].neighbors[i];

        /* Sort ids */
        for (int i = 0; i < count; i++)
            for (int j = i + 1; j < count; j++)
                if (ids[j] < ids[i]) {
                    int tmp = ids[i];
                    ids[i] = ids[j];
                    ids[j] = tmp;
                }

        lock_regions(shared->region_mutexes, ids, count);

        int legal = 0;

        if (shared->regions[r].owner == me->id) {
            illegal++;
        } else {
            for (int i = 0; i < shared->regions[r].num_neighbors; i++) {
                int n = shared->regions[r].neighbors[i];
                if (shared->regions[n].owner == me->id) {
                    legal = 1;
                    break;
                }
            }

            if (legal) {
                shared->regions[r].owner = me->id;
                me->points++;
                illegal = 0;
            } else {
                illegal++;
            }
        }

        unlock_regions(shared->region_mutexes, ids, count);

        if (legal)
            ms_sleep(MOVE_MS);
    }

    me->gave_up = 1;
    return NULL;
}

void *signal_thread(void *arg) {
    shared_t *shared = arg;
    sigset_t set;
    int sig;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);

    while (1) {
        sigwait(&set, &sig);

        if (sig == SIGINT) {
            /* Pick random owned region */
            int owned[shared->num_regions];
            int cnt = 0;

            for (int i = 0; i < shared->num_regions; i++) {
                if (shared->regions[i].owner == 'A' ||
                    shared->regions[i].owner == 'B')
                    owned[cnt++] = i;
            }

            if (cnt == 0)
                continue;

            int r = owned[rand() % cnt];

            pthread_mutex_lock(&shared->region_mutexes[r]);
            char owner = shared->regions[r].owner;
            shared->regions[r].owner = '-';
            pthread_mutex_unlock(&shared->region_mutexes[r]);

            if (owner == 'A')
                shared->A->points--;
            else
                shared->B->points--;

            /* Atomic print */
            for (int i = 0; i < shared->num_regions; i++)
                pthread_mutex_lock(&shared->region_mutexes[i]);

            print_board(shared->regions, shared->num_regions);

            for (int i = shared->num_regions - 1; i >= 0; i--)
                pthread_mutex_unlock(&shared->region_mutexes[i]);
        }

        else if (sig == SIGTERM) {
            shared->terminate = 1;
            printf("Robin Hood wins\n");
            pthread_exit(NULL);
        }
    }
}

/* ===================== MAIN ===================== */

int main(int argc, char **argv) {
    if (argc != 2)
        usage(argv);

    srand(time(NULL));

    /* Block signals in all threads */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    int num_regions;
    region_t *regions = load_regions(argv[1], &num_regions);

    pthread_mutex_t *region_mutexes =
        malloc(sizeof(pthread_mutex_t) * num_regions);

    for (int i = 0; i < num_regions; i++)
        pthread_mutex_init(&region_mutexes[i], NULL);

    player_t A = {.id = 'A', .points = 0, .gave_up = 0};
    player_t B = {.id = 'B', .points = 0, .gave_up = 0};

    int a_start = rand() % num_regions;
    int b_start;
    do { b_start = rand() % num_regions; } while (b_start == a_start);

    regions[a_start].owner = 'A';
    regions[b_start].owner = 'B';

    shared_t shared = {
        .regions = regions,
        .region_mutexes = region_mutexes,
        .num_regions = num_regions,
        .A = &A,
        .B = &B,
        .terminate = 0
    };

    pthread_t ta, tb, ts;
    pthread_create(&ta, NULL, player_thread, &shared);
    pthread_create(&tb, NULL, player_thread, &shared);
    pthread_create(&ts, NULL, signal_thread, &shared);

    while (!(A.gave_up && B.gave_up) && !shared.terminate) {
        ms_sleep(SHOW_MS);
        for (int i = 0; i < num_regions; i++)
            pthread_mutex_lock(&region_mutexes[i]);

        print_board(regions, num_regions);

        for (int i = num_regions - 1; i >= 0; i--)
            pthread_mutex_unlock(&region_mutexes[i]);
    }

    pthread_join(ta, NULL);
    pthread_join(tb, NULL);
    pthread_join(ts, NULL);

    printf("Player A points: %d\n", A.points);
    printf("Player B points: %d\n", B.points);

    for (int i = 0; i < num_regions; i++) {
        pthread_mutex_destroy(&region_mutexes[i]);
        free(regions[i].neighbors);
    }

    free(region_mutexes);
    free(regions);

    return 0;
}
