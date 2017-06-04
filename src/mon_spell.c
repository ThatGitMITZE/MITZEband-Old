#include "angband.h"

#include "mon_spell.h"
#include <assert.h>

/*************************************************************************
 * Parse Tables (for r_info.txt)
 *
 * The parser will first look in various parse tables  for spell names,
 * such as THROW, SHOOT or HASTE. If not present, then the token prefix
 * determines the spell type (such as BR_ for MST_BREATH) while the
 * token suffix determines the effect type (currently we use the GF_*
 * system, though this needs some reworking). For example, BA_ACID for
 * an acid ball {MST_BALL, GF_ACID}.
 *
 * Now, sometimes you want to override the default combinatorics. For
 * example, BA_MANA is indeed {MST_BALL, GF_MANA}, but it is not "mana ball"
 * at all, but "mana storm". Similar for BA_CHAOS ("invoke logrus") and
 * a few others. Thus, the parse tables give you the option for display
 * overrides, while the default fallback combinatorics make it easier to
 * add new stuff without tabling everything up (Though if you want default
 * damage numbers, you'll need to add those by hand ... we also currently
 * assert these cases into errors but they may change).
 *
 * You can BA_MANA for a mana storm, but you can also BA_MANA(350) for a 350hp
 * mana storm, bypassing the normal damage calculation.
 *
 * You can BR_FIRE to breathe fire, but you can also BR_FIRE(80%) to use
 * 80% of chp for damage rather than the default of just 20%.
 *
 * You can BA_NETHER for a normal damage nether ball, BA_NETHER(10d10+200)
 * for custom damage, or even S:POWER_200% | BA_NETHER to use double the
 * normal damage calculation. POWER_* only works for spells that use damage
 * dice (so it skips breaths).
 *
 ************************************************************************/

/* Spells: We use a series of parse tables to handle overrides, as
 * discussed above. Each MST_* code gets its own table to preserve
 * my sanity. Note: We store spell lore by the numeric code, so
 * any changes to these values will cause problems! */
typedef struct {
    cptr token;
    mon_spell_id_t id;
    mon_spell_display_t display;
    u32b flags;
} _parse_t, *_parse_ptr;

/* MST_ANNOY */
enum {
    ANNOY_AMNESIA,
    ANNOY_ANIMATE_DEAD,
    ANNOY_BLIND,
    ANNOY_CONFUSE,
    ANNOY_DARKNESS,
    ANNOY_PARALYZE,
    ANNOY_SCARE,
    ANNOY_SHRIEK,
    ANNOY_SLOW,
    ANNOY_TELE_LEVEL,
    ANNOY_TELE_TO,
    ANNOY_TRAPS,
    ANNOY_WORLD,
};
static _parse_t _annoy_tbl[] = {
    { "AMNESIA", { MST_ANNOY, ANNOY_AMNESIA },
        { "Amnesia", TERM_L_BLUE,
          "$CASTER tries to blank your mind.",
          "$CASTER tries to blank your mind."}, MSF_TARGET},
    { "ANIM_DEAD", { MST_ANNOY, ANNOY_ANIMATE_DEAD },
        { "Animate Dead", TERM_L_DARK,
          "$CASTER casts a spell to revive the dead.",
          "$CASTER mumbles."}},
    { "BLIND", { MST_ANNOY, ANNOY_BLIND },
        { "Blind", TERM_WHITE,
          "$CASTER casts a spell, burning your eyes!",
          "$CASTER mumbles."}, MSF_TARGET},
    { "CONFUSE", { MST_ANNOY, ANNOY_CONFUSE },
        { "Confuse", TERM_L_UMBER,
          "$CASTER creates a mesmerizing illusion.",
          "$CASTER mumbles, and you hear puzzling noises."}, MSF_TARGET},
    { "DARKNESS", { MST_ANNOY, ANNOY_DARKNESS },
        { "Create Darkness", TERM_L_DARK }},
    { "PARALYZE", { MST_ANNOY, ANNOY_PARALYZE },
        { "Paralyze", TERM_RED,
          "$CASTER stares deep into your eyes!",
          "$CASTER mumbles."}, MSF_TARGET},
    { "SCARE", { MST_ANNOY, ANNOY_SCARE },
        { "Terrify", TERM_RED,
          "$CASTER casts a fearful illusion.",
          "$CASTER mumbles, and you hear scary noises."}, MSF_TARGET},
    { "SLOW", { MST_ANNOY, ANNOY_SLOW },
        { "Slow", TERM_L_UMBER,
          "$CASTER drains power from your muscles!",
          "$CASTER drains power from your muscles!"}, MSF_TARGET},
    { "SHRIEK", { MST_ANNOY, ANNOY_SHRIEK },
        { "Shriek", TERM_L_BLUE,
          "$CASTER makes a high pitched shriek.",
          "$CASTER makes a high pitched shriek." }, MSF_INNATE },
    { "TELE_LEVEL", { MST_ANNOY, ANNOY_TELE_LEVEL },
        { "Teleport Level", TERM_WHITE,
          "$CASTER gestures at your feet.",
          "$CASTER mumbles strangely."}, MSF_TARGET},
    { "TELE_TO", { MST_ANNOY, ANNOY_TELE_TO },
        { "Teleport To", TERM_WHITE,
          "$CASTER commands you to return.",
          "$CASTER mumbles." }, MSF_TARGET},
    { "TRAPS", { MST_ANNOY, ANNOY_TRAPS },
        { "Create Traps", TERM_WHITE,
          "$CASTER casts a spell and cackles evilly.",
          "$CASTER mumbles gleefully." }, MSF_TARGET},
    { "WORLD", { MST_ANNOY, ANNOY_WORLD },
        { "Stop Time", TERM_L_BLUE}},
    {0}
};

/* MST_BIFF */
enum {
    BIFF_ANTI_MAGIC,
    BIFF_DISPEL_MAGIC,
    BIFF_POLYMORPH,
};
static _parse_t _biff_tbl[] = {
    { "ANTI_MAGIC", { MST_BIFF, BIFF_ANTI_MAGIC },
        { "Anti-Magic", TERM_L_BLUE,
          "$CASTER invokes <color:B>Anti-Magic</color>.",
          "$CASTER mumbles powerfully.",
          ""
          "You invoke <color:B>Anti-Magic</color>." }, MSF_TARGET},
    { "DISPEL_MAGIC", { MST_BIFF, BIFF_DISPEL_MAGIC },
        { "Dispel Magic", TERM_L_BLUE,
          "$CASTER invokes <color:B>Dispel Magic</color>.",
          "$CASTER mumbles powerfully.",
          "",
          "You invoke <color:B>Dispel Magic</color>." }, MSF_TARGET},
    { "POLYMORPH", { MST_BIFF, BIFF_POLYMORPH },
        { "Polymorph Other", TERM_RED,
          "$CASTER invokes <color:r>Polymorph Other</color>.",
          "$CASTER mumbles powerfully.",
          "",
          "You invoke <color:r>Polymorph Other<color>." }, MSF_TARGET},
    {0}
};

/* MST_BUFF */
enum {
    BUFF_HASTE,
    BUFF_INVULN,
};
static _parse_t _buff_tbl[] = {
    { "HASTE", { MST_BUFF, BUFF_HASTE },
        { "Haste Self", TERM_WHITE,
          "$CASTER concentrates on $CASTER_POS body.",
          "$CASTER mumbles.",
          "$CASTER concentrates on $CASTER_POS body.",
          "You concentrate on your body." }},
    { "INVULN", { MST_BUFF, BUFF_INVULN },
        { "Invulnerability", TERM_YELLOW,
          "$CASTER casts a <color:y>Globe of Invulnerability</color>.",
          "$CASTER mumbles powerfully.",
          "$CASTER casts a <color:y>Globe of Invulnerability</color>.",
          "You cast a <color:y>Globe of Invulnerability</color>." }},
    {0}
};

/* MST_BALL */
static _parse_t _ball_tbl[] = {
    { "BA_CHAOS", { MST_BALL, GF_CHAOS },
        { "Invoke Logrus", TERM_VIOLET,
          "$CASTER invokes a <color:v>Raw Logrus</color>.",
          "$CASTER mumbles frighteningly.",
          "$CASTER invokes a <color:v>Raw Logrus</color> at $TARGET.",
          "You invoke a <color:v>Raw Logrus</color>." }, MSF_TARGET},
    { "BA_DARK", { MST_BALL, GF_DARK },
        { "Darkness Storm", TERM_L_DARK,
          "$CASTER invokes a <color:D>Darkness Storm</color>.",
          "$CASTER mumbles powerfully.",
          "$CASTER invokes a <color:D>Darkness Storm</color> at $TARGET.",
          "You invoke a <color:D>Darkness Storm</color>." }, MSF_TARGET},
    { "BA_LITE", { MST_BALL, GF_LITE },
        { "Starburst", TERM_YELLOW,
          "$CASTER invokes a <color:y>Starburst</color>.",
          "$CASTER mumbles powerfully.",
          "$CASTER invokes a <color:y>Starburst</color> at $TARGET.",
          "You invoke a <color:y>Starburst</color>." }, MSF_TARGET},
    { "MANA_STORM", { MST_BALL, GF_MANA },
        { "Mana Storm", TERM_L_BLUE,
          "$CASTER invokes a <color:B>Mana Storm</color>.",
          "$CASTER mumbles powerfully.",
          "$CASTER invokes a <color:B>Mana Storm</color> at $TARGET.",
          "You invoke a <color:B>Mana Storm</color>." }, MSF_TARGET},
    { "BA_NUKE", { MST_BALL, GF_NUKE },
        { "Radiation Ball", TERM_L_GREEN,
          "$CASTER casts a <color:G>Ball of Radiation</color>.",
          "$CASTER mumbles.",
          "$CASTER casts a <color:G>Ball of Radiation</color> at $TARGET.",
          "You cast a <color:G>Ball of Radiation</color>." }, MSF_TARGET},
    { "BA_POIS", { MST_BALL, GF_POIS },
        { "Stinking Cloud", TERM_L_GREEN,
          "$CASTER casts a <color:G>Stinking Cloud</color>.",
          "$CASTER mumbles.",
          "$CASTER casts a <color:G>Stinking Cloud</color> at $TARGET.",
          "You cast a <color:G>Stinking Cloud</color>." }, MSF_TARGET},
    { "BA_WATER", { MST_BALL, GF_WATER },
        { "Whirlpool", TERM_L_BLUE,
          "$CASTER gestures fluidly. You are engulfed in a <color:B>Whirlpool</color>.",
          "$CASTER mumbles. You are engulfed in a <color:B>Whirlpool</color>.",
          "$CASTER gestures fluidly. $TARGET is engulfed in a <color:B>Whirlpool</color>.",
          "You gesture fluidly." }, MSF_TARGET},
    { "BRAIN_SMASH", { MST_BALL, GF_BRAIN_SMASH },
        { "Brain Smash", TERM_L_BLUE,
          "$CASTER gazes deep into your eyes.",
          "You feel something focusing on your mind.", 
          "$CASTER gazes deep into the eyes of $TARGET.",
          "You gaze deeply." }, MSF_BALL0 | MSF_TARGET },
    { "DRAIN_MANA", { MST_BALL, GF_DRAIN_MANA },
        { "Drain Mana", TERM_L_BLUE}, MSF_BALL0 | MSF_TARGET },
    { "MIND_BLAST", { MST_BALL, GF_MIND_BLAST },
        { "Mind Blast", TERM_L_BLUE,
          "$CASTER gazes deep into your eyes.",
          "You feel something focusing on your mind.", 
          "$CASTER gazes deep into the eyes of $TARGET.",
          "You gaze deeply." }, MSF_BALL0 | MSF_TARGET},
    { "PULVERISE", { MST_BALL, GF_TELEKINESIS },
        { "Pulverise", TERM_L_BLUE,
          "$CASTER <color:B>pulverises</color> you.",
          "Something <color:B>pulverises</color> you.",
          "$CASTER <color:B>pulverises</color> $TARGET.",
          "" }, MSF_TARGET},
    { "ROCKET", { MST_BALL, GF_ROCKET },
        { "Rocket", TERM_L_UMBER,
          "$CASTER fires a <color:U>Rocket</color>.",
          "$CASTER shoots something.",
          "$CASTER fires a <color:U>Rocket</color> at $TARGET.",
          "You fire a <color:U>Rocket</color>." }, MSF_INNATE | MSF_TARGET },
    { "THROW", { MST_BALL, GF_ROCK }, /* non-reflectable! */
        { "Throw Boulder", TERM_L_UMBER,
          "$CASTER throws a large rock.",
          "$CASTER shouts, 'Haaa!!'.",
          "$CASTER throws a large rock at $TARGET.", 
          "You throw a large rock." }, MSF_INNATE | MSF_BALL0 | MSF_TARGET},
    {0}
};

/* MST_BOLT */
static _parse_t _bolt_tbl[] = {
    { "GAZE", { MST_BOLT, GF_ATTACK },
        { "Gaze", TERM_RED,
          "$CASTER gazes at you.",
          "",
          "$CASTER gazes at $TARGET.",
          ""}, MSF_TARGET},
    { "MISSILE", { MST_BOLT, GF_MISSILE },
        { "Magic Missile", TERM_WHITE,
          "$CASTER casts a Magic Missile.",
          "$CASTER mumbles.",
          "$CASTER casts a Magic Missile at $TARGET.",
          "You cast a Magic Missile." }, MSF_TARGET},
    { "SHOOT", { MST_BOLT, GF_ARROW },
        { "Shoot", TERM_L_UMBER,
          "$CASTER fires an arrow.",
          "$CASTER makes a strange noise.",
          "$CASTER fires an arrow at $TARGET.",
          "You fire an arrow." }, MSF_INNATE | MSF_TARGET },
    {0}
};

/* MST_BEAM */
static _parse_t _beam_tbl[] = {
    { "PSY_SPEAR", { MST_BEAM, GF_PSY_SPEAR },
        { "Psycho-Spear", TERM_L_BLUE,
          "$CASTER throws a <color:B>Psycho-Spear</color>.",
          "$CASTER mumbles.", 
          "$CASTER throws a <color:B>Psycho-Spear</color> at $TARGET.",
          "You throw a <color:B>Psycho-Spear</color>." }, MSF_TARGET },
    { "HELL_LANCE", { MST_BEAM, GF_HELL_FIRE },
        { "Hell Lance", TERM_RED,
          "$CASTER throws a <color:r>Hell Lance</color>.",
          "$CASTER mumbles.",
          "$CASTER throws a <color:r>Hell Lance</color> at $TARGET.",
          "You throw a <color:r>Hell Lance</color>." }, MSF_TARGET},
    { "HOLY_LANCE", { MST_BEAM, GF_HOLY_FIRE },
        { "Holy Lance", TERM_YELLOW,
          "$CASTER throws a <color:y>Holy Lance</color>.",
          "$CASTER mumbles.",
          "$CASTER throws a <color:y>Holy Lance</color> at $TARGET.",
          "You throw a <color:y>Holy Lance</color>." }, MSF_TARGET},
    {0}
};

/* MST_CURSE */
static _parse_t _curse_tbl[] = {
    { "CAUSE_1", { MST_CURSE, GF_CAUSE_1 },
        { "Cause Light Wounds", TERM_RED,
          "$CASTER points at you and curses.",
          "$CASTER curses.",
          "$CASTER points at $TARGET and curses.",
          "You curse."}, MSF_TARGET },
    { "CAUSE_2", { MST_CURSE, GF_CAUSE_2 },
        { "Cause Serious Wounds", TERM_RED,
          "$CASTER points at you and curses horribly.",
          "$CASTER curses horribly.",
          "$CASTER points at $TARGET and curses horribly.",
          "You curse horribly." }, MSF_TARGET },
    { "CAUSE_3", { MST_CURSE, GF_CAUSE_3 },
        { "Cause Critical Wounds", TERM_RED,
          "$CASTER points at you, incanting terribly!",
          "$CASTER incants terribly.",
          "$CASTER points at $TARGET, incanting terribly!",
          "You incant terribly." }, MSF_TARGET },
    { "CAUSE_4", { MST_CURSE, GF_CAUSE_4 },
        { "Cause Mortal Wounds", TERM_RED,
          "$CASTER points at you, screaming the word DIE!",
          "$CASTER screams the word DIE!", 
          "$CASTER points at $TARGET, screaming the word DIE!",
          "You scream the word DIE!" }, MSF_TARGET },
    { "HAND_DOOM", { MST_CURSE, GF_HAND_DOOM },
        { "Hand of Doom", TERM_RED,
          "$CASTER invokes the <color:r>Hand of Doom</color>!",
          "$CASTER invokes the <color:r>Hand of Doom</color>!",
          "$CASTER invokes the <color:r>Hand of Doom</color> at $TARGET.",
          "You invoke the <color:r>Hand of Doom</color>!" }, MSF_TARGET },
    {0}
};

/* MST_ESCAPE */
enum {
    ESCAPE_TELE_SELF,
    ESCAPE_TELE_OTHER,
};
static _parse_t _escape_tbl[] = {
    { "TELE_OTHER", { MST_ESCAPE, ESCAPE_TELE_OTHER },
        { "Teleport Away", TERM_WHITE }, MSF_TARGET },
    { "TELE_SELF", { MST_ESCAPE, ESCAPE_TELE_SELF },
        { "Teleport", TERM_WHITE }},
    {0}
};
enum {
    TACTIC_BLINK = 1000,
    TACTIC_BLINK_OTHER,
};
static _parse_t _tactic_tbl[] = {
    { "BLINK", { MST_TACTIC, TACTIC_BLINK },
        { "Blink", TERM_WHITE }},
    { "BLINK_OTHER", { MST_TACTIC, TACTIC_BLINK_OTHER },
        { "Blink Away", TERM_WHITE }, MSF_TARGET },
    {0}
};

/* MST_HEAL */
enum {
    HEAL_SELF,
};
static _parse_t _heal_tbl[] = {
    { "HEAL", { MST_HEAL, HEAL_SELF },
        { "Heal Self", TERM_WHITE,
          "$CASTER concentrates on $CASTER_POS wounds.",
          "$CASTER mumbles.",
          "$CASTER concentrates on $CASTER_POS wounds.",
          "$CASTER concentrates on $CASTER_POS wounds." }},
    {0}
};

/* MST_WEIRD */
enum {
    WEIRD_SPECIAL, /* XXX */
};
static _parse_t _weird_tbl[] = {
    { "SPECIAL", { MST_WEIRD, WEIRD_SPECIAL },
        { "Something Weird", TERM_RED }},
    {0}
};

static _parse_ptr _spell_parse_name_aux(cptr token, _parse_ptr tbl)
{
    int i;
    for (i = 0; ; i++)
    {
        _parse_ptr info = &tbl[i];
        if (!info->token) return NULL;
        if (strcmp(info->token, token) == 0) return info;
    }
}

static _parse_ptr _spell_parse_name(cptr token)
{
    _parse_ptr tbls[] = {_annoy_tbl, _ball_tbl, _beam_tbl, _biff_tbl,
                         _bolt_tbl, _buff_tbl, _curse_tbl, _escape_tbl,
                         _heal_tbl, _tactic_tbl, _weird_tbl, NULL};
    int i;
    for (i = 0;; i++)
    {
        _parse_ptr tbl = tbls[i];
        _parse_ptr p;
        if (!tbl) return NULL;
        p = _spell_parse_name_aux(token, tbl);
        if (p) return p;
    }
}

#if 0
/* stupid annoying compiler warnings punish foresight ... sigh */
static _parse_ptr _spell_lookup(mon_spell_id_t id)
{
    int i;
    for (i = 0; ; i++)
    {
        _parse_ptr info = &_parse_tbl[i];
        if (!info->token) return NULL;
        if (info->id.type == id.type && info->id.effect == id.effect) return info;
    }
}
#endif
static bool _spell_is_(mon_spell_ptr spell, int type, int effect)
{
    return spell->id.type == type && spell->id.effect == effect;
}

typedef struct {
    int id;
    cptr name;
    byte color;
    int  prob;
} _mst_info_t, *_mst_info_ptr;
static _mst_info_t _mst_tbl[] = {
    { MST_BREATH, "Breathe", TERM_RED, 15 },
    { MST_BALL, "Ball", TERM_RED, 15 },
    { MST_BOLT, "Bolt", TERM_RED, 15 },
    { MST_BEAM, "Beam", TERM_RED, 15 },
    { MST_CURSE, "Curse", TERM_RED, 15 },
    { MST_BUFF, "Buff", TERM_L_BLUE, 5 },
    { MST_BIFF, "Biff", TERM_RED, 10 },
    { MST_ESCAPE, "Escape", TERM_L_BLUE, 5 },
    { MST_ANNOY, "Annoy", TERM_ORANGE, 5 },
    { MST_SUMMON, "Summon", TERM_ORANGE, 8 },
    { MST_HEAL, "Heal", TERM_L_BLUE, 10 },
    { MST_TACTIC, "Tactic", TERM_L_BLUE, 10 },
    { MST_WEIRD, "Weird", TERM_L_UMBER, 100 },
    { 0 }
};
static _mst_info_ptr _mst_lookup(int which)
{
    int i;
    for (i = 0;; i++)
    {
        _mst_info_ptr p = &_mst_tbl[i];
        if (!p->name) return NULL;
        if (p->id == which) return p;
    }
}

/*************************************************************************
 * ID
 ************************************************************************/
mon_spell_id_t mon_spell_id(int type, int effect)
{
    mon_spell_id_t id;
    id.type = type;
    id.effect = effect;
    return id;
}

static mon_spell_id_t _id(int type, int effect)
{
    return mon_spell_id(type, effect);
}

int mon_spell_hash(mon_spell_id_t id)
{
    int hash = id.type << 16;
    hash += id.effect;
    return hash;
}

/*************************************************************************
 * Parm
 ************************************************************************/
static dice_t _dice(int dd, int ds, int base)
{
    dice_t dice;
    dice.dd = dd;
    dice.ds = ds;
    dice.base = base;
    return dice;
}

static hp_pct_t _hp_pct(int pct, int max)
{
    hp_pct_t hp;
    hp.pct = pct;
    hp.max = max;
    return hp;
}

mon_spell_parm_t mon_spell_parm_dice(int dd, int ds, int base)
{
    mon_spell_parm_t parm;
    parm.tag = MSP_DICE;
    parm.v.dice = _dice(dd, ds, base);
    return parm;
}

mon_spell_parm_t mon_spell_parm_hp_pct(int pct, int max)
{
    mon_spell_parm_t parm;
    parm.tag = MSP_HP_PCT;
    parm.v.hp_pct = _hp_pct(pct, max);
    return parm;
}

static mon_spell_parm_t _breathe_parm(int which)
{
    mon_spell_parm_t parm = {0};
    parm.tag = MSP_HP_PCT;
    switch (which)
    {
    case GF_ACID:
    case GF_ELEC:
    case GF_FIRE:
    case GF_COLD:
        parm.v.hp_pct = _hp_pct(20, 900);
        break;
    case GF_POIS:
    case GF_NUKE:
        parm.v.hp_pct = _hp_pct(17, 600);
        break;
    case GF_NETHER:
        parm.v.hp_pct = _hp_pct(14, 550);
        break;
    case GF_LITE:
    case GF_DARK:
    case GF_CONFUSION:
        parm.v.hp_pct = _hp_pct(17, 400);
        break;
    case GF_SOUND:
        parm.v.hp_pct = _hp_pct(17, 450);
        break;
    case GF_CHAOS:
        parm.v.hp_pct = _hp_pct(17, 600);
        break;
    case GF_DISENCHANT:
    case GF_SHARDS:
        parm.v.hp_pct = _hp_pct(17, 500);
        break;
    case GF_NEXUS:
        parm.v.hp_pct = _hp_pct(33, 250);
        break;
    case GF_STORM:
        parm.v.hp_pct = _hp_pct(13, 300);
        break;
    case GF_INERT:
    case GF_PLASMA:
    case GF_HELL_FIRE:
    case GF_HOLY_FIRE:
        parm.v.hp_pct = _hp_pct(17, 200);
        break;
    case GF_GRAVITY:
    case GF_FORCE:
        parm.v.hp_pct = _hp_pct(33, 200);
        break;
    case GF_MANA:
        parm.v.hp_pct = _hp_pct(33, 250);
        break;
    case GF_DISINTEGRATE:
        parm.v.hp_pct = _hp_pct(17, 150);
        break;
    case GF_TIME:
        parm.v.hp_pct = _hp_pct(33, 150);
        break;
    default:
        /*assert(FALSE);*/
        msg_format("Unsupported breathe %s (%d)", gf_name(which), which);
    }
    return parm;
}

static mon_spell_parm_t _ball_parm(int which, int rlev)
{
    mon_spell_parm_t parm = {0};
    parm.tag = MSP_DICE;
    switch (which)
    {
    case GF_ACID:
        parm.v.dice = _dice(1, 3*rlev, 15);
        break;
    case GF_ELEC:
        parm.v.dice = _dice(1, 3*rlev/2, 8);
        break;
    case GF_FIRE:
        parm.v.dice = _dice(1, 7*rlev/2, 10);
        break;
    case GF_COLD:
        parm.v.dice = _dice(1, 3*rlev/2, 10);
        break;
    case GF_POIS:
        parm.v.dice = _dice(12, 2, 0);
        break;
    case GF_NUKE:
        parm.v.dice = _dice(10, 6, rlev);
        break;
    case GF_NETHER:
        parm.v.dice = _dice(10, 10, 50 + rlev);
        break;
    case GF_DARK:
    case GF_LITE:
    case GF_MANA:
        parm.v.dice = _dice(10, 10, 50 + 4*rlev);
        break;
    case GF_CHAOS:
        parm.v.dice = _dice(10, 10, rlev);
        break;
    case GF_WATER:
        parm.v.dice = _dice(1, rlev, 50);
        break;
    case GF_DRAIN_MANA:
        parm.v.dice = _dice(0, 0, 1 + rlev/2);
        break;
    case GF_MIND_BLAST:
        parm.v.dice = _dice(7, 7, 0);
        break;
    case GF_BRAIN_SMASH:
        parm.v.dice = _dice(12, 12, 0);
        break;
    case GF_TELEKINESIS:
        parm.v.dice = _dice(8, 8, 0);
        break;
    case GF_ROCK:
        parm.v.dice = _dice(0, 0, 3*rlev);
        break;
    case GF_ROCKET:
        parm.v.dice = _dice(0, 0, 6*rlev);
        break;
    default:
        parm.v.dice = _dice(5, 5, rlev);
    }
    return parm;
}

static mon_spell_parm_t _bolt_parm(int which, int rlev)
{
    mon_spell_parm_t parm = {0};
    parm.tag = MSP_DICE;
    switch (which)
    {
    case GF_ACID:
        parm.v.dice = _dice(7, 8, rlev/3);
        break;
    case GF_ELEC:
        parm.v.dice = _dice(4, 8, rlev/3);
        break;
    case GF_FIRE:
        parm.v.dice = _dice(9, 8, rlev/3);
        break;
    case GF_COLD:
        parm.v.dice = _dice(6, 8, rlev/3);
        break;
    case GF_ICE:
        parm.v.dice = _dice(6, 8, rlev);
        break;
    case GF_NETHER:
        parm.v.dice = _dice(5, 5, 30 + rlev);
        break;
    case GF_WATER:
        parm.v.dice = _dice(10, 10, rlev);
        break;
    case GF_PLASMA:
        parm.v.dice = _dice(8, 7, 10 + rlev);
        break;
    case GF_MANA:
        parm.v.dice = _dice(1, 7*rlev/2, 50);
        break;
    case GF_MISSILE:
        parm.v.dice = _dice(2, 6, rlev/3);
    case GF_ATTACK:
    case GF_ARROW:
        /* SHOOT always specifies dice overrides */
        break;
    default:
        assert(FALSE);
    }
    return parm;
}

static mon_spell_parm_t _beam_parm(int which, int rlev)
{
    mon_spell_parm_t parm = {0};
    parm.tag = MSP_DICE;
    switch (which)
    {
    case GF_PSY_SPEAR:
        parm.v.dice = _dice(1, rlev*3/2, 100);
        break;
    case GF_HELL_FIRE:
    case GF_HOLY_FIRE:
        parm.v.dice = _dice(0, 0, 2*rlev);
        break;
    default:
        assert(FALSE);
    }
    return parm;
}

static mon_spell_parm_t _curse_parm(int which)
{
    mon_spell_parm_t parm = {0};
    parm.tag = MSP_DICE;
    switch (which)
    {
    case GF_CAUSE_1:
        parm.v.dice = _dice(3, 8, 0);
        break;
    case GF_CAUSE_2:
        parm.v.dice = _dice(8, 8, 0);
        break;
    case GF_CAUSE_3:
        parm.v.dice = _dice(10, 15, 0);
        break;
    case GF_CAUSE_4:
        parm.v.dice = _dice(15, 15, 0);
        break;
    case GF_HAND_DOOM:
        parm.v.dice = _dice(1, 20, 40); /* This is percentage of chp! */
        break;
    default:
        assert(FALSE);
    }
    return parm;
}

static mon_spell_parm_t _heal_parm(int which, int rlev)
{
    mon_spell_parm_t parm = {0};
    parm.tag = MSP_DICE;
    switch (which)
    {
    case HEAL_SELF:
        parm.v.dice = _dice(0, 0, rlev*6);
        break;
    default:
        assert(FALSE);
    }
    return parm;
}

static mon_spell_parm_t _summon_parm(int which)
{
    mon_spell_parm_t parm = {0};
    parm.tag = MSP_DICE;
    switch (which)
    {
    case SUMMON_CYBER:
        parm.v.dice = _dice(1, 3, 0);
        break;
    default: /* XXX */
        parm.v.dice = _dice(1, 3, 1);
    }
    return parm;
}

static mon_spell_parm_t _tactic_parm(int which, int rlev)
{
    mon_spell_parm_t parm = {0};
    if (which >= TACTIC_BLINK) return parm;
    parm.tag = MSP_DICE;
    parm.v.dice = _dice(0, 0, rlev);
    return parm;
}

mon_spell_parm_t mon_spell_parm_default(mon_spell_id_t id, int rlev)
{
    mon_spell_parm_t empty = {0};

    switch (id.type)
    {
    case MST_BREATH:
        return _breathe_parm(id.effect);
    case MST_BALL:
        return _ball_parm(id.effect, rlev);
    case MST_BOLT:
        return _bolt_parm(id.effect, rlev);
    case MST_BEAM:
        return _beam_parm(id.effect, rlev);
    case MST_CURSE:
        return _curse_parm(id.effect);
    case MST_HEAL:
        return _heal_parm(id.effect, rlev);
    case MST_SUMMON:
        return _summon_parm(id.effect);
    case MST_TACTIC:
        return _tactic_parm(id.effect, rlev);
    }

    return empty;
}

errr mon_spell_parm_parse(mon_spell_parm_ptr parm, char *token)
{
    char arg[100], sentinel = '~', check;
    int  dd, ds, base, pct;

    sprintf(arg, "%s%c", token, sentinel);

    /* Note: The parser will default parm with mon_spell_parm_default(id),
     * but we allow the user to override. This means that BR_FOO should
     * already be set with the default max, which is good since we don't
     * currently support syntax for overriding it. */
    if (2 == sscanf(arg, "%d%%%c", &pct, &check) && check == sentinel)
    {
        if (parm->tag != MSP_HP_PCT)
        {
            msg_print("Error: A hitpoint percentage is only valid on BR_* spells.");
            return PARSE_ERROR_GENERIC;
        }
        parm->v.hp_pct.pct = MAX(0, MIN(100, pct));
    }
    else if (4 == sscanf(arg, "%dd%d+%d%c", &dd, &ds, &base, &check) && check == sentinel)
    {
        if (parm->tag != MSP_DICE)
        {
            msg_print("Error: Dice parameters are not supported on this spell type.");
            return PARSE_ERROR_GENERIC;
        }
        parm->v.dice.dd = MAX(0, dd);
        parm->v.dice.ds = MAX(0, ds);
        parm->v.dice.base = base;
    }
    else if (3 == sscanf(arg, "%dd%d%c", &dd, &ds, &check) && check == sentinel)
    {
        if (parm->tag != MSP_DICE)
        {
            msg_print("Error: Dice parameters are not supported on this spell type.");
            return PARSE_ERROR_GENERIC;
        }
        parm->v.dice.dd = MAX(0, dd);
        parm->v.dice.ds = MAX(0, ds);
        parm->v.dice.base = 0;
    }
    else if (2 == sscanf(arg, "%d%c", &base, &check) && check == sentinel)
    {
        if (parm->tag != MSP_DICE)
        {
            msg_print("Error: Dice parameters are not supported on this spell type.");
            return PARSE_ERROR_GENERIC;
        }
        parm->v.dice.dd = 0;
        parm->v.dice.ds = 0;
        parm->v.dice.base = base;
    }
    else
    {
        msg_format("Error: Unknown argument %s.", token);
        return PARSE_ERROR_GENERIC;
    }
    return 0;
}

static int _avg_dam_roll(int dd, int ds) { return dd * (ds + 1) / 2; }
static int _avg_hp(mon_race_ptr r)
{
    if (r->flags1 & RF1_FORCE_MAXHP)
        return r->hdice * r->hside;
    return _avg_dam_roll(r->hdice, r->hside);
}
void mon_spell_parm_print(mon_spell_parm_ptr parm, string_ptr s, mon_race_ptr race)
{
    if (parm->tag == MSP_DICE)
    {
        if (parm->v.dice.dd && parm->v.dice.ds)
        {
            string_printf(s, "%dd%d", parm->v.dice.dd, parm->v.dice.ds);
            if (parm->v.dice.base)
                string_append_c(s, '+');
        }
        if (parm->v.dice.base)
            string_printf(s, "%d", parm->v.dice.base);
    }
    else if (parm->tag == MSP_HP_PCT)
    {
        if (race)
        {
            int hp = _avg_hp(race);
            int dam = hp * parm->v.hp_pct.pct / 100;
            if (dam > parm->v.hp_pct.max)
                dam = parm->v.hp_pct.max;
            string_printf(s, "%d", dam);
        }
        else
            string_printf(s, "%d%% up to %d", parm->v.hp_pct.pct, parm->v.hp_pct.max);
    }
}

/*************************************************************************
 * Spell
 ************************************************************************/
/* BA_MANA or BR_FIRE(30%) or BO_FIRE(3d5+10) or HEAL(120) ... */
errr mon_spell_parse(mon_spell_ptr spell, int rlev, char *token)
{
    int           i;
    char         *name;
    char         *args[10];
    int           arg_ct = parse_args(token, &name, args, 10);
    _parse_ptr    p;

    if (arg_ct < 0)
    {
        msg_format("Error: Malformed argument %s. Missing )?", name);
        return PARSE_ERROR_GENERIC;
    }

    p = _spell_parse_name(name);
    if (p)
    {
        spell->id = p->id;
        spell->display = &p->display;
        spell->flags = p->flags;
    }
    else if (prefix(name, "S_"))
    {
        parse_tbl_ptr p = summon_type_parse(name + 2);
        if (!p)
        {
            msg_format("Error: Unknown summon type %s.", name + 2);
            return PARSE_ERROR_GENERIC;
        }
        spell->id.type = MST_SUMMON;
        spell->id.effect = p->id;
    }
    else
    {
        gf_info_ptr  gf;
        cptr         suffix;
        if (prefix(name, "BR_"))
        {
            spell->id.type = MST_BREATH;
            spell->flags |= MSF_INNATE | MSF_TARGET;
            suffix = name + 3;
        }
        else if (prefix(name, "BA_"))
        {
            spell->id.type = MST_BALL;
            spell->flags |= MSF_TARGET;
            suffix = name + 3;
        }
        else if (prefix(name, "BO_"))
        {
            spell->id.type = MST_BOLT;
            spell->flags |= MSF_TARGET;
            suffix = name + 3;
        }
        else if (prefix(name, "JMP_"))
        {
            spell->id.type = MST_TACTIC;
            suffix = name + 4;
        }
        else
        {
            msg_format("Error: Unkown spell %s.", name);
            return PARSE_ERROR_GENERIC;
        }
        gf = gf_parse_name(suffix);
        if (!gf)
        {
            msg_format("Error: Unkown type %s.", name + 3);
            return PARSE_ERROR_GENERIC;
        }
        spell->id.effect = gf->id;
    }

    spell->parm = mon_spell_parm_default(spell->id, rlev);
    for (i = 0; i < arg_ct; i++) /* XXX should only be 1 at the moment */
    {
        errr rc = mon_spell_parm_parse(&spell->parm, args[i]);
        if (rc) return rc;
    }
    return 0;
}

void mon_spell_print(mon_spell_ptr spell, string_ptr s)
{
    if (spell->display)
    {
        string_printf(s, "<color:%c>%s</color>",
            attr_to_attr_char(spell->display->color), spell->display->name);
    }
    else if (spell->id.type == MST_SUMMON)
    {
        parse_tbl_ptr p = summon_type_lookup(spell->id.effect);
        if (!p)
            string_printf(s, "Summon %d", spell->id.effect);
        else
            string_printf(s, "<color:%c>Summon %s</color>", attr_to_attr_char(p->color), p->name);
    }
    else if (spell->id.type == MST_BREATH)
    {
        gf_info_ptr gf = gf_lookup(spell->id.effect);
        if (gf)
        {
            string_printf(s, "<color:%c>Breathe %s</color>",
                attr_to_attr_char(gf->color), gf->name);
        }
        else
            string_printf(s, "Breathe %d", spell->id.effect);
    }
    else if (spell->id.type == MST_TACTIC) /* XXX BLINK and BLINK_OTHER have spell->display set */
    {
        gf_info_ptr gf = gf_lookup(spell->id.effect);
        if (gf)
        {
            string_printf(s, "<color:%c>%s Jump</color>",
                attr_to_attr_char(gf->color), gf->name);
        }
        else
            string_printf(s, "%d Jump", spell->id.effect);
    }
    else
    {
        gf_info_ptr   gf = gf_lookup(spell->id.effect);
        _mst_info_ptr mst = _mst_lookup(spell->id.type);
        assert(mst);
        if (gf)
        {
            string_printf(s, "<color:%c>%s %s</color>",
                attr_to_attr_char(gf->color), gf->name, mst->name);
        }
        else
            string_printf(s, "%s %d", mst->name, spell->id.effect);
    }
}
void mon_spell_display(mon_spell_ptr spell, string_ptr s)
{
    if (spell->id.type == MST_BREATH)
    {
        gf_info_ptr gf = gf_lookup(spell->id.effect);
        if (gf)
        {
            string_printf(s, "<color:%c>%s</color>",
                attr_to_attr_char(gf->color), gf->name);
        }
        else
            string_printf(s, "Unknown %d", spell->id.effect);
    }
    else if (spell->id.type == MST_SUMMON)
    {
        parse_tbl_ptr p = summon_type_lookup(spell->id.effect);
        if (p)
            string_printf(s, "<color:%c>%s</color>", attr_to_attr_char(p->color), p->name);
        else
            string_printf(s, "Unknown %d", spell->id.effect);
    }
    else
        mon_spell_print(spell, s);
}

void mon_spell_doc(mon_spell_ptr spell, doc_ptr doc)
{
    string_ptr s = string_alloc();
    mon_spell_print(spell, s);
    doc_insert(doc, string_buffer(s));
    string_free(s);
}

/*************************************************************************
 * Spell Group
 ************************************************************************/
mon_spell_group_ptr mon_spell_group_alloc(void)
{
    mon_spell_group_ptr group = malloc(sizeof(mon_spell_group_t));
    memset(group, 0, sizeof(mon_spell_group_t));
    return group;
}

void mon_spell_group_free(mon_spell_group_ptr group)
{
    if (group)
    {
        if (group->spells)
            free(group->spells);
        free(group);
    }
}

static void _group_grow(mon_spell_group_ptr group)
{
    if (!group->allocated)
    {
        group->allocated = 1;
        group->spells = malloc(sizeof(mon_spell_t)*group->allocated);
    }
    else
    {
        group->allocated *= 2;
        group->spells = realloc(group->spells, sizeof(mon_spell_t)*group->allocated);
        assert(group->spells);
    }
}

void mon_spell_group_add(mon_spell_group_ptr group, mon_spell_ptr spell)
{
    assert(spell);
    if (group->count == group->allocated)
        _group_grow(group);
    assert(group->count < group->allocated);
    group->spells[group->count++] = *spell;
}

mon_spell_ptr mon_spell_group_find(mon_spell_group_ptr group, mon_spell_id_t id)
{
    int i;
    for (i = 0; i < group->count; i++)
    {
        mon_spell_ptr spell = &group->spells[i];
        if (spell->id.type == id.type && spell->id.effect == id.effect)
            return spell;
    }
    return NULL;
}

/*************************************************************************
 * Spells
 ************************************************************************/
mon_spells_ptr mon_spells_alloc(void)
{
    mon_spells_ptr spells = malloc(sizeof(mon_spells_t));
    memset(spells, 0, sizeof(mon_spells_t));
    return spells;
}

void mon_spells_free(mon_spells_ptr spells)
{
    if (spells)
    {
        int i;
        for (i = 0; i < MST_COUNT; i++)
        {
            mon_spell_group_ptr group = spells->groups[i];
            if (group)
                mon_spell_group_free(group);
            spells->groups[i] = NULL;
        }
        free(spells);
    }
}

void mon_spells_add(mon_spells_ptr spells, mon_spell_ptr spell)
{
    mon_spell_group_ptr group = spells->groups[spell->id.type];
    if (!group)
    {
        group = mon_spell_group_alloc();
        group->type = spell->id.type;
        spells->groups[spell->id.type] = group;
    }
    mon_spell_group_add(group, spell);
}

errr mon_spells_parse(mon_spells_ptr spells, int rlev, char *token)
{
    mon_spell_t spell = {0};
    errr        rc = mon_spell_parse(&spell, rlev, token);

    if (rc == 0)
        mon_spells_add(spells, &spell);

    return rc;
}

vec_ptr mon_spells_all(mon_spells_ptr spells)
{
    vec_ptr v = vec_alloc(NULL);
    int     i, j;
    for (i = 0; i < MST_COUNT; i++)
    {
        mon_spell_group_ptr group = spells->groups[i];
        if (!group) continue;
        for (j = 0; j < group->count; j++)
        {
            mon_spell_ptr spell = &group->spells[j];
            vec_add(v, spell);
        }
    }
    return v;
}

mon_spell_ptr mon_spells_find(mon_spells_ptr spells, mon_spell_id_t id)
{
    int i;
    mon_spell_group_ptr group = spells->groups[id.type];
    if (!group) return NULL;
    for (i = 0; i < group->count; i++)
    {
        mon_spell_ptr spell = &group->spells[i];
        if (spell->id.effect != id.effect) continue;
        return spell;
    }
    return NULL;
}

mon_spell_ptr mon_spells_random(mon_spells_ptr spells)
{
    vec_ptr v = mon_spells_all(spells);
    mon_spell_ptr spell = NULL;
    if (vec_length(v))
    {
        int i = randint0(vec_length(v));
        spell = vec_get(v, i);
    }
    vec_free(v);
    return spell;
}

/*************************************************************************
 * Cast
 ************************************************************************/
mon_spell_cast_t _current = {0};

mon_spell_ptr mon_spell_current(void)
{
    return _current.spell;
}
mon_ptr mon_current(void)
{
    return _current.mon;
}

static void _spell_cast_aux(void);
static bool _default_ai(mon_spell_cast_ptr cast);
static bool _default_ai_mon(mon_spell_cast_ptr cast);

static void _mon_desc(mon_ptr mon, char *buf, char color)
{
    char tmp[MAX_NLEN];
    monster_desc(tmp, mon, 0);
    tmp[0] = toupper(tmp[0]);
    sprintf(buf, "<color:%c>%s</color>", color, tmp);
}
static mon_race_ptr _race(mon_ptr mon)
{
    if (!mon) return NULL;
    return &r_info[mon->ap_r_idx];
}
static void _spell_cast_init(mon_spell_cast_ptr cast, mon_ptr mon)
{
    cast->mon = mon;
    cast->race = _race(mon);
    cast->spell = NULL;
    cast->src = point(mon->fx, mon->fy);
    cast->dest = point(px, py);
    _mon_desc(mon, cast->name, 'G'); 
    cast->flags = MSC_SRC_MONSTER | MSC_DEST_PLAYER;
}

static void _spell_cast_init_mon(mon_spell_cast_ptr cast, mon_ptr mon)
{
    cast->mon = mon;
    cast->race = _race(mon);
    cast->spell = NULL;
    cast->src = point(mon->fx, mon->fy);
    _mon_desc(mon, cast->name, 'G'); 
    cast->flags = MSC_SRC_MONSTER | MSC_DEST_MONSTER;
}

static bool _can_cast(mon_ptr mon)
{
    if (MON_CONFUSED(mon))
    {
        reset_target(mon);
        return FALSE;
    }
    if (!is_hostile(mon)) return FALSE;
    if (mon->mflag & MFLAG_NICE) return FALSE;
    if (!is_aware(mon)) return FALSE;
    if (!p_ptr->playing || p_ptr->is_dead) return FALSE;
    if (p_ptr->leaving) return FALSE;

    return TRUE;
}

bool mon_spell_cast(mon_ptr mon, mon_spell_ai ai)
{
    mon_spell_cast_t cast = {0};

    if (!_can_cast(mon)) return FALSE;
    if (mon->cdis > MAX_RANGE && !mon->target_y) return FALSE;

    if (!ai) ai = _default_ai;

    _spell_cast_init(&cast, mon);
    if (ai(&cast))
    {
        _current = cast;
        _spell_cast_aux();
        memset(&_current, 0, sizeof(mon_spell_cast_t));
        return TRUE;
    }
    return FALSE;
}

bool mon_spell_cast_mon(mon_ptr mon, mon_spell_ai ai)
{
    mon_spell_cast_t cast = {0};

    /* XXX This causes problems inside_battle:
     * if (!_can_cast(mon)) return FALSE; */
    if (MON_CONFUSED(mon))
    {
        reset_target(mon);
        return FALSE;
    }

    if (!ai) ai = _default_ai_mon;

    _spell_cast_init_mon(&cast, mon);
    if (ai(&cast))
    {
        _current = cast;
        if (_current.flags & MSC_UNVIEW)
            mon_fight = TRUE;
        _spell_cast_aux();
        memset(&_current, 0, sizeof(mon_spell_cast_t));
        return TRUE;
    }
    return FALSE;
}

static bool _spell_fail(void)
{
    int fail;

    if (_current.spell->flags & MSF_INNATE)
        return FALSE;
    if (_current.race->flags2 & RF2_STUPID)
        return FALSE;
    if (py_in_dungeon() && (d_info[dungeon_type].flags1 & DF1_NO_MAGIC))
        return TRUE;

    fail = 25 - (_current.race->level + 3)/4;
    if (MON_STUNNED(_current.mon))
        fail += 50 * MIN(100, MON_STUNNED(_current.mon))/100;

    if (fail && randint0(100) < fail)
    {
        mon_lore_aux_spell(_current.race);
        msg_format("%^s tries to cast a spell, but fails.", _current.name);
        return TRUE;
    }
    return FALSE;
}

static bool _spell_blocked(void)
{
    if (_current.spell->flags & MSF_INNATE)
        return FALSE;
    if (magic_barrier_aux(_current.mon))
    {
        msg_format("Your anti-magic barrier blocks the spell which %^s casts.", _current.name);
        return (TRUE);
    }

    if (psion_check_disruption_aux(_current.mon))
    {
        msg_format("Your psionic disruption blocks the spell which %^s casts.", _current.name);
        return TRUE;
    }
    return FALSE;
}

static void _breathe(void)
{
    int dam;
    int pct = _current.spell->parm.v.hp_pct.pct;
    int max = _current.spell->parm.v.hp_pct.max;

    assert(_current.spell->parm.tag == MSP_HP_PCT);

    dam = _current.mon->hp * pct / 100;
    if (dam > max) dam = max;

    project(
        _current.mon->id,
        _current.race->level >= 50 ? -3 : -2,
        _current.dest.y,
        _current.dest.x,
        dam,
        _current.spell->id.effect,
        PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAYER,
        -1
    );
}

static int _roll(dice_t dice)
{
    int roll = dice.base;
    if (dice.dd && dice.ds)
        roll += damroll(dice.dd, dice.ds);
    return roll;
}
static int _avg_roll(dice_t dice)
{
    int roll = dice.base;
    if (dice.dd && dice.ds)
        roll += dice.dd * (dice.ds + 1)/2;
    return roll;
}
static void _ball(void)
{
    int    dam;
    dice_t dice = _current.spell->parm.v.dice;
    int    rad = 0;
    int    flags = PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAYER;

    assert(_current.spell->parm.tag == MSP_DICE);
    dam = _roll(dice);
    if (!(_current.spell->flags & MSF_BALL0))
    {
        /* XXX This was previously set on a spell by spell basis ... */
        if (dam > 300) rad = 4;
        else if (dam > 150) rad = 3;
        else rad = 2;
    }

    switch (_current.spell->id.effect)
    {
    case GF_ROCKET:
        flags |= PROJECT_STOP;
        break;
    case GF_DRAIN_MANA:
    case GF_MIND_BLAST:
    case GF_BRAIN_SMASH:
        flags |= PROJECT_HIDE | PROJECT_AIMED;
        break;
    }

    project(
        _current.mon->id,
        rad,
        _current.dest.y,
        _current.dest.x,
        dam,
        _current.spell->id.effect,
        flags,
        -1
    );
}
static void _bolt(void)
{
    int ct = 1, i;
    int flags = PROJECT_STOP | PROJECT_KILL | PROJECT_PLAYER | PROJECT_REFLECTABLE;
    assert(_current.spell->parm.tag == MSP_DICE);
    if (_current.race->id == MON_ARTEMIS && _spell_is_(_current.spell, MST_BOLT, GF_ARROW))
    {
        ct = 4;
        flags &= ~PROJECT_REFLECTABLE;
    }
    if (_current.spell->id.effect == GF_ATTACK)
        flags |= PROJECT_HIDE;
    for (i = 0; i < ct; i++)
    {
        project(
            _current.mon->id,
            0,
            _current.dest.y,
            _current.dest.x,
            _roll(_current.spell->parm.v.dice),
            _current.spell->id.effect,
            flags,
            -1
        );
    }
}
static void _beam(void)
{
    assert(_current.spell->parm.tag == MSP_DICE);
    project(
        _current.mon->id,
        0,
        _current.dest.y,
        _current.dest.x,
        _roll(_current.spell->parm.v.dice),
        _current.spell->id.effect,
        PROJECT_BEAM | PROJECT_KILL | PROJECT_THRU | PROJECT_PLAYER,
        -1
    );
}
static void _curse(void)
{
    int dam;
    assert(_current.spell->parm.tag == MSP_DICE);
    dam = _roll(_current.spell->parm.v.dice);
    if (_current.spell->id.effect == GF_HAND_DOOM)
        dam = dam * p_ptr->chp / 100;
    project(
        _current.mon->id,
        0,
        _current.dest.y,
        _current.dest.x,
        dam,
        _current.spell->id.effect,
        PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL |
        PROJECT_PLAYER | PROJECT_HIDE | PROJECT_AIMED,
        -1
    );
}
static int _curse_save_odds(void)
{
    int roll = 100 + _current.race->level/2;
    int sav = duelist_skill_sav(_current.mon->id);
    int odds = sav * 100 / roll;
    return odds;
}
static bool _curse_save(void)
{
    int odds = _curse_save_odds();
    return randint0(100) < odds;
}
static bool _projectable(point_t src, point_t dest);
static void _annoy(void)
{
    switch (_current.spell->id.effect)
    {
    case ANNOY_AMNESIA:
        gf_damage_p(_current.mon->id, GF_AMNESIA, 0, GF_DAMAGE_SPELL);
        break;
    case ANNOY_ANIMATE_DEAD:
        animate_dead(_current.mon->id, _current.mon->fy, _current.mon->fx);
        break;
    case ANNOY_BLIND:
        if (res_save_default(RES_BLIND) || _curse_save())
            msg_print("You resist the effects!");
        else
            set_blind(12 + randint0(4), FALSE);
        update_smart_learn(_current.mon->id, RES_BLIND);
        break;
    case ANNOY_CONFUSE:
        if (res_save_default(RES_CONF) || _curse_save())
            msg_print("You disbelieve the feeble spell.");
        else
            set_confused(p_ptr->confused + randint0(4) + 4, FALSE);
        update_smart_learn(_current.mon->id, RES_CONF);
        break;
    case ANNOY_DARKNESS:
        if (p_ptr->blind)
            msg_format("%s mumbles.", _current.name);

        if ( p_ptr->pclass == CLASS_NINJA
          && !(_current.race->flags3 & (RF3_UNDEAD | RF3_HURT_LITE))
          && !(_current.race->flags7 & RF7_DARK_MASK) )
        {
            if (!p_ptr->blind)
                msg_format("%s cast a spell to light up.", _current.name);
            lite_area(0, 3);
        }
        else
        {
            if (!p_ptr->blind)
                msg_format("%s gestures in shadow.", _current.name);
            unlite_area(0, 3);
        }
        break;
    case ANNOY_PARALYZE:
        gf_damage_p(_current.mon->id, GF_PARALYSIS, 0, GF_DAMAGE_SPELL);
        break;
    case ANNOY_SCARE:
        fear_scare_p(_current.mon);
        update_smart_learn(_current.mon->id, RES_FEAR);
        break;
    case ANNOY_SHRIEK:
        aggravate_monsters(_current.mon->id);
        break;
    case ANNOY_SLOW:
        if (p_ptr->free_act)
        {
            msg_print("You are unaffected!");
            equip_learn_flag(OF_FREE_ACT);
        }
        else if (_curse_save())
            msg_print("You resist the effects!");
        else
            set_slow(p_ptr->slow + randint0(4) + 4, FALSE);
        update_smart_learn(_current.mon->id, SM_FREE_ACTION);
        break;
    case ANNOY_TELE_LEVEL:
        if (res_save_default(RES_NEXUS) || _curse_save())
            msg_print("You resist the effects!");
        else
            teleport_level(0);
        update_smart_learn(_current.mon->id, RES_NEXUS);
        break;
    case ANNOY_TELE_TO:
        /* Only powerful monsters can choose this spell when the player is not in
           los. In this case, it is nasty enough to warrant a saving throw. */
        if (!_projectable(_current.src, _current.dest) && _curse_save())
            msg_print("You resist the effects!");
        else if (res_save_default(RES_TELEPORT))
            msg_print("You resist the effects!");
        else
            teleport_player_to(_current.src.y, _current.src.x, TELEPORT_PASSIVE);
        update_smart_learn(_current.mon->id, RES_TELEPORT);
        break;
    case ANNOY_TRAPS:
        trap_creation(_current.dest.y, _current.dest.x);
        break;
    case ANNOY_WORLD: {
        int who = 0;
        if (_current.mon->id == MON_DIO) who = 1; /* XXX Seriously?! */
        else if (_current.mon->id == MON_WONG) who = 3;
        process_the_world(randint1(2)+2, who, TRUE);
        break; }
    }
    /* XXX this sort of stuff needs to be a class hook ... */
    if (p_ptr->tim_spell_reaction && !p_ptr->fast)
        set_fast(4, FALSE);
}
static void _biff(void)
{
    if (check_foresight()) return;
    switch (_current.spell->id.effect)
    {
    case BIFF_ANTI_MAGIC:
        if (_curse_save())
            msg_print("You resist the effects!");
        else if (mut_present(MUT_ONE_WITH_MAGIC) && one_in_(2))
            msg_print("You resist the effects!");
        else if (psion_mental_fortress())
            msg_print("Your mental fortress is impenetrable!");
        else
            set_tim_no_spells(p_ptr->tim_no_spells + 3 + randint1(3), FALSE);
        break;
    case BIFF_DISPEL_MAGIC:
        if (mut_present(MUT_ONE_WITH_MAGIC) && one_in_(2))
            msg_print("You resist the effects!");
        else if (psion_mental_fortress())
            msg_print("Your mental fortress is impenetrable!");
        else
        {
            dispel_player();
            if (p_ptr->riding)
                dispel_monster_status(p_ptr->riding);
        }
        break;
    case BIFF_POLYMORPH:
        gf_damage_p(_current.mon->id, GF_OLD_POLY, 0, GF_DAMAGE_SPELL);
        break;
    }
}
static void _buff(void)
{
    switch (_current.spell->id.effect)
    {
    case BUFF_HASTE:
        if (set_monster_fast(_current.mon->id, MON_FAST(_current.mon) + 100))
            msg_format("%s starts moving faster.", _current.name);
        break;
    case BUFF_INVULN:
        if (!MON_INVULNER(_current.mon))
            set_monster_invulner(_current.mon->id, randint1(4) + 4, FALSE);
        break;
    }
}
static void _escape(void)
{
    switch (_current.spell->id.effect)
    {
    case ESCAPE_TELE_SELF:
        if (teleport_barrier(_current.mon->id))
            msg_format("Magic barrier obstructs teleporting of %s.", _current.name);
        else
        {
            if (!p_ptr->blind && _current.mon->ml)
                msg_format("%s teleports away.", _current.name);
            teleport_away_followable(_current.mon->id);
        }
        break;
    case ESCAPE_TELE_OTHER:
        /* Duelist Unending Pursuit */
        if ( p_ptr->pclass == CLASS_DUELIST
          && p_ptr->duelist_target_idx == _current.mon->id
          && p_ptr->lev >= 30 )
        {
            if (get_check(format("%^s is attempting to teleport you. Prevent? ", _current.name)))
            {
                if (one_in_(3))
                    msg_print("Failed!");
                else
                {
                    msg_print("You invoke Unending Pursuit ... The duel continues!");
                    break;
                }
            }
        }

        msg_format("%s teleports you away.", _current.name);
        if (res_save_default(RES_TELEPORT))
            msg_print("You resist the effects!");
        else
            teleport_player_away(_current.mon->id, 100);
        update_smart_learn(_current.mon->id, RES_TELEPORT);
        break;
    }
}
static void _tactic(void)
{
    switch (_current.spell->id.effect)
    {
    case TACTIC_BLINK:
        if (teleport_barrier(_current.mon->id))
            msg_format("Magic barrier obstructs teleporting of %s.", _current.name);
        else
        {
            if (!p_ptr->blind && _current.mon->ml)
                msg_format("%s blinks away.", _current.name);
            teleport_away(_current.mon->id, 10, 0);
            p_ptr->update |= PU_MONSTERS;
        }
        break;
    case TACTIC_BLINK_OTHER:
        msg_format("%s blinks you away.", _current.name);
        if (res_save_default(RES_TELEPORT))
            msg_print("You resist the effects!");
        else
            teleport_player_away(_current.mon->id, 10);
        update_smart_learn(_current.mon->id, RES_TELEPORT);
        break;
    default: /* JMP_<type> */
        assert(_current.spell->parm.tag == MSP_DICE);
        project( _current.mon->id, 5, _current.src.y, _current.src.x,
            _roll(_current.spell->parm.v.dice) * 2,
            _current.spell->id.effect,
            PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAYER, -1);
        teleport_away(_current.mon->id, 10, 0); 
        p_ptr->update |= PU_MONSTERS;
        break;
    }
}

static void hp_mon(mon_ptr mon, int amt) /* this should be public */
{
    mon->hp += amt;
    if (mon->hp >= mon->maxhp)
    {
        mon->hp = mon->maxhp;
        if (!p_ptr->blind && mon->ml)
            msg_format("%s looks completely healed!", _current.name); /* XXX */
        else if (!(_current.flags & MSC_UNVIEW))
            msg_format("%s sounds healed!", _current.name); /* XXX */
    }
    else
    {
        if (!p_ptr->blind && mon->ml)
            msg_format("%s looks healther.", _current.name); /* XXX */
        else if (!(_current.flags & MSC_UNVIEW))
            msg_format("%s looks sounds healthier.", _current.name); /* XXX */
    }
    check_mon_health_redraw(mon->id);
    if (MON_MONFEAR(mon))
    {
        set_monster_monfear(mon->id, 0);
        if (!p_ptr->blind && mon->ml)
            msg_format("%s recovers its courage.", _current.name); /* XXX */
    }
}
static void _heal(void)
{
    int amt;
    assert(_current.spell->parm.tag == MSP_DICE);
    amt = _roll(_current.spell->parm.v.dice);
    hp_mon(_current.mon, amt);
}
static void _summon_r_idx(int r_idx)
{
    int who = SUMMON_WHO_NOBODY;
    if (_current.flags & MSC_SRC_PLAYER)
        who = SUMMON_WHO_PLAYER;
    else if (_current.mon)
        who = _current.mon->id;  /* Banor=Rupart deletes himself when splitting */
    summon_named_creature(
        who,
        _current.dest.y,
        _current.dest.x,
        r_idx,
        PM_ALLOW_GROUP | PM_ALLOW_UNIQUE
    );
}
static void _summon_type(int type)
{
    int who = SUMMON_WHO_NOBODY;
    if (_current.flags & MSC_SRC_PLAYER)
        who = SUMMON_WHO_PLAYER;
    else if (_current.mon)
        who = _current.mon->id;
    summon_specific(
        who,
        _current.dest.y,
        _current.dest.x,
        _current.race->level,
        type,
        PM_ALLOW_GROUP | PM_ALLOW_UNIQUE
    );
}
/* XXX Vanilla has a 'friends' concept ... perhaps we could do likewise? */
static void _summon_special(void)
{
    int num = randint1(4);
    int r_idx = 0, r_idx2 = 0, i;
    switch (_current.race->id)
    {
    case MON_SANTACLAUS:
        msg_format("%s says 'Now Dasher! Now Dancer! Now, Prancer and Vixen! On, Comet! On, Cupid! On, Donner and Blitzen!'", _current.name);
        r_idx = MON_REINDEER;
        break;
     case MON_ZEUS:
        msg_format("%s summons Shamblers!", _current.name);
        r_idx = MON_SHAMBLER;
        break;
    case MON_POSEIDON:
        fire_ball_hide(GF_WATER_FLOW, 0, 3, 8);
        msg_format("%s summons Greater Kraken!", _current.name);
        r_idx = MON_GREATER_KRAKEN;
        break;
    case MON_HADES:
        num = randint1(2);
        fire_ball_hide(GF_LAVA_FLOW, 0, 3, 8);
        msg_format("%s summons Death!", _current.name);
        r_idx = MON_GREATER_BALROG;
        r_idx2 = MON_ARCHLICH;
        break;
    case MON_ATHENA:
        msg_format("%s summons friends!", _current.name);
        if (one_in_(3) && r_info[MON_ZEUS].max_num == 1)
        {
            num = 1;
            r_idx = MON_ZEUS;
        }
        else
        {
            num = randint1(2);
            r_idx = MON_ULT_MAGUS;
        }
        break;
    case MON_ARES:
        num = 1;
        msg_format("%s yells 'Mommy! Daddy! Help!!'", _current.name);
        r_idx = MON_ZEUS;
        r_idx2 = MON_HERA;
        break;
    case MON_APOLLO:
        msg_format("%s summons help!", _current.name);
        if (one_in_(3) && r_info[MON_ARTEMIS].max_num == 1)
        {
            num = 1;
            r_idx = MON_ARTEMIS;
        }
        else
            r_idx = MON_FENGHUANG;
        break;
    case MON_ARTEMIS:
        num = 1;
        msg_format("%s summons help!", _current.name);
        r_idx = MON_APOLLO;
        break;
    case MON_HEPHAESTUS:
        msg_format("%s summons friends!", _current.name);
        if (one_in_(3) && r_info[MON_ZEUS].max_num == 1)
        {
            num = 1;
            r_idx = MON_ZEUS;
        }
        else if (one_in_(3) && r_info[MON_HERA].max_num == 1)
        {
            num = 1;
            r_idx = MON_HERA;
        }
        else
            r_idx = MON_SPELLWARP_AUTOMATON;
        break;
    case MON_HERMES:
        num = randint1(16); /* XXX Why so high, RF1_FRIENDS? */
        msg_format("%s summons friends!", _current.name);
        r_idx = MON_MAGIC_MUSHROOM;
        break;
    case MON_HERA:
        msg_format("%s summons aid!'", _current.name);
        if (one_in_(3) && r_info[MON_ARES].max_num == 1)
        {
            num = 1;
            r_idx = MON_ARES;
        }
        else if (one_in_(3) && r_info[MON_HEPHAESTUS].max_num == 1)
        {
            num = 1;
            r_idx = MON_HEPHAESTUS;
        }
        else
            r_idx = MON_DEATH_BEAST;
        break;
    case MON_DEMETER:
        msg_format("%s summons ents!", _current.name);
        r_idx = MON_ENT;
        break;
    case MON_ROLENTO:
        if (p_ptr->blind) msg_format("%s spreads something.", _current.name);
        else msg_format("%s throws some hand grenades.", _current.name);
        num = 1 + randint1(3);
        r_idx = MON_SHURYUUDAN;
        break;
    case MON_BULLGATES:
        msg_format("%s summons his minions.", _current.name);
        r_idx = MON_IE;
        break;
    case MON_CALDARM:
        num = randint1(3);
        msg_format("%s summons his minions.", _current.name);
        r_idx = MON_LOCKE_CLONE;
        break;
    case MON_TALOS:
        num = randint1(3);
        msg_format("%s summons his minions.", _current.name);
        r_idx = MON_SPELLWARP_AUTOMATON;
        break;
    }
    for (i = 0; i < num; i++)
    {
        if (r_idx) _summon_r_idx(r_idx);
        if (r_idx2) _summon_r_idx(r_idx2);
    }
}
static void _summon(void)
{
    int ct, i;
    if (_current.spell->id.effect == SUMMON_SPECIAL)
    {
        _summon_special();
        return;
    }
    assert(_current.spell->parm.tag == MSP_DICE);
    ct = _roll(_current.spell->parm.v.dice);
    if (_current.spell->id.effect == SUMMON_KIN)
        summon_kin_type = _current.race->d_char;
    for (i = 0; i < ct; i++)
        _summon_type(_current.spell->id.effect);
}
static void _weird_bird(void)
{
    if (_current.flags & MSC_SRC_PLAYER)
    {
        msg_print("Not implemented yet!");
        return;
    }
    if (one_in_(3) || !(_current.flags & MSC_DIRECT))
    {
        msg_format("%s suddenly go out of your sight!", _current.name);
        teleport_away(_current.mon->id, 10, TELEPORT_NONMAGICAL);
        p_ptr->update |= (PU_MONSTERS);
    }
    else
    {
        int dam = 0;
        int get_damage = 0;

        msg_format("%s holds you, and drops from the sky.", _current.name);
        dam = damroll(4, 8);
        teleport_player_to(_current.src.y, _current.src.x, TELEPORT_NONMAGICAL | TELEPORT_PASSIVE);

        sound(SOUND_FALL);

        if (p_ptr->levitation)
            msg_print("You float gently down to the ground.");
        else
        {
            msg_print("You crashed into the ground.");
            dam += damroll(6, 8);
        }

        /* Mega hack -- this special action deals damage to the player. Therefore the code of "eyeeye" is necessary.
           -- henkma
         */
        get_damage = take_hit(DAMAGE_NOESCAPE, dam, _current.name, -1);
        if (get_damage > 0)
            weaponmaster_do_readied_shot(_current.mon);

        if (IS_REVENGE() && get_damage > 0 && !p_ptr->is_dead)
        {
            char m_name_self[80];

            monster_desc(m_name_self, _current.mon, MD_PRON_VISIBLE | MD_POSSESSIVE | MD_OBJECTIVE);

            msg_format("The attack of %s has wounded %s!", _current.name, m_name_self);
            project(0, 0, _current.src.y, _current.src.x, psion_backlash_dam(get_damage), GF_MISSILE, PROJECT_KILL, -1);
            if (p_ptr->tim_eyeeye)
                set_tim_eyeeye(p_ptr->tim_eyeeye-5, TRUE);
        }

        if (p_ptr->riding)
        {
            bool fear;
            mon_take_hit_mon(p_ptr->riding, dam, &fear,
                extract_note_dies(real_r_ptr(&m_list[p_ptr->riding])), _current.mon->id);
        }
    }
}
static void _weird(void)
{
    if (_current.race->d_char == 'B')
    {
        _weird_bird();
        return;
    }
    switch (_current.race->id)
    {
    case MON_BANORLUPART: {
        int hp = (_current.mon->hp + 1) / 2;
        int maxhp = _current.mon->maxhp/2;

        if ( p_ptr->inside_arena
          || p_ptr->inside_battle
          || !summon_possible(_current.mon->fy, _current.mon->fx)) return;

        delete_monster_idx(_current.mon->id);
        _current.mon = NULL;
        summon_named_creature(0, _current.src.y, _current.src.x, MON_BANOR, 0);
        m_list[hack_m_idx_ii].hp = hp;
        m_list[hack_m_idx_ii].maxhp = maxhp;

        summon_named_creature(0, _current.src.y, _current.src.x, MON_LUPART, 0);
        m_list[hack_m_idx_ii].hp = hp;
        m_list[hack_m_idx_ii].maxhp = maxhp;

        msg_print("Banor=Rupart splits in two!");

        break; }

    case MON_BANOR:
    case MON_LUPART: {
        int k, hp = 0, maxhp = 0;
        point_t where;

        if (!r_info[MON_BANOR].cur_num || !r_info[MON_LUPART].cur_num) return;
        for (k = 1; k < m_max; k++)
        {
            if (m_list[k].r_idx == MON_BANOR || m_list[k].r_idx == MON_LUPART)
            {
                mon_ptr mon = &m_list[k];
                hp += mon->hp;
                maxhp += mon->maxhp;
                if (mon->r_idx != _current.race->id)
                    where = point(mon->fx, mon->fy);
                delete_monster_idx(mon->id);
            }
        }
        _current.mon = NULL;
        _current.dest = where;
        _summon_r_idx(MON_BANORLUPART);
        _current.mon = &m_list[hack_m_idx_ii];
        _current.mon->hp = hp;
        _current.mon->maxhp = maxhp;

        msg_print("Banor and Rupart combine into one!");

        break; }
    }
} 
static void _spell_msg(void);
static void _spell_cast_aux(void)
{
    disturb(1, 0);
    reset_target(_current.mon);
    if (_spell_fail() || _spell_blocked())
        return;
    _spell_msg();
    if (is_original_ap_and_seen(_current.mon))  /* Banor=Rupart may disappear ... */
        _current.spell->lore = MIN(MAX_SHORT, _current.spell->lore + 1);

    switch (_current.spell->id.type)
    {
    case MST_ANNOY:   _annoy();   break;
    case MST_BALL:    _ball();    break;
    case MST_BEAM:    _beam();    break;
    case MST_BIFF:    _biff();    break;
    case MST_BOLT:    _bolt();    break;
    case MST_BREATH: _breathe(); break;
    case MST_BUFF:    _buff();    break;
    case MST_CURSE:   _curse();   break;
    case MST_ESCAPE:  _escape();  break;
    case MST_HEAL:    _heal();    break;
    case MST_SUMMON:  _summon();  break;
    case MST_TACTIC:  _tactic();  break;
    case MST_WEIRD:   _weird();   break;
    }
}

/*************************************************************************
 * Message
 ************************************************************************/
static char _msg[255];
static cptr _a_an(cptr noun)
{
    if (strchr("aeiou", noun[0])) return "an";
    return "a";
}
static cptr _possessive(mon_race_ptr race)
{
    if (!race) return "its";
    if (race->flags1 & RF1_MALE) return "his";
    if (race->flags1 & RF1_FEMALE) return "her";
    return "its";
}
/* Some monsters override the default message with something cutesy */
typedef struct {
    int race_id;
    mon_spell_id_t spell_id;
    cptr cast_msg;
    cptr blind_msg;
    cptr cast_mon_msg;
    cptr cast_plr_msg;
} _custom_msg_t, *_custom_msg_ptr;
static _custom_msg_t _mon_msg_tbl[] = {
    { MON_NINJA, {MST_BOLT, GF_ARROW},
        "$CASTER throws a syuriken.",
        "",
        "$CASTER throws a syuriken at $TARGET.",
        "You throw a syuriken." },
    { MON_JAIAN, {MST_BREATH, GF_SOUND},
        "'Booooeeeeee'",
        "'Booooeeeeee'",
        "'Booooeeeeee'",
        "'Booooeeeeee'" },
    { MON_BOTEI, {MST_BREATH, GF_SHARDS},
        "'Botei-Build cutter!!!'",
        "'Botei-Build cutter!!!'",
        "'Botei-Build cutter!!!'",
        "'Botei-Build cutter!!!'" } ,
    { MON_ROLENTO, {MST_BALL, GF_FIRE},
        "$CASTER throws a hand grenade.", 
        "$CASTER throws a hand grenade.", 
        "$CASTER throws a hand grenade at $TARGET.",
        "You throw a hand grenade." },
    {0}
};
static cptr _custom_msg(void)
{
    int i;
    for (i = 0;; i++)
    {
        _custom_msg_ptr msg = &_mon_msg_tbl[i];
        if (!msg->race_id) return NULL;
        if (msg->race_id != _current.race->id) continue;
        if (msg->spell_id.type != _current.spell->id.type) continue;
        if (msg->spell_id.effect != _current.spell->id.effect) continue;
        if (_current.flags & MSC_SRC_PLAYER)
            return msg->cast_plr_msg;
        else if (_current.flags & MSC_DEST_PLAYER)
        {
            if (p_ptr->blind) return msg->blind_msg;
            return msg->cast_msg;
        }
        else if (_current.flags & MSC_DEST_MONSTER)
        {
            if (_current.flags & MSC_UNVIEW)
                return "";
            else
                return msg->cast_mon_msg;
        }
    }
}
static cptr _display_msg(void)
{
    if (!_current.spell->display) return NULL;
    else if (_current.flags & MSC_SRC_PLAYER)
        return _current.spell->display->cast_plr_msg;
    else if (_current.flags & MSC_DEST_PLAYER)
    {
        if (p_ptr->blind) return _current.spell->display->blind_msg;
        return _current.spell->display->cast_msg;
    }
    else if (_current.flags & MSC_DEST_MONSTER)
    {
        if (_current.flags & MSC_UNVIEW)
            return "";
        else
            return _current.spell->display->cast_mon_msg;
    }
    return "";
}
static cptr _breathe_msg(void)
{
    gf_info_ptr gf = gf_lookup(_current.spell->id.effect);
    assert(gf);
    if (_current.flags & MSC_SRC_PLAYER)
    {
        sprintf(_msg, "You breathe <color:%c>%s</color>.",
            attr_to_attr_char(gf->color), gf->name);
        return _msg;
    }
    else if (_current.flags & MSC_DEST_PLAYER)
    {
        if (p_ptr->blind) return "$CASTER roars.";
        sprintf(_msg, "$CASTER breathes <color:%c>%s</color>.",
            attr_to_attr_char(gf->color), gf->name);
        return _msg;
    }
    else if (_current.flags & MSC_DEST_MONSTER)
    {
        if (!(_current.flags & MSC_UNVIEW))
        {
            sprintf(_msg, "$CASTER breathes <color:%c>%s</color> at $TARGET.",
                attr_to_attr_char(gf->color), gf->name);
            return _msg;
        }
    }
    return NULL;
}
static cptr _ball_msg(void)
{
    gf_info_ptr gf = gf_lookup(_current.spell->id.effect);
    if (!gf)
    {
        msg_format("Unkown Ball %d", _current.spell->id.effect);
        return NULL;
    }
    assert(gf);
    if (_current.flags & MSC_SRC_PLAYER)
    {
        sprintf(_msg, "You cast %s <color:%c>%s Ball</color>.",
            _a_an(gf->name), attr_to_attr_char(gf->color), gf->name);
        return _msg;
    }
    else if (_current.flags & MSC_DEST_PLAYER)
    {
        if (p_ptr->blind) return "$CASTER mumbles.";
        sprintf(_msg, "$CASTER casts %s <color:%c>%s Ball</color>.",
            _a_an(gf->name), attr_to_attr_char(gf->color), gf->name);
        return _msg;
    }
    else if (_current.flags & MSC_DEST_MONSTER)
    {
        if (!(_current.flags & MSC_UNVIEW))
        {
            sprintf(_msg, "$CASTER casts %s <color:%c>%s Ball</color> at $TARGET.",
                _a_an(gf->name), attr_to_attr_char(gf->color), gf->name);
            return _msg;
        }
    }
    return NULL;
}
static cptr _bolt_msg(void)
{
    gf_info_ptr gf = gf_lookup(_current.spell->id.effect);
    assert(gf);
    if (_current.flags & MSC_SRC_PLAYER)
    {
        sprintf(_msg, "You cast %s <color:%c>%s Bolt</color>.",
            _a_an(gf->name), attr_to_attr_char(gf->color), gf->name);
        return _msg;
    }
    else if (_current.flags & MSC_DEST_PLAYER)
    {
        if (p_ptr->blind) return "$CASTER mumbles.";
        sprintf(_msg, "$CASTER casts %s <color:%c>%s Bolt</color>.",
            _a_an(gf->name), attr_to_attr_char(gf->color), gf->name);
        return _msg;
    }
    else if (_current.flags & MSC_DEST_MONSTER)
    {
        if (!(_current.flags & MSC_UNVIEW))
        {
            sprintf(_msg, "$CASTER casts %s <color:%c>%s Bolt</color> at $TARGET.",
                _a_an(gf->name), attr_to_attr_char(gf->color), gf->name);
            return _msg;
        }
    }
    return NULL;
}
static cptr _summon_msg(void)
{
    parse_tbl_ptr p = summon_type_lookup(_current.spell->id.effect);
    assert(p);
    if (_current.flags & MSC_SRC_PLAYER)
    {
        sprintf(_msg, "You summon <color:%c>%s</color>.",
            attr_to_attr_char(p->color), p->name);
        return _msg;
    }
    else if (_current.flags & MSC_DEST_PLAYER)
    {
        if (p_ptr->blind) return "$CASTER mumbles.";
        sprintf(_msg, "$CASTER summons <color:%c>%s</color>.",
            attr_to_attr_char(p->color), p->name);
        return _msg;
    }
    else if (_current.flags & MSC_DEST_MONSTER)
    {
        if (!(_current.flags & MSC_UNVIEW))
        {
            sprintf(_msg, "$CASTER summons <color:%c>%s</color>.",
                attr_to_attr_char(p->color), p->name);
            return _msg;
        }
    }
    return NULL;
}
static cptr _get_msg(void)
{
    cptr msg = _custom_msg();
    if (!msg) msg = _display_msg();
    if (!msg)
    {
        switch (_current.spell->id.type)
        {
        case MST_BREATH:
            msg = _breathe_msg();
            break;
        case MST_BALL:
            msg = _ball_msg();
            break;
        case MST_BOLT:
            msg = _bolt_msg();
            break;
        case MST_SUMMON:
            msg = _summon_msg();
            break;
        }
    }
    return msg;
}
static cptr _msg_var(cptr var)
{
    if (strcmp(var, "CASTER") == 0)
        return _current.name;
    if (strcmp(var, "CASTER_POS") == 0)
    {
        if (_current.flags & MSC_SRC_PLAYER)
            return "your";
        return _possessive(_current.race);
    }
    if (strcmp(var, "TARGET") == 0)
        return _current.name2;
    if (strcmp(var, "TARGET_POS") == 0)
    {
        if (_current.flags & MSC_DEST_PLAYER)
            return "your";
        return _possessive(_race(_current.mon2));
    }
    
    /*return format("<color:v>%s</color>", var);*/
    return "<color:v>?</color>";
}
static bool _is_ident_char(char c)
{
    if (c == '_') return TRUE;
    return BOOL(isupper(c));
}
static void _spell_msg(void)
{
    char  out[255], token[50];
    cptr  pos;
    char *dest;
    cptr  msg = _get_msg();
    if (!msg) return;
    if (!strlen(msg)) return;
    pos = msg;
    dest = out;
    for (;;)
    {
        char c = *pos;
        if (!c) break;
        if (c == '$')
        {
            cptr seek = ++pos;
            cptr replace;
            while (_is_ident_char(*seek))
                seek++;
            strncpy(token, pos, seek - pos);
            token[seek - pos] = '\0';
            replace = _msg_var(token);
            if (replace)
            {
                strcpy(dest, replace); 
                dest += strlen(replace);
            }
            pos = seek;
        }
        else
            *dest++ = *pos++;
    }
    *dest++ = '\0';
    if (!strlen(out)) return;
    msg_print(out);
}

/*************************************************************************
 * AI
 ************************************************************************/
static void _init_spells(mon_spells_ptr spells)
{
    int i, j;
    for (i = 0; i < MST_COUNT; i++)
    {
        mon_spell_group_ptr group = spells->groups[i];
        _mst_info_ptr       mp;
        if (!group) continue;
        mp = _mst_lookup(i);
        assert(mp);
        for (j = 0; j < group->count; j++)
        {
            mon_spell_ptr spell = &group->spells[j];
            spell->prob = mp->prob;
        }
    }
}

static bool _projectable(point_t src, point_t dest)
{
    return projectable(src.y, src.x, dest.y, dest.x);
}
static bool _projectable_splash(point_t src, point_t dest)
{
    return _projectable(src, dest)
        && cave_have_flag_bold(dest.y, dest.x, FF_PROJECT);
}
static bool _disintegrable(point_t src, point_t dest)
{
    return in_disintegration_range(dest.y, dest.x, src.y, src.x);
}
#if 0
static bool _visible(point_t src, point_t dest)
{
    return los(src.y, src.x, dest.y, dest.x);
}
static bool _visible_splash(point_t src, point_t dest)
{
    return _los(src, dest)
        && cave_have_flag_bold(dest.y, dest.x, FF_LOS);
#endif
static bool _distance(point_t src, point_t dest)
{
    return distance(src.y, src.x, dest.y, dest.x);
}

typedef bool (*_path_p)(point_t src, point_t dest);
typedef bool (*_spell_p)(mon_spell_ptr spell);

static bool _ball0_p(mon_spell_ptr spell)
{
    return BOOL(spell->flags & MSF_BALL0);
}
static bool _not_innate_p(mon_spell_ptr spell)
{
    return !(spell->flags & MSF_INNATE);
}
static bool _blink_check_p(mon_spell_ptr spell)
{
    switch (spell->id.type)
    {
    case MST_BREATH:
    case MST_BALL:
    case MST_BOLT:
    case MST_BEAM:
    case MST_CURSE: return TRUE;
    }
    return _spell_is_(spell, MST_ANNOY, ANNOY_TRAPS);
}
static bool _jump_p(mon_spell_ptr spell)
{
    if (spell->id.type != MST_TACTIC) return FALSE;
    if (spell->id.effect >= TACTIC_BLINK) return FALSE;
    return TRUE;
}

static point_t _choose_splash_point(point_t src, point_t dest, _path_p filter)
{
    point_t best_pt = {0};
    int     i, best_distance = 99999;

    assert(filter);
    for (i = 0; i < 8; i++)
    {
        point_t pt;
        int     d;

        pt.x = dest.x + ddx_ddd[i];
        pt.y = dest.y + ddy_ddd[i];
        if (!filter(src, pt)) continue;
        d = _distance(src, pt);
        if (d < best_distance)
        {
            best_distance = d;
            best_pt = pt;
        }
    }
    return best_pt;
}

static void _adjust_group(mon_spell_group_ptr group, _spell_p p, int pct)
{
    int i;
    if (!group) return;
    for (i = 0; i < group->count; i++)
    {
        mon_spell_ptr spell = &group->spells[i];
        if (p && !p(spell)) continue;
        spell->prob = MIN(250, spell->prob * pct / 100);
    }
}
static void _remove_group(mon_spell_group_ptr group, _spell_p p)
{
    _adjust_group(group, p, 0);
}
static void _adjust_spells(mon_spells_ptr spells, _spell_p p, int pct)
{
    int i;
    for (i = 0; i < MST_COUNT; i++)
    {
        mon_spell_group_ptr group = spells->groups[i];
        if (!group) continue;
        _adjust_group(group, p, pct);
    }
}
static void _remove_spells(mon_spells_ptr spells, _spell_p p)
{
    _adjust_spells(spells, p, 0);
}
static mon_spell_ptr _find_spell(mon_spells_ptr spells, _spell_p p)
{
    int i, j;
    assert(p);
    for (i = 0; i < MST_COUNT; i++)
    {
        mon_spell_group_ptr group = spells->groups[i];
        if (!group) continue;
        for (j = 0; j < group->count; j++)
        {
            mon_spell_ptr spell = &group->spells[j];
            if (p(spell)) return spell;
        }
    }
    return NULL;
}

static bool _have_smart_flag(u32b flags, int which)
{
    return BOOL(flags & (1U << which));
}

static void _smart_tweak_res_dam(mon_spell_ptr spell, int res, u32b flags)
{
    int pct, tweak = 100;
    if (res == RES_INVALID) return;
    if (!_have_smart_flag(flags, res)) return;
    pct = res_pct(res);
    if (!pct) return;
    if (pct == 100) tweak = 0;
    else if (res_is_high(res) && res > 30) tweak = 100 - pct/2;
    else if (res > 50) tweak = 100 - pct/3;
    spell->prob = MIN(200, spell->prob*tweak/100);
}
static void _smart_tweak_res_sav(mon_spell_ptr spell, int res, u32b flags)
{
    int pct, tweak = 100, need;
    if (res == RES_INVALID) return;
    if (!_have_smart_flag(flags, res)) return;
    pct = res_pct(res);
    if (!pct) return;
    need = res_is_high(res) ? 33 : 55;
    if (pct >= need) tweak = 0;
    else tweak = 100 - pct*80/need;
    spell->prob = MIN(200, spell->prob*tweak/100);
}
static void _smart_remove_annoy(mon_spell_group_ptr group, u32b flags)
{
    int i;
    if (!group) return;
    for (i = 0; i < group->count; i++)
    {
        mon_spell_ptr spell = &group->spells[i];
        switch (spell->id.effect)
        {
        case ANNOY_BLIND:
            _smart_tweak_res_sav(spell, RES_BLIND, flags);
            break;
        case ANNOY_CONFUSE:
            _smart_tweak_res_sav(spell, RES_CONF, flags);
            break;
        case ANNOY_PARALYZE:
        case ANNOY_SLOW:
            if (_have_smart_flag(flags, SM_FREE_ACTION) && p_ptr->free_act)
                spell->prob = 0;
            break;
        case ANNOY_SCARE:
            if (_have_smart_flag(flags, RES_FEAR))
            {
                int ct = p_ptr->resist[RES_FEAR];
                int i;
                for (i = 0; i < ct; i++)
                    spell->prob = spell->prob * 75 / 100;
            }
            break;
        case ANNOY_TELE_TO:
            _smart_tweak_res_sav(spell, RES_TELEPORT, flags);
            break;
        case ANNOY_TELE_LEVEL:
            _smart_tweak_res_sav(spell, RES_NEXUS, flags);
            break;
        }
    }
}
static void _smart_remove_aux(mon_spell_group_ptr group, u32b flags)
{
    int i;
    if (!group) return;
    for (i = 0; i < group->count; i++)
    {
        mon_spell_ptr spell = &group->spells[i];
        gf_info_ptr   gf = gf_lookup(spell->id.effect);
        if (!gf) continue; /* GF_ARROW? */
        _smart_tweak_res_dam(spell, gf->resist, flags);
    }
}

static void _remove_spell(mon_spells_ptr spells, mon_spell_id_t id)
{
    mon_spell_ptr spell = mon_spells_find(spells, id);
    if (spell)
        spell->prob = 0;
}

static void _smart_remove(mon_spell_cast_ptr cast)
{
    mon_spells_ptr spells = cast->race->spells;
    u32b           flags = cast->mon->smart;

    if (smart_cheat) flags = 0xFFFFFFFF;
    _smart_remove_aux(spells->groups[MST_BREATH], flags);
    _smart_remove_aux(spells->groups[MST_BALL], flags);
    if (_have_smart_flag(flags, SM_REFLETION) && p_ptr->reflect)
        _remove_group(spells->groups[MST_BOLT], NULL);
    else
        _smart_remove_aux(spells->groups[MST_BOLT], flags);
    _smart_remove_aux(spells->groups[MST_BEAM], flags);
    _smart_remove_annoy(spells->groups[MST_ANNOY], flags);
}

static bool _clean_shot(point_t src, point_t dest)
{
    return clean_shot(src.y, src.x, dest.y, dest.x, FALSE);
}
static bool _summon_possible(point_t where)
{
    return summon_possible(where.y, where.x);
}

/* When wounded, skew the spell probabilities away from offense
 * and trickery to summoning, healing and escape tactics. Make
 * this transition gradual but allow smart monsters to panic near
 * death. */
static void _ai_wounded(mon_spell_cast_ptr cast)
{
    bool           smart  = BOOL(cast->race->flags2 & RF2_SMART);
    mon_spells_ptr spells = cast->race->spells;
    if ( spells->groups[MST_HEAL]
      || spells->groups[MST_ESCAPE]
      || spells->groups[MST_SUMMON] )
    {
        int pct_healthy = cast->mon->hp * 100 / cast->mon->maxhp;
        int pct_wounded = 100 - pct_healthy;
        if (pct_wounded > 20)
        {
            int buff = pct_wounded * pct_wounded * pct_wounded / 500;
            int biff = pct_wounded / 2;
            if (smart && pct_wounded > 90 && one_in_(2))
            {
                biff = 100;
                _adjust_group(spells->groups[MST_SUMMON], NULL, 100 + buff/3);
            }
            _adjust_group(spells->groups[MST_BREATH], NULL, 100 - biff);
            _adjust_group(spells->groups[MST_BALL], NULL, 100 - biff);
            _adjust_group(spells->groups[MST_BOLT], NULL, 100 - biff);
            _adjust_group(spells->groups[MST_BEAM], NULL, 100 - biff);
            _adjust_group(spells->groups[MST_CURSE], NULL, 100 - biff);
            _adjust_group(spells->groups[MST_BIFF], NULL, 100 - biff);
            _adjust_group(spells->groups[MST_ANNOY], NULL, 100 - biff);
            _adjust_group(spells->groups[MST_WEIRD], NULL, 100 - biff);

            _adjust_group(spells->groups[MST_HEAL], NULL, 100 + buff);
            _adjust_group(spells->groups[MST_ESCAPE], NULL, 100 + buff);
        }
        else /*if (pct_wounded <= 20)*/
        {
            _remove_group(spells->groups[MST_HEAL], NULL);
            _remove_group(spells->groups[MST_ESCAPE], NULL);
        }
    }
}

static void _remove_bad_spells(mon_spell_cast_ptr cast)
{
    bool           stupid = BOOL(cast->race->flags2 & RF2_STUPID);
    bool           smart  = BOOL(cast->race->flags2 & RF2_SMART);
    mon_spells_ptr spells = cast->race->spells;
    mon_spell_ptr  spell;

    /* Apply monster knowledge of player's strengths and weaknesses */
    if (!stupid && (smart_cheat || smart_learn))
        _smart_remove(cast);

    /* Require a projectable player for projection spells. Even stupid
     * monsters aren't dumb enough to bounce spells off walls! Non-stupid
     * monsters will splash the player if possible, though with reduced
     * odds. */
    if (_projectable(cast->src, cast->dest))
        cast->flags |= MSC_DIRECT;
    else
    {
        point_t new_dest = {0};
        
        if (!stupid)
            new_dest = _choose_splash_point(cast->src, cast->dest, _projectable_splash);

        _remove_group(spells->groups[MST_ANNOY], NULL);
        _remove_group(spells->groups[MST_BOLT], NULL);
        _remove_group(spells->groups[MST_BEAM], NULL);
        _remove_group(spells->groups[MST_BIFF], NULL);
        _remove_group(spells->groups[MST_BUFF], NULL);
        _remove_group(spells->groups[MST_CURSE], NULL);
        _remove_group(spells->groups[MST_WEIRD], NULL);
        _remove_spell(spells, _id(MST_BALL, GF_ROCKET));

        if (new_dest.x || new_dest.y) /* XXX assume (0,0) out of bounds */
        {
            cast->dest = new_dest;
            cast->flags |= MSC_SPLASH;
            _adjust_group(spells->groups[MST_BREATH], NULL, 50);
            _adjust_group(spells->groups[MST_BALL], NULL, 50);
            _adjust_group(spells->groups[MST_BALL], _ball0_p, 0);
            _adjust_group(spells->groups[MST_SUMMON], NULL, 50);
            /* Heal and Self Telportation OK */
            _remove_spell(spells, _id(MST_ESCAPE, ESCAPE_TELE_OTHER));
        }
        else
        {
            _remove_group(spells->groups[MST_BREATH], NULL);
            _remove_group(spells->groups[MST_BALL], NULL);
            _remove_group(spells->groups[MST_SUMMON], NULL);
            _remove_group(spells->groups[MST_HEAL], NULL);
            _remove_group(spells->groups[MST_ESCAPE], NULL);
            _remove_group(spells->groups[MST_TACTIC], NULL);
            spell = mon_spells_find(spells, _id(MST_BREATH, GF_DISINTEGRATE));
            if ( spell
              && cast->mon->cdis < MAX_RANGE / 2
              && _disintegrable(cast->src, cast->dest)
              && one_in_(10) ) /* Note: This will be the only spell possible so any prob = 100% */
            {
                spell->prob = 150;
            }
        }
    }
    if (p_ptr->inside_arena || p_ptr->inside_battle)
        _remove_group(spells->groups[MST_SUMMON], NULL);

    /* Stupid monsters are done! */
    if (stupid)
        return;

    /* Anti-magic caves? Don't bother casting spells with 100% fail rates */
    if (py_in_dungeon() && (d_info[dungeon_type].flags1 & DF1_NO_MAGIC))
        _remove_spells(spells, _not_innate_p);

    _ai_wounded(cast);

    if (smart && (cast->flags & MSC_DIRECT))
    {
        spell = mon_spells_find(spells, _id(MST_ANNOY, ANNOY_TELE_LEVEL));
        if (spell && TELE_LEVEL_IS_INEFF(0))
            spell->prob = 0;
        spell = mon_spells_find(spells, _id(MST_BIFF, BIFF_DISPEL_MAGIC));
        if (spell)
            spell->prob = dispel_check(cast->mon->id) ? 50 : 0;
        spell = mon_spells_find(spells, _id(MST_BIFF, BIFF_ANTI_MAGIC));
        if (spell)
            spell->prob = anti_magic_check();
    }
    spell = mon_spells_find(spells, _id(MST_ANNOY, ANNOY_TELE_TO));
    if (spell)
    {
        if (_distance(cast->src, cast->dest) < 2)
            spell->prob = 0;
        else
            spell->prob += cast->mon->anger;
    }

    spell = mon_spells_find(spells, _id(MST_ANNOY, ANNOY_WORLD));
    if (spell && world_monster) /* prohibit if already cast */
        spell->prob = 0;

    /* XXX Currently, tactical spells involve making space for spellcasting monsters. */
    if (spells->groups[MST_TACTIC] && (cast->flags & (MSC_DIRECT | MSC_SPLASH)) && _distance(cast->src, cast->dest) < 4 && _find_spell(spells, _blink_check_p))
        _adjust_group(spells->groups[MST_TACTIC], NULL, 700);

    if (_distance(cast->src, cast->dest) > 5)
        _remove_group(spells->groups[MST_TACTIC], _jump_p);

    /* beholders prefer to gaze, but won't do so if adjacent */
    spell = mon_spells_find(spells, _id(MST_BOLT, GF_ATTACK));
    if (spell)
    {
        if (p_ptr->blind || _distance(cast->src, cast->dest) < 2)
            spell->prob = 0;
        else
            spell->prob *= 7;
    }

    /* Useless buffs? */
    spell = mon_spells_find(spells, _id(MST_BUFF, BUFF_INVULN));
    if (spell && cast->mon->mtimed[MTIMED_INVULNER])
        spell->prob = 0;

    spell = mon_spells_find(spells, _id(MST_BUFF, BUFF_HASTE));
    if (spell && cast->mon->mtimed[MTIMED_FAST])
        spell->prob = 0;

    /* Uselss annoys? */
    if (!p_ptr->csp)
        _remove_spell(spells, _id(MST_BALL, GF_DRAIN_MANA));
    if (p_ptr->blind)
        _remove_spell(spells, _id(MST_ANNOY, ANNOY_BLIND));
    if (p_ptr->slow)
        _remove_spell(spells, _id(MST_ANNOY, ANNOY_SLOW));
    if (p_ptr->paralyzed)
        _remove_spell(spells, _id(MST_ANNOY, ANNOY_PARALYZE));
    if (p_ptr->confused)
        _remove_spell(spells, _id(MST_ANNOY, ANNOY_CONFUSE));

    /* require a direct shot to player for bolts */
    if ((cast->flags & MSC_DIRECT) && !_clean_shot(cast->src, cast->dest))
    {
        _remove_group(spells->groups[MST_BOLT], NULL);
        _remove_spell(spells, _id(MST_BALL, GF_ROCKET));
    }

    if (spells->groups[MST_SUMMON] && !_summon_possible(cast->dest))
    {
        _remove_group(spells->groups[MST_SUMMON], NULL);
        /* XXX Use GF_DISINTEGRATE to open things up */
    }

    spell = mon_spells_find(spells, _id(MST_ANNOY, ANNOY_ANIMATE_DEAD));
    if (spell && !raise_possible(cast->mon))
        spell->prob = 0;

    if (p_ptr->invuln && (cast->flags & MSC_DIRECT))
    {
        _remove_group(spells->groups[MST_BREATH], NULL);
        _remove_group(spells->groups[MST_BALL], NULL);
        _remove_group(spells->groups[MST_BOLT], NULL);
        _remove_group(spells->groups[MST_BEAM], NULL);
        _remove_group(spells->groups[MST_CURSE], NULL);
        spell = mon_spells_find(spells, _id(MST_BEAM, GF_PSY_SPEAR));
        if (spell)
            spell->prob = 30;
    }
}

static mon_spell_ptr _choose_spell(mon_spells_ptr spells)
{
    int i, j, total = 0, roll;
    for (i = 0; i < MST_COUNT; i++)
    {
        mon_spell_group_ptr group = spells->groups[i];
        if (!group) continue;
        for (j = 0; j < group->count; j++)
        {
            mon_spell_ptr spell = &group->spells[j];
            total += spell->prob;
        }
    }
    if (!total) return NULL;
    roll = randint1(total);
    for (i = 0; i < MST_COUNT; i++)
    {
        mon_spell_group_ptr group = spells->groups[i];
        if (!group) continue;
        for (j = 0; j < group->count; j++)
        {
            mon_spell_ptr spell = &group->spells[j];
            roll -= spell->prob;
            if (roll <= 0) return spell;
        }
    }
    return NULL; /* ?! */
}

static bool _default_ai(mon_spell_cast_ptr cast)
{
    if (!cast->race->spells) return FALSE;
    _init_spells(cast->race->spells);
    _remove_bad_spells(cast);
    cast->spell = _choose_spell(cast->race->spells);
    return cast->spell != NULL;
}

/*************************************************************************
 * AI Mon
 ************************************************************************/
static vec_ptr _enemies(mon_ptr mon)
{
    int i;
    vec_ptr v = vec_alloc(NULL);
    for (i = 1; i < m_max; i++)
    {
        mon_ptr tgt = &m_list[i];
        if (tgt->id == mon->id) continue;
        if (!tgt->r_idx) continue;
        if (!are_enemies(mon, tgt)) continue;
        if (!_projectable(point(mon->fx, mon->fy), point(tgt->fx, tgt->fy))) continue;
        vec_add(v, tgt);
    }
    return v;
}
static bool _choose_target(mon_spell_cast_ptr cast)
{
    mon_ptr mon2 = NULL;
    if (pet_t_m_idx && is_pet(cast->mon))
    {
        mon2 = &m_list[pet_t_m_idx];
        if (mon2->id == cast->mon->id || !_projectable(cast->src, point(mon2->fx, mon2->fy)))
            mon2 = NULL;
    }
    if (!mon2 && cast->mon->target_y)
    {
        int t_idx = cave[cast->mon->target_y][cast->mon->target_x].m_idx;

        if (t_idx)
        {
            mon2 = &m_list[t_idx];
            if ( mon2->id == cast->mon->id
              || !are_enemies(cast->mon, mon2)
              || !_projectable(cast->src, point(mon2->fx, mon2->fy)) )
            {
                mon2 = NULL;
            }
        }
    }
    if (!mon2)
    {
        vec_ptr v = _enemies(cast->mon);
        if (vec_length(v))
            mon2 = vec_get(v, randint0(vec_length(v)));
        vec_free(v);
    }
    if (mon2)
    {
        cast->mon2 = mon2;
        _mon_desc(mon2, cast->name2, 'o');
        cast->dest = point(mon2->fx, mon2->fy);
        if (!cast->mon->ml && !cast->mon2->ml)
            cast->flags |= MSC_UNVIEW;
        return TRUE;
    }
    return FALSE;
}

static void _remove_bad_spells_pet(mon_spell_cast_ptr cast)
{
}

static void _remove_bad_spells_mon(mon_spell_cast_ptr cast)
{
    mon_spells_ptr spells = cast->race->spells;
    mon_spell_ptr  spell;

    /* _choose_target only selects projectable foes for now */
    assert(_projectable(cast->src, cast->dest));
    cast->flags |= MSC_DIRECT;

    /* not implemented ... yet */
    _remove_group(spells->groups[MST_ANNOY], NULL);
    _remove_group(spells->groups[MST_BIFF], NULL);
    _remove_spell(spells, _id(MST_BALL, GF_DRAIN_MANA));

    if (p_ptr->inside_arena || p_ptr->inside_battle)
        _remove_group(spells->groups[MST_SUMMON], NULL);

    if (is_pet(cast->mon))
        _remove_bad_spells_pet(cast);

    /* Stupid monsters are done! */
    if (cast->race->flags2 & RF2_STUPID)
        return;

    /* Anti-magic caves? Don't bother casting spells with 100% fail rates */
    if (py_in_dungeon() && (d_info[dungeon_type].flags1 & DF1_NO_MAGIC))
        _remove_spells(spells, _not_innate_p);

    _ai_wounded(cast);

    /* XXX Currently, tactical spells involve making space for spellcasting monsters. */
    if (spells->groups[MST_TACTIC] && _distance(cast->src, cast->dest) < 4 && _find_spell(spells, _blink_check_p))
        _adjust_group(spells->groups[MST_TACTIC], NULL, 700);

    if (_distance(cast->src, cast->dest) > 5)
        _remove_group(spells->groups[MST_TACTIC], _jump_p);

    /* Useless buffs? */
    spell = mon_spells_find(spells, _id(MST_BUFF, BUFF_INVULN));
    if (spell && cast->mon->mtimed[MTIMED_INVULNER])
        spell->prob = 0;

    spell = mon_spells_find(spells, _id(MST_BUFF, BUFF_HASTE));
    if (spell && cast->mon->mtimed[MTIMED_FAST])
        spell->prob = 0;

    /* require a direct shot for bolts */
    if (!_clean_shot(cast->src, cast->dest))
    {
        _remove_group(spells->groups[MST_BOLT], NULL);
        _remove_spell(spells, _id(MST_BALL, GF_ROCKET));
    }

    if (spells->groups[MST_SUMMON] && !_summon_possible(cast->dest))
    {
        _remove_group(spells->groups[MST_SUMMON], NULL);
        if (cast->race->id == MON_BANORLUPART)
            _remove_spell(spells, _id(MST_WEIRD, WEIRD_SPECIAL));
    }

    /* Hack: No monster summoning unless the player is nearby.
     * XXX: Or any player pets ... */
    if (!_projectable(cast->src, point(px, py)))
        _remove_group(spells->groups[MST_SUMMON], NULL);

    spell = mon_spells_find(spells, _id(MST_ANNOY, ANNOY_ANIMATE_DEAD));
    if (spell && !raise_possible(cast->mon))
        spell->prob = 0;

    /* XXX Pets need to avoid harming the player!!! */
}
static bool _default_ai_mon(mon_spell_cast_ptr cast)
{
    if (!cast->race->spells) return FALSE;
    if (!_choose_target(cast)) return FALSE;
    _init_spells(cast->race->spells);
    _remove_bad_spells_mon(cast);
    cast->spell = _choose_spell(cast->race->spells);
    return cast->spell != NULL;
}


/*************************************************************************
 * Wizard Probe
 ************************************************************************/
static bool _is_attack_spell(mon_spell_ptr spell)
{
    switch (spell->id.type)
    {
    case MST_BREATH: case MST_BALL: case MST_BOLT: case MST_BEAM: case MST_CURSE:
        return TRUE;
    }
    return FALSE;
}
static bool _is_gf_spell(mon_spell_ptr spell)
{
    switch (spell->id.type)
    {
    case MST_BREATH: case MST_BALL: case MST_BOLT: case MST_BEAM:
        return TRUE;
    }
    return FALSE;
}
static int _spell_res(mon_spell_ptr spell)
{
    int res = 0;
    if (_is_gf_spell(spell))
    {
        gf_info_ptr gf = gf_lookup(spell->id.effect);
        if (gf && gf->resist != RES_INVALID)
            res = res_pct(gf->resist);
    }
    return res;
}

static int _avg_spell_dam_aux(mon_spell_ptr spell, int hp)
{
    if (!_is_attack_spell(spell)) return 0;
    if (spell->parm.tag == MSP_DICE)
    {
        dice_t dice = spell->parm.v.dice;
        int dam = _avg_roll(dice);
        int res = _spell_res(spell);
        if (res)
            dam -= dam * res / 100;
        if (spell->id.type == MST_CURSE && spell->id.effect == GF_HAND_DOOM)
            dam = p_ptr->chp * dam / 100;
        return dam;
    }
    if (spell->parm.tag == MSP_HP_PCT)
    {
        int dam = hp * spell->parm.v.hp_pct.pct / 100;
        int res = _spell_res(spell);
        if (dam > spell->parm.v.hp_pct.max)
            dam = spell->parm.v.hp_pct.max;
        if (res)
            dam -= dam * res / 100;
        return dam;
    }
    return 0;
}
int mon_spell_avg_dam(mon_spell_ptr spell, mon_race_ptr race)
{
    return _avg_spell_dam_aux(spell, _avg_hp(race));
}
int _avg_spell_dam(mon_ptr mon, mon_spell_ptr spell)
{
    return _avg_spell_dam_aux(spell, mon->hp);
}
void mon_spell_wizard(mon_ptr mon, mon_spell_ai ai, doc_ptr doc)
{
    mon_spell_cast_t cast = {0};
    int              i, j, total = 0, total_dam = 0;
    _spell_cast_init(&cast, mon);
    if (!ai) ai = _default_ai;
    if (!cast.race->spells) return;
    if (!ai(&cast)) return;
    doc_printf(doc, "%s: %d%%\n", cast.name, cast.race->spells->freq);
    for (i = 0; i < MST_COUNT; i++)
    {
        mon_spell_group_ptr group = cast.race->spells->groups[i];
        if (!group) continue;
        group->prob = 0;
        for (j = 0; j < group->count; j++)
        {
            mon_spell_ptr spell = &group->spells[j];
            total += spell->prob;
            group->prob += spell->prob;
        }
    }
    if (!total) return;
    for (i = 0; i < MST_COUNT; i++)
    {
        mon_spell_group_ptr group = cast.race->spells->groups[i];
        _mst_info_ptr       mp;
        if (!group) continue;
        mp = _mst_lookup(i);
        assert(mp);
        doc_printf(doc, "%2d.%d%%", group->prob * 100 / total, (group->prob * 1000 / total)%10);
        doc_printf(doc, " <color:%c>%-10.10s</color>", attr_to_attr_char(mp->color), mp->name);
        for (j = 0; j < group->count; j++)
        {
            mon_spell_ptr spell = &group->spells[j];
            int           dam = _avg_spell_dam(mon, spell);
            doc_printf(doc, "<tab:20>%2d.%d%% ", spell->prob * 100 / total, (spell->prob * 1000 / total) % 10);
            mon_spell_doc(spell, doc);
            if (spell->parm.tag == MSP_DICE)
            {
                dice_t dice = spell->parm.v.dice;
                doc_insert(doc, "<tab:50>");
                if (dice.dd)
                    doc_printf(doc, "%dd%d", dice.dd, dice.ds);
                if (dice.base)
                {
                    if (dice.dd) doc_insert(doc, "+");
                    doc_printf(doc, "%d", dice.base);
                }
            }
            else if (spell->parm.tag == MSP_HP_PCT)
            {
                hp_pct_t hp = spell->parm.v.hp_pct;
                int      dam = mon->hp * hp.pct / 100;
                if (dam > hp.max) dam = hp.max;
                doc_printf(doc, "<tab:50>%d", dam);
            }
            if (dam)
            {
                total_dam += spell->prob * dam;
                doc_printf(doc, "<tab:65>%d", dam);
            }
            doc_newline(doc);
        }
    }
    doc_printf(doc, "<tab:65><color:r>%d</color>", total_dam / total);
    doc_newline(doc);
}

