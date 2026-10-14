/* undo.c: a window's history, as the web's undoStack(). An action runs and is remembered (up to 50); its toast offers
   undo, an undo's toast offers redo. Ctrl (Cmd) and Z undoes, with Shift (or Ctrl+Y) redoes - outside text fields,
   which keep a history of their own. Each window has its own: undo in one never takes back what another did. */
#include "rk_internal.h"

#define HISTORY 50

struct rk_history_entry { rk_action a; char *label; };

static void drop(rk_history_entry *e) {
  if (e->a.free_user) e->a.free_user(e->a.user);
  free(e->label);
  memset(e, 0, sizeof *e);
}

static void undo_toast(void *user) { rk_window_undo(user); }
static void redo_toast(void *user) { rk_window_redo(user); }

void rk_window_do(rk_window *w, const rk_action *action) {
  if (!w || !action) return;
  rk_action a = { sizeof a, NULL, NULL, NULL, NULL, NULL };
  if (!RK__SIZED(action, "rk_window_do", "rk_action")) return;
  memcpy(&a, action, RK_MIN((size_t)action->size, sizeof a));
  while (w->nhistory > w->ndone) drop(&w->history[--w->nhistory]);   /* a new action: what was undone cannot come back */
  if (w->nhistory == HISTORY) {                            /* full: the oldest goes */
    drop(&w->history[0]);
    memmove(w->history, w->history + 1, (HISTORY - 1) * sizeof *w->history);
    w->nhistory--; w->ndone--;
  }
  w->history = rk__grow(w->history, &w->hcap, w->nhistory + 1, sizeof *w->history);
  rk_history_entry *e = &w->history[w->nhistory++];
  e->a = a; e->label = rk__strdup(a.label ? a.label : "");
  w->ndone = w->nhistory;
  if (a.run) a.run(a.user);
  rk_window_toast(w, e->label, "undo", undo_toast, w);
}

bool rk_window_undo(rk_window *w) {
  if (!w || !w->ndone) return false;
  rk_history_entry *e = &w->history[--w->ndone];
  if (e->a.undo) e->a.undo(e->a.user);
  char text[256];
  snprintf(text, sizeof text, "Undone: %s", e->label);
  rk_window_toast(w, text, "redo", redo_toast, w);
  return true;
}

bool rk_window_redo(rk_window *w) {
  if (!w || w->ndone == w->nhistory) return false;
  rk_history_entry *e = &w->history[w->ndone++];
  if (e->a.run) e->a.run(e->a.user);
  rk_window_toast(w, e->label, "undo", undo_toast, w);
  return true;
}

void rk__history_free(rk_window *w) {
  while (w->nhistory) drop(&w->history[--w->nhistory]);
  free(w->history); w->history = NULL; w->ndone = w->hcap = 0;
}
