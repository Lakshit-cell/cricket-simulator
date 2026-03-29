#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

#include "headers.hpp"

void* bowler_thread(void* arg) {
        int idx = *(int*)arg;
        FielderData* me = &field[idx];
        char tname[48];
        snprintf(tname, sizeof(tname), "BWL[%s]", me->name);

        while (true) {
                pthread_mutex_lock(&state_mx);
                while (state != 1 && !innings_done) {
                      
                        pthread_cond_wait(&bowler_cv, &state_mx);
                }

                if (innings_done) {
                        pthread_mutex_unlock(&state_mx);
                        break;
                }
                gantt_log(tname, "BOWLER");
                sim_tick++;

                umpire_decision = bowlerprob(&me->rnseed);
                if (umpire_decision == VALID) {
                        state = 2;
                        pthread_cond_broadcast(&batsman_cv);
                        pthread_mutex_unlock(&state_mx);

                } else {
                        state = 0;
                        pthread_cond_signal(&umpire_cv);
                        pthread_mutex_unlock(&state_mx);
                }
        }
        return NULL;
}