#include "risk.h"
#include "time.h"


typedef struct {
    char id;                  // 'A' or 'B'
    int points;
    int gave_up;
} player_t;

typedef struct {
    region_t *regions;
    pthread_mutex_t *region_mutexes;
    int num_regions;
    player_t *player;
} shared_t;

void usage(int argc, char** argv)
{
    fprintf(stderr, "USAGE: %s levelname.risk\n", argv[0]);
    exit(EXIT_FAILURE);
}


void lock_regions(pthread_mutex_t *mutexes, int *ids, int count) {
    for (int i = 0; i < count; i++)
        pthread_mutex_lock(&mutexes[ids[i]]);
}

void unlock_regions(pthread_mutex_t *mutexes, int *ids, int count) {
    for (int i = count - 1; i >= 0; i--)
        pthread_mutex_unlock(&mutexes[ids[i]]);
}

void* player_thread(void* arg)
{
    shared_t *shared = arg;
    player_t *p = shared->player;
    int illegal = 0;

    while (illegal < FRUSTRATION_LIMIT) {

        int r = rand() % shared->num_regions;

        /* Build lock set: r + neighbors */
        int lock_ids[shared->regions[r].num_neighbors + 1];
        int count = 0;

        lock_ids[count++] = r;
        for (int i = 0; i < shared->regions[r].num_neighbors; i++)
            lock_ids[count++] = shared->regions[r].neighbors[i];

        /* Sort to avoid deadlock */
        for (int i = 0; i < count; i++)
            for (int j = i + 1; j < count; j++)
                if (lock_ids[j] < lock_ids[i]) {
                    int tmp = lock_ids[i];
                    lock_ids[i] = lock_ids[j];
                    lock_ids[j] = tmp;
                }

        lock_regions(shared->region_mutexes, lock_ids, count);

        int legal = 0;

        if (shared->regions[r].owner == p->id) {
            illegal++;
        } else {
            for (int i = 0; i < shared->regions[r].num_neighbors; i++) {
                int n = shared->regions[r].neighbors[i];
                if (shared->regions[n].owner == p->id) {
                    legal = 1;
                    break;
                }
            }

            if (legal) {
                shared->regions[r].owner = p->id;
                p->points++;
                illegal = 0;
            } else {
                illegal++;
            }
        }

        unlock_regions(shared->region_mutexes, lock_ids, count);

        if (legal)
            ms_sleep(MOVE_MS);
    }

    p->gave_up = 1;
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
    printf("============================\n\n");
}
int main(int argc, char **argv)
{
    if (argc != 2)
        usage(argc, argv);

    srand(time(NULL));

    int num_regions;
    region_t *regions = load_regions(argv[1], &num_regions);

    /* Per-region mutexes */
    pthread_mutex_t *region_mutexes =
        malloc(sizeof(pthread_mutex_t) * num_regions);

    for (int i = 0; i < num_regions; i++)
        pthread_mutex_init(&region_mutexes[i], NULL);

    /* Players */
    player_t A = {.id='A', .points=0, .gave_up=0};
    player_t B = {.id='B', .points=0, .gave_up=0};

    int a_start = rand() % num_regions;
    int b_start;
    do { b_start = rand() % num_regions; } while (b_start == a_start);

    regions[a_start].owner = 'A';
    regions[b_start].owner = 'B';

    shared_t sharedA = {regions, region_mutexes, num_regions, &A};
    shared_t sharedB = {regions, region_mutexes, num_regions, &B};

    pthread_t ta, tb;
    pthread_create(&ta, NULL, player_thread, &sharedA);
    pthread_create(&tb, NULL, player_thread, &sharedB);

    /* Periodic printing */
    while (!(A.gave_up && B.gave_up)) {
        ms_sleep(SHOW_MS);

        for (int i = 0; i < num_regions; i++)
            pthread_mutex_lock(&region_mutexes[i]);

        print_board(regions, num_regions);

        for (int i = num_regions - 1; i >= 0; i--)
            pthread_mutex_unlock(&region_mutexes[i]);
    }

    pthread_join(ta, NULL);
    pthread_join(tb, NULL);

    printf("Player A points: %d\n", A.points);
    printf("Player B points: %d\n", B.points);

    

    free(region_mutexes);
    free(regions);
    return 0;
}
