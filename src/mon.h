#ifndef INCLUDED_MON_H
#define INCLUDED_MON_H

/* XXX Refactor monster_type, monster_race, etc ... */
typedef struct monster_type mon_t, *mon_ptr;
typedef struct monster_race mon_race_t, *mon_race_ptr;

extern int mon_ac(mon_ptr mon);

/* Should we show a message to the player for this monster?
 * When monsters battle each other, especially off screen, this
 * can generate a lot of message spam. cf ignore_unview. */
extern bool mon_show_msg(mon_ptr mon);

extern mon_race_ptr mon_race(mon_ptr mon);
extern mon_race_ptr mon_true_race(mon_ptr mon);

#endif
