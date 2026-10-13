/* undo.c: the app's history, as the web's undoStack(). An action runs and is remembered (up to 50); its toast offers
   undo, an undo's toast offers redo. Ctrl (Cmd) and Z undoes, with Shift (or Ctrl+Y) redoes - outside text fields,
   which keep a history of their own. */
#include "rk_internal.h"

#define HISTORY 50

typedef struct entry {
  char *label;
  void (*run)(void *user); void (*undo)(void *user); void *user; void (*free_user)(void *user);
} entry;

static entry history[HISTORY];
static int nhistory, ndone;                                /* [0, ndone) are done; [ndone, nhistory) can be redone */

static void drop(entry *e) {
  if (e->free_user) e->free_user(e->user);
  free(e->label);
  memset(e, 0, sizeof *e);
}

static void undo_toast(void *user) { rk_window_undo(user); }
static void redo_toast(void *user) { rk_window_redo(user); }

void rk_window_do(rk_window *w, const char *label, void (*run)(void *), void (*undo)(void *), void *user, void (*free_user)(void *)) {
  while (nhistory > ndone) drop(&history[--nhistory]);     /* a new action: what was undone cannot come back */
  if (nhistory == HISTORY) {                               /* full: the oldest goes */
    drop(&history[0]);
    memmove(history, history + 1, (HISTORY - 1) * sizeof *history);
    nhistory--; ndone--;
  }
  entry *e = &history[nhistory++];
  e->label = rk__strdup(label ? label : ""); e->run = run; e->undo = undo; e->user = user; e->free_user = free_user;
  ndone = nhistory;
  if (run) run(user);
  if (w) rk_window_toast(w, e->label, "undo", undo_toast, w);
}

bool rk_window_undo(rk_window *w) {
  if (!ndone) return false;
  entry *e = &history[--ndone];
  if (e->undo) e->undo(e->user);
  if (w) {
    char text[256];
    snprintf(text, sizeof text, "Undone: %s", e->label);
    rk_window_toast(w, text, "redo", redo_toast, w);
  }
  return true;
}

bool rk_window_redo(rk_window *w) {
  if (ndone == nhistory) return false;
  entry *e = &history[ndone++];
  if (e->run) e->run(e->user);
  if (w) rk_window_toast(w, e->label, "undo", undo_toast, w);
  return true;
}

void rk__undo_clear(void) { while (nhistory) drop(&history[--nhistory]); ndone = 0; }
