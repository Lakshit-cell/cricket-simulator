#pragma once
#include <dispatch/dispatch.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>
/* ====================== Constants ====================== */
#define NUM_PLAYERS 11
#define NUM_BOWLERS 6
#define MAX_OVERS 20
#define MAX_GANTT 100000
/* ====================== Types ========================== */
typedef enum { DOT, SINGLE, DOUBLE, TRIPLE, FOUR, SIX, WIDE, WICKET, VALID, SHOT, RUNOUT, CATCHOUT, DEFAULT } Outcome;

typedef struct {
        int tick;

        char thread[40];
        char event[24];
        int score;
        int wickets;
} GanttEntry;

typedef struct {
        int id;
        int batting_pos;
        char name[32];
        int runs;
        int balls_faced;
        int wait_time;
        int dismissed;
        int played;
        unsigned int rnseed;
} BatsmanData;

typedef struct {
        int id;
        char name[32];
        int overs_bowled;
        int balls_bowled;
        int runs_given;
        int wickets;
        int priority;
        unsigned int rnseed;
} FielderData;

struct Prob {
        double dot;
        double single;
        double dbl;
        double triple;
        double four;
        double six;
        double wicket;
};

struct BallEvent {
        int over_num;
        int ball_num;
        int score;
        int wicket;
        std::string non_striker_name;
        std::string bowler_name;
        std::string striker_name;
        std::string result_text;
};

enum NodeType { THREAD_STRIKER, THREAD_NON_STRIKER, RESOURCE_A, RESOURCE_B };

struct Edge {
        NodeType from;
        NodeType to;
        bool is_request; // True for Thread -> Resource (Request), False for Resource -> Thread (Assignment)
};

/* ====================== Shared State =================== */
extern BatsmanData bats[NUM_PLAYERS];
extern FielderData field[NUM_PLAYERS];
extern int score;
extern int wickets;
extern int striker;
extern int non_striker;
extern int cur_over;
extern int legal_ball;
extern int cur_bowler;
extern int sim_tick;
extern int chase_target;
extern int local_striker;
extern int local_non_striker;
extern int state;
extern bool innings_done;
extern bool striker_ready;
extern bool non_striker_ready;
extern bool runout_initiated;
extern bool striker_running_done;
extern bool non_striker_running_done;

extern std::vector<Edge> rag;
extern pthread_mutex_t rag_mutex;
extern bool runout_flag;

extern dispatch_semaphore_t sem_A;
extern dispatch_semaphore_t sem_B;

extern std::vector<std::vector<std::string>> data;
extern std::vector<BallEvent> innings_log;
extern Outcome umpire_decision;
extern GanttEntry gantt[MAX_GANTT];
extern int m;
extern int next_batting;
extern int gantt_n;
extern std::chrono::high_resolution_clock::time_point start;
extern std::chrono::high_resolution_clock::time_point end;
extern std::chrono::milliseconds duration;

/* ====================== Synchronisation ================ */
extern pthread_mutex_t state_mx;
extern pthread_mutex_t gantt_mx;
extern pthread_cond_t fielder_cv;
extern pthread_cond_t batsman_cv;
extern pthread_cond_t bowler_cv;
extern pthread_cond_t umpire_cv;
extern pthread_cond_t crease_cv;
extern pthread_mutex_t sync_mx;

extern dispatch_semaphore_t crease_sm;
extern dispatch_semaphore_t crease0_sm;
extern dispatch_semaphore_t crease1_sm;
/* ====================== Function Prototypes ============ */
unsigned int my_rand_r(unsigned int* seed);
int next_bowler(void);
int runs_of(Outcome out);
int run_innings(const char* bat_names[], const char* bowl_names[], const char* bat_team, const char* bowl_team,
                int target);
void* bowler_thread(void* arg);
void* fielder_thread(void* arg);
void* batsman_thread(void* arg);
void* umpire_thread(void* arg);
Outcome bowlerprob(unsigned int* seed);
Outcome batprob(unsigned int* seed, int bat_index);
Outcome fldprob(unsigned int* seed, int bat_index);
void init_batting_team(const char* names[]);
void init_bowling_team(const char* names[]);
void update_balls();
void print_gantt();
void gantt_log(const char* thread, const char* event);
std::vector<int> calculate_column_widths(const std::vector<std::vector<std::string>>& table);
void print_bowler_data();
void print_batsman_data();
std::string get_outcome_string(Outcome dec);
void print_full_match_log();
void print_match_result(bool india_bats_first, int inn1_score, int inn1_wkts, int inn2_score, int inn2_wkts);
void print_and_export_report();

void update_rag_request(NodeType thread, NodeType resource);
void update_rag_reverse(NodeType thread, NodeType resource);
void update_rag_release(NodeType thread, NodeType resource);
bool is_deadlocked();
void* batsman_running_behavior(void* arg);
class CricketUtils {
       public:
        static std::string repeat(const std::string& s, int n) {
                std::string res;
                for (int i = 0; i < n; ++i)
                        res += s;
                return res;
        }

        static std::string center(const std::string& text, int width) {
                int pad = width - (int)text.length();
                if (pad <= 0)
                        return text;
                int left = pad / 2;
                return std::string(left, ' ') + text + std::string(pad - left, ' ');
        }

        static std::string to_overs(int balls) { return std::to_string(balls / 6) + "." + std::to_string(balls % 6); }

        static std::string to_fixed(double val, int precision = 2) {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(precision) << val;
                return ss.str();
        }

        static std::string get_timestamp() {
                std::time_t now = std::time(nullptr);
                char buf[20];
                std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M", std::localtime(&now));
                return std::string(buf);
        }
};