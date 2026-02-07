# Prometheus

A bicameral-mind AI agent that plays Minecraft, inspired by Julian Jaynes' theory of consciousness. A C++ backplane (the **Head**) runs fast reflexes and deliberative reasoning, connected via ZeroMQ to a Node.js mineflayer bot (the **Body**) that perceives and acts in the game world.

## Architecture

```
┌─────────────────────── Head (C++) ───────────────────────┐
│                                                          │
│  ┌─────────┐   ┌────────┐   ┌─────────┐                │
│  │ Lizard  │──▶│Arbiter │◀──│  Soul   │                │
│  │(System 1)│   │        │   │(System 2)│                │
│  └────▲────┘   └───┬────┘   └────▲────┘                │
│       │            │              │                      │
│  fast reflexes  dispatch     deliberation                │
│  < 100 ms      winner        2–5 s / query              │
│       │            │              │                      │
│  ┌────┴────┐   ┌───▼────┐   ┌────┴─────┐  ┌──────────┐│
│  │BodyLink │   │BodyLink│   │llama-srv │  │ Teacher  ││
│  │SUB :5555│   │PUSH    │   │(Qwen 2.5)│  │(Gemini)  ││
│  │         │   │  :5556 │   └──────────┘  └──────────┘││
│  └────▲────┘   └───┬────┘                              │
│       │            │       ┌──────────┐  ┌───────────┐ │
│       │            │       │  Memory  │  │ Circadian │ │
│       │            │       │(ChromaDB)│  │  (sleep)  │ │
│       │            │       └──────────┘  └───────────┘ │
└───────┼────────────┼───────────────────────────────────┘
        │ ZMQ PUB    │ ZMQ PUSH
        │ percepts   │ actions
        │            ▼
┌───────┴──────── Body (Node.js) ─────────────────────────┐
│                                                          │
│   mineflayer bot  ◀──▶  Minecraft Server (PaperMC)      │
│   "Prometheus"           localhost:25565                  │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### Subsystems

| Module | Role | Implementation |
|---|---|---|
| **Lizard** (System 1) | Fast reflexes: flee, eat, dodge | Embedded Phi-3.5-mini via llama.cpp (< 100 ms) |
| **Soul** (System 2) | Deliberative reasoning, planning | Qwen 2.5-VL-32B via llama-server HTTP |
| **Arbiter** | Subsumption conflict resolution | Layer 0 reflexes veto Soul unless explicit override |
| **BodyLink** | ZMQ IPC with mineflayer bot | SUB/PUB for percepts, PUSH/PULL for actions |
| **Memory** | Two-tier memory system | Short-term markers + ChromaDB long-term retrieval |
| **Circadian** | Sleep/wake state machine | Awake (20 min) → Tired → Sleep (consolidation) → Wake |
| **Teacher** | Session grading and lessons | Gemini API grades daily logs, injects lessons on wake |

### Wire Protocol

Two ZMQ socket pairs on localhost:

| Direction | Pattern | Endpoint | Payload |
|---|---|---|---|
| Body → Head | PUB → SUB | `tcp://127.0.0.1:5555` | Percept JSON (100 ms interval) |
| Head → Body | PUSH → PULL | `tcp://127.0.0.1:5556` | Action JSON |

**Percept example:**
```json
{
  "type": "percept",
  "health": 20, "food": 20,
  "position": {"x": 8.5, "y": 112.0, "z": -9.5},
  "nearby_entities": [{"name": "zombie", "distance": 3.1, "hostile": true}],
  "ground": "safe"
}
```

**Action examples:**
```json
{"action": "flee", "reason": "hostile_nearby"}
{"action": "eat", "reason": "hungry"}
{"action": "explore", "reason": "no_threat"}
{"action": "idle", "reason": "no_threat"}
```

## Prerequisites

- **C++ toolchain**: GCC 13+ with C++20 support
- **CMake**: 3.20+
- **libzmq5**: System shared library (`apt install libzmq5`; dev headers fetched automatically)
- **Node.js**: 18+
- **Java**: 21+ (for PaperMC server)
- **Minecraft server**: PaperMC 1.20.4 (included in `server/`)

Optional (for full inference, not required for the reflex loop):
- **llama.cpp**: For embedded Phi-3.5-mini (Lizard)
- **llama-server**: Hosting Qwen 2.5-VL-32B (Soul)
- **ChromaDB**: Long-term memory store
- **Gemini API key**: Teacher grading

## Quick Start

### 1. Build the Head

```bash
cd head
cmake -S . -B build
cmake --build build
```

CMake will automatically fetch the `zmq.h` header if dev headers aren't installed.

### 2. Install Body dependencies

```bash
cd body
npm install
```

### 3. Start the Minecraft server

```bash
cd server
./start.sh
```

Wait for `Done (Xs)!` in the console. Set difficulty if needed:
```
difficulty normal
```

### 4. Start the Body

```bash
cd body
node bot.js
```

The bot connects to the Minecraft server and begins publishing percepts on `:5555`.

### 5. Start the Head

```bash
cd head
./build/prometheus_head
```

The head subscribes to percepts and begins the reflex loop. With hostile mobs nearby, you'll see:
```
[BODY] >> {"action":"flee","reason":"hostile_nearby"}
```

## Project Structure

```
prometheus/
├── head/                    # C++ backplane
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp         # Thread orchestration
│       ├── lizard/          # System 1 — fast reflexes
│       ├── soul/            # System 2 — deliberative reasoning
│       ├── arbiter/         # Subsumption conflict resolution
│       ├── ipc/             # ZeroMQ BodyLink
│       ├── memory/          # Short-term + long-term memory
│       ├── circadian/       # Sleep/wake state machine
│       └── teacher/         # Gemini-based session grading
├── body/                    # Node.js mineflayer bot
│   ├── bot.js               # Percept publisher + action dispatcher
│   └── package.json
└── server/                  # PaperMC Minecraft server
    ├── start.sh
    ├── server.properties
    └── paper.jar
```
