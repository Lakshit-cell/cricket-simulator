#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "headers.hpp"

void gantt_log(const char* tname, const char* evt) {
        pthread_mutex_lock(&gantt_mx);
        if (gantt_n < MAX_GANTT) {
                end = std::chrono::high_resolution_clock::now();
                gantt[gantt_n].tick = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                gantt[gantt_n].score = score;
                gantt[gantt_n].wickets = wickets;
                strncpy(gantt[gantt_n].thread, tname, 39);
                strncpy(gantt[gantt_n].event, evt, 23);
                gantt_n++;
        }
        pthread_mutex_unlock(&gantt_mx);
}
