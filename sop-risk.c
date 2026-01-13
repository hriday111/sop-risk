#include "risk.h"
#include "time.h"

typedef struct{
    region_t* regions;
    int num_regions;
    int gave_up;
    pthread_mutex_t mutex;
} shared_t;

void usage(int argc, char** argv)
{
    fprintf(stderr, "USAGE: %s levelname.risk\n", argv[0]);
    exit(EXIT_FAILURE);
}

void* player_A(void* args)
{
    shared_t *shared = args;
    int illegal_moves=0;
    while(illegal_moves<FRUSTRATION_LIMIT)

    {        
        int r = rand()%shared->num_regions;
        int owns_neighbor=0;
        pthread_mutex_lock(&shared->mutex);
        if(shared->regions[r].owner=='A')
        {
            printf("Player A already owns region\n");
            illegal_moves++;
        }
        else
        {
            int n=0;
            for(n=0; n< shared->regions->num_neighbors; n++)
            {
                if(shared->regions[n].owner=='A')
                {
                    owns_neighbor=1;
                    break;
                }
            }
            if(owns_neighbor==0) 
            {
                printf("Player A doesn't own any neighboring region\n");
                illegal_moves++;
            }
            else
            {
                shared->regions[n].owner='A';
                illegal_moves=0;
            }
        }
        pthread_mutex_unlock(&shared->mutex);
        if(owns_neighbor)
        {
            ms_sleep(SHOW_MS);
        }
    }
    pthread_mutex_lock(&shared->mutex);
    shared->gave_up=1;
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

    if(argc!=2){usage(argc, argv);}
    int num_regions=0;
    region_t* regions = load_regions(argv[1], &num_regions);
    srand(time(NULL));
    int start_field = rand()%num_regions;

    printf("%d picked %d\n", num_regions, start_field);
    regions[start_field].owner='A';
    pthread_t player;
    shared_t shared = {
        .regions=regions,
        .num_regions=num_regions,
        .gave_up=0,
        .mutex = PTHREAD_MUTEX_INITIALIZER
    };
    pthread_mutex_init(&shared.mutex,NULL);
    pthread_create(&player, NULL, player_A, &shared);
    while(1)
    {
        ms_sleep(SHOW_MS);
        pthread_mutex_lock(&shared.mutex);
        print_board(regions, num_regions);
        if(shared.gave_up)
        {
            pthread_mutex_unlock(&shared.mutex);
            break;
        }
        pthread_mutex_lock(&shared.mutex);
    }

    pthread_join(player,NULL);

    free(regions);
    return 0;
}
