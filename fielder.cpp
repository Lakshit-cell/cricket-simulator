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

                umpire_decision = fldprob(&me->rnseed);
                gantt_log(me->name, "FIELDER");

                if (umpire_decision == CATCHOUT) {
                        bats[striker].dismissed = 1;
                        state = 4;  // Fix: Prevent fielder from spinning continuously
                        pthread_cond_broadcast(&batsman_cv);
                        pthread_mutex_unlock(&state_mx);
                } else if (umpire_decision == SINGLE || umpire_decision == DOUBLE || umpire_decision == TRIPLE) {
                        bool do_runout = true;

                        if (do_runout) {
                                /*
                                 * Reset done-flags and runout_flag BEFORE waking batsmen so
                                 * they cannot race ahead and set the flags before we start
                                 * waiting on them.
                                 */
                                runout_flag = false;
                                striker_running_done = false;
                                non_striker_running_done = false;
                                runout_initiated = true;
                                state = 5;

                                /* Wake both batsmen into do_running_deadlock_sim */
                                pthread_cond_broadcast(&batsman_cv);
                                pthread_mutex_unlock(&state_mx);
                                /*
                                 * Wait (with cond_wait so we release state_mx) until BOTH
                                 * batsmen have finished their deadlock simulation and set their
                                 * respective _done flags.
                                 */
                                while (!striker_running_done || !non_striker_running_done) {
                                        if (is_deadlocked()) {
                                                bats[striker].dismissed = 1;
                                                update_rag_release(THREAD_STRIKER, RESOURCE_A);
                                                update_rag_release(THREAD_NON_STRIKER, RESOURCE_B);
                                                dispatch_semaphore_signal(sem_A);
                                                dispatch_semaphore_signal(sem_B);
                                        }
                                        usleep(10);
                                }
                                pthread_mutex_lock(&state_mx);
                                /* Both sims finished — determine the outcome */
                                if (bats[striker].dismissed) {
                                        umpire_decision = RUNOUT;
                                        bats[non_striker].dismissed = 0;
                                } else if (bats[non_striker].dismissed) {
                                        umpire_decision = RUNOUT;
                                        bats[striker].dismissed = 0;
                                }
                                /* else: no deadlock formed — keep original umpire_decision (runs) */

                                /*
                                 * FIX: Clear runout_initiated and reset flags while STILL
                                 * holding state_mx, then set state=0 and signal the umpire —
                                 * all in one critical section.  Only ONE unlock follows.
                                 * (The original code had two unlocks, causing undefined behaviour.)
                                 */
                                runout_initiated = false;
                                striker_running_done = false;
                                non_striker_running_done = false;
                                runout_flag = false;

                                /* Wake batsman threads that are waiting on runout_initiated */
                                pthread_cond_broadcast(&batsman_cv);
                                //   pthread_mutex_unlock(&state_mx);
                                /* Hand control back to umpire — still holding the lock */
                                state = 0;
                                pthread_cond_signal(&umpire_cv);

                                /* FIX: single unlock — the original code unlocked twice here */
                                pthread_mutex_unlock(&state_mx);
                                continue; /* this fielder thread's job for this ball is done */

                        } else {
                                state = 4;  // Fix: Prevent fielder from spinning continuously
                                pthread_cond_broadcast(&batsman_cv);
                                pthread_mutex_unlock(&state_mx);
                        }
                } else {
                        state = 4;  // Fix: Prevent fielder from spinning continuously
                        pthread_cond_broadcast(&batsman_cv);
                        pthread_mutex_unlock(&state_mx);
                }
        }

        return NULL;
}