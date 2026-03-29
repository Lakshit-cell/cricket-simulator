#include "headers.hpp"

unsigned int my_rand_r(unsigned int* seed) {
        *seed = (*seed * 1103515245ll + 12345ll) & 0x7fffffff;
        return *seed;
}
NodeType nodes_list[] = {THREAD_STRIKER, THREAD_NON_STRIKER, RESOURCE_A, RESOURCE_B};
void update_rag_request(NodeType thread, NodeType resource) {
        pthread_mutex_lock(&rag_mutex);
        rag.push_back({thread, resource, true});
        pthread_mutex_unlock(&rag_mutex);
}

// Helper to find an edge — must be called with rag_mutex held
static auto find_edge(NodeType f, NodeType t, bool is_r) {
        for (auto it = rag.begin(); it != rag.end(); ++it) {
                if (it->from == f && it->to == t && it->is_request == is_r)
                        return it;
        }
        return rag.end();
}

void update_rag_reverse(NodeType thread, NodeType resource) {
        pthread_mutex_lock(&rag_mutex);
        auto it = find_edge(thread, resource, true);  // Find Request Thread->Resource
        if (it != rag.end()) {
                rag.erase(it);
                rag.push_back({resource, thread, false});  // Add Assignment Resource->Thread
        }
        pthread_mutex_unlock(&rag_mutex);
}

void update_rag_release(NodeType thread, NodeType resource) {
        pthread_mutex_lock(&rag_mutex);
        auto it = find_edge(resource, thread, false);  // Find Assignment Resource->Thread
        if (it != rag.end()) {
                rag.erase(it);
        } else {
                // Also check request edge in case we are breaking before full acquisition
                auto it_req = find_edge(thread, resource, true);
                if (it_req != rag.end()) {
                        rag.erase(it_req);
                }
        }
        pthread_mutex_unlock(&rag_mutex);
}

/*
 * FIX: detect_cycle now works on a pre-taken snapshot of the RAG so it never
 * calls pthread_mutex_lock(&rag_mutex) recursively (which would deadlock on a
 * non-recursive mutex and cause the OS to kill the process with OOM/SIGKILL).
 */
static bool detect_cycle(NodeType u, std::vector<bool>& visited, std::vector<bool>& recStack,
                         const std::vector<Edge>& snapshot) {
        visited[u] = true;
        recStack[u] = true;

        for (const auto& edge : snapshot) {
                if (edge.from != u)
                        continue;
                NodeType v = edge.to;
                if (!visited[v] && detect_cycle(v, visited, recStack, snapshot))
                        return true;
                if (recStack[v])
                        return true;  // back-edge → cycle
        }

        recStack[u] = false;  // remove from recursion stack on the way out
        return false;
}

bool is_deadlocked() {
        // Fast-path: another thread already confirmed deadlock
        if (runout_flag)
                return true;

        /*
         * FIX: Take a snapshot of the RAG while holding the lock, then release
         * the lock before running DFS.  This ensures detect_cycle never tries to
         * re-acquire rag_mutex, preventing the recursive-lock hang.
         */
        pthread_mutex_lock(&rag_mutex);
        std::vector<Edge> snapshot = rag;  // O(n) copy — RAG has at most 4 edges
        pthread_mutex_unlock(&rag_mutex);

        const int num_nodes = 4;
        std::vector<bool> visited(num_nodes, false);
        std::vector<bool> recStack(num_nodes, false);

        for (int i = 0; i < num_nodes; ++i) {
                NodeType current = nodes_list[i];
                if (!visited[current]) {
                        if (detect_cycle(current, visited, recStack, snapshot)) {
                                printf("\n[DEADLOCK DETECTOR] DFS FOUND CYCLE! Deadlock Triggered!\n");
                                runout_flag = true;
                                return true;
                        }
                }
        }
        return false;
}
void update_balls() {
        legal_ball++;
        field[cur_bowler].balls_bowled++;
        if (legal_ball % 6 == 0) {
                field[cur_bowler].overs_bowled++;
                cur_over++;
                cur_bowler = next_bowler();
                int temp = striker;
                striker = non_striker;
                non_striker = temp;
        }
}

std::string get_outcome_string(Outcome dec) {
        if (dec == WIDE)
                return "Wide";
        if (dec == WICKET)
                return "WICKET!!";
        if (dec == SINGLE)
                return "Single";
        if (dec == DOUBLE)
                return "Double";
        if (dec == TRIPLE)
                return "Triple";
        if (dec == FOUR)
                return "4 Runs!";
        if (dec == SIX)
                return "6 Runs!!!";
        if (dec == CATCHOUT)
                return "Caught Out!";
        if (dec == RUNOUT)
                return "Run Out!";
        return "DOT";
}

int runs_of(Outcome o) {
        switch (o) {
                case SINGLE:
                        return 1;
                case DOUBLE:
                        return 2;
                case TRIPLE:
                        return 3;
                case FOUR:
                        return 4;
                case SIX:
                        return 6;
                case WIDE:
                        return 1;
                default:
                        return 0;
        }
}

/* ================= Helper: Match Pressure ================= */
double compute_pressure() {
        int balls_left = 120 - (cur_over * 6 + legal_ball);
        int runs_needed = chase_target - score;

        if (balls_left <= 0 || runs_needed <= 0)
                return 0.0;

        double rrr = (double)runs_needed / balls_left * 6.0;
        return rrr / 8.0;  // normalized pressure
}

/* ================= Bowler Probability ================= */
Outcome bowlerprob(unsigned int* seed) {
        int r = my_rand_r(seed) % 1000;

        FielderData* bow = &field[cur_bowler];

        int wide_prob = 30;  // base 3%

        // Bowler fatigue
        if (bow->balls_bowled > 18)
                wide_prob += 10;

        // Expensive bowler bowls more wides
        if (bow->balls_bowled > 0) {
                double econ = (double)bow->runs_given / bow->balls_bowled * 6.0;
                if (econ > 9)
                        wide_prob += 10;
        }

        if (r < wide_prob)
                return WIDE;

        return VALID;
}

/* ================= Batting Decision ================= */
Outcome batprob(unsigned int* seed) {
        int r = my_rand_r(seed) % 1000;
        BatsmanData* b = &bats[striker];

        double pressure = compute_pressure();

        // Settle logic: New batsmen are more likely to play dots
        double settle = 0.0;
        if (b->balls_faced < 8)
                settle = -0.15;
        else if (b->balls_faced > 20)
                settle = 0.10;

        double aggression = pressure + settle;

        // Tighter probabilities
        // Real T20s have ~35-40% dot balls
        int dot_prob = 380 - (aggression * 100);
        int wicket_prob = 25 + (aggression * 20);

        if (dot_prob < 150)
                dot_prob = 150;  // Minimum dots even for hitters
        if (wicket_prob < 15)
                wicket_prob = 15;

        if (r < dot_prob)
                return DOT;
        if (r < dot_prob + wicket_prob)
                return WICKET;

        return SHOT;
}

/* ================= Field Outcome ================= */
Outcome fldprob(unsigned int* seed) {
        int r = my_rand_r(seed) % 1000;
        int over = cur_over;

        // Base weights (Total = 1000)
        int catch_p = 30;    // 3% of shots are catches
        int runout_p = 10;   // 1%
        int single_p = 500;  // 50%
        int double_p = 120;  // 12%
        int triple_p = 5;    // 0.5%
        int four_p = 190;    // 19%
        int six_p = 145;     // 14.5% (The remainder)

        // Adjust for Powerplay (Overs 0-5)
        if (over < 6) {
                four_p += 60;
                single_p -= 60;
        }
        // Adjust for Death Overs (Overs 15+)
        if (over >= 15) {
                six_p += 80;
                single_p -= 40;
                four_p -= 20;
                catch_p += 20;  // Riskier shots = more catches
        }

        // Cumulative Check Logic
        int cumulative = 0;
        if (r < (cumulative += catch_p))
                return CATCHOUT;
        if (r < (cumulative += single_p))
                return SINGLE;
        if (r < (cumulative += double_p))
                return DOUBLE;
        if (r < (cumulative += triple_p))
                return TRIPLE;
        if (r < (cumulative += four_p))
                return FOUR;

        return SIX;
}