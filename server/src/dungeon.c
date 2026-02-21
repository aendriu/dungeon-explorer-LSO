#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../header/dungeon.h"
#include "../header/game_state.h"

int quest_items_collected = 0;
pthread_mutex_t quest_items_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ---------- Allocazione griglie per Room ---------- */

// Alloca una griglia bidimensionale di Item
// Inizializza tutti gli item come ITEM_NONE e già raccolti
static Item **alloc_item_grid(int width, int height) {
    Item **grid = malloc(height * sizeof(Item *));
    if (!grid) return NULL;

    // Crea ogni riga della griglia
    for (int y = 0; y < height; y++) {
        grid[y] = malloc(width * sizeof(Item));
        if (!grid[y]) {
            // Se fallisce, libera quello che è stato allocato
            for (int j = 0; j < y; j++) free(grid[j]);
            free(grid);
            return NULL;
        }
        // Inizializza ogni item nelle coordinate corrette
        for (int x = 0; x < width; x++) {
            grid[y][x] = (Item){ .x = x, .y = y, .type = ITEM_NONE,
                                 .collected = true };
        }
    }
    return grid;
}

// Alloca una griglia bidimensionale di puntatori a Monster
// Usa calloc quindi inizializza a NULL (nessun mostro inizialmente)
static Monster ***alloc_monster_grid(int width, int height) {
    Monster ***grid = malloc(height * sizeof(Monster **));
    if (!grid) return NULL;

    // Crea ogni riga della griglia e la inizializza a NULL
    for (int y = 0; y < height; y++) {
        grid[y] = calloc(width, sizeof(Monster *));
        if (!grid[y]) {
            // Se fallisce, libera quello che è stato allocato
            for (int j = 0; j < y; j++) free(grid[j]);
            free(grid);
            return NULL;
        }
    }
    return grid;
}

// Libera la memoria allocata per la griglia di item
static void free_item_grid(Item **grid, int height) {
    if (!grid) return;
    // Libera ogni riga
    for (int i = 0; i < height; i++) free(grid[i]);
    // Libera l'array di puntatori
    free(grid);
}

// Libera la memoria allocata per la griglia di mostri
static void free_monster_grid(Monster ***grid, int width, int height) {
    if (!grid) return;
    // Libera ogni mostro in ogni cella della griglia
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) free(grid[y][x]);
        // Libera ogni riga
        free(grid[y]);
    }
    // Libera l'array di puntatori
    free(grid);
}

/* ---------- Room lifecycle ---------- */

// Crea una nuova stanza con le griglie di item e mostri
Room* room_create(int id, int width, int height) {
    Room *r = malloc(sizeof(Room));
    if (!r) return NULL;

    // Imposta le proprietà della stanza
    r->id = id;
    r->width = width;
    r->height = height;

    // Alloca la griglia di item
    r->items = alloc_item_grid(width, height);
    if (!r->items) { free(r); return NULL; }

    // Alloca la griglia di mostri
    r->monsters = alloc_monster_grid(width, height);
    if (!r->monsters) { free_item_grid(r->items, height); free(r); return NULL; }

    return r;
}

// Distrugge una stanza e libera tutta la memoria associata
void room_destroy(Room *r) {
    if (!r) return;
    // Libera le griglie
    free_item_grid(r->items, r->height);
    free_monster_grid(r->monsters, r->width, r->height);
    // Libera la struttura Room
    free(r);
}

// Genera casualmente il tipo di item con probabilità diverse
// QUEST (1%), APPLE (1.3%), TRAP (2%), NORMAL (4%), NONE (resto)
static ItemType roll_item_type(unsigned int *seed) {
    if (rand_r(seed) % 100 == 0) return ITEM_QUEST;  // Raro
    if (rand_r(seed) % 75  == 0) return ITEM_APPLE;  // Raro
    if (rand_r(seed) % 50  == 0) return ITEM_TRAP;   // Poco raro
    if (rand_r(seed) % 25  == 0) return ITEM_NORMAL; // Comune
    return ITEM_NONE;  // Vuoto
}

// Posiziona un item in una cella e imposta le sue proprietà
static void place_item(Item *item, ItemType type) {
    switch (type) {
    // Item validi: non raccolti
    case ITEM_QUEST:
    case ITEM_APPLE:
    case ITEM_TRAP:
    case ITEM_NORMAL:
        item->type      = type;
        item->collected  = false;  // Da raccogliere
        break;
    default:
        // Item vuoto: già raccolto (non esiste)
        item->type      = ITEM_NONE;
        item->collected  = true;
        break;
    }
}

// Genera casualmente gli item in tutte le celle della stanza
static void room_generate_items(Room *r) {
    if (!r) return;
    unsigned int *seed = get_rand_seed();

    // Passa per ogni cella e genera un item casuale
    for (int y = 0; y < r->height; y++)
        for (int x = 0; x < r->width; x++)
            place_item(&r->items[y][x], roll_item_type(seed));

    printf("- Room %d items generated (%dx%d)\n", r->id, r->width, r->height);
}

// Posiziona un mostro in una cella con probabilità 2% (1 ogni 50 celle)
static void place_monster(Monster **cell, int x, int y, unsigned int *seed) {
    // Libera il mostro precedente se esiste
    if (*cell != NULL) {
        free(*cell);
        *cell = NULL;
    }
    // 2% di probabilità di spawnare un mostro
    if (rand_r(seed) % 50 != 0) return;

    // Crea e inizializza un nuovo mostro
    Monster *m = malloc(sizeof(Monster));
    if (!m) return;
    *m = (Monster){ .x = x, .y = y, .state = MONSTER_ALIVE,
                    .target_player_id = -1, .failed_moves = 0 };
    *cell = m;
}

// Genera casualmente i mostri nella stanza
// Evita di piazzarli al centro (spawn player) o sopra gli item
static void room_generate_monsters(Room *r) {
    unsigned int *seed = get_rand_seed();
    // Calcola il centro della stanza (spawn point del player)
    int spawn_x = r->width / 2;
    int spawn_y = r->height / 2;

    for (int y = 0; y < r->height; y++)
        for (int x = 0; x < r->width; x++) {
            // Non mettere mostri al centro (spawn player)
            if (x == spawn_x && y == spawn_y) continue;
            // Non mettere mostri sopra i raccoglibili
            if (r->items[y][x].type != ITEM_NONE && !r->items[y][x].collected) continue;
            // Piazza il mostro se il random lo consente
            place_monster(&r->monsters[y][x], x, y, seed);
        }

    printf("- Room %d monsters generated (%dx%d)\n", r->id, r->width, r->height);
}

// Popola la stanza con item e mostri casuali
static void room_populate(Room *r) {
    if (!r) return;
    // Genera prima gli item
    room_generate_items(r);
    // Poi genera i mostri
    room_generate_monsters(r);
}

// Recupera un mostro vivente in una posizione
// Ritorna NULL se non esiste, è fuori limite o è morto
Monster* room_get_monster(Room *r, int x, int y) {
    // Valida i parametri
    if (!r || x < 0 || x >= r->width || y < 0 || y >= r->height) {
        return NULL;
    }

    // Prendi il mostro dalla posizione
    Monster *monster = r->monsters[y][x];
    // Ritorna NULL se non esiste o è morto
    if (monster == NULL || monster->state == MONSTER_DEAD) {
        return NULL;
    }

    return monster;
}

// Recupera un item non raccolto in una posizione
// Ritorna NULL se non esiste, è fuori limite o già raccolto
Item* room_get_item(Room *r, int x, int y) {
    // Valida i parametri
    if (!r || x < 0 || x >= r->width || y < 0 || y >= r->height) {
        return NULL;
    }

    // Prendi l'item dalla posizione
    Item *item = &r->items[y][x];
    // Ritorna NULL se vuoto o già raccolto
    if (item->type == ITEM_NONE || item->collected) {
        return NULL;
    }

    return item;
}

// Crea un dungeon completo con numero di stanze specificate
// Ogni stanza ha dimensioni casuali tra i limiti definiti
Dungeon* dungeon_create(int num_rooms) {
    // Alloca la struttura del dungeon
    Dungeon *d = malloc(sizeof(Dungeon));
    if (!d) return NULL;

    // Imposta il numero di stanze
    d->num_rooms = num_rooms;
    // Alloca l'array di stanze
    d->rooms = malloc(num_rooms * sizeof(Room*));
    if (!d->rooms) {
        free(d);
        return NULL;
    }

    // Ottieni il seed per la generazione casuale
    unsigned int *seed = get_rand_seed();

    // Crea ogni stanza con dimensioni casuali
    for (int i = 0; i < num_rooms; i++) {
        // Genera altezza casuale tra ROOM_H_MIN e ROOM_H_MAX
        int rh = ROOM_H_MIN + rand_r(seed) % (ROOM_H_MAX - ROOM_H_MIN + 1);
        // Genera larghezza casuale tra ROOM_W_MIN e ROOM_W_MAX
        int rw = ROOM_W_MIN + rand_r(seed) % (ROOM_W_MAX - ROOM_W_MIN + 1);
        // Crea la stanza
        d->rooms[i] = room_create(i, rw, rh);
        if (!d->rooms[i]) {
            // Se fallisce, libera tutto quello che è stato creato
            for (int j = 0; j < i; j++) {
                room_destroy(d->rooms[j]);
            }
            free(d->rooms);
            free(d);
            return NULL;
        }
        // Popola la stanza con item e mostri
        room_populate(d->rooms[i]);
        printf("- Room %d: %dx%d\n", i, rw, rh);
    }

    printf("- Dungeon created: %d rooms\n", num_rooms);
    return d;
}

// Distrugge il dungeon e libera tutta la memoria associata
void dungeon_destroy(Dungeon *d) {
    if (!d) return;

    // Distrugge ogni stanza
    for (int i = 0; i < d->num_rooms; i++) {
        room_destroy(d->rooms[i]);
    }
    // Libera l'array di stanze
    free(d->rooms);
    // Libera la struttura del dungeon
    free(d);  
}

// Recupera una stanza dal dungeon per ID
// Ritorna NULL se l'ID è invalido
Room* dungeon_get_room(Dungeon *d, int room_id) {
    // Valida il dungeon e l'ID della stanza
    if (!d || room_id < 0 || room_id >= d->num_rooms) {
        return NULL;
    }

    // Ritorna la stanza richiesta
    return d->rooms[room_id];
}


