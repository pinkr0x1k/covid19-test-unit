#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX_PAITENT 33     // MAX_PAITENT count at hospital, paitent mean is patient
#define C_UNIT 8           // Covid-19 Test Unit count at hospital
#define U_CAP 3            // Each unit has 3 person capatiy 


void *op_unit(void *pid);

void *op_paitent(void *hsid);

void  monitorize_current_event();

/* Limits the 33 of paitents allowed to enter the waiting room at one time.*/
sem_t paitent_semaphore;
/* Used to allow to sleep until a paitent arrives.*/
sem_t healthcare_staff_semaphore[C_UNIT];
/* Capacity of each unit*/
sem_t waiting_room_semaphore[C_UNIT];
/* It holds, is unit cleaning or full*/
sem_t stat_waiting_room_semaphore[C_UNIT];
/* For monitorize correctly*/
pthread_mutex_t debugging_lock = PTHREAD_MUTEX_INITIALIZER;


int main() {
    pthread_t waiting_room_id[C_UNIT];
    pthread_t paitent_id[MAX_PAITENT];
    int cap_hospital[MAX_PAITENT];

    int i;
    for (i = 0; i < MAX_PAITENT; i++)
        cap_hospital[i] = i;

    sem_init(&paitent_semaphore, 0, MAX_PAITENT);

    // Initialize 'healthcare_staff_semaphore'
    for (i = 0; i < C_UNIT; i++)
        sem_init(healthcare_staff_semaphore + i, 0, 0);
    // Initialize 'waiting_room_semaphore'
    for (i = 0; i < C_UNIT; i++)
        sem_init(waiting_room_semaphore + i, 0, 0);
    // Initialize 'stat_waiting_room_semaphore'
    for (i = 0; i < C_UNIT; i++)
        sem_init(stat_waiting_room_semaphore + i, 0, 1);
    // Create units and paitents
    for (i = 0; i < C_UNIT; i++)
        pthread_create(waiting_room_id + i, NULL, &op_paitent, (void *) &cap_hospital[i]);

    for (i = 0; i < MAX_PAITENT; i++)
        pthread_create(paitent_id + i, NULL, &op_unit, (void *) &cap_hospital[i]);

    for (i = 0; i < MAX_PAITENT; i++)
        pthread_join(paitent_id[i], NULL);

    for (i = 0; i < C_UNIT; i++)
        pthread_join(waiting_room_id[i], NULL);

    return 0;
}



/** @brief _op_unit oparates all unit states and print them
 *
 *	States;
 *		1. Entry free state
 *		2. Idle (Empty) state
 *		3. Full and busy state
 *
 *  @param pid
 *  @return Void.
 */
void *op_unit(void *pid) 
{
    int max = 0;
    int current = 0;
    int healthcare_staff_sleep = 0;
    int index = 0;
    int waiting_room_id = 0;

    int waiting_room[C_UNIT], idle_waiting_rooms[C_UNIT];
    sleep(rand() % 10); //a random time has been created for the paitent to arrive

    int p_id = *(int *) pid;
    pthread_mutex_lock(&debugging_lock);
    printf("Paitent %d came to the hospital.\n", p_id);
    pthread_mutex_unlock(&debugging_lock);
    
    int i = 0;
    for (i = 0; i < C_UNIT; i++) {
        sem_getvalue(stat_waiting_room_semaphore + i, &healthcare_staff_sleep);
        sem_getvalue(waiting_room_semaphore + i, &current);
        waiting_room[i] = current;
        if (current >= max && current < 3 && !healthcare_staff_sleep == 0) {
            max = current;
        }
    }

    for (i = 0; i < C_UNIT; i++) {
        if (max == waiting_room[i]) {
            idle_waiting_rooms[index++] = i;
        }
    }
    
    int hcsleep = 0;
    waiting_room_id = idle_waiting_rooms[rand() % index];
    
    sem_getvalue(healthcare_staff_semaphore + waiting_room_id, &hcsleep);
    sem_post(healthcare_staff_semaphore + waiting_room_id);
    sem_post(waiting_room_semaphore + waiting_room_id);

    pthread_mutex_lock(&debugging_lock);
    printf("%d th People entered %dth COVID-19 Test Unit \n", p_id, waiting_room_id);
    monitorize_current_event();
    pthread_mutex_unlock(&debugging_lock);
    
}

/** @brief _op_paitent oparates all paitent states and print them
 *
 *	The states for each paitent;
 *		1. Waiting in room
 *		2. Waiting at hospital
 *		3. Full and busy state
 *
 *  @param hsid
 *  @return Void.
 */
void *op_paitent(void *hsid)
{

    int hs_id = *(int *) hsid;

    //The loop is kill when covid 19 unit test is full
    while (1) {
        pthread_mutex_lock(&debugging_lock);
        printf("The %d covid 19 unit test is cleaning\n", hs_id - 1);
        pthread_mutex_unlock(&debugging_lock);

        //Room keeper cleaning the covid 19 unit test at the beginning
        sem_wait(healthcare_staff_semaphore + hs_id);

        int paitent_count = 0;
        while (paitent_count < 3) {
            sem_getvalue(waiting_room_semaphore + hs_id, &paitent_count);
            if (paitent_count == 2) {
                // announce covid 19 unit test status
                pthread_mutex_lock(&debugging_lock);    
                printf("The last %d people, let's get up! Please, pay attention to your social distance and hygiene; use a mask\n ", (3 - paitent_count));
                pthread_mutex_unlock(&debugging_lock);
            }
            usleep(rand() % 3000 + 3000);
        }

        // if covid 19 unit test is full
        if (paitent_count >= 3) { 
            sem_wait(stat_waiting_room_semaphore + hs_id);
            usleep(rand() % 3000 + 3000);

            int i;
            for (i = 0; i < 3; i++) { 
                sem_wait(&paitent_semaphore);
            }

            for (i = 0; i < 3; i++) { 
                sem_wait(waiting_room_semaphore + hs_id);
            }

            pthread_mutex_lock(&debugging_lock); 
            printf("Covid-19 Test Unit %d is empty now!!!", hs_id);
            monitorize_current_event();
            pthread_mutex_unlock(&debugging_lock);
           
            // get count remaining people in unit
            int p_total = 0;
            sem_getvalue(&paitent_semaphore, &p_total);
            pthread_mutex_lock(&debugging_lock);
            printf("%d people wait in hospital\n", p_total);
            pthread_mutex_unlock(&debugging_lock);
            sem_post(stat_waiting_room_semaphore + hs_id);

            for (i = 0; i < U_CAP; i++) {
                sem_wait(healthcare_staff_semaphore + hs_id);
            }
        }
    }
} 

/** @brief event_debug : debug and monitorize all events
 *
 *  @param 
 *  @return Void.
 */
void event_debug() {
    int k = 0;
    int vp_count = 0;
    printf("\n+-------------------+\n");
    for (k = 0; k < C_UNIT; ++k) {
        sem_getvalue(waiting_room_semaphore + k, &vp_count);
        printf("COVID-19 Test Unit %d  : %d\n", k, vp_count);
    }
    printf("\n+-------------------+\n");
}