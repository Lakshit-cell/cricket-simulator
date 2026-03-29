#include "headers.hpp"
void do_running_deadlock_sim(int batsman_id, bool is_striker) {
        NodeType current_thread = is_striker ? THREAD_STRIKER : THREAD_NON_STRIKER;
        NodeType res_first = is_striker ? RESOURCE_A : RESOURCE_B;
        NodeType res_second = is_striker ? RESOURCE_B : RESOURCE_A;

        dispatch_semaphore_t sem_first = is_striker ? sem_A : sem_B;
        dispatch_semaphore_t sem_second = is_striker ? sem_B : sem_A;

        /* --- Step 1: Request first resource (request edge: thread → res_first) --- */
        update_rag_request(current_thread, res_first);

        /* --- Step 2: Spin-try to acquire first semaphore --- */
        dispatch_semaphore_wait(sem_first, DISPATCH_TIME_FOREVER);
        /* sem_first is now held */

        /* --- Step 3: Convert request → assignment edge --- */
        update_rag_reverse(current_thread, res_first);

        /* --- Step 4: Request second resource (request edge: thread → res_second) --- */
        update_rag_request(current_thread, res_second);

        /* --- Step 5: Spin-try to acquire second semaphore --- */
        dispatch_semaphore_wait(sem_second, DISPATCH_TIME_FOREVER);
        /* Both sems now held */
        if (bats[striker].dismissed == 1) {
                dispatch_semaphore_signal(sem_second);
                return;
        }
        /* --- Step 6: Convert second request → assignment --- */
        update_rag_reverse(current_thread, res_second);

        update_rag_release(current_thread, res_second);
        dispatch_semaphore_signal(sem_second);
        update_rag_release(current_thread, res_first);
        dispatch_semaphore_signal(sem_first);
}

void* batsman_thread(void* arg) {
        int idx = *(int*)arg;
        BatsmanData* me = &bats[idx];

        dispatch_semaphore_wait(crease_sm, DISPATCH_TIME_FOREVER);

        // Try striker crease
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

                        umpire_decision = batprob(&me->rnseed);

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
                                        /*
                                         * Fielder set STATE_RUNNING and woke us.
                                         * Run the deadlock sim WITHOUT holding state_mx so the
                                         * non-striker thread (and fielder cond_wait) can proceed.
                                         */
                                        pthread_mutex_unlock(&state_mx);
                                        do_running_deadlock_sim(striker, true);

                                        /*
                                         * Re-acquire the lock, mark done, and broadcast so the
                                         * fielder's cond_wait (waiting for both _done flags) wakes.
                                         */
                                        pthread_mutex_lock(&state_mx);
                                        striker_running_done = true;
                                        //     pthread_mutex_unlock(&state_mx);
                                        /*
                                         * Wait until the fielder has finished resolving the runout
                                         * (it clears runout_initiated after setting state=0 and
                                         * signalling the umpire).
                                         */
                                        while (runout_initiated && !innings_done) {
                                                pthread_cond_wait(&batsman_cv, &state_mx);
                                        }
                                        pthread_mutex_unlock(&state_mx);

                                        /* Check if we were the dismissed batsman */
                                        if (bats[striker].dismissed) {
                                                dispatch_semaphore_signal(crease0_sm);
                                                break;
                                        }

                                        /*
                                         * Survived runout — fielder already set state=0 and woke
                                         * the umpire.  Loop back to wait for state==2 (next ball).
                                         */
                                        continue; /* back to top of while(true) */
                                }

                                // Fix: Check both the dismissed flag AND the umpire decision directly
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
                        /* FIX: lock is released exactly once here — the original code had
                         * a spurious second unlock that caused undefined behaviour. */

                        /* If we were the runout victim, exit */
                        if (bats[non_striker].dismissed) {
                                dispatch_semaphore_signal(crease1_sm);
                                break;
                        }
                }
        }

        dispatch_semaphore_signal(crease_sm);

        return NULL;
}