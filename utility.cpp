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

static auto find_edge(NodeType f, NodeType t, bool is_r) {
        for (auto it = rag.begin(); it != rag.end(); ++it) {
                if (it->from == f && it->to == t && it->is_request == is_r)
                        return it;
        }
        return rag.end();
}

void update_rag_reverse(NodeType thread, NodeType resource) {
        pthread_mutex_lock(&rag_mutex);
        auto it = find_edge(thread, resource, true);  
        if (it != rag.end()) {
                rag.erase(it);
                rag.push_back({resource, thread, false});  
        }
        pthread_mutex_unlock(&rag_mutex);
}

void update_rag_release(NodeType thread, NodeType resource) {
        pthread_mutex_lock(&rag_mutex);
        auto it = find_edge(resource, thread, false);  
        if (it != rag.end()) {
                rag.erase(it);
        } else {
               
                auto it_req = find_edge(thread, resource, true);
                if (it_req != rag.end()) {
                        rag.erase(it_req);
                }
        }
        pthread_mutex_unlock(&rag_mutex);
}


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
                        return true;  
        }

        recStack[u] = false;  
        return false;
}

bool is_deadlocked() {
        
        if (runout_flag)
                return true;

      
        pthread_mutex_lock(&rag_mutex);
        std::vector<Edge> snapshot = rag; 
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

double compute_pressure() {
        int balls_left = 120 - (cur_over * 6 + legal_ball);
        int runs_needed = chase_target - score;

        if (balls_left <= 0 || runs_needed <= 0)
                return 0.0;

        double rrr = (double)runs_needed / balls_left * 6.0;
        return rrr / 8.0;  // normalized pressure
}

Outcome bowlerprob(unsigned int* seed) {
        int r = my_rand_r(seed) % 1000;

        FielderData* bow = &field[cur_bowler];

        int wide_prob = 30;  

        if (bow->balls_bowled > 18)
                wide_prob += 10;

        if (bow->balls_bowled > 0) {
                double econ = (double)bow->runs_given / bow->balls_bowled * 6.0;
                if (econ > 9)
                        wide_prob += 10;
        }

        if (r < wide_prob)
                return WIDE;

        return VALID;
}

Outcome batprob(unsigned int* seed, int bat_index) {
        int r = my_rand_r(seed) % 1000;
        BatsmanData* b = &bats[striker];

        double pressure = compute_pressure();

        double settle = 0.0;
        if (b->balls_faced < 8)
                settle = -0.15;
        else if (b->balls_faced > 20)
                settle = 0.10;

        double aggression = pressure + settle;


        int dot_prob = 380 - (aggression * 100);
        int wicket_prob = 25 + (aggression * 20);
        if (bat_index < 5) {
                wicket_prob -= 10;
        } else {
                wicket_prob += 100;
        }
        if (dot_prob < 150)
                dot_prob = 150;  
        if (wicket_prob < 15)
                wicket_prob = 15;

        if (r < dot_prob)
                return DOT;
        if (r < dot_prob + wicket_prob)
                return WICKET;

        return SHOT;
}

Outcome fldprob(unsigned int* seed, int bat_index) {
        int r = my_rand_r(seed) % 1000;
        int over = cur_over;

        
        int catch_p = 30;    
        int runout_p = 10;  
        int single_p = 500;  
        int double_p = 120;  
        int triple_p = 5;    
        int four_p = 190;    
        int six_p = 145;     

        if (bat_index < 5) {
                four_p += 30;
                six_p += 20;
                catch_p -= 10;
                single_p -= 40;  
        } else {
                four_p -= 40;
                six_p -= 100;
                catch_p += 100;
                single_p += 45;  
        }

        if (over < 6) {
                four_p += 60;
                single_p -= 60;
        }

        if (over >= 15) {
                six_p += 80;
                single_p -= 40;
                four_p -= 20;
                catch_p += 20;  
        }

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