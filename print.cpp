#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "headers.hpp"

class CricketTable {
       private:
        std::string title;
        std::vector<std::string> headers;
        std::vector<std::vector<std::string>> rows;
        std::vector<int> widths;
        int padding = 2;

        std::string draw_divider(const std::string& l, const std::string& m, const std::string& r) const {
                std::string res = l;
                for (size_t i = 0; i < widths.size(); ++i) {
                        res += CricketUtils::repeat("\u2500", widths[i] + 2);
                        if (i < widths.size() - 1)
                                res += m;
                }
                return res + r;
        }

        std::string draw_row_content(const std::vector<std::string>& data, bool use_color, bool is_header,
                                     bool is_wicket) const {
                std::string res = "\u2502";
                for (size_t i = 0; i < widths.size(); ++i) {
                        res += " ";
                        if (use_color) {
                                if (is_header)
                                        res += "\033[1;36m";
                                else if (is_wicket)
                                        res += "\033[1;31m";
                        }
                        res += CricketUtils::center(i < data.size() ? data[i] : "", widths[i]);
                        if (use_color)
                                res += "\033[0m";
                        res += " \u2502";
                }
                return res;
        }

       public:
        CricketTable(std::string t, std::vector<std::string> h) : title(std::move(t)), headers(std::move(h)) {
                for (const auto& col : headers)
                        widths.push_back((int)col.length() + padding);
        }

        void add_row(const std::vector<std::string>& data) {
                rows.push_back(data);
                for (size_t i = 0; i < data.size() && i < widths.size(); ++i) {
                        widths[i] = std::max(widths[i], (int)data[i].length() + padding);
                }
        }

        std::string str(bool use_color) const {
                std::ostringstream oss;
                int total_w = 1;
                for (int w : widths)
                        total_w += w + 3;

                oss << "\n"
                    << (use_color ? "\033[1;36m" : "") << CricketUtils::center(title, total_w)
                    << (use_color ? "\033[0m" : "") << "\n";
                oss << draw_divider("\u250C", "\u252C", "\u2510") << "\n";
                oss << draw_row_content(headers, use_color, true, false) << "\n";
                oss << draw_divider("\u251C", "\u253C", "\u2524") << "\n";

                for (const auto& row : rows) {
                        bool is_wicket = false;
                        for (const auto& cell : row)
                                if (cell.find("WICKET") != std::string::npos ||
                                    cell.find("Run Out") != std::string::npos ||
                                    cell.find("Caught Out") != std::string::npos)
                                        is_wicket = true;
                        oss << draw_row_content(row, use_color, false, is_wicket) << "\n";
                }

                oss << draw_divider("\u2514", "\u2534", "\u2518") << "\n";
                return oss.str();
        }
};

std::string get_ball_log_str(bool use_color) {
        if (innings_log.empty())
                return "";

        std::ostringstream oss;
        int current_over = -1;
        CricketTable* current_tbl = nullptr;

        for (const auto& ball : innings_log) {
                if (ball.over_num != current_over) {
                        if (current_tbl) {
                                oss << current_tbl->str(use_color);
                                delete current_tbl;
                        }
                        current_over = ball.over_num;
                        current_tbl =
                            new CricketTable("Over " + std::to_string(current_over + 1),
                                             {"Ball", "Bowler", "Striker", "Non-striker", "Result", "Score", "Wkts"});
                }
                current_tbl->add_row({std::to_string(ball.over_num) + "." + std::to_string(ball.ball_num),
                                      ball.bowler_name, ball.striker_name, ball.non_striker_name, ball.result_text,
                                      std::to_string(ball.score), std::to_string(ball.wicket)});
        }
        if (current_tbl) {
                oss << current_tbl->str(use_color);
                delete current_tbl;
        }
        return oss.str();
}

void print_and_export_report() {
        std::string filename = "match_report_" + CricketUtils::get_timestamp() + ".txt";

        // THE FIX: Added std::ios::app right here!
        std::ofstream file(filename, std::ios::app);

        CricketTable bat_tbl("BATTING SCORECARD", {"Name", "Runs", "Balls", "S/R"});
        for (int i = 0; i < NUM_PLAYERS; i++) {
                if (bats[i].balls_faced == 0)
                        continue;
                double sr = (bats[i].runs * 100.0) / bats[i].balls_faced;
                bat_tbl.add_row({bats[i].name, std::to_string(bats[i].runs), std::to_string(bats[i].balls_faced),
                                 CricketUtils::to_fixed(sr)});
        }

        CricketTable bowl_tbl("BOWLING SCORECARD", {"Name", "Overs", "Runs", "Wickets", "Economy"});
        for (int i = 0; i < NUM_PLAYERS; i++) {
                if (field[i].balls_bowled == 0)
                        continue;
                double econ = (field[i].runs_given * 6.0) / field[i].balls_bowled;
                bowl_tbl.add_row({field[i].name, CricketUtils::to_overs(field[i].balls_bowled),
                                  std::to_string(field[i].runs_given), std::to_string(field[i].wickets),
                                  CricketUtils::to_fixed(econ)});
        }

        std::cout << bat_tbl.str(true) << bowl_tbl.str(true) << get_ball_log_str(true);

        if (file.is_open()) {
                file << "============================================================\n";
                file << "                  FULL MATCH REPORT\n";
                file << "============================================================\n";
                file << bat_tbl.str(false) << bowl_tbl.str(false) << "\nBALL-BY-BALL LOG:\n" << get_ball_log_str(false);
                file.close();
        }
}

void print_full_match_log() {
        if (innings_log.empty())
                return;
        std::cout << get_ball_log_str(true);
}

void print_match_result(bool india_bats_first, int inn1_score, int inn1_wkts, int inn2_score, int inn2_wkts) {
        std::string t1 = india_bats_first ? "India" : "Australia";
        std::string t2 = india_bats_first ? "Australia" : "India";
        std::string score1 = t1 + " : " + std::to_string(inn1_score) + "/" + std::to_string(inn1_wkts);
        std::string score2 = t2 + " : " + std::to_string(inn2_score) + "/" + std::to_string(inn2_wkts);

        CricketTable res("FINAL MATCH RESULT", {"Match Summary"});
        res.add_row({score1});
        res.add_row({score2});

        std::string winner =
            (inn2_score > inn1_score) ? t2 + " WON!" : (inn1_score > inn2_score ? t1 + " WON!" : "MATCH TIED!");
        res.add_row({"*** " + winner + " ***"});

        std::cout << res.str(true);

        std::ofstream file("match_report_" + CricketUtils::get_timestamp() + ".txt", std::ios::app);
        if (file.is_open()) {
                file << res.str(false);
                file.close();
        }
}

void print_gantt(void) {
        if (gantt_n == 0) {
                std::cout << "\n[No Gantt Entries Logged]\n";
                return;
        }

        // Initialize the table with headers matching your GanttEntry struct
        CricketTable gantt_tbl("GANTT CHART (Pitch Resource Timeline)",
                               {"Tick (µs)", "Thread", "Event", "Score", "Wkts"});

        // Populate the table with global gantt array data
        for (int i = 0; i < gantt_n; i++) {
                gantt_tbl.add_row({std::to_string(gantt[i].tick), gantt[i].thread, gantt[i].event,
                                   std::to_string(gantt[i].score), std::to_string(gantt[i].wickets)});
        }

        // Print to console with color support
        std::cout << gantt_tbl.str(true);

        // Optional: Export to the match report file
        std::string filename = "match_report_" + CricketUtils::get_timestamp() + ".txt";
        std::ofstream file(filename, std::ios::app);
        if (file.is_open()) {
                file << "\nRESOURCE TIMELINE LOG:\n";
                file << gantt_tbl.str(false);  // No ANSI colors for the file
                file.close();
        }
}