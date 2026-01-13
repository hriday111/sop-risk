#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "risk.h"
typedef struct {
    region_t *regions;
    int num_regions;
} player_args_t;


typedef struct {
    region_t *regions;
    int num_regions;
    int gave_up;
    pthread_mutex_t mutex;
} shared_t;


void usage(int argc, char** argv)
{
    fprintf(stderr, "USAGE: %s levelname.risk\n", argv[0]);
    exit(EXIT_FAILURE);
}


void *player_A(void* arg)
{
    shared_t *shared = arg;
    int illegal =0;
    while(illegal<FRUSTRATION_LIMIT)
    {
        int r = rand() % shared->num_regions;
        int legal =0;
        pthread_mutex_lock(&shared->mutex);
        if(shared->regions[r].owner=='A')
        {
            printf("Player A: field %d already owned\n", r);
            illegal++;

        }

        else
        {
            for(int i=0; i<shared->regions[r].num_neighbors; i++)
            {
                int neightbor = shared->regions[r].neighbors[i];
                if(shared->regions[neightbor].owner == 'A')
                {
                    legal=1;
                    break;
                }
            }
            if(!legal)
            {
                printf("Player A: no neighboring field owned\n");
                illegal++;
            }
            else
            {
                shared->regions[r].owner= 'A';
                illegal = 0;
            }
        }  
        
        pthread_mutex_unlock(&shared->mutex);
        if(legal)
        {
            ms_sleep(MOVE_MS);
        }

    }
    pthread_mutex_lock(&shared->mutex);
    shared->gave_up = 1;
    pthread_mutex_unlock(&shared->mutex);
    return NULL;

}
void print_board(region_t *regions, int num_regions) {
    for (int i = 0; i < num_regions; i++) {
        printf("%d [%c] : ", i, regions[i].owner);
        for (int j = 0; j < regions[i].num_neighbors; j++) {
            printf("%d", regions[i].neighbors[j]);
            if (j + 1 < regions[i].num_neighbors)
                printf(";");
        }
        printf("\n");
    }
}
int main(int argc, char** argv) {


    if(argc != 2) {
        usage(argc, argv);
    }
    int num_regions = 0;
    region_t *regions = load_regions(argv[1], &num_regions);

    srand(time(NULL));

    int start = rand() % num_regions;
    regions[start].owner = 'A';

    shared_t shared = {
        .regions = regions,
        .num_regions = num_regions,
        .gave_up = 0,
        .mutex = PTHREAD_MUTEX_INITIALIZER
    };


    pthread_t player;
    pthread_mutex_init(&shared.mutex, NULL);
    shared.gave_up = 0;

    pthread_create(&player, NULL, player_A, &shared);

    while(1)
    {
        ms_sleep(SHOW_MS);
        pthread_mutex_lock(&shared.mutex);
        print_board(shared.regions, shared.num_regions);
        if(shared.gave_up)
        {
            pthread_mutex_unlock(&shared.mutex);
            break;

        }
        pthread_mutex_unlock(&shared.mutex);
    }
    pthread_join(player, NULL);
    
    free(regions);
    return 0;

}

