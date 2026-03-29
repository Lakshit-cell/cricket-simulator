#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <fstream>

#include "headers.hpp"

void init_batting_team(const char* names[]) {
        struct timespec _ts;
        clock_gettime(CLOCK_MONOTONIC, &_ts);
        for (int i = 0; i < NUM_PLAYERS; i++) {
                bats[i] = (BatsmanData){.id = i,
                                        .batting_pos = i + 1,
                                        .runs = 0,
                                        .balls_faced = 0,
                                        .dismissed = 0,
                                        .played=0,
                                        .rnseed = (unsigned int)(_ts.tv_nsec ^ (unsigned)i * 2654435761u)};
                strncpy(bats[i].name, names[i], 31);
        }
}

void init_bowling_team(const char* names[]) {
        struct timespec _ts;
        clock_gettime(CLOCK_MONOTONIC, &_ts);
        for (int i = 0; i < NUM_PLAYERS; i++) {
                field[i] = (FielderData){.id = i,
                                         .overs_bowled = 0,
                                         .balls_bowled = 0,
                                         .runs_given = 0,
                                         .wickets = 0,
                                         .priority = ((i == 2) ? 10 : ((i == 0) ? 8 : 5)),
                                         .rnseed = (unsigned int)(_ts.tv_nsec ^ (unsigned)i * 2246822519u)};
                strncpy(field[i].name, names[i], 31);
        }
}

/* ====================== Run One Innings ================ */
int run_innings(const char* bat_names[], const char* bowl_names[], const char* bat_team, const char* bowl_team,
                int target) {
        // Log Innings Header to File
        std::ofstream f_header("match_report_" + CricketUtils::get_timestamp() + ".txt", std::ios::app);
        if (f_header.is_open()) {
                f_header << "\n" << CricketUtils::repeat("=", 60) << "\n";
                f_header << "  INNINGS START: " << bat_team << (target > 0 ? " (CHASE)" : " (1st INNINGS)") << "\n";
                f_header << "============================================================\n";
                f_header.close();
        }

        printf("\n========================================================\n");
        printf("  INNINGS: %s batting", bat_team);
        if (target > 0)
                printf("  (Target: %d runs)", target);
        printf("\n");
        printf("========================================================\n\n");
        runout_flag=false;
        rag.clear();
        /* Reset all state */
        score = wickets = cur_over = legal_ball = cur_bowler = sim_tick = 0;
        chase_target = target;
        gantt_n = 0;
        innings_done = false;
        state = 1;
        striker_ready = false;
        non_striker_ready = false;
        innings_log.clear();  // Clear ball history for the new innings

        sem_A = dispatch_semaphore_create(1);
        sem_B = dispatch_semaphore_create(1);

        crease_sm = dispatch_semaphore_create(2);
        crease0_sm = dispatch_semaphore_create(1);
        crease1_sm = dispatch_semaphore_create(1);

        init_batting_team(bat_names);
        init_bowling_team(bowl_names);

        bats[0].played = 1;      
        bats[1].played = 1;
        pthread_t bat_tid[NUM_PLAYERS];
        pthread_t fld_tid[NUM_PLAYERS];
        pthread_t bowl_tid;
        pthread_t umpire_tid;
        int bat_idx[NUM_PLAYERS];
        int fld_idx[NUM_PLAYERS];
        int bowl_idx = 0;
        

        for (int i = 0; i < NUM_PLAYERS; i++) {
                fld_idx[i] = i;
                pthread_create(&fld_tid[i], NULL, fielder_thread, &fld_idx[i]);
        }
        
        for (int i = 0; i < NUM_PLAYERS; i++) {
                bat_idx[i] = i;
                pthread_create(&bat_tid[i], NULL, batsman_thread, &bat_idx[i]);
        }
        pthread_create(&umpire_tid, NULL, umpire_thread, NULL);
        usleep(100);
        pthread_create(&bowl_tid, NULL, bowler_thread, &bowl_idx);

        
        pthread_mutex_lock(&state_mx);

        start = std::chrono::high_resolution_clock::now();
        pthread_cond_broadcast(&bowler_cv);
        pthread_mutex_unlock(&state_mx);

        pthread_join(umpire_tid, NULL);

        int final_score = score;
        int final_wickets = wickets;

        pthread_cond_broadcast(&fielder_cv);
        for (int i = 0; i < NUM_PLAYERS; i++) {
                pthread_join(fld_tid[i], NULL);
        }
        pthread_cond_broadcast(&batsman_cv);
        for (int i = 0; i < NUM_PLAYERS; i++) {
                pthread_join(bat_tid[i], NULL);
        }
        pthread_cond_broadcast(&bowler_cv);
        pthread_join(bowl_tid, NULL);

        dispatch_release(sem_A);
        dispatch_release(sem_B);
        dispatch_release(crease_sm);
        dispatch_release(crease0_sm);
        dispatch_release(crease1_sm);
        print_and_export_report();
        print_gantt();
        int sum = 0;
        for(int i = 3;i<8;++i){
                sum+=(bats[i].wait_time == 0) ? legal_ball : (bats[i].wait_time);
        }
        std::cout<<"Average Wait Time : "<<sum/double(5);

        return final_score;
}

int main(void) {
                struct timespec _seed_ts;
        clock_gettime(CLOCK_REALTIME, &_seed_ts);
        unsigned int main_seed =
            (unsigned int)(_seed_ts.tv_sec ^ _seed_ts.tv_nsec ^ (unsigned long)getpid() * 2654435761u);
        srand(main_seed);
        printf("Enter 1 for SJF and 2 for FCFS\n");
        std::cin>>m;
        crease_sm = dispatch_semaphore_create(2);
        crease0_sm = dispatch_semaphore_create(1);
        crease1_sm = dispatch_semaphore_create(1);

        printf("+------------------------------------------------------+\n");
        printf("|     T20 WC 2026 Cricket Simulator  --  OS CSC-204   |\n");
        printf("+------------------------------------------------------+\n");


        const char* india_bat[NUM_PLAYERS] = {"Rohit Sharma",   "Virat Kohli",     "Suryakumar Yadav", "KL Rahul",
                                              "Hardik Pandya",  "Ravindra Jadeja", "MS Dhoni",         "Axar Patel",
                                              "Jasprit Bumrah", "Mohammed Siraj",  "Arshdeep Singh"};


        const char* aus_bat[NUM_PLAYERS] = {"Travis Head",    "David Warner", "Steven Smith",  "Glenn Maxwell",
                                            "Marcus Stoinis", "Tim David",    "Matthew Wade",  "Pat Cummins",
                                            "Mitchell Starc", "Adam Zampa",   "Josh Hazlewood"};


        const char* india_bowl[NUM_PLAYERS] = {
            "Mohammed Siraj", "Arshdeep Singh",   "Jasprit Bumrah", "Hardik Pandya", "Ravindra Jadeja", "Rohit Sharma",
            "Virat Kohli",    "Suryakumar Yadav", "KL Rahul",       "MS Dhoni(WK)",  "Axar Patel"};
        const char* aus_bowl[NUM_PLAYERS] = {"Pat Cummins",   "Mitchell Starc", "Josh Hazlewood",  "Adam Zampa",
                                             "Glenn Maxwell", "Marcus Stoinis", "Tim David",       "David Warner",
                                             "Travis Head",   "Steven Smith",   "Matthew Wade(WK)"};


        int toss_winner = rand() % 2;
        const char* winner_name = toss_winner ? "Australia" : "India";
        int bat_choice = rand() % 2;
        int india_bats_first = toss_winner ? (bat_choice == 1) : (bat_choice == 0);

        printf("+------------------------------------------------------+\n");
        printf("|          TOSS  --  Decided by Third Umpire           |\n");
        printf("+------------------------------------------------------+\n");
        printf("|  [3rd Umpire] %s won the toss.%-24s|\n", winner_name, "");
        printf("|  >>> %-48s|\n", india_bats_first ? "India will BAT first." : "Australia will BAT first.");
        printf("+------------------------------------------------------+\n\n");

        /* -- INNINGS -- */
        int inn1, inn1_wkts, inn2, inn2_wkts;
        if (india_bats_first) {
                inn1 = run_innings(india_bat, aus_bowl, "India", "Australia", 0);
                inn1_wkts = wickets;
                inn2 = run_innings(aus_bat, india_bowl, "Australia", "India", inn1 + 1);
                inn2_wkts = wickets;
        } else {
                inn1 = run_innings(aus_bat, india_bowl, "Australia", "India", 0);
                inn1_wkts = wickets;
                inn2 = run_innings(india_bat, aus_bowl, "India", "Australia", inn1 + 1);
                inn2_wkts = wickets;
        }

      print_match_result(india_bats_first, inn1, inn1_wkts, inn2, inn2_wkts);

        return 0;
}