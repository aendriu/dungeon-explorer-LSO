# Dungeon Explorer

Gioco cooperativo multiplayer (fino a 4 giocatori) in cui si esplorano stanze di un dungeon generato casualmente, si raccolgono oggetti, si evitano trappole e si combattono mostri — il tutto da terminale con **ncurses**.

> Progetto per il corso di **Laboratorio di Sistemi Operativi** — Università degli Studi di Napoli Federico II  
> Autori: **Andrea Antimone di Luise** & **Adriano di Giovanni**

---

## Indice

- [Come si gioca](#come-si-gioca)
- [Requisiti](#requisiti)
- [Compilazione](#compilazione)
- [Esecuzione](#esecuzione)
- [Docker](#docker)
- [Architettura](#architettura)
- [Protocollo di rete](#protocollo-di-rete)
- [Meccaniche di gioco](#meccaniche-di-gioco)
- [Struttura del progetto](#struttura-del-progetto)

---

## Come si gioca

1. Un giocatore avvia il **server**
2. Fino a 4 giocatori si collegano con il **client**
3. Dalla lobby, un giocatore preme Invio per avviare la partita
4. Si esplorano 10 stanze collegate tra loro, muovendosi con **WASD** o le **frecce**
5. **Obiettivo**: raccogliere **3 oggetti quest** sparsi nel dungeon
6. **Game over**: se le vite della squadra arrivano a 0

---

## Requisiti

- GCC con supporto C99/POSIX
- `libncurses-dev` (per il client)
- `make`
- `pthread` (incluso in glibc su Linux)

Su Debian/Ubuntu:
```bash
sudo apt install gcc make libncurses5-dev libncursesw5-dev
```

---

## Compilazione

```bash
make          # compila server.out e client.out
make clean    # rimuove i binari
```

Flag usati: `-Wall -Wextra -O2 -g -pthread`

---

## Esecuzione

**Server** (in un terminale):
```bash
./server.out
```
Stampa gli IP locali e la porta (default: `9090`), poi attende connessioni.

**Client** (in un altro terminale, anche su un'altra macchina):
```bash
./client.out
```
Inserire IP e porta del server quando richiesto (default: `127.0.0.1:9090`).

---

## Docker

### Build
```bash
docker compose build
```

### Avvio server
```bash
docker compose up              # in primo piano
docker compose up -d           # in background
```

### Avvio client (uno per terminale)
```bash
docker compose run client
```
Quando il client chiede l'IP, scrivere **`server`** — Docker Compose risolve il nome del servizio nella rete interna.

### Stop
```bash
docker compose down
```

---

## Architettura

### Server — multi-thread, 1 thread per giocatore

```
 Main thread
 ├── accept() loop
 ├── SIGALRM handler (timer idle 1s)
 │
 ├── Player thread 0  ─┐
 ├── Player thread 1   │  recv JSON → parse → handle → send risposta
 ├── Player thread 2   │
 └── Player thread 3  ─┘
```

**Mutex** (5 totali):

| Mutex | Protegge |
|-------|----------|
| `game_state.mutex` | Mappa, posizioni, accesso al dungeon |
| `team.lives_mutex` | Vite condivise e mostri uccisi |
| `player_states_mutex` | Statistiche individuali dei giocatori |
| `conn_mutex` | Slot connessioni e conteggio giocatori |
| `quest_mutex` | Contatore oggetti quest raccolti |

### Client — 2 thread

```
 Main thread              Poller thread
 ├── ncurses UI            ├── ogni ~500ms chiede GET_GAME_STATE
 ├── input utente          └── aggiorna stato condiviso
 └── invio comandi
```

**Mutex** (2 totali):

| Mutex | Protegge |
|-------|----------|
| `net_mutex` | Socket TCP (serializza send/recv tra i 2 thread) |
| `poller.mutex` | GameState condiviso tra UI e poller |

### Segnali

| Segnale | Server | Client |
|---------|--------|--------|
| `SIGINT` / `SIGTERM` | Shutdown graceful | Uscita pulita + cleanup ncurses |
| `SIGALRM` | Timer idle (ogni 1s decrementa countdown per boost mostri) | — |

---

## Protocollo di rete

- **Trasporto**: TCP, porta **9090**
- **Frame**: dimensione fissa **4096 byte**, zero-padded
- **Formato**: JSON (libreria cJSON)

### Comandi

| Codice | Nome | Direzione | Descrizione |
|--------|------|-----------|-------------|
| `0` | `GET_N_OF_CONNECTED_PLAYERS` | C → S | Quanti giocatori connessi? |
| `1` | `START_GAME` | C → S | Avvia la partita |
| `2` | `MOVE_PLAYER` | C → S | Cambia stanza (payload: `target_room`) |
| `3` | `GET_GAME_STATE` | C → S | Stato completo del gioco |
| `4` | `UPDATE_PLAYER_POSITION` | C → S | Aggiorna posizione (payload: `pos_y`, `pos_x`) |

Ogni risposta (tranne il comando 0) è un JSON con: mappa, posizioni giocatori, griglia stanza, statistiche squadra/giocatore, eventuale errore.

---

## Meccaniche di gioco

### Dungeon

- **10 stanze** connesse da un percorso casuale (catena Hamiltoniana) — tutte raggiungibili
- Ogni stanza ha dimensioni random (16–28 righe × 24–40 colonne)
- 4 porte sui punti medi delle pareti (N/E/S/W)

### Oggetti

| Tipo | Simbolo | Probabilità | Effetto |
|------|---------|-------------|---------|
| Normale | `?` | ~4% | Raccolto nell'inventario |
| Quest | `?` (nascosto) | ~1% | Incrementa contatore quest — 3 per vincere |
| Mela | `@` | ~1.3% | +1 vita alla squadra |
| Trappola | `T` | ~2% | −1 vita alla squadra |
| Trappola-mostro | `T` | — | Creata dai mostri che camminano su oggetti normali |

### Mostri

- Spawnano con **2% di probabilità** per cella vuota
- Quando un giocatore entra nella stanza, ogni mostro sceglie un bersaglio casuale
- Si muovono di 1 cella verso il bersaglio ad ogni tick, con probabilità crescente
- Se raggiungono il giocatore → **−1 vita squadra**
- Se il giocatore cammina su un mostro → **mostro ucciso**
- Se un mostro passa su un oggetto normale → lo trasforma in **trappola-mostro**

### Boost idle

Se un giocatore resta fermo per **3 secondi**, i mostri nella sua stanza diventano più aggressivi (+1 alla probabilità di movimento). Il timer si resetta appena il giocatore si muove.

### Vittoria e sconfitta

-  **Vittoria**: raccogliere **3 oggetti quest** tra tutti i giocatori
-  **Sconfitta**: vite della squadra a **0** (si parte con **12 vite condivise**)

Dopo la fine della partita, si può ricominciare — il dungeon viene rigenerato.

---

## Struttura del progetto

```
.
├── makefile                  # Build server.out e client.out
├── Dockerfile                # Immagine Ubuntu + gcc + ncurses
├── docker-compose.yml        # Servizi: server e client
├── requirements              # Dipendenze apt
│
├── server/
│   ├── header/
│   │   ├── connection.h      # Socket, costanti rete, struct Conn
│   │   ├── dungeon.h         # Struct Room, Dungeon, Item, Monster
│   │   ├── game_state.h      # Struct GameState, Team, idle countdown
│   │   ├── monster.h         # Prototipi AI mostri
│   │   ├── player.h          # Gestione connessione giocatori
│   │   ├── player_state.h    # Struct PlayerState (statistiche)
│   │   └── protocol.h        # Struct Request, dispatcher
│   └── src/
│       ├── main.c            # Entry point, accept loop, segnali
│       ├── connection.c      # Init socket, listen, accept, cleanup
│       ├── protocol.c        # Parsing comandi, handler, logica di gioco
│       ├── game_state.c      # Stato globale, generazione mappa, JSON
│       ├── dungeon.c         # Creazione stanze, spawn oggetti/mostri
│       ├── monster.c         # Movimento mostri, IA, cattura
│       └── player.c          # Slot giocatori, thread, vite
│
├── client/
│   ├── header/
│   │   ├── connection.h      # IP/porta default, prototipi connessione
│   │   ├── game_state.h      # Struct GameState lato client
│   │   ├── gui.h             # Prototipi schermate ncurses
│   │   ├── game_actions.h    # Poller, azioni di gioco
│   │   ├── protocol.h        # Invio/ricezione frame, net_mutex
│   │   ├── mysignals.h       # Handler segnali client
│   │   ├── screens.h         # Enum schermate
│   │   └── ui_helpers.h      # Helper ncurses
│   └── src/
│       ├── main.c            # Entry point, macchina a stati
│       ├── connection.c      # Connessione TCP al server
│       ├── protocol.c        # Frame JSON, mutex rete
│       ├── game_state.c      # Parsing JSON → GameState
│       ├── gui.c             # Tutte le schermate ncurses
│       ├── game_actions.c    # Thread poller, azioni rete
│       ├── mysignals.c       # SIGINT/SIGTERM
│       └── ui_helpers.c      # Utility ncurses
│
└── utils/
    ├── enums.h               # Enum condivisi (comandi, item, stati)
    └── cjson/
        ├── cJSON.c           # Libreria JSON (terze parti)
        └── cJSON.h
```
