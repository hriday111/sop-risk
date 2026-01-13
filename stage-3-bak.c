#include "risk.h"
#include "time.h"


typedef struct{
    int id;
    int points;
    int gave_up;
} player_t;

typedef struct{
    region_t* regions;
    int num_regions;
    pthread_mutex_t *region_mutexes;
    player_t* player;
} shared_t;

void usage(int argc, char** argv)
{
    fprintf(stderr, "USAGE: %s levelname.risk\n", argv[0]);
    exit(EXIT_FAILURE);
}


void lock_all_regions(pthread_mutex_t* mutexes, int* ids, int count)
{
    for(int i=0; i<count; i++)
    {
        pthread_mutex_lock(&mutexes[ids[i]]);
    }
}

void unlock_all_regions(pthread_mutex_t* mutexes, int* ids, int count)
{
    for(int i=0; i<count; i++)
    {
        pthread_mutex_unlock(&mutexes[ids[i]]);
    }
}
void* player_thread(void* args)
{
    shared_t *shared = args;
    player_t *p = shared->player;
    int illegal_moves=0;
    while(illegal_moves<FRUSTRATION_LIMIT)

    {        
        int r = rand()%shared->num_regions;
        

        int lock_ids[shared->regions[r].num_neighbors+1];
        int count = 0;
        lock_ids[count++] =r;
        for(int i=0; i<shared->regions[r].num_neighbors; i++)
        {
            lock_ids[count++] = shared->regions[r].neighbors[i];
        }

        for (int i = 0; i < count; i++)
            for (int j = i + 1; j < count; j++)
                if (lock_ids[j] < lock_ids[i]) {
                    int tmp = lock_ids[i];
                    lock_ids[i] = lock_ids[j];
                    lock_ids[j] = tmp;
                }
        lock_all_regions(shared->region_mutexes, lock_ids, count);

        int owns_neighbor=0;
        if(shared->regions[r].owner==p->id)
        {
            printf("Player A already owns region\n");
            illegal_moves++;
        }
        else
        {
            int n=0;
            for(n=0; n< shared->regions->num_neighbors; n++)
            {
                if(shared->regions[n].owner==p->id)
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
                shared->regions[n].owner=p->id;
                p->points++;
                illegal_moves=0;
            }
        }
        unlock_all_regions(shared->region_mutexes, lock_ids, count);
        if(owns_neighbor)
        {
            ms_sleep(SHOW_MS);
        }
    }
    p->gave_up=1;
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
    pthread_mutex_t *region_mutexes = malloc(sizeof(pthread_mutex_t)* num_regions);
    for(int i=0; i<num_regions; i++)
    {
        pthread_mutex_init(&region_mutexes[i], NULL);
    }


    srand(time(NULL));
    int start_A = rand()%num_regions;
    int start_B;
    do{start_B= rand()%num_regions;} while(start_B==start_A);
    regions[start_A].owner = 'A';
    regions[start_B].owner = 'B';
    player_t player_A = {.id='A', .points=0, .gave_up=0};
    player_t player_B = {.id='B', .points=0, .gave_up=0};
    
    shared_t shared_A = {
        .regions=regions,
        .num_regions=num_regions,
        .region_mutexes=region_mutexes,
        .player = &player_A
    };

    shared_t shared_B = {
        .regions=regions,
        .num_regions=num_regions,
        .region_mutexes=region_mutexes,
        .player = &player_B
    };


    pthread_t  thread_A, thread_B;
    pthread_create(&thread_A, NULL, player_thread, &shared_A);
    pthread_create(&thread_B, NULL, player_thread, &shared_B);
    while(!(player_A.gave_up && player_B.gave_up))
    {
        ms_sleep(SHOW_MS);
        for (int i = 0; i < num_regions; i++)
            pthread_mutex_lock(&region_mutexes[i]);
        print_board(regions, num_regions);
        for (int i = num_regions-1; i >=0; i--)
            pthread_mutex_unlock(&region_mutexes[i]);
    }

    pthread_join(thread_A,NULL);
    pthread_join(thread_B, NULL);
    printf("Player A points: %d\n", player_A.points);
    printf("Player B points: %d\n", player_B.points);
    for (int i = 0; i < num_regions; i++) {
        pthread_mutex_destroy(&region_mutexes[i]);
        free(regions[i].neighbors);
    }
    free(region_mutexes);
    free(regions);
    return 0;
}
