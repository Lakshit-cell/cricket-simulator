
#include "headers.hpp"
void do_running_deadlock_sim(int batsman_id, bool is_striker) {
        NodeType current_thread = is_striker ? THREAD_STRIKER : THREAD_NON_STRIKER;
        NodeType res_first = is_striker ? RESOURCE_A : RESOURCE_B;
        NodeType res_second = is_striker ? RESOURCE_B : RESOURCE_A;

        dispatch_semaphore_t sem_first = is_striker ? sem_A : sem_B;
        dispatch_semaphore_t sem_second = is_striker ? sem_B : sem_A;

        update_rag_request(current_thread, res_first);

        dispatch_semaphore_wait(sem_first, DISPATCH_TIME_FOREVER);
        
        update_rag_reverse(current_thread, res_first);

        update_rag_request(current_thread, res_second);

        dispatch_semaphore_wait(sem_second, DISPATCH_TIME_FOREVER);
        if (bats[striker].dismissed == 1) {
                dispatch_semaphore_signal(sem_second);
                return;
        }

        update_rag_reverse(current_thread, res_second);

        update_rag_release(current_thread, res_second);
        dispatch_semaphore_signal(sem_second);
        update_rag_release(current_thread, res_first);
        dispatch_semaphore_signal(sem_first);
}

int next_batsman() {
        if (m == 1) {
                for (int i = NUM_PLAYERS - 1; i >= 0; i--) {
                        if (bats[i].played)
                                continue;
                        bats[i].played = 1;
                        return i;
                }
        }
        if (m == 2) {
                for (int i = 0; i < NUM_PLAYERS; i++) {
                        if (bats[i].played)
                                continue;
                        bats[i].played = 1;
                        return i;
                }
        }
        return 0;
}


void* batsman_thread(void* arg) {
        int idx = *(int*)arg;
        BatsmanData* me = &bats[idx];
        pthread_mutex_lock(&sync_mx);
        while(idx!=next_batting && !innings_done && idx != 0 && idx!= 1){
                pthread_cond_wait(&crease_cv , &sync_mx);
        }
        pthread_mutex_unlock(&sync_mx);
        dispatch_semaphore_wait(crease_sm, DISPATCH_TIME_FOREVER);
        bats[idx].wait_time = legal_ball;

        if (dispatch_semaphore_wait(crease0_sm, DISPATCH_TIME_NOW) == 0) {
                striker = idx;
                while (true) {
                        pthread_mutex_lock(&state_mx);
                        while (state != 2 && !innings_done) {
                                striker_ready = true;
                                pthread_cond_signal(&umpire_cv);
                                pthread_cond_wait(&batsman_cv, &state_mx);
                        }
                        striker_ready = false;

                        if (innings_done) {
                                
                                pthread_mutex_unlock(&state_mx);
                                dispatch_semaphore_signal(crease0_sm);
                                break;
                        }
                        gantt_log(me->name, "STRIKER");
                        local_striker = striker;
                        local_non_striker = non_striker;

                        umpire_decision = batprob(&me->rnseed,local_striker);

                        if (umpire_decision == DOT) {
                                state = 0;
                                pthread_cond_signal(&umpire_cv);
                                pthread_mutex_unlock(&state_mx);
                        } else if (umpire_decision == WICKET) {
                                bats[striker].dismissed = 1;
                                state = 0;
                                pthread_cond_signal(&umpire_cv);
                                pthread_mutex_unlock(&state_mx);
                                dispatch_semaphore_signal(crease0_sm);
                                break;
                        } else {
                                state = 3;
                                pthread_cond_broadcast(&fielder_cv);
                                pthread_cond_wait(&batsman_cv, &state_mx);

                                if (runout_initiated) {

                                        pthread_mutex_unlock(&state_mx);
                                        do_running_deadlock_sim(striker, true);


                                        pthread_mutex_lock(&state_mx);
                                        striker_running_done = true;

                                        while (runout_initiated && !innings_done) {
                                                pthread_cond_wait(&batsman_cv, &state_mx);
                                        }
                                        pthread_mutex_unlock(&state_mx);


                                        if (bats[striker].dismissed) {
                                                dispatch_semaphore_signal(crease0_sm);
                                                break;
                                        }

        
                                        continue; 
                                }


                                if (bats[striker].dismissed || umpire_decision == CATCHOUT) {
                                        state = 0;
                                        pthread_cond_signal(&umpire_cv);
                                        pthread_mutex_unlock(&state_mx);
                                        dispatch_semaphore_signal(crease0_sm);
                                        break;
                                }
                                state = 0;
                                pthread_cond_signal(&umpire_cv);
                                pthread_mutex_unlock(&state_mx);
                        }
                }
        } else {
                dispatch_semaphore_wait(crease1_sm, DISPATCH_TIME_FOREVER);

                non_striker = idx;
                while (true) {
                        pthread_mutex_lock(&state_mx);
                        while (state != 5 && !innings_done) {
                                non_striker_ready = true;
                                pthread_cond_signal(&umpire_cv);
                                pthread_cond_wait(&batsman_cv, &state_mx);
                        }
                        non_striker_ready = false;

                        if (innings_done) {
                                pthread_mutex_unlock(&state_mx);
                                dispatch_semaphore_signal(crease1_sm);
                                break;
                        }
                        gantt_log(me->name, "NON-STRIKER");

                        pthread_mutex_unlock(&state_mx);
                        do_running_deadlock_sim(non_striker, false);
                        pthread_mutex_lock(&state_mx);
                        non_striker_running_done = true;
                        while (runout_initiated && !innings_done) {
                                pthread_cond_wait(&batsman_cv, &state_mx);
                        }
                        pthread_mutex_unlock(&state_mx);

                        if (bats[non_striker].dismissed) {
                                dispatch_semaphore_signal(crease1_sm);
                                break;
                        }
                }
        }
        next_batting = next_batsman();
        pthread_cond_broadcast(&crease_cv);
        dispatch_semaphore_signal(crease_sm);

        return NULL;
}