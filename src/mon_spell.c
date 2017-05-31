#include "angband.h"

#include "mon_spell.h"
#include <assert.h>

/*************************************************************************
 * Parse Tables (for r_info.txt)
 *
 * The parser will first look in various parse tables  for spell names,
 * such as THROW, SHOOT or HASTE. If not present, then the token prefix
 * determines the spell type (such as BR_ for MST_BREATHE) while the
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

/* GF_* codes ... These need a rewrite. They are not projection types,
 * though projection uses them. They are not damage types, since they
 * may not cause physical damage. They are not element types, though they
 * often are. Probably, they are best considered as effects: on the player,
 * a monster or terrain. They are used by monster spells, player spells,
 * monster melee, player innate melee, traps, terrain effects on the player,
 * monster auras, player auras and probably much more besides. For now,
 * let's just table up what we need for monster spells: */
typedef struct {
    cptr token;
    int  id;
    cptr name;
    byte color;
    int  resist;
} _gf_info_t, *_gf_info_ptr;

static _gf_info_t _gf_tbl[] = {
    { "ACID", GF_ACID, "Acid", TERM_GREEN, RES_ACID },
    { "ELEC", GF_ELEC, "Lightning", TERM_BLUE, RES_ELEC },
    { "FIRE", GF_FIRE, "Fire", TERM_RED, RES_FIRE },
    { "COLD", GF_COLD, "Cold", TERM_L_WHITE, RES_COLD },
    { "POIS", GF_POIS, "Poison", TERM_L_GREEN, RES_POIS },
    { "LITE", GF_LITE, "Light", TERM_YELLOW, RES_LITE },
    { "DARK", GF_DARK, "Dark", TERM_L_DARK, RES_DARK },
    { "CONFUSION", GF_CONFUSION, "Confusion", TERM_L_UMBER, RES_CONF },
    { "NETHER", GF_NETHER, "Nether", TERM_L_DARK, RES_NETHER },
    { "NEXUS", GF_NEXUS, "Nexus", TERM_VIOLET, RES_NEXUS },
    { "SOUND", GF_SOUND, "Sound", TERM_ORANGE, RES_SOUND },
    { "SHARDS", GF_SHARDS, "Shards", TERM_L_UMBER, RES_SHARDS },
    { "CHAOS", GF_CHAOS, "Chaos", TERM_VIOLET, RES_CHAOS },
    { "DISENCHANT", GF_DISENCHANT, "Disenchantment", TERM_VIOLET, RES_DISEN },
    { "TIME", GF_TIME, "Time", TERM_L_BLUE, RES_TIME },
    { "MANA", GF_MANA, "Mana", TERM_L_BLUE, RES_INVALID },
    { "GRAVITY", GF_GRAVITY, "Gravity", TERM_L_UMBER, RES_INVALID },
    { "INERTIA", GF_INERT, "Inertia", TERM_L_UMBER, RES_INVALID },
    { "PLASMA", GF_PLASMA, "Plasma", TERM_L_RED, RES_INVALID },
    { "FORCE", GF_FORCE, "Force", TERM_L_BLUE, RES_INVALID },
    { "NUKE", GF_NUKE, "Toxic Waste", TERM_L_GREEN, RES_INVALID },
    { "DISINTEGRATE", GF_DISINTEGRATE, "Disintegration", TERM_L_DARK, RES_INVALID },
    { "STORM", GF_STORM, "Storm Winds", TERM_BLUE, RES_INVALID },
    { "HOLY_FIRE", GF_HOLY_FIRE, "Holy Fire", TERM_YELLOW, RES_INVALID },
    { "HELL_FIRE", GF_HELL_FIRE, "Hell Fire", TERM_L_DARK, RES_INVALID },
    { "ICE", GF_ICE, "Ice", TERM_L_WHITE, RES_COLD },
    { "WATER", GF_WATER, "Water", TERM_L_BLUE, RES_INVALID },
    {0}
};

static _gf_info_ptr _gf_parse_name(cptr token)
{
    int i;
    for (i = 0; ; i++)
    {
        _gf_info_ptr info = &_gf_tbl[i];
        if (!info->token) return NULL;
        if (strcmp(info->token, token) == 0) return info;
    }
}

static _gf_info_ptr _gf_lookup(int id)
{
    int i;
    for (i = 0; ; i++)
    {
        _gf_info_ptr info = &_gf_tbl[i];
        if (!info->token) return NULL;
        if (info->id == id) return info;
    }
}

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
    ANNOY_TELE_TO,
    ANNOY_TRAPS,
    ANNOY_WORLD,
};
static _parse_t _annoy_tbl[] = {
    { "AMNESIA", { MST_ANNOY, ANNOY_AMNESIA },
        { "Amnesia", TERM_L_BLUE,
          "%s tries to blank your mind.",
          "%s tries to blank your mind."}},
    { "ANIM_DEAD", { MST_ANNOY, ANNOY_ANIMATE_DEAD },
        { "Animate Dead", TERM_L_DARK,
          "%s casts a spell to revive the dead.",
          "%s mumbles."}},
    { "BLIND", { MST_ANNOY, ANNOY_BLIND },
        { "Blind", TERM_WHITE,
          "%s casts a spell, burning your eyes!",
          "%s mumbles."}},
    { "CONFUSE", { MST_ANNOY, ANNOY_CONFUSE },
        { "Confuse", TERM_L_UMBER,
          "%s creates a mesmerizing illusion.",
          "%s mumbles, and you hear puzzling noises."}},
    { "DARKNESS", { MST_ANNOY, ANNOY_DARKNESS },
        { "Create Darkness", TERM_L_DARK }},
    { "PARALYZE", { MST_ANNOY, ANNOY_PARALYZE },
        { "Paralyze", TERM_RED,
          "%s stares deep into your eyes!",
          "%s mumbles."}},
    { "SCARE", { MST_ANNOY, ANNOY_SCARE },
        { "Terrify", TERM_RED,
          "%s casts a fearful illusion.",
          "%s mumbles, and you hear scary noises."}},
    { "SLOW", { MST_ANNOY, ANNOY_SLOW },
        { "Slow", TERM_L_UMBER,
          "%s drains power from your muscles!",
          "%s drains power from your muscles!"}},
    { "SHRIEK", { MST_ANNOY, ANNOY_SHRIEK },
        { "Shriek", TERM_L_BLUE,
          "%s makes a high pitched shriek.",
          "%s makes a high pitched shriek." }, MSF_INNATE },
    { "TELE_TO", { MST_ANNOY, ANNOY_TELE_TO },
        { "Teleport To", TERM_WHITE,
          "%s commands you to return.",
          "%s mumbles." }},
    { "TRAPS", { MST_ANNOY, ANNOY_TRAPS },
        { "Create Traps", TERM_WHITE,
          "%s casts a spell and cackles evilly.",
          "%s mumbles gleefully." }},
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
          "%s invokes anti-magic.",
          "%s mumbles powefully." }},
    { "DISPEL_MAGIC", { MST_BIFF, BIFF_DISPEL_MAGIC },
        { "Dispel Magic", TERM_L_BLUE,
          "%s invokes dispel magic.",
          "%s mumbles powefully." }},
    { "POLYMORPH", { MST_BIFF, BIFF_POLYMORPH },
        { "Polymorph Other", TERM_RED,
          "%s invokes polymorph other.",
          "%s mumbles powefully." }},
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
          "%s concentrates on its body.", /* XXX */
          "%s mumbles." }},
    { "INVULN", { MST_BUFF, BUFF_INVULN },
        { "Invulnerability", TERM_WHITE,
          "%s casts a Globe of Invulnerability",
          "%s mumbles powerfully." }},
    {0}
};

/* MST_BALL */
static _parse_t _ball_tbl[] = {
    { "BA_CHAOS", { MST_BALL, GF_CHAOS },
        { "Invoke Logrus", TERM_VIOLET,
          "%s invokes a raw logrus.",
          "%s mumbles frighteningly." }},
    { "BA_DARK", { MST_BALL, GF_DARK },
        { "Darkness Storm", TERM_L_DARK,
          "%s invokes a darkness storm.",
          "%s mumbles powerfully." }},
    { "BA_LITE", { MST_BALL, GF_LITE },
        { "Starburst", TERM_YELLOW,
          "%s invokes a starburst.",
          "%s mumbles powerfully." }},
    { "BA_MANA", { MST_BALL, GF_MANA },
        { "Mana Storm", TERM_L_BLUE,
          "%s invokes a mana storm.",
          "%s mumbles powerfully." }},
    { "BA_NUKE", { MST_BALL, GF_NUKE },
        { "Radiation Ball", TERM_L_GREEN,
          "%s casts a ball of radiation.",
          "%s mumbles." }},
    { "BA_POIS", { MST_BALL, GF_POIS },
        { "Stinking Cloud", TERM_L_GREEN,
          "%s casts a stinking cloud.",
          "%s mumbles." }},
    { "BA_WATER", { MST_BALL, GF_WATER },
        { "Whirlpool", TERM_L_BLUE,
          "%s gestures fluidly.",
          "%s mumbles.",
          "You are engulfed in a whirlpool."}},
    { "BRAIN_SMASH", { MST_BALL, GF_BRAIN_SMASH },
        { "Brain Smash", TERM_L_BLUE,
          "%s gazes deep into your eyes.",
          "You feel something focusing on your mind."}, MSF_BALL0 },
    { "DRAIN_MANA", { MST_BALL, GF_DRAIN_MANA },
        { "Drain Mana", TERM_L_BLUE}, MSF_BALL0 },
    { "MIND_BLAST", { MST_BALL, GF_MIND_BLAST },
        { "Mind Blast", TERM_L_BLUE,
          "%s gazes deep into your eyes.",
          "You feel something focusing on your mind."}, MSF_BALL0 },
    { "ROCKET", { MST_BALL, GF_ROCKET },
        { "Rocket", TERM_L_UMBER,
          "%s fires a rocket.",
          "%s shoots something." }, MSF_INNATE },
    { "THROW", { MST_BALL, GF_ROCK }, /* non-reflectable! */
        { "Throw Boulder", TERM_L_UMBER,
          "%s throws a large rock.",
          "%s shouts, 'Haaa!!'." }, MSF_INNATE | MSF_BALL0 },
    {0}
};

/* MST_BOLT */
static _parse_t _bolt_tbl[] = {
    { "GAZE", { MST_BOLT, GF_ATTACK },
        { "Gaze", TERM_RED,
          "%s gazes at you."}},
    { "MISSILE", { MST_BOLT, GF_MISSILE },
        { "Magic Missile", TERM_WHITE,
          "%s casts a magic missile.",
          "%s mumbles."}},
    { "SHOOT", { MST_BOLT, GF_ARROW },
        { "Shoot", TERM_L_UMBER,
          "%s fires an arrow.",
          "%s makes a strange noise." }, MSF_INNATE },
    {0}
};

/* MST_BEAM */
static _parse_t _beam_tbl[] = {
    { "PSY_SPEAR", { MST_BEAM, GF_PSY_SPEAR },
        { "Psycho-Spear", TERM_L_BLUE,
          "%s throws a Psycho-Spear.",
          "%s mumbles."}},
    {0}
};

/* MST_CURSE */
static _parse_t _curse_tbl[] = {
    { "CAUSE_1", { MST_CURSE, GF_CAUSE_1 },
        { "Cause Light Wounds", TERM_RED,
          "%s points at you and curses.",
          "%s curses."}},
    { "CAUSE_2", { MST_CURSE, GF_CAUSE_2 },
        { "Cause Serious Wounds", TERM_RED,
          "%s points at you and curses horribly.",
          "%s curses horribly."}},
    { "CAUSE_3", { MST_CURSE, GF_CAUSE_3 },
        { "Cause Critical Wounds", TERM_RED,
          "%s points at you, incanting terribly!"
          "%s incants terribly."}},
    { "CAUSE_4", { MST_CURSE, GF_CAUSE_4 },
        { "Cause Mortal Wounds", TERM_RED,
          "%s points at you, screaming the word DIE!",
          "%s screams the word DIE!"}},
    { "HAND_DOOM", { MST_CURSE, GF_HAND_DOOM },
        { "Hand of Doom", TERM_RED,
          "%s invokes the Hand of Doom!",
          "%s invokes the Hand of Doom!"}},
    {0}
};

/* MST_ESCAPE */
enum {
    ESCAPE_BLINK,
    ESCAPE_TELE_LEVEL,
    ESCAPE_TELE_SELF,
    ESCAPE_TELE_OTHER,
};
static _parse_t _escape_tbl[] = {
    { "BLINK", { MST_ESCAPE, ESCAPE_BLINK },
        { "Blink Self", TERM_WHITE }},
    { "TELE_LEVEL", { MST_ESCAPE, ESCAPE_TELE_LEVEL },
        { "Teleport Level", TERM_WHITE,
          "%s gestures at your feet.",
          "%s mumbles strangely."}},
    { "TELE_OTHER", { MST_ESCAPE, ESCAPE_TELE_OTHER },
        { "Teleport Away", TERM_WHITE }},
    { "TELE_SELF", { MST_ESCAPE, ESCAPE_TELE_SELF },
        { "Teleport", TERM_WHITE }},
    {0}
};

/* MST_HEAL */
enum {
    HEAL_SELF,
};
static _parse_t _heal_tbl[] = {
    { "HEAL", { MST_HEAL, HEAL_SELF },
        { "Heal Self", TERM_WHITE,
          "%s concentrates on .", /* XXX */
          "%s mumbles." }},
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
                         _heal_tbl, _weird_tbl, NULL};
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
    { MST_BREATHE, "Breathe", TERM_RED, 15 },
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
    { MST_WEIRD, "Weird", TERM_L_UMBER, 10 },
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
    if (which == GF_ROCKET)
    {
        parm.tag = MSP_HP_PCT;
        parm.v.hp_pct = _hp_pct(18, 600);
        return parm;
    }
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
    case GF_ROCK:
        parm.v.dice = _dice(0, 0, 3*rlev);
        break;
    default:
        assert(FALSE);
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

mon_spell_parm_t mon_spell_parm_default(mon_spell_id_t id, int rlev)
{
    mon_spell_parm_t empty = {0};

    switch (id.type)
    {
    case MST_BREATHE:
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
        int id = parse_summon_type(name + 2);
        if (!id && strcmp(name + 2, "MONSTER") != 0)
        {
            msg_format("Error: Unknown summon type %s.", name + 2);
            return PARSE_ERROR_GENERIC;
        }
        spell->id.type = MST_SUMMON;
        spell->id.effect = id;
    }
    else
    {
        _gf_info_ptr  gf;
        if (prefix(name, "BR_"))
        {
            spell->id.type = MST_BREATHE;
            spell->flags |= MSF_INNATE;
        }
        else if (prefix(name, "BA_"))
            spell->id.type = MST_BALL;
        else if (prefix(name, "BO_"))
            spell->id.type = MST_BOLT;
        else
        {
            msg_format("Error: Unkown spell %s.", name);
            return PARSE_ERROR_GENERIC;
        }
        gf = _gf_parse_name(name + 3);
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
        string_printf(s, "%d", spell->id.effect);
    }
    else
    {
        _gf_info_ptr gf = _gf_lookup(spell->id.effect);
        if (gf)
        {
            string_printf(s, "<color:%c>%s</color>",
                attr_to_attr_char(gf->color), gf->name);
        }
        else
            string_printf(s, "%d", spell->id.effect);
    }
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
    spells->dam_pct = 100;
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
    int i, j;
    for (i = 0; i < MST_COUNT; i++)
    {
        mon_spell_group_ptr group = spells->groups[i];
        if (!group) continue;
        for (j = 0; j < group->count; j++)
        {
            mon_spell_ptr spell = &group->spells[j];
            if (spell->id.type != id.type) continue;
            if (spell->id.effect != id.effect) continue;
            return spell;
        }
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

static void _spell_cast_init(mon_spell_cast_ptr cast, mon_ptr mon)
{
    char tmp[MAX_NLEN];

    cast->mon = mon;
    cast->race = &r_info[mon->ap_r_idx];
    cast->spell = NULL;
    cast->src = point(mon->fx, mon->fy);
    cast->dest = point(px, py);

    monster_desc(tmp, mon, 0);
    tmp[0] = toupper(tmp[0]);
    sprintf(cast->name, "<color:g>%s</color>", tmp);
}

bool mon_spell_cast(mon_ptr mon, mon_spell_ai ai)
{
    mon_spell_cast_t cast = {0};

    if (MON_CONFUSED(mon))
    {
        reset_target(mon);
        return FALSE;
    }
    if (mon->mflag & MFLAG_NICE) return FALSE;
    if (!is_hostile(mon)) return FALSE;
    if (!is_aware(mon)) return FALSE;
    if (!p_ptr->playing || p_ptr->is_dead) return FALSE;
    if (p_ptr->leaving) return FALSE;
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
    /* XXX */
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

/* Some monsters override the default message with something cutesy */
typedef struct {
    int race_id;
    mon_spell_id_t spell_id;
    cptr msg;
    bool allow_blind;
} _custom_msg_t, *_custom_msg_ptr;
static _custom_msg_t _mon_msg_tbl[] = {
    { MON_NINJA, {MST_BOLT, GF_ARROW}, "%s throws a syuriken." },
    { MON_JAIAN, {MST_BREATHE, GF_SOUND}, "'Booooeeeeee'", TRUE },
    { MON_BOTEI, {MST_BREATHE, GF_SHARDS}, "'Botei-Build cutter!!!'", TRUE },
    { MON_ROLENTO, {MST_BALL, GF_FIRE}, "%^s throws a hand grenade." },
    {0}
};

static bool _mon_custom_msg(mon_spell_ptr spell)
{
    int i;
    for (i = 0;; i++)
    {
        _custom_msg_ptr msg = &_mon_msg_tbl[i];
        if (!msg->race_id) return FALSE;
        if (msg->race_id != _current.race->id) continue;
        if (msg->spell_id.type != spell->id.type) continue;
        if (msg->spell_id.effect != spell->id.effect) continue;
        if (p_ptr->blind && !msg->allow_blind) return FALSE;
        msg_format(msg->msg, _current.name);
        return TRUE;
    }
}

/* Some spells override the default message with something more flavorful:
 * 'Mana Storm' rather than 'ball of mana' or 'Invoke Logrus' rather than
 * 'ball of chaos' (cf _parse_tbl) */
static bool _custom_msg(mon_spell_ptr spell)
{
    if (_mon_custom_msg(spell)) return TRUE;
    if (!spell->display) return FALSE;
    /* Note: Null messages in _parse_tbl give a mechanism to suppress
     * the default messaging. For example, DRAIN_MANA has historically
     * been quiet, relying on gf_damage_p for any messages. */
    if (p_ptr->blind)
    {
        if (spell->display->blind_msg)
            msg_format(spell->display->blind_msg, _current.name);
    }
    else
    {
        if (spell->display->cast_msg)
            msg_format(spell->display->cast_msg, _current.name);
    }
    if (spell->display->xtra_msg)
        msg_print(spell->display->xtra_msg);
    return TRUE;
}

static void _breathe(void)
{
    int dam;
    int pct = _current.spell->parm.v.hp_pct.pct;
    int max = _current.spell->parm.v.hp_pct.max;

    assert(_current.spell->parm.tag == MSP_HP_PCT);

    if (!_custom_msg(_current.spell))
    {
        if (p_ptr->blind)
            msg_format("%s roars!", _current.name);
        else
        {
            _gf_info_ptr gf = _gf_lookup(_current.spell->id.effect);
            char         color = attr_to_attr_char(gf->color);
            msg_format("%s breathes <color:%c>%s</color>.",
                _current.name, color, gf->name);
        }
    }

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
static int _scale(int amt)
{
    if (_current.race->spells->dam_pct)
        amt = amt * _current.race->spells->dam_pct / 100;
    return amt;
}

static cptr _a_an(cptr noun)
{
    if (strchr("aeiou", noun[0])) return "an";
    return "a";
}
static void _ball(void)
{
    int    dam;
    dice_t dice = _current.spell->parm.v.dice;
    int    rad = 0;
    int    flags = PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL | PROJECT_PLAYER;


    if (!_custom_msg(_current.spell))
    {
        if (p_ptr->blind)
            msg_format("%s mumbles.", _current.name);
        else
        {
            _gf_info_ptr gf = _gf_lookup(_current.spell->id.effect);
            char         color = attr_to_attr_char(gf->color);
            msg_format("%s casts %s <color:%c>%s Ball</color>.",
                _current.name, _a_an(gf->name), color, gf->name);
        }
    }

    if (_current.spell->id.effect == GF_ROCKET) /* odd ball */
    {
        int pct = _current.spell->parm.v.hp_pct.pct;
        int max = _current.spell->parm.v.hp_pct.max;
        assert(_current.spell->parm.tag == MSP_HP_PCT);
        flags |= PROJECT_STOP;
        rad = 2;
        dam = _current.mon->hp * pct / 100;
        if (dam > max) dam = max;
    }
    else
    {
        assert(_current.spell->parm.tag == MSP_DICE);
        dam = _scale(_roll(dice));
        if (!(_current.spell->flags & MSF_BALL0))
        {
            /* XXX This was previously set on a spell by spell basis ... */
            if (dam > 300) rad = 4;
            else if (dam > 150) rad = 3;
            else rad = 2;
        }
    }
    switch (_current.spell->id.effect)
    {
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

    if (!_custom_msg(_current.spell))
    {
        if (p_ptr->blind)
            msg_format("%s mumbles.", _current.name);
        else
        {
            _gf_info_ptr gf = _gf_lookup(_current.spell->id.effect);
            char         color = attr_to_attr_char(gf->color);
            msg_format("%s casts %s <color:%c>%s Bolt</color>.",
                _current.name, _a_an(gf->name), color, gf->name);
        }
    }

    if (_current.race->id == MON_ARTEMIS && _spell_is_(_current.spell, MST_BOLT, GF_ARROW))
    {
        ct = 4;
        flags &= ~PROJECT_REFLECTABLE;
    }
    for (i = 0; i < ct; i++)
    {
        project(
            _current.mon->id,
            0,
            _current.dest.y,
            _current.dest.x,
            _scale(_roll(_current.spell->parm.v.dice)),
            _current.spell->id.effect,
            flags,
            -1
        );
    }
}
static void _beam(void)
{
    assert(_current.spell->parm.tag == MSP_DICE);

    if (!_custom_msg(_current.spell))
    {
        if (p_ptr->blind)
            msg_format("%s mumbles.", _current.name);
        else
        {
            _gf_info_ptr gf = _gf_lookup(_current.spell->id.effect);
            char         color = attr_to_attr_char(gf->color);
            msg_format("%s casts %s <color:%c>%s Beam</color>.",
                _current.name, _a_an(gf->name), color, gf->name);
        }
    }

    project(
        _current.mon->id,
        0,
        _current.dest.y,
        _current.dest.x,
        _scale(_roll(_current.spell->parm.v.dice)),
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
    else
        dam = _scale(dam);
    _custom_msg(_current.spell);
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
    int roll = 100 + _current.race->level;
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
    _custom_msg(_current.spell);
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
        break;
    case ANNOY_CONFUSE:
        if (res_save_default(RES_CONF) || _curse_save())
            msg_print("You disbelieve the feeble spell.");
        else
            set_confused(p_ptr->confused + randint0(4) + 4, FALSE);
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
        if (p_ptr->free_act)
        {
            msg_print("You are unaffected!");
            equip_learn_flag(OF_FREE_ACT);
        }
        else if (_curse_save())
            msg_print("You resist the effects!");
        else
            set_paralyzed(randint1(3), FALSE);
        break;
    case ANNOY_SCARE:
        fear_scare_p(_current.mon);
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
    _custom_msg(_current.spell);
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
    _custom_msg(_current.spell);
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
    _custom_msg(_current.spell);
    switch (_current.spell->id.effect)
    {
    case ESCAPE_BLINK:
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
    case ESCAPE_TELE_LEVEL:
        if (res_save_default(RES_NEXUS) || _curse_save())
        {
            msg_print("You resist the effects!");
            update_smart_learn(_current.mon->id, RES_NEXUS);
        }
        else
            teleport_level(0);
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
        else
            msg_format("%s looks sounds healed!", _current.name); /* XXX */
    }
    else
    {
        if (!p_ptr->blind && mon->ml)
            msg_format("%s looks healther.", _current.name); /* XXX */
        else
            msg_format("%s looks sounds healthier.", _current.name); /* XXX */
    }
    check_mon_health_redraw(mon->id);
    if (MON_MONFEAR(mon))
    {
        set_monster_monfear(mon->id, 0);
        msg_format("%^s recovers its courage.", _current.name); /* XXX */
    }
}
static void _heal(void)
{
    int amt;
    _custom_msg(_current.spell);
    assert(_current.spell->parm.tag == MSP_DICE);
    amt = _roll(_current.spell->parm.v.dice);
    hp_mon(_current.mon, amt);
}
static void _summon_r_idx(int r_idx)
{
    summon_named_creature(
        _current.mon->id,
        _current.dest.y,
        _current.dest.x,
        r_idx,
        PM_ALLOW_GROUP | PM_ALLOW_UNIQUE
    );
}
static void _summon_type(int type)
{
    summon_specific(
        _current.mon->id,
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
    if (!_custom_msg(_current.spell))
        msg_format("%s summons help.", _current.name);
    assert(_current.spell->parm.tag == MSP_DICE);
    ct = _roll(_current.spell->parm.v.dice);
    for (i = 0; i < ct; i++)
        _summon_type(_current.spell->id.effect);
}
static void _weird(void)
{
}

static void _spell_cast_aux(void)
{
    disturb(1, 0);
    reset_target(_current.mon);
    if (_spell_fail() || _spell_blocked())
        return;

    switch (_current.spell->id.type)
    {
    case MST_ANNOY:   _annoy();   break;
    case MST_BALL:    _ball();    break;
    case MST_BEAM:    _beam();    break;
    case MST_BIFF:    _biff();    break;
    case MST_BOLT:    _bolt();    break;
    case MST_BREATHE: _breathe(); break;
    case MST_BUFF:    _buff();    break;
    case MST_CURSE:   _curse();   break;
    case MST_ESCAPE:  _escape();  break;
    case MST_HEAL:    _heal();    break;
    case MST_SUMMON:  _summon();  break;
    case MST_WEIRD:   _weird();   break;
    }
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
    case MST_BREATHE:
    case MST_BALL:
    case MST_BOLT:
    case MST_BEAM:
    case MST_CURSE: return TRUE;
    }
    return _spell_is_(spell, MST_ANNOY, ANNOY_TRAPS);
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

static void _smart_remove_aux(mon_spell_group_ptr group, u32b flags)
{
    int i;
    if (!group) return;
    for (i = 0; i < group->count; i++)
    {
        mon_spell_ptr spell = &group->spells[i];
        _gf_info_ptr  gf = _gf_lookup(spell->id.effect);
        if (!gf) continue; /* GF_ARROW? */
        if (gf->resist != RES_INVALID && _have_smart_flag(flags, gf->resist))
        {
            int pct = res_pct(gf->resist);
            int tweak = 100 - pct;
            spell->prob = MIN(200, spell->prob*tweak/100);
        }
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
    _smart_remove_aux(spells->groups[MST_BREATHE], flags);
    _smart_remove_aux(spells->groups[MST_BALL], flags);
    if (_have_smart_flag(flags, SM_REFLETION) && p_ptr->reflect)
        _remove_group(spells->groups[MST_BOLT], NULL);
    else
        _smart_remove_aux(spells->groups[MST_BOLT], flags);
    _smart_remove_aux(spells->groups[MST_BEAM], flags);

    if (_have_smart_flag(flags, SM_FREE_ACTION) && p_ptr->free_act)
    {
        _remove_spell(spells, _id(MST_ANNOY, ANNOY_PARALYZE));
        _remove_spell(spells, _id(MST_ANNOY, ANNOY_SLOW));
    }
    if (_have_smart_flag(flags, SM_DRAIN_MANA) && !p_ptr->csp)
        _remove_spell(spells, _id(MST_BALL, GF_DRAIN_MANA));
}

static bool _clean_shot(point_t src, point_t dest)
{
    return clean_shot(src.y, src.x, dest.y, dest.x, FALSE);
}
static bool _summon_possible(point_t where)
{
    return summon_possible(where.y, where.x);
}

static void _remove_bad_spells(mon_spell_cast_ptr cast)
{
    bool           stupid = BOOL(cast->race->flags2 & RF2_STUPID);
    bool           smart  = BOOL(cast->race->flags2 & RF2_SMART);
    bool           splash = FALSE;
    mon_spells_ptr spells = cast->race->spells;
    mon_spell_ptr  spell;

    /* Apply monster knowledge of player's strengths and weaknesses */
    if (!stupid && (smart_cheat || smart_learn))
        _smart_remove(cast);

    /* Require a projectable player for projection spells. Even stupid
     * monsters are dumb enough to bounce spells off walls! Non-stupid
     * monsters will splash the player if possible, though with reduced
     * odds. */
    if (!_projectable(cast->src, cast->dest))
    {
        point_t new_dest = {0};
        
        if (!stupid)
            new_dest = _choose_splash_point(cast->src, cast->dest, _projectable_splash);

        _remove_group(spells->groups[MST_ANNOY], NULL);
        _remove_group(spells->groups[MST_BOLT], NULL);
        _remove_group(spells->groups[MST_BEAM], NULL);
        _remove_group(spells->groups[MST_BIFF], NULL);
        _remove_group(spells->groups[MST_CURSE], NULL);

        if (new_dest.x || new_dest.y) /* XXX assume (0,0) out of bounds */
        {
            cast->dest = new_dest;
            splash = TRUE;
            _adjust_group(spells->groups[MST_BREATHE], NULL, 50);
            _adjust_group(spells->groups[MST_BALL], NULL, 50);
            _adjust_group(spells->groups[MST_BALL], _ball0_p, 0);
            _adjust_group(spells->groups[MST_SUMMON], NULL, 50);
        }
        else
        {
            _remove_group(spells->groups[MST_BREATHE], NULL);
            _remove_group(spells->groups[MST_BALL], NULL);
            _remove_group(spells->groups[MST_SUMMON], NULL);
            spell = mon_spells_find(spells, _id(MST_BREATHE, GF_DISINTEGRATE));
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

    /* When wounded, skew the spell probabilities away from offense
     * and trickery to summoning, healing and escape tactics. Make
     * this transition gradual but allow smart monsters to panic near
     * death. */
    if ( spells->groups[MST_HEAL]
      || spells->groups[MST_ESCAPE]
      || spells->groups[MST_SUMMON] )
    {
        int pct_healthy = cast->mon->hp * 100 / cast->mon->maxhp;
        int pct_wounded = 100 - pct_healthy;
        if (pct_wounded > 20)
        {
            int offence = pct_healthy;
            int panic = 5 * pct_wounded;
            if (smart && pct_wounded > 90 && one_in_(2))
            {
                offence = 0;
                panic *= 3;
                _adjust_group(spells->groups[MST_SUMMON], NULL, 5*pct_wounded);
            }
            _adjust_group(spells->groups[MST_BREATHE], NULL, offence);
            _adjust_group(spells->groups[MST_BALL], NULL, offence);
            _adjust_group(spells->groups[MST_BOLT], NULL, offence);
            _adjust_group(spells->groups[MST_BEAM], NULL, offence);
            _adjust_group(spells->groups[MST_CURSE], NULL, offence);
            _adjust_group(spells->groups[MST_BIFF], NULL, offence);
            _adjust_group(spells->groups[MST_ANNOY], NULL, offence);
            _adjust_group(spells->groups[MST_WEIRD], NULL, offence);

            _adjust_group(spells->groups[MST_HEAL], NULL, panic);
            _adjust_group(spells->groups[MST_ESCAPE], NULL, panic);
        }
        else /*if (pct_wounded <= 20)*/
        {
            _remove_group(spells->groups[MST_HEAL], NULL);
            _remove_group(spells->groups[MST_ESCAPE], NULL);
        }
    }

    if (smart)
    {
        spell = mon_spells_find(spells, _id(MST_ESCAPE, ESCAPE_TELE_LEVEL));
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

    /* Blink away if too close and good offense available */
    spell = mon_spells_find(spells, _id(MST_ESCAPE, ESCAPE_BLINK));
    if (spell && _distance(cast->src, cast->dest) < 4 && _find_spell(spells, _blink_check_p))
        spell->prob = 100;

    /* Useless buffs? */
    spell = mon_spells_find(spells, _id(MST_BUFF, BUFF_INVULN));
    if (spell && cast->mon->mtimed[MTIMED_INVULNER])
        spell->prob = 0;

    spell = mon_spells_find(spells, _id(MST_BUFF, BUFF_HASTE));
    if (spell && cast->mon->mtimed[MTIMED_FAST])
        spell->prob = 0;

    if (!p_ptr->csp) /* XXX Historical: Bypass SM_DRAIN_MANA? */
        _remove_spell(spells, _id(MST_BALL, GF_DRAIN_MANA));

    /* require a direct shot to player for bolts */
    if (!splash && spells->groups[MST_BOLT] && !_clean_shot(cast->src, cast->dest))
        _remove_group(spells->groups[MST_BOLT], NULL);

    if (spells->groups[MST_SUMMON] && !_summon_possible(cast->dest))
        _remove_group(spells->groups[MST_SUMMON], NULL);

    spell = mon_spells_find(spells, _id(MST_ANNOY, ANNOY_ANIMATE_DEAD));
    if (spell && !raise_possible(cast->mon))
        spell->prob = 0;

    /* XXX Invulnerable => Psychospear */
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

void mon_spell_wizard(mon_ptr mon, mon_spell_ai ai, doc_ptr doc)
{
    mon_spell_cast_t cast = {0};
    int              i, j, total = 0;
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
            doc_printf(doc, "<tab:20>%2d.%d%% ", spell->prob * 100 / total, (spell->prob * 1000 / total) % 10);
            mon_spell_doc(spell, doc);
            if (spell->parm.tag == MSP_DICE)
            {
                dice_t dice = spell->parm.v.dice;
                doc_insert(doc, "<tab:40>");
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
                doc_printf(doc, "<tab:40>%d", dam);
            }
            doc_newline(doc);
        }
    }
}
