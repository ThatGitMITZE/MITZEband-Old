#ifndef INCLUDED_MON_SPELL_H
#define INCLUDED_MON_SPELL_H

/* Monster Spell System */

/* Monster Spell Type (MST_*)
 * The spell type is used to intelligently choose spells depending on
 * the current situation. For example, wounded monsters will prefer
 * to MST_HEAL or possibly MST_ESCAPE. If the player is not "projectable",
 * then the attack types are excluded, but we might keep MST_BREATHE and
 * MST_BALL around if the player can be "splashed" by the effect. Etc. */
enum {
    MST_BREATHE,
    MST_BALL,
    MST_BOLT,
    MST_BEAM,
    MST_CURSE,
    MST_BUFF,
    MST_BIFF,
    MST_ESCAPE,
    MST_ANNOY,
    MST_SUMMON,
    MST_HEAL,
    MST_WEIRD,
    MST_COUNT
};

/* Monster Spell Flags (MSF_*) */
#define MSF_INNATE 0x0001
#define MSF_BALL0  0x0002 /* hack for THROW = {MST_BALL, GF_ROCK} */

/* Every spell can be identified by (Type, Effect) pair */
typedef struct {
    byte type;
    s16b effect;
} mon_spell_id_t;

extern mon_spell_id_t mon_spell_id(int type, int effect);
extern int            mon_spell_hash(mon_spell_id_t id);

/* Every spell can be parameterized (MSP_*) */
enum {
    MSP_NONE = 0,
    MSP_DICE,     /* for damage or duration of timed effects */
    MSP_HP_PCT,   /* percentage of chp for damage up to a max */
};
typedef struct {  /* XdY+Z */
    byte dd;
    byte ds;
    s16b base;
} dice_t;
typedef struct {  /* MIN(m->chp*pct/100, max) */
    byte pct;
    s16b max;
} hp_pct_t;
typedef struct {
    union {
        dice_t   dice;
        hp_pct_t hp_pct;
    } v;
    byte tag;
} mon_spell_parm_t, *mon_spell_parm_ptr;

extern mon_spell_parm_t mon_spell_parm_dice(int dd, int ds, int base);
extern mon_spell_parm_t mon_spell_parm_hp_pct(int pct, int max);
extern mon_spell_parm_t mon_spell_parm_default(mon_spell_id_t id, int rlev);
extern errr             mon_spell_parm_parse(mon_spell_parm_ptr parm, char *token);
extern void             mon_spell_parm_print(mon_spell_parm_ptr parm, string_ptr s);

/* Casting information for a spell (idea from Vanilla) */
typedef struct {
    cptr name;
    byte color;
    cptr cast_msg;
    cptr blind_msg;
    cptr xtra_msg;
} mon_spell_display_t, *mon_spell_display_ptr;

/* A single monster spell */
typedef struct {
    mon_spell_id_t   id;
    mon_spell_parm_t parm;
    mon_spell_display_ptr
                     display;
    s16b             lore;
    byte             prob;
    byte             flags;
} mon_spell_t, *mon_spell_ptr;

extern errr mon_spell_parse(mon_spell_ptr spell, int rlev, char *token);
extern void mon_spell_print(mon_spell_ptr spell, string_ptr s);

/* A collection of related spells, grouped together for tactical purposes.
 * Each tactical group has a dynamic probability depending on the current
 * context (e.g., heal when wounded, no offense when player out of los, etc.) */
typedef struct {
    byte type;   /* MST_* for the group */
    byte prob;
    byte count;
    byte allocated;
    mon_spell_ptr spells; /* vec<mon_spell_t> */
} mon_spell_group_t, *mon_spell_group_ptr;

extern mon_spell_group_ptr mon_spell_group_alloc(void);
extern void mon_spell_group_free(mon_spell_group_ptr group);
extern void mon_spell_group_prep(mon_spell_group_ptr group);
extern void mon_spell_group_add(mon_spell_group_ptr group, mon_spell_ptr spell);
extern mon_spell_ptr mon_spell_group_find(mon_spell_group_ptr group, mon_spell_id_t id);
extern mon_spell_ptr mon_spell_group_random(mon_spell_group_ptr group);

typedef struct {
    byte freq;
    s16b dam_pct;  /* 0 => 100%. Only works on MSP_DICE effects. Obsoletes RF2_POWERFUL. */
    u32b flags;
    mon_spell_group_ptr groups[MST_COUNT];
} mon_spells_t, *mon_spells_ptr;

extern mon_spells_ptr mon_spells_alloc(void);
extern void           mon_spells_free(mon_spells_ptr spells);
extern void           mon_spells_add(mon_spells_ptr spells, mon_spell_ptr spell);
extern errr           mon_spells_parse(mon_spells_ptr spells, int rlev, char *token);
extern vec_ptr        mon_spells_all(mon_spells_ptr spells);
extern mon_spell_ptr  mon_spells_random(mon_spells_ptr spells); /* stupid monsters */

/* Finally, it is time to cast a spell!
 * Note, this is slightly more complicated then I would like since monsters
 * may splash the player. We need not just a spell from the AI, but possibly
 * a target location as well. We'll group everything we need to actually cast
 * a spell into the following: */
typedef struct {
    mon_ptr       mon;
    char          name[MAX_NLEN];
    mon_race_ptr  race;
    mon_spell_ptr spell;
    point_t       target;
} mon_spell_cast_t, *mon_spell_cast_ptr;

/* Allow clients to plug in a smarter/alternative AI */
typedef bool (*mon_spell_ai)(mon_spell_cast_ptr cast);

extern bool           mon_spell_cast(mon_ptr mon, mon_spell_ai ai);
extern bool           mon_spell_cast_mon(mon_ptr mon, mon_spell_ai ai);
extern mon_spell_ptr  mon_spell_find(mon_race_ptr race, mon_spell_id_t id);

/* Some classes need to know what spell is being cast, and who is doing it: */
extern mon_spell_ptr  mon_spell_current(void);
extern mon_ptr        mon_current(void);

#endif

