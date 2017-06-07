#ifndef INCLUDED_GF_H
#define INCLUDED_GF_H

/* Player and monster effects. GF_ is a mysterious historical abbreviation */
#define GF_ELEC         1
#define GF_POIS         2
#define GF_ACID         3
#define GF_COLD         4
#define GF_FIRE         5
#define GF_PSY_SPEAR    9
#define GF_MISSILE      10
#define GF_ARROW        11
#define GF_PLASMA       12
#define GF_WATER        14
#define GF_LITE         15
#define GF_DARK         16
#define GF_LITE_WEAK    17  /* Only hurts monsters vulnerable to light ... */
#define GF_DARK_WEAK    18  /* Only hurts monsters vulnerable to dark ... */
#define GF_SHARDS       20
#define GF_SOUND        21
#define GF_CONFUSION    22
#define GF_FORCE        23
#define GF_INERT        24  /* M$ now uses GF_INERTIA for mouse gestures */
#define GF_MANA         26
#define GF_METEOR       27
#define GF_ICE          28
#define GF_CHAOS        30
#define GF_NETHER       31
#define GF_DISENCHANT   32
#define GF_NEXUS        33
#define GF_TIME         34
#define GF_GRAVITY      35
#define GF_KILL_WALL    40
#define GF_KILL_DOOR    41
#define GF_KILL_TRAP    42
#define GF_MAKE_WALL    45
#define GF_MAKE_DOOR    46
#define GF_MAKE_TRAP    47
#define GF_MAKE_TREE    48
#define GF_OLD_BLIND    49  /* HACK */
#define GF_OLD_CLONE    51
#define GF_OLD_POLY     52
#define GF_OLD_HEAL     53
#define GF_OLD_SPEED    54
#define GF_OLD_SLOW     55
#define GF_OLD_CONF     56
#define GF_OLD_SLEEP    57
#define GF_OLD_DRAIN    58
#define GF_AWAY_UNDEAD  61
#define GF_AWAY_EVIL    62
#define GF_AWAY_ALL     63
#define GF_TURN_UNDEAD  64
#define GF_TURN_EVIL    65
#define GF_TURN_ALL     66
#define GF_DISP_UNDEAD  67
#define GF_DISP_EVIL    68
#define GF_DISP_ALL     69
#define GF_DISP_DEMON   70      /* New types for Zangband begin here... */
#define GF_DISP_LIVING  71
#define GF_ROCKET       72
#define GF_NUKE         73
#define GF_MAKE_GLYPH   74
#define GF_STASIS       75
#define GF_STONE_WALL   76
#define GF_DEATH_RAY    77
#define GF_STUN         78
#define GF_HOLY_FIRE    79
#define GF_HELL_FIRE    80
#define GF_DISINTEGRATE 81
#define GF_CHARM        82
#define GF_CONTROL_UNDEAD   83
#define GF_CONTROL_ANIMAL   84
#define GF_PSI         85
#define GF_PSI_DRAIN   86
#define GF_TELEKINESIS  87
#define GF_JAM_DOOR     88
#define GF_DOMINATION   89
#define GF_DISP_GOOD    90
#define GF_DRAIN_MANA   91
#define GF_MIND_BLAST   92
#define GF_BRAIN_SMASH  93
#define GF_CAUSE_1      94
#define GF_CAUSE_2      95
#define GF_CAUSE_3      96
#define GF_CAUSE_4      97
#define GF_HAND_DOOM    98
#define GF_CAPTURE      99
#define GF_ANIM_DEAD   100
#define GF_CONTROL_LIVING   101
#define GF_IDENTIFY    102
#define GF_ATTACK      103
#define GF_ENGETSU     104
#define GF_GENOCIDE    105
#define GF_PHOTO       106
#define GF_CONTROL_DEMON   107
#define GF_LAVA_FLOW   108
#define GF_BLOOD_CURSE 109
#define GF_SEEKER 110
#define GF_SUPER_RAY 111
#define GF_STAR_HEAL 112
#define GF_WATER_FLOW   113
#define GF_CRUSADE     114
#define GF_STASIS_EVIL 115
#define GF_WOUNDS      116
#define GF_BLOOD       117
#define GF_ELDRITCH            118
#define GF_ELDRITCH_STUN    119
#define GF_ELDRITCH_DRAIN    120
#define GF_ELDRITCH_DISPEL    121
#define GF_ELDRITCH_CONFUSE    122
#define GF_REMOVE_OBSTACLE    123
#define GF_PHARAOHS_CURSE   124
#define GF_ISOLATION        125
#define GF_ELDRITCH_HOWL 126
#define GF_ENTOMB    127
#define GF_UNHOLY_WORD  128
#define GF_DRAINING_TOUCH 129
#define GF_DEATH_TOUCH 130
#define GF_PSI_EGO_WHIP 131
#define GF_PSI_BRAIN_SMASH 132
#define GF_PSI_STORM 133
#define GF_MANA_CLASH 134
#define GF_ANTIMAGIC 135
#define GF_ROCK      136
#define GF_WEB      137
#define GF_AMNESIA  138
#define GF_STEAL    139
#define GF_WATER2   140
#define GF_STORM    141
#define GF_QUAKE    142
#define GF_CHARM_RING_BEARER  143
#define GF_SUBJUGATION 144
#define GF_PARALYSIS 145
#define GF_CONTROL_PACT_MONSTER  146

#define MAX_GF                147


typedef struct {
    int  id;
    cptr name;
    byte color;
    int  resist;
    cptr parse;
} gf_info_t, *gf_info_ptr;

extern gf_info_ptr gf_parse_name(cptr token);
extern gf_info_ptr gf_lookup(int id);

/* Directly damage the player or a monster with a GF_* attack type, 
 * bypassing the project() code. This is needed for monster melee,
 * monster auras, and player innate attacks where project() is overly
 * complicated and error prone if flg is incorrectly set.
 *
 * We still need a "who" done it parameter. This is either the player (0)
 * or a monster index (m_idx > 0). Occasionally, it might be a special
 * negative code when using project(), but not for the gf_damage_* routines.
 */
#define GF_WHO_PLAYER      0  /* same as PROJECT_WHO_PLAYER */

/* We also need information on whether the effect is spell/breath based,
 * or whether it is the result of melee contact. Mostly, this if for message
 * purposes, but, occasionally, it has gameplay effects. For example, GF_DISENCHANT
 * will usually dispel_player() on a touch effect (GF_DAMAGE_ATTACK or _AURA)
 * while it always apply_disenchant()s on a breath (GF_DAMAGE_SPELL).
 * The flags param only contains the following GF_DAMAGE_* flags, and never
 * needs any of the PROJECT_* stuff. At the moment, using bits is inappropriate
 * as the options are mutually exclusive, but other options may follow in the future.
 */
#define GF_DAMAGE_SPELL  0x01 /* Monster spell or breath from a project() */
#define GF_DAMAGE_ATTACK 0x02 /* Monster melee B:HIT:HURT(10d10):DISENCHANT */
#define GF_DAMAGE_AURA   0x04 /* Monster aura  A:DISENCHANT(3d5) */
extern int gf_damage_p(int who, int type, int dam, int flags);
extern bool gf_damage_m(int who, point_t where, int type, int dam, int flags);

/* exposed for the sake of wizard commands: calculate damage based
 * on the player's alignment */
extern int gf_holy_dam(int dam);
extern int gf_hell_dam(int dam);

/* XXX Remove these ... */
extern int gf_distance_hack;
#endif
