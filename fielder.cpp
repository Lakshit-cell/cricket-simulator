#include "headers.hpp"

void* fielder_thread(void* arg) {
        int idx = *(int*)arg;
        FielderData* me = &field[idx];

        while (true) {
                pthread_mutex_lock(&state_mx);
                while ((state != 3) && !innings_done) {
                        pthread_cond_wait(&fielder_cv, &state_mx);
                }

                if (innings_done) {
                        pthread_mutex_unlock(&state_mx);
                        break;
                }

                umpire_decision = fldprob(&me->rnseed,local_striker);
                gantt_log(me->name, "FIELDER");

                if (umpire_decision == CATCHOUT) {
                        bats[striker].dismissed = 1;
                        state = 4; 
                        pthread_cond_broadcast(&batsman_cv);
                        pthread_mutex_unlock(&state_mx);
                } else if (umpire_decision == SINGLE || umpire_decision == DOUBLE || umpire_decision == TRIPLE) {
                        bool do_runout = true;

                        if (do_runout) {
                            
                                runout_flag = false;
                                striker_running_done = false;
                                non_striker_running_done = false;
                                runout_initiated = true;
                                state = 5;

                                pthread_cond_broadcast(&batsman_cv);
                                pthread_mutex_unlock(&state_mx);
                            
                                while (!striker_running_done || !non_striker_running_done) {
                                        if (is_deadlocked()) {
                                                bats[((rand()%2)?striker:non_striker)].dismissed = 1;

                                                update_rag_release(THREAD_STRIKER, RESOURCE_A);
                                                update_rag_release(THREAD_NON_STRIKER, RESOURCE_B);
                                                dispatch_semaphore_signal(sem_A);
                                                dispatch_semaphore_signal(sem_B);
                                        }
                                        usleep(10);
                                }
                                pthread_mutex_lock(&state_mx);

                                if (bats[striker].dismissed) {
                                        umpire_decision = RUNOUT;
                                        bats[non_striker].dismissed = 0;
                                } else if (bats[non_striker].dismissed) {
                                        umpire_decision = RUNOUT;
                                        bats[striker].dismissed = 0;
                                }
                                
                                runout_initiated = false;
                                striker_running_done = false;
                                non_striker_running_done = false;
                                runout_flag = false;

                                pthread_cond_broadcast(&batsman_cv);
             
                                state = 0;
                                pthread_cond_signal(&umpire_cv);

                                pthread_mutex_unlock(&state_mx);
                                continue; 

                        } else {
                                state = 4; 
                                pthread_cond_broadcast(&batsman_cv);
                                pthread_mutex_unlock(&state_mx);
                        }
                } else {
                        state = 4;  
                        pthread_cond_broadcast(&batsman_cv);
                        pthread_mutex_unlock(&state_mx);
                }
        }

        return NULL;
}