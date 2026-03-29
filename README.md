# 🏏 T20 Cricket Simulator — OS CSC-204

A multithreaded T20 cricket match simulator built in C++ that demonstrates core Operating Systems concepts including **thread synchronization**, **deadlock detection**, **resource allocation graphs (RAG)**, **semaphores**, **mutexes**, and **scheduling algorithms** — all mapped onto a live cricket simulation.

---

## 📌 Project Overview

This simulator models a full T20 cricket match (India vs Australia) where every player is a **POSIX thread** and every game resource (crease, bat, field) is a **synchronization primitive**. Key OS concepts demonstrated:

| OS Concept | Cricket Mapping |
|---|---|
| Threads | Batsmen, Bowler, Fielders, Umpire |
| Mutex + Condition Variables | Ball-by-ball state machine |
| Semaphores (`dispatch_semaphore_t`) | Crease slots (only 2 batsmen allowed) |
| Resource Allocation Graph | Run-out deadlock detection |
| Deadlock Detection (DFS cycle) | Run-out scenario between two batsmen |
| CPU Scheduling (FCFS / SJF) | Batting order selection |
| Gantt Chart | Thread execution timeline |

---

## 📁 File Structure

```
.
├── main.cpp          # Entry point, innings orchestration, toss logic
├── headers.hpp       # All structs, enums, externs, and prototypes
├── globals.cpp       # Global shared state definitions
├── batsman.cpp       # Batsman thread logic + deadlock sim
├── bowler.cpp        # Bowler thread logic
├── fielder.cpp       # Fielder thread + run-out resolution
├── umpire.cpp        # Umpire thread + over/ball management 
├── utility.cpp       # RAG management, deadlock detection, ball tracking, Probability functions (batting, bowling, fielding)
├── gantt.cpp         # Gantt chart logging
└── print.cpp         # Scorecard, ball-by-ball log, report export
```

---

## 🛠️ Prerequisites

This project uses **Apple's Grand Central Dispatch (GCD)** via `<dispatch/dispatch.h>` and is designed for **macOS**.

| Requirement | Version |
|---|---|
| macOS | 11.0+ (Big Sur or later) |
| Xcode Command Line Tools | Latest |
| Clang (via Xcode) | C++17 support required |
| POSIX threads (`pthreads`) | Included with macOS |

> ⚠️ **Linux users**: GCD (`dispatch/dispatch.h`) is not natively available on Linux. You would need to install [libdispatch](https://github.com/apple/swift-corelibs-libdispatch) and adapt the build command accordingly.

### Install Xcode Command Line Tools (if not already installed)

```bash
xcode-select --install
```

---

## 🚀 How to Run

### 1. Clone the repository

```bash
git clone https://github.com/Lakshit-cell/cricket-simulator/
cd cricket-simulator
```

### 2. Compile

```bash
clang++ -std=c++17 -pthread \
    main.cpp batsman.cpp bowler.cpp fielder.cpp umpire.cpp \
    utility.cpp gantt.cpp print.cpp globals.cpp \
    -framework CoreFoundation \
    -o cricket_sim
```

> The `-framework CoreFoundation` flag is required for `dispatch/dispatch.h` (GCD) on macOS.

### 3. Run the simulation

```bash
./cricket_sim
```

You will see the toss result, live ball-by-ball commentary, batting/bowling scorecards, and a Gantt chart of thread execution — all printed to the terminal.

---

## 📤 Output

### Terminal output includes:
- **Toss result** — randomly decided, determines batting order
- **Ball-by-ball log** — every delivery with bowler, striker, non-striker, result, and running score
- **Batting scorecard** — runs, balls faced, strike rate per batsman
- **Bowling scorecard** — overs, runs given, wickets, economy per bowler
- **Gantt chart** — microsecond-level thread execution timeline
- **Final match result** — winner or tie

### File output:
A match report is automatically saved to:
```
match_report_YYYY-MM-DD_HH-MM.txt
```
This file contains the full scorecards, ball-by-ball log, and Gantt chart in plain text (no ANSI color codes).

---

## ⚙️ Configuration

Key constants are defined in `headers.hpp`:

| Constant | Default | Description |
|---|---|---|
| `NUM_PLAYERS` | `11` | Players per team |
| `NUM_BOWLERS` | `6` | Max bowlers used per innings |
| `MAX_OVERS` | `20` | Overs per innings |
| `MAX_GANTT` | `100000` | Max Gantt log entries |

---

## 🔍 How the Simulation Works

### State Machine
The umpire thread drives a shared `state` variable that all threads listen to via condition variables:

```
state 0 → Umpire's turn (process last ball, set up next)
state 1 → Bowler's turn  (bowl the ball)
state 2 → Striker's turn  (play the shot)
state 3 → Fielder's turn  (field the ball)
state 4 → Shot resolved   (fielder done, batsmen update)
state 5 → Running (non-striker runs during run-out sim)
```

### Deadlock / Run-out Scenario
When a run-out is possible (on 1s, 2s, 3s), the fielder triggers a **deadlock simulation**:
- The striker holds `sem_A` and requests `sem_B`
- The non-striker holds `sem_B` and requests `sem_A`
- A DFS cycle-detection algorithm runs on the Resource Allocation Graph
- If a cycle is found → **deadlock = run-out**, and one batsman is dismissed

### Scheduling
Batting order follows **FCFS** (First Come, First Served) by default — batsmen enter the crease in order 0–10. The `next_batsman()` function in `batsman.cpp` manages the queue.

---

## 🐛 Known Limitations

- Requires macOS due to `dispatch/dispatch.h` (GCD)

---

## 👥 Team

Built as part of **CSC-204 Operating Systems** coursework.

---

## 📄 License

This project is for academic purposes.
