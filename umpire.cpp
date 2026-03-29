#include "headers.hpp"
int next_bowler(void) {
        if (cur_over == MAX_OVERS - 1) {
                int best = -1, bp = -1;

                for (int i = 0; i < NUM_PLAYERS; i++) {
                        if (field[i].overs_bowled < 4 && field[i].priority > bp && cur_bowler != i) {
                                bp = field[i].priority;
                                best = i;
                        }
                }
                printf("  [PRIORITY SCHED] Death over! Assigning specialist: %s\n", field[best].name);
                return best;
        }
        int i = (cur_bowler + 1) % NUM_BOWLERS; 
        while (field[i].overs_bowled >= 4) {
                i = (i + 1) % NUM_BOWLERS;
        }
        return i;
}

void* umpire_thread(void* arg) {
        while (true) {
                pthread_mutex_lock(&state_mx);
                while (state != 0 && !innings_done) {
                        pthread_cond_wait(&umpire_cv, &state_mx);
                }

                usleep(10);
                if (innings_done) {
                        pthread_mutex_unlock(&state_mx);
                        break;
                }
                gantt_log("XYZ", "UMPIRE");
                int local_bowler = cur_bowler;
                BallEvent current_ball;
                current_ball.over_num = cur_over;
                current_ball.ball_num = legal_ball % 6 + 1;
                current_ball.bowler_name = field[local_bowler].name;
                current_ball.striker_name = bats[local_striker].name;
                current_ball.non_striker_name = bats[local_non_striker].name;
                current_ball.result_text = get_outcome_string(umpire_decision);

                if (umpire_decision != WIDE) {
                        update_balls();
                        bats[local_striker].balls_faced++;
                }
                if (umpire_decision == SINGLE || umpire_decision == DOUBLE || umpire_decision == TRIPLE ||
                    umpire_decision == FOUR || umpire_decision == SIX || umpire_decision == WIDE) {
                        int runs = runs_of(umpire_decision);
                        score += runs;
                        bats[local_striker].runs += runs;
                        current_ball.score = score;
                        field[local_bowler].runs_given += runs;

                        if ((runs & 1) == 1 && umpire_decision != WIDE) {
                                int temp = striker;
                                striker = non_striker;
                                non_striker = temp;
                        }
                } else if (umpire_decision == WICKET || umpire_decision == RUNOUT || umpire_decision == CATCHOUT) {
                        wickets++;
                        field[local_bowler].wickets++;
                        current_ball.wicket = wickets;
                }
                innings_log.push_back(current_ball);
                if (wickets >= 10 || (score > chase_target && chase_target > 0) || (cur_over == MAX_OVERS)) {
                        innings_done = true;
                        pthread_cond_broadcast(&bowler_cv);
                        pthread_cond_broadcast(&batsman_cv);
                        pthread_cond_broadcast(&fielder_cv);
                        pthread_mutex_unlock(&state_mx);
                        break;
                }
                while (!striker_ready || !non_striker_ready) {
                        pthread_cond_wait(&umpire_cv, &state_mx);
                }
                local_non_striker = non_striker;
                local_striker = striker;
                state = 1;
                pthread_cond_broadcast(&bowler_cv);
                pthread_mutex_unlock(&state_mx);
        }
        return NULL;
}