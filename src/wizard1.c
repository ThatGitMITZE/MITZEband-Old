/* File: wizard1.c */

/*
 * Copyright (c) 1997 Ben Harrison, and others
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies. Other copyrights may also apply.
 */

/* Purpose: Spoiler generation -BEN-
 * This file has been mostly rewritten. If something is broke,
 * don't blame -BEN- :) */

#include "angband.h"
#include <math.h>

#ifdef ALLOW_SPOILERS


/*
 * The spoiler file being created
 */
static FILE *fff = NULL;




/*
 * A tval grouper
 */
typedef struct
{
    byte tval;
    cptr name;
} grouper;



/*
 * Item Spoilers by: benh@phial.com (Ben Harrison)
 */


/*
 * The basic items categorized by type
 */
static grouper group_item[] =
{
    { TV_SHOT,          "Ammo" },

    { TV_ARROW,         NULL },
    { TV_BOLT,          NULL },

    { TV_BOW,           "Bows" },

    { TV_DIGGING,       "Weapons" },

    { TV_POLEARM,       NULL },
    { TV_HAFTED,        NULL },
    { TV_SWORD,         NULL },

    { TV_SOFT_ARMOR,    "Armour (Body)" },

    { TV_HARD_ARMOR,    NULL },
    { TV_DRAG_ARMOR,    NULL },

    { TV_BOOTS,         "Armour (Misc)" },

    { TV_GLOVES,        NULL },
    { TV_HELM,          NULL },
    { TV_CROWN,         NULL },
    { TV_SHIELD,        NULL },
    { TV_CLOAK,         NULL },

    { TV_LITE,          "Light Sources" },
    { TV_AMULET,        "Amulets" },
    { TV_RING,          "Rings" },

    { TV_STAFF,         "Staffs" },
    { TV_WAND,          "Wands" },
    { TV_ROD,           "Rods" },

    { TV_SCROLL,        "Scrolls" },
    { TV_POTION,        "Potions" },
    { TV_FOOD,          "Food" },

    { TV_LIFE_BOOK,     "Books (Life)" },
    { TV_SORCERY_BOOK,  "Books (Sorcery)" },
    { TV_NATURE_BOOK,   "Books (Nature)" },
    { TV_CHAOS_BOOK,    "Books (Chaos)" },
    { TV_DEATH_BOOK,    "Books (Death)" },
    { TV_TRUMP_BOOK,    "Books (Trump)" },
    { TV_ARCANE_BOOK,   "Books (Arcane)" },
    { TV_CRAFT_BOOK,    "Books (Craft)" },
    { TV_DAEMON_BOOK,   "Books (Daemon)" },
    { TV_CRUSADE_BOOK,  "Books (Crusade)" },
    { TV_NECROMANCY_BOOK, "Books (Necromancy)" },
    { TV_ARMAGEDDON_BOOK, "Books (Armageddon)" },
    { TV_MUSIC_BOOK,    "Song Books" },
    { TV_HISSATSU_BOOK, "Books (Kendo)" },
    { TV_HEX_BOOK,      "Books (Hex)" },
    { TV_RAGE_BOOK,      "Books (Rage)" },

    { TV_WHISTLE,       "Whistle" },
    { TV_CAPTURE,       "Capture Ball" },
    { TV_CARD,          "Express Card" },

    { TV_CHEST,         "Chests" },

    { TV_FIGURINE,      "Magical Figurines" },
    { TV_STATUE,        "Statues" },
    { TV_CORPSE,        "Corpses" },

    { TV_SKELETON,      "Misc" },

    { TV_BOTTLE,        NULL },
    { TV_JUNK,          NULL },
    { TV_SPIKE,         NULL },
    { TV_FLASK,         NULL },
    { TV_PARCHMENT,     NULL },

    { 0, "" }
};


/*
 * Describe the kind
 */
static void kind_info(char *buf, char *dam, char *wgt, int *lev, s32b *val, int k)
{
    object_type forge;
    object_type *q_ptr;


    /* Get local object */
    q_ptr = &forge;

    /* Prepare a fake item */
    object_prep(q_ptr, k);

    /* It is known */
    q_ptr->ident |= (IDENT_KNOWN);

    /* Cancel bonuses */
    q_ptr->pval = 0;
    q_ptr->to_a = 0;
    q_ptr->to_h = 0;
    q_ptr->to_d = 0;


    /* Level */
    (*lev) = k_info[q_ptr->k_idx].level;

    /* Value */
    (*val) = obj_value(q_ptr);


    /* Hack */
    if (!buf || !dam || !wgt) return;


    /* Description (too brief) */
    object_desc(buf, q_ptr, (OD_NAME_ONLY | OD_STORE));


    /* Misc info */
    strcpy(dam, "");

    /* Damage */
    switch (q_ptr->tval)
    {
        /* Bows */
        case TV_BOW:
        {
            break;
        }

        /* Ammo */
        case TV_SHOT:
        case TV_BOLT:
        case TV_ARROW:
        {
            sprintf(dam, "%dd%d", q_ptr->dd, q_ptr->ds);
            break;
        }

        /* Weapons */
        case TV_HAFTED:
        case TV_POLEARM:
        case TV_SWORD:
        case TV_DIGGING:
        {
            sprintf(dam, "%dd%d", q_ptr->dd, q_ptr->ds);
            break;
        }

        /* Armour */
        case TV_BOOTS:
        case TV_GLOVES:
        case TV_CLOAK:
        case TV_CROWN:
        case TV_HELM:
        case TV_SHIELD:
        case TV_SOFT_ARMOR:
        case TV_HARD_ARMOR:
        case TV_DRAG_ARMOR:
        {
            sprintf(dam, "%d", q_ptr->ac);
            break;
        }
    }


    /* Weight */
    sprintf(wgt, "%3d.%d", q_ptr->weight / 10, q_ptr->weight % 10);
}


/*
 * Create a spoiler file for items
 */
static void spoil_obj_desc(cptr fname)
{
    int i, k, s, t, n = 0, group_start = 0;

    u16b who[200];

    char buf[1024];

    char wgt[80];
    char dam[80];


    /* Build the filename */
    path_build(buf, sizeof(buf), ANGBAND_DIR_USER, fname);

    /* File type is "TEXT" */
    FILE_TYPE(FILE_TYPE_TEXT);

    /* Open the file */
    fff = my_fopen(buf, "w");

    /* Oops */
    if (!fff)
    {
        msg_print("Cannot create spoiler file.");
        return;
    }


    /* Header */
    fprintf(fff, "Spoiler File -- Basic Items (PosChengband %d.%d.%d)\n\n\n",
        VER_MAJOR, VER_MINOR, VER_PATCH);

    /* More Header */
    fprintf(fff, "%-45s     %8s%7s%5s%9s\n",
        "Description", "Dam/AC", "Wgt", "Lev", "Cost");
    fprintf(fff, "%-45s     %8s%7s%5s%9s\n",
        "----------------------------------------",
        "------", "---", "---", "----");

    /* List the groups */
    for (i = 0; TRUE; i++)
    {
        /* Write out the group title */
        if (group_item[i].name)
        {
            if (n)
            {
                /* Hack -- bubble-sort by cost and then level */
                for (s = 0; s < n - 1; s++)
                {
                    for (t = 0; t < n - 1; t++)
                    {
                        int i1 = t;
                        int i2 = t + 1;

                        int e1;
                        int e2;

                        s32b t1;
                        s32b t2;

                        kind_info(NULL, NULL, NULL, &e1, &t1, who[i1]);
                        kind_info(NULL, NULL, NULL, &e2, &t2, who[i2]);

                        if ((t1 > t2) || ((t1 == t2) && (e1 > e2)))
                        {
                            int tmp = who[i1];
                            who[i1] = who[i2];
                            who[i2] = tmp;
                        }
                    }
                }

                fprintf(fff, "\n\n%s\n\n", group_item[group_start].name);

                /* Spoil each item */
                for (s = 0; s < n; s++)
                {
                    int e;
                    s32b v;

                    /* Describe the kind */
                    kind_info(buf, dam, wgt, &e, &v, who[s]);

                    /* Dump it */
                    fprintf(fff, "     %-45s%8s%7s%5d%9d\n",
                        buf, dam, wgt, e, v);
                }

                /* Start a new set */
                n = 0;
            }

            /* Notice the end */
            if (!group_item[i].tval) break;

            /* Start a new set */
            group_start = i;
        }

        /* Acquire legal item types */
        for (k = 1; k < max_k_idx; k++)
        {
            object_kind *k_ptr = &k_info[k];

            /* Skip wrong tval's */
            if (k_ptr->tval != group_item[i].tval) continue;

            /* Hack -- Skip instant-artifacts */
            if (k_ptr->gen_flags & (OFG_INSTA_ART)) continue;

            /* Save the index */
            who[n++] = k;
        }
    }


    /* Check for errors */
    if (ferror(fff) || my_fclose(fff))
    {
        msg_print("Cannot close spoiler file.");
        return;
    }

    /* Message */
    msg_print("Successfully created a spoiler file.");
}

/************************************************************************
 * Object and Artifact Tables
 * Note: The Object Tables are designed to be used with the wizard
 *       commands for gathering statistics. Create a new character,
 *       gather statistics and then display the Object tables. Egos
 *       and rand-arts are forgotten when you restart the game.
 ************************************************************************/
/*
 * The artifacts categorized by type
 */
static grouper group_artifact[] =
{
    { TV_SWORD,             "Edged-Weapons" },
    { TV_POLEARM,           "Polearms" },
    { TV_HAFTED,            "Hafted-Weapons" },
    { TV_DIGGING,           "Diggers" },
    { TV_BOW,               "Bows" },
    { TV_ARROW,             "Ammo" },

    { TV_SOFT_ARMOR,        "Body-Armor" },
    { TV_HARD_ARMOR,        NULL },
    { TV_DRAG_ARMOR,        NULL },

    { TV_CLOAK,             "Cloaks" },
    { TV_SHIELD,            "Shields" },
    { TV_CARD,              NULL },
    { TV_HELM,              "Headgear" },
    { TV_CROWN,             NULL },
    { TV_GLOVES,            "Gloves" },
    { TV_BOOTS,             "Boots" },

    { TV_LITE,              "Light-Sources" },
    { TV_AMULET,            "Amulets" },
    { TV_RING,              "Rings" },

    { 0, NULL }
};

/*
 * Hack -- Create a "forged" artifact
 * Compare to create_named_art_aux() in artifact.c
 */
static bool make_fake_artifact(object_type *o_ptr, int name1)
{
    int i;

    artifact_type *a_ptr = &a_info[name1];


    /* Ignore "empty" artifacts */
    if (!a_ptr->name) return FALSE;

    /* Acquire the "kind" index */
    i = lookup_kind(a_ptr->tval, a_ptr->sval);

    /* Oops */
    if (!i) return (FALSE);

    /* Create the artifact */
    object_prep(o_ptr, i);

    /* Save the name */
    o_ptr->name1 = name1;

    /* Extract the fields */
    o_ptr->pval = a_ptr->pval;
    o_ptr->ac = a_ptr->ac;
    o_ptr->dd = a_ptr->dd;
    o_ptr->ds = a_ptr->ds;
    o_ptr->mult = a_ptr->mult;
    o_ptr->to_a = a_ptr->to_a;
    o_ptr->to_h = a_ptr->to_h;
    o_ptr->to_d = a_ptr->to_d;
    o_ptr->weight = a_ptr->weight;

    /* Success */
    return (TRUE);
}

static void spoil_artifact_desc(void)
{
    int i,j;
    doc_ptr doc = doc_alloc(80);

    spoiler_hack = TRUE;
    for (i = 0; group_artifact[i].tval; i++)
    {
        if (group_artifact[i].name)
        {
            if (i) doc_insert(doc, "</indent>\n");
            doc_printf(doc, "<topic:%s><style:heading>%s</style>\n  <indent>\n",
                group_artifact[i].name, group_artifact[i].name);
        }

        for (j = 1; j < max_a_idx; ++j)
        {
            artifact_type *a_ptr = &a_info[j];
            object_type    forge = {0};

            if (a_ptr->tval != group_artifact[i].tval) continue;

            if (!make_fake_artifact(&forge, j)) continue;

            obj_identify_fully(&forge);

            obj_display_doc(&forge, doc);
            doc_newline(doc);
        }
    }

    doc_display(doc, "Artifact Spoilers", 0);
    doc_free(doc);
    spoiler_hack = FALSE;
}

#define ART_RANDOM -1
#define ART_EGO    -2

typedef struct {
    int  id;
    char name[MAX_NLEN];
    int  score;
    int  k_idx; /* For rand-arts and egos ... */
    int  level;
    int  pct;   /* replacement arts */
} _art_info_t, *_art_info_ptr;
static int _art_score_cmp(_art_info_ptr l, _art_info_ptr r)
{
    if (l->score > r->score)
        return -1;
    if (l->score < r->score)
        return 1;
    return 0;
}

static int _term_width(void)
{
    int cx, cy;
    Term_get_size(&cx, &cy);
    return cx;
}

typedef bool (*_obj_p)(object_type *o_ptr);

#define _SPOIL_ARTS      0x01
#define _SPOIL_RAND_ARTS 0x02
#define _SPOIL_EGOS      0x04
static void _spoil_table_aux(doc_ptr doc, cptr title, _obj_p pred, int options)
{
    int i;
    vec_ptr entries = vec_alloc(free);
    int     ct_std = 0, ct_rnd = 0, ct_ego = 0;
    int     score_std = 0, score_rnd = 0, score_ego = 0;
    int     max_score_std = 0, max_score_rnd = 0, max_score_ego = 0;

    if ((options & _SPOIL_ARTS) /* FIXED_ART ... && (!random_artifacts || random_artifact_pct < 100)*/)
    {
        for (i = 1; i < max_a_idx; ++i)
        {
            object_type    forge = {0};
            _art_info_ptr  entry;

            if (!p_ptr->wizard && (a_info[i].gen_flags & OFG_QUESTITEM)) continue;
            if (!create_named_art_aux(i, &forge)) continue;
            if ((options & _SPOIL_EGOS) && !a_info[i].found) continue; /* Hack */
            if (pred && !pred(&forge)) continue;

            obj_identify_fully(&forge);

            entry = malloc(sizeof(_art_info_t));
            entry->id = i;
            if (p_ptr->prace == RACE_ANDROID)
            {
                entry->score = android_obj_exp(&forge);
                if (!entry->score)
                    entry->score = obj_value_real(&forge);
            }
            else
                entry->score = obj_value_real(&forge);
            object_desc(entry->name, &forge, OD_COLOR_CODED);
            entry->k_idx = forge.k_idx;
            entry->level = a_info[i].level;
            vec_add(entries, entry);

            if (a_info[entry->id].found)
            {
                ct_std++;
                score_std += entry->score;
                if (entry->score > max_score_std)
                    max_score_std = entry->score;
            }
        }
    }
    if (options & _SPOIL_RAND_ARTS)
    {
        vec_ptr v = stats_rand_arts();
        for (i = 0; i < vec_length(v); i++)
        {
            object_type   *o_ptr = vec_get(v, i);
            _art_info_ptr  entry;

            if (pred && !pred(o_ptr)) continue;

            entry = malloc(sizeof(_art_info_t));
            entry->id = ART_RANDOM;
            if (p_ptr->prace == RACE_ANDROID)
            {
                entry->score = android_obj_exp(o_ptr);
                if (!entry->score)
                    entry->score = obj_value_real(o_ptr);
            }
            else
                entry->score = obj_value_real(o_ptr);
            object_desc(entry->name, o_ptr, OD_COLOR_CODED);
            entry->k_idx = o_ptr->k_idx;
            entry->level = o_ptr->level;
            entry->pct = 0;
            if (o_ptr->name3)
            {
                obj_t forge = {0};
                if (create_named_art_aux(o_ptr->name3, &forge))
                {
                    int base_score;
                    if (object_is_weapon_ammo(&forge))
                    {
                        forge.to_h = MAX(10, forge.to_h);
                        forge.to_d = MAX(10, forge.to_d);
                    }
                    if (object_is_armour(&forge))
                    {
                        forge.to_a = MAX(10, forge.to_a);
                    }
                    base_score = obj_value_real(&forge);
                    entry->pct = entry->score * 100 / base_score;
                }
            }
            vec_add(entries, entry);

            ct_rnd++;
            score_rnd += entry->score;
            if (entry->score > max_score_rnd)
                max_score_rnd = entry->score;
        }
    }
    if (options & _SPOIL_EGOS)
    {
        vec_ptr v = stats_egos();
        for (i = 0; i < vec_length(v); i++)
        {
            object_type   *o_ptr = vec_get(v, i);
            _art_info_ptr  entry;

            if (pred && !pred(o_ptr)) continue;

            entry = malloc(sizeof(_art_info_t));
            entry->id = ART_EGO;
            if (p_ptr->prace == RACE_ANDROID)
            {
                entry->score = android_obj_exp(o_ptr);
                if (!entry->score)
                    entry->score = obj_value_real(o_ptr);
            }
            else
                entry->score = obj_value_real(o_ptr);
            object_desc(entry->name, o_ptr, OD_COLOR_CODED);
            entry->k_idx = o_ptr->k_idx;
            entry->level = o_ptr->level;
            vec_add(entries, entry);

            ct_ego++;
            score_ego += entry->score;
            if (entry->score > max_score_ego)
                max_score_ego = entry->score;
        }
    }
    if (vec_length(entries))
    {
        vec_sort(entries, (vec_cmp_f)_art_score_cmp);

        doc_printf(doc, "<topic:%s><style:heading>%s</style>\n\n", title, title);
        doc_insert(doc, "<style:wide>     <color:G>  Score Lvl Rty Cts Object Description</color>\n");
        for (i = 0; i < vec_length(entries); i++)
        {
            _art_info_ptr  entry = vec_get(entries, i);

            if (entry->id == ART_RANDOM)
            {
                if (entry->pct)
                    doc_printf(doc, "<color:v>%3d) %7d</color> %3d %3d %3d ", i+1, entry->score, entry->level, entry->pct, k_info[entry->k_idx].counts.found);
                else
                    doc_printf(doc, "<color:v>%3d) %7d</color> %3d     %3d ", i+1, entry->score, entry->level, k_info[entry->k_idx].counts.found);
                doc_printf(doc, "<indent><style:indent>%s</style></indent>\n", entry->name);
            }
            else if (entry->id == ART_EGO)
            {
                doc_printf(doc, "<color:B>%3d) %7d</color> %3d     %3d ", i+1, entry->score, entry->level, k_info[entry->k_idx].counts.found);
                doc_printf(doc, "<indent><style:indent>%s</style></indent>\n", entry->name);
            }
            else
            {
                artifact_type *a_ptr = &a_info[entry->id];

                doc_printf(doc, "<color:%c>%3d) %7d</color> %3d %3d ",
                    (a_ptr->found) ? 'y' : 'w',
                    i+1, entry->score, a_ptr->level, a_ptr->rarity);

                if (a_ptr->gen_flags & OFG_INSTA_ART)
                    doc_insert(doc, "    ");
                else
                    doc_printf(doc, "%3d ", k_info[entry->k_idx].counts.found);
                doc_printf(doc, "<indent><style:indent>%s <color:D>#%d</color></style></indent>\n", entry->name, entry->id);
            }
        }

        if (ct_std || ct_rnd || ct_ego)
        {
            doc_printf(doc, "\n<color:G>%20.20s   Ct Average    Best</color>\n", "");
            if (ct_std)
            {
                doc_printf(doc, "<color:B>%20.20s</color> %4d %7d %7d\n",
                    "Stand Arts", ct_std, score_std/ct_std, max_score_std);
            }
            if (ct_rnd)
            {
                doc_printf(doc, "<color:B>%20.20s</color> %4d %7d %7d\n",
                    "Rand Arts", ct_rnd, score_rnd/ct_rnd, max_score_rnd);
            }
            if (ct_ego)
            {
                doc_printf(doc, "<color:B>%20.20s</color> %4d %7d %7d\n",
                    "Egos", ct_ego, score_ego/ct_ego, max_score_ego);
            }
        }
        doc_insert(doc, "</style>\n\n");
    }
    vec_free(entries);
}

static void _spoil_artifact_table_aux(doc_ptr doc, cptr title, _obj_p pred)
{
    _spoil_table_aux(doc, title, pred, _SPOIL_ARTS | _SPOIL_RAND_ARTS);
}

static void _spoil_object_table_aux(doc_ptr doc, cptr title, _obj_p pred)
{
    _spoil_table_aux(doc, title, pred, _SPOIL_ARTS | _SPOIL_RAND_ARTS | _SPOIL_EGOS);
}

static void spoil_artifact_tables(void)
{
    doc_ptr doc = doc_alloc(MIN(100, _term_width()));

    spoiler_hack = TRUE;
    _spoil_artifact_table_aux(doc, "All Artifacts", NULL);
    _spoil_artifact_table_aux(doc, "Weapons", object_is_melee_weapon);
    _spoil_artifact_table_aux(doc, "Shields", object_is_shield);
    _spoil_artifact_table_aux(doc, "Bows", object_is_bow);
    _spoil_artifact_table_aux(doc, "Rings", object_is_ring);
    _spoil_artifact_table_aux(doc, "Amulets", object_is_amulet);
    _spoil_artifact_table_aux(doc, "Lights", object_is_lite);
    _spoil_artifact_table_aux(doc, "Body Armor", object_is_body_armour);
    _spoil_artifact_table_aux(doc, "Cloaks", object_is_cloak);
    _spoil_artifact_table_aux(doc, "Helmets", object_is_helmet);
    _spoil_artifact_table_aux(doc, "Gloves", object_is_gloves);
    _spoil_artifact_table_aux(doc, "Boots", object_is_boots);
    spoiler_hack = FALSE;

    doc_display(doc, "Artifact Tables", 0);
    doc_free(doc);
}

static void spoil_object_tables(void)
{
    doc_ptr doc = doc_alloc(MIN(100, _term_width()));

    spoiler_hack = TRUE;
    _spoil_object_table_aux(doc, "All Objects", NULL);
    _spoil_object_table_aux(doc, "Weapons", object_is_melee_weapon);
    _spoil_object_table_aux(doc, "Shields", object_is_shield);
    _spoil_object_table_aux(doc, "Bows", object_is_bow);
    _spoil_object_table_aux(doc, "Rings", object_is_ring);
    _spoil_object_table_aux(doc, "Amulets", object_is_amulet);
    _spoil_object_table_aux(doc, "Lights", object_is_lite);
    _spoil_object_table_aux(doc, "Body Armor", object_is_body_armour);
    _spoil_object_table_aux(doc, "Cloaks", object_is_cloak);
    _spoil_object_table_aux(doc, "Helmets", object_is_helmet);
    _spoil_object_table_aux(doc, "Gloves", object_is_gloves);
    _spoil_object_table_aux(doc, "Boots", object_is_boots);
    _spoil_object_table_aux(doc, "Ammo", obj_is_ammo);
    _spoil_object_table_aux(doc, "Quivers", obj_is_quiver);
    spoiler_hack = FALSE;

    doc_display(doc, "Object Tables", 0);
    doc_free(doc);
}

/************************************************************************
 * Monster Tables
 ************************************************************************/
typedef bool (*_mon_pred)(monster_race *r_ptr);

static bool _mon_is_unique(monster_race *r_ptr) { return r_ptr->flags1 & RF1_UNIQUE; }
static bool _mon_is_nonunique(monster_race *r_ptr) { return !_mon_is_unique(r_ptr); }

static int _compare_r_level(monster_race *l, monster_race *r)
{
    if (l->level < r->level) return -1;
    if (l->level > r->level) return 1;
    if (l->mexp < r->mexp) return -1;
    if (l->mexp > r->mexp) return 1;
    if (l->id < r->id) return -1;
    if (l->id > r->id) return 1;
    return 0;
}
static int _compare_r_level_desc(monster_race *l, monster_race *r)
{
    return -_compare_r_level(l, r);
}

static vec_ptr _mon_table(_mon_pred p)
{
    vec_ptr monsters = vec_alloc(NULL);
    int     i;

    for (i = 1; i < max_r_idx; i++)
    {
        monster_race *r_ptr = &r_info[i];

        if (!r_ptr->name) continue;
        if (r_ptr->id == MON_MONKEY_CLONE) continue;
        if (r_ptr->id == MON_KAGE) continue;
        if (p && !p(r_ptr)) continue;

        vec_add(monsters, r_ptr);
    }

    vec_sort(monsters, (vec_cmp_f)_compare_r_level);
    return monsters;
}

static void _spoil_mon_table(doc_ptr doc, cptr heading, _mon_pred p)
{
    vec_ptr monsters = _mon_table(p);
    int     i;

    doc_printf(doc, "<topic:%s><color:G>%-38.38s Lvl Rar Spd     HP    AC Display</color>\n", heading, heading);
    for (i = 0; i < vec_length(monsters); i++)
    {
        monster_race *r_ptr = vec_get(monsters, i);
        if (r_ptr->flags1 & RF1_UNIQUE)
            doc_printf(doc, "<color:%c>%-38.38s</color> ", p == _mon_is_unique ? 'w' : 'v', r_name + r_ptr->name);
        else if (r_ptr->flags7 & RF7_UNIQUE2)
            doc_printf(doc, "<color:%c>%-38.38s</color> ", p == _mon_is_nonunique ? 'v' : 'w', r_name + r_ptr->name);
        else
            doc_printf(doc, "The %-34.34s ", r_name + r_ptr->name);

        doc_printf(doc, "%3d %3d %+3d", r_ptr->level, r_ptr->rarity, r_ptr->speed - 110);
        if ((r_ptr->flags1 & RF1_FORCE_MAXHP) || r_ptr->hside == 1)
            doc_printf(doc, "%7d ", r_ptr->hdice * r_ptr->hside);
        else
        {
            char buf[20];
            sprintf(buf, "%dd%d", r_ptr->hdice, r_ptr->hside);
            doc_printf(doc, "%7.7s ", buf);
        }
        doc_printf(doc, "%5d ", r_ptr->ac);
        doc_printf(doc, "  <color:%c>%c</color>", attr_to_attr_char(r_ptr->d_attr), r_ptr->d_char);
        if (use_graphics && (r_ptr->x_char != r_ptr->d_char || r_ptr->x_attr != r_ptr->d_attr))
        {
            doc_insert(doc, " / ");
            doc_insert_char(doc, r_ptr->x_attr, r_ptr->x_char);
        }
        doc_newline(doc);
    }
    doc_newline(doc);

    vec_free(monsters);
}

static void spoil_mon_desc(void)
{
    doc_ptr doc = doc_alloc(80);

    doc_change_name(doc, "mon-desc.html");
    doc_printf(doc, "<color:heading>Monster Tables for PosChengband Version %d.%d.%d</color>\n\n",
                     VER_MAJOR, VER_MINOR, VER_PATCH);
    doc_insert(doc, "<style:table>");

    _spoil_mon_table(doc, "All Monsters", NULL);
    _spoil_mon_table(doc, "Uniques", _mon_is_unique);
    _spoil_mon_table(doc, "Non-uniques", _mon_is_nonunique);

    doc_insert(doc, "</style>");
    doc_display(doc, "Monster Tables", 0);
    doc_free(doc);
}

/******************************************************************************
 * Spoilers: Monster max damage by resistance type
 * The goal here is to not list all possible damage types. Rather, the goal
 * is to give the player an idea of when certain resistances are desirable
 * as well as how to play with resistance holes
 ******************************************************************************/
static void _display_res(doc_ptr doc, int res)
{
    int pct = res_pct_known(res);
    char color = 'D';
    if (pct == 100)
        color = 'v';
    else if (pct < 0)
        color = 'D';
    else if (res_is_low(res))
    {
        if (pct >= 72)
            color =  'r';
        else if (pct >= 65)
            color =  'R';
        else if (pct >= 50)
            color =  'y';
    }
    else
    {
        if (pct >= 45)
            color =  'r';
        else if (pct >= 40)
            color =  'R';
        else if (pct >= 30)
            color =  'y';
    }
    doc_printf(doc, " <color:%c>%3d</color>", color, pct);
}
static void _display_dam(doc_ptr doc, int res, int amt)
{
    int pct = (res == RES_INVALID) ? 0 : res_pct_known(res);
    int dam = amt - amt * pct / 100;
    int ratio = dam * 100 / p_ptr->chp;
    char color;
    if (ratio > 100) color = 'v';
    else if (ratio > 75) color = 'r';
    else if (ratio > 50) color = 'R';
    else if (ratio > 35) color = 'o';
    else if (ratio > 20) color = 'y';
    else if (ratio > 10) color = 'U';
    else if (dam == 0) color = 'D';
    else color = 'w';
    doc_printf(doc, " <color:%c>%3d</color>", color, dam);
}

static char _melee_dam_color(int dam)
{
    int ratio = dam * 100 / p_ptr->chp;
    if (ratio > 40) return 'v';
    else if (ratio > 25) return 'r';
    else if (ratio > 15) return 'R';
    else if (ratio > 10) return 'o';
    else if (ratio >  5) return 'y';
    else if (ratio >  3) return 'U';
    else if (dam == 0) return 'D';
    return 'w';
}

static void _display_melee_dam(doc_ptr doc, int dam1, int dam2)
{
    if (dam1 != dam2)
    {
        doc_printf(doc, " <color:%c>%3d</color>-><color:%c>%3d</color>",
            _melee_dam_color(dam1), dam1,
            _melee_dam_color(dam2), dam2);
    }
    else
    {
        doc_printf(doc, " <color:%c>%3d</color>     ", _melee_dam_color(dam1), dam1);
    }
}

static void _display_speed(doc_ptr doc, int speed)
{
    char color;
    if (speed >= 30) color = 'v';
    else if (speed >= 20) color = 'r';
    else if (speed >= 10) color = 'R';
    else if (speed >=  5) color = 'y';
    else color = 'w';
    doc_printf(doc, " <color:%c>%5d</color>", color, speed);
}

static int _avg_dam_roll(int dd, int ds) { return dd * (ds + 1) / 2; }
static int _avg_dam(int dd, int ds, int xtra, int mult)
{
    int d = _avg_dam_roll(dd, ds);
    d += xtra;
    d *= mult;
    return d;
}
static int _max_breath(int hp, int div, int max) { return MIN(hp/div, max); }

static void _spoil_mon_spell_dam_aux(doc_ptr doc, vec_ptr v)
{
    int i, j;
    for (i = 0; i < vec_length(v); i++)
    {
        mon_race_ptr r = vec_get(v, i);
        int          hp = 0;
        int          unresist = 0;
        int          dam[RES_MAX] = {0};
        bool         powerful = BOOL(r->flags2 & RF2_POWERFUL);
        int          power_mult = powerful ? 2 : 1; /* elemental balls and bolts */
        int          power_mult2 = powerful ? 3 : 2; /* some high level balls */
        int          power_div = powerful ? 2 : 3; /* cause, you know, diversity is a good thing */
        char         color = 'w';

        if (r->flags1 & RF1_FORCE_MAXHP)
            hp = r->hdice * r->hside;
        else
            hp = r->hdice * (1 + r->hside)/2;

        /* Damage Logic Duplicated from mspells1.c */
        if (r->flags4 & RF4_ROCKET)
            dam[RES_SHARDS] = MAX(dam[RES_SHARDS], _max_breath(hp, 6, 600));
        if (r->flags4 & RF4_BR_ACID)
            dam[RES_ACID] = MAX(dam[RES_ACID], _max_breath(hp, 5, 900));
        if (r->flags4 & RF4_BR_ELEC)
            dam[RES_ELEC] = MAX(dam[RES_ELEC], _max_breath(hp, 5, 900));
        if (r->flags4 & RF4_BR_FIRE)
            dam[RES_FIRE] = MAX(dam[RES_FIRE], _max_breath(hp, 5, 900));
        if (r->flags4 & RF4_BR_COLD)
            dam[RES_COLD] = MAX(dam[RES_COLD], _max_breath(hp, 5, 900));
        if (r->flags4 & RF4_BR_POIS)
            dam[RES_POIS] = MAX(dam[RES_POIS], _max_breath(hp, 6, 600));
        if (r->flags4 & RF4_BR_NETH)
            dam[RES_NETHER] = MAX(dam[RES_NETHER], _max_breath(hp, 7, 550));
        if (r->flags4 & RF4_BR_LITE)
            dam[RES_LITE] = MAX(dam[RES_LITE], _max_breath(hp, 6, 400));
        if (r->flags4 & RF4_BR_DARK)
            dam[RES_DARK] = MAX(dam[RES_DARK], _max_breath(hp, 6, 400));
        if (r->flags4 & RF4_BR_CONF)
            dam[RES_CONF] = MAX(dam[RES_CONF], _max_breath(hp, 6, 400));
        if (r->flags4 & RF4_BR_SOUN)
            dam[RES_SOUND] = MAX(dam[RES_SOUND], _max_breath(hp, 6, 450));
        if (r->flags4 & RF4_BR_CHAO)
            dam[RES_CHAOS] = MAX(dam[RES_CHAOS], _max_breath(hp, 6, 600));
        if (r->flags4 & RF4_BR_DISE)
            dam[RES_DISEN] = MAX(dam[RES_DISEN], _max_breath(hp, 6, 500));
        if (r->flags4 & RF4_BR_NEXU)
            dam[RES_NEXUS] = MAX(dam[RES_NEXUS], _max_breath(hp, 3, 250));
        if (r->flags4 & RF4_BR_SHAR)
            dam[RES_SHARDS] = MAX(dam[RES_SHARDS], _max_breath(hp, 6, 500));
        if (r->flags4 & RF4_BR_NUKE)
            dam[RES_POIS] = MAX(dam[RES_POIS], _max_breath(hp, 6, 600));


        if (r->flags5 & RF5_BA_ACID)
            dam[RES_ACID] = MAX(dam[RES_ACID], _avg_dam(1, 3*r->level, 15, power_mult));
        if (r->flags5 & RF5_BA_ELEC)
            dam[RES_ELEC] = MAX(dam[RES_ELEC], _avg_dam(1, 3*r->level/2, 8, power_mult));
        if (r->flags5 & RF5_BA_FIRE)
            dam[RES_FIRE] = MAX(dam[RES_FIRE], _avg_dam(1, 7*r->level/2, 10, power_mult));
        if (r->flags5 & RF5_BA_COLD)
            dam[RES_COLD] = MAX(dam[RES_COLD], _avg_dam(1, 3*r->level/2, 10, power_mult));
        if (r->flags5 & RF5_BA_POIS)
            dam[RES_POIS] = MAX(dam[RES_POIS], _avg_dam(12, 2, 0, power_mult));
        if (r->flags4 & RF4_BA_NUKE)
            dam[RES_POIS] = MAX(dam[RES_POIS], _avg_dam(10, 6, r->level, power_mult));
        if (r->flags5 & RF5_BA_NETH)
            dam[RES_NETHER] = MAX(dam[RES_NETHER], _avg_dam_roll(10, 10) + 50 + r->level*power_mult);

        if (r->flags5 & RF5_BO_ACID)
            dam[RES_ACID] = MAX(dam[RES_ACID], _avg_dam(7, 8, r->level/3, power_mult));
        if (r->flags5 & RF5_BO_ELEC)
            dam[RES_ELEC] = MAX(dam[RES_ELEC], _avg_dam(4, 8, r->level/3, power_mult));
        if (r->flags5 & RF5_BO_FIRE)
            dam[RES_FIRE] = MAX(dam[RES_FIRE], _avg_dam(9, 8, r->level/3, power_mult));
        if (r->flags5 & RF5_BO_COLD)
            dam[RES_COLD] = MAX(dam[RES_COLD], _avg_dam(6, 8, r->level/3, power_mult));
        if (r->flags5 & RF5_BO_ICEE)
            dam[RES_COLD] = MAX(dam[RES_COLD], _avg_dam_roll(6, 6) + r->level * 3 / power_div);
        if (r->flags5 & RF5_BO_NETH)
            dam[RES_NETHER] = MAX(dam[RES_NETHER], 30 + _avg_dam_roll(5, 5) + r->level * 4 / power_div);

        if (r->flags5 & RF5_BA_DARK)
            dam[RES_DARK] = MAX(dam[RES_DARK], _avg_dam_roll(10, 10) + r->level*4 + 50);
        if (r->flags5 & RF5_BA_LITE)
            dam[RES_LITE] = MAX(dam[RES_LITE], _avg_dam_roll(10, 10) + r->level*4 + 50);
        if (r->flags4 & RF4_BA_CHAO)
            dam[RES_CHAOS] = MAX(dam[RES_CHAOS], _avg_dam_roll(10, 10) + r->level*power_mult2);

        if (r->flags4 & RF4_THROW)
            unresist = MAX(unresist, r->level*3);
        if (r->flags4 & RF4_BR_STORM)
            unresist = MAX(unresist, _max_breath(hp, 7, 300));
        if (r->flags4 & RF4_BR_INER)
            unresist = MAX(unresist, _max_breath(hp, 6, 200));
        if (r->flags4 & RF4_BR_GRAV)
            unresist = MAX(unresist, _max_breath(hp, 3, 200));
        if (r->flags4 & RF4_BR_PLAS)
            unresist = MAX(unresist, _max_breath(hp, 6, 200));
        if (r->flags4 & RF4_BR_WALL)
            unresist = MAX(unresist, _max_breath(hp, 3, 200));
        if (r->flags4 & RF4_BR_MANA)
            unresist = MAX(unresist, _max_breath(hp, 3, 250));
        if (r->flags4 & RF4_BR_DISI)
            unresist = MAX(unresist, _max_breath(hp, 6, 150));

        if (r->flags5 & RF5_MIND_BLAST)
            unresist = MAX(unresist, _avg_dam_roll(7, 7));
        if (r->flags5 & RF5_BRAIN_SMASH)
            unresist = MAX(unresist, _avg_dam_roll(12, 12));
        if (r->flags5 & RF5_CAUSE_1)
            unresist = MAX(unresist, _avg_dam_roll(3, 8));
        if (r->flags5 & RF5_CAUSE_2)
            unresist = MAX(unresist, _avg_dam_roll(8, 8));
        if (r->flags5 & RF5_CAUSE_3)
            unresist = MAX(unresist, _avg_dam_roll(10, 15));
        if (r->flags5 & RF5_CAUSE_4)
            unresist = MAX(unresist, _avg_dam_roll(15, 15));
        if (r->flags5 & RF5_MISSILE)
            unresist = MAX(unresist, _avg_dam_roll(2, 6) + r->level/3);

        if (r->flags5 & RF5_BA_WATE)
            unresist = MAX(unresist, _avg_dam_roll(1, r->level*power_mult2) +  50);
        if (r->flags5 & RF5_BO_WATE)
            unresist = MAX(unresist, _avg_dam_roll(10, 10) + r->level * 3 / power_div);
        if (r->flags5 & RF5_BO_PLAS)
            unresist = MAX(unresist, 10 + _avg_dam_roll(8, 7) + r->level * 3 / power_div);
        if (r->flags5 & RF5_BO_MANA)
            unresist = MAX(unresist, _avg_dam_roll(1, 7*r->level/2) + 50);
        if (r->flags5 & RF5_BA_MANA)
            unresist = MAX(unresist, _avg_dam_roll(10, 10) + r->level*4 + 50);
        if (r->flags6 & RF6_HAND_DOOM)
            unresist = MAX(unresist, p_ptr->chp / 2); 
        if (r->flags6 & RF6_PSY_SPEAR)
        {
            unresist = MAX(unresist, powerful ?  (_avg_dam_roll(1, 2*r->level) + 150)
                                              :  (_avg_dam_roll(1, 3*r->level/2) + 100));
        }

        if (i%25 == 0)
        {
            doc_printf(doc, "\n<tab:21><color:B>%3d %5d</color>", p_ptr->lev, p_ptr->chp);
            for (j = RES_ACID; j <= RES_DISEN; j++)
                _display_res(doc, j);
            doc_printf(doc, "\n<color:G>%-20.20s Lvl    HP  Ac  El  Fi  Co  Po  Li  Dk  Cf  Nt  Nx  So  Sh  Ca  Di  Un</color>\n", "Name");
        }

        if (r->flags9 & RF9_DEPRECATED)
            color = 'D';
        else if (r->flags3 & RF3_OLYMPIAN)
            color = 'U';
        else if (r->id > 1132)
            color = 'B';
        doc_printf(doc, "<color:%c>%-20.20s</color> %3d %5d", color, r_name + r->name, r->level, hp);
        for (j = RES_ACID; j <= RES_DISEN; j++)
            _display_dam(doc, j, dam[j]);
        _display_dam(doc, RES_INVALID, unresist);
        if (r->freq_spell && r->freq_spell < 50)
            doc_printf(doc, " %d%% %d%%", r->freq_spell, 2 * r->freq_spell * r->freq_spell / 100);
        else if (r->freq_spell)
        {
            char color = r->freq_spell > 70 ? 'v' : 'r';
            doc_printf(doc, " <color:%c>%d%%</color>", color, r->freq_spell);
        }
        doc_newline(doc);
    }
}
typedef struct {
    int raw;
    int reduced;  /* HURT, SUPERHURT, SHATTER reduce by player's AC */
    int effective;/* account for 'dodge rate' (ie melee accuracy) */
} _melee_dam_t;

static void _spoil_mon_melee_dam_aux(doc_ptr doc, vec_ptr v)
{
    int i, j, k;
    int ac = p_ptr->ac + p_ptr->to_a;
    int ac2 = 3*ac/4;
    for (i = 0; i < vec_length(v); i++)
    {
        mon_race_ptr r = vec_get(v, i);
        int          hp = 0;
        _melee_dam_t melee1 = {0}; /* evaluate tweaks to ac_melee_pct() */
        _melee_dam_t melee2 = {0}; /* this makes the code yuk, but, unfortunately, I need to see it */
        int          auras = 0;
        int          blows = 0;
        char         color = 'w';

        if (r->flags1 & RF1_FORCE_MAXHP)
            hp = r->hdice * r->hside;
        else
            hp = r->hdice * (1 + r->hside)/2;

        for (j = 0; j < MAX_MON_BLOWS; j++)
        {
            int dam = 0, dam1 = 0, dam2 = 0;
            mon_blow_ptr blow = &r->blows[j];
            int skill, chance;

            if (r->freq_spell == 100) break;
            if (!blow->method) continue;
            if (blow->method == RBM_EXPLODE) continue;
            if (blow->method == RBM_SHOOT) continue;

            for (k = 0; k < MAX_MON_BLOW_EFFECTS; k++)
            {
                mon_effect_ptr effect = &blow->effects[k];
                int            effect_dam;

                if (!effect->effect) continue;
                /* skip non-damaging effects */
                if (effect->effect == RBE_DR_MANA) continue;
                if (effect->effect == RBE_STUN) continue;
                if (effect->effect == RBE_CUT) continue;

                effect_dam = _avg_dam_roll(effect->dd, effect->ds);

                /* reduce for resistances */
                switch (effect->effect)
                {
                case GF_ACID:
                    effect_dam -= effect_dam * res_pct_known(RES_ACID) / 100;
                    break;
                case GF_ELEC:
                    effect_dam -= effect_dam * res_pct_known(RES_ELEC) / 100;
                    break;
                case GF_FIRE:
                    effect_dam -= effect_dam * res_pct_known(RES_FIRE) / 100;
                    break;
                case GF_COLD: case GF_ICE:
                    effect_dam -= effect_dam * res_pct_known(RES_COLD) / 100;
                    break;
                case GF_POIS:
                    effect_dam -= effect_dam * res_pct_known(RES_POIS) / 100;
                    break;
                case GF_DISENCHANT:
                    effect_dam -= effect_dam * res_pct_known(RES_DISEN) / 100;
                    break;
                case GF_TIME:
                    effect_dam -= effect_dam * res_pct_known(RES_TIME) / 100;
                    break;
                }
                if (effect->pct)
                    effect_dam = effect_dam * effect->pct / 100;

                /* reduce for player AC, keeping old and new amounts so I can
                 * see the effects of changes side by side */
                dam += effect_dam;
                switch (effect->effect)
                {
                case RBE_HURT: case RBE_SHATTER: case RBE_SUPERHURT:
                    dam1 += effect_dam * ac_melee_pct_aux(ac, 60, 150) / 100;
                    dam2 += effect_dam * ac_melee_pct(ac) / 100;
                    break;
                default:
                    dam1 += effect_dam;
                    dam2 += effect_dam;
                }
            }
            skill = blow->power;
            skill += 3 * r->level;
            if (skill > ac2)
                chance = 50 + 19*(1000 - ac2*1000/skill)/20;
            else
                chance = 50;

            /* now keep totals */
            melee1.raw += dam;
            melee1.reduced += dam1;
            melee1.effective += chance * dam1 / 1000;

            melee2.raw += dam;
            melee2.reduced += dam2;
            melee2.effective += chance * dam2 / 1000;

            blows++;
        }
        if (!p_ptr->lightning_reflexes)
        {
            int dd = 1 + r->level/26;
            int ds = 1 + r->level/17;
            int base_dam = _avg_dam_roll(dd, ds);
            if ((r->flags2 & RF2_AURA_REVENGE) && blows > 0)
                auras += melee2.effective/blows;
            if (r->flags2 & RF2_AURA_FIRE)
            {
                int pct = res_pct_known(RES_FIRE);
                int dam = base_dam - base_dam * pct / 100;
                auras += dam;
            }
            if (r->flags3 & RF3_AURA_COLD)
            {
                int pct = res_pct_known(RES_COLD);
                int dam = base_dam - base_dam * pct / 100;
                auras += dam;
            }
            if (r->flags2 & RF2_AURA_ELEC)
            {
                int pct = res_pct_known(RES_ELEC);
                int dam = base_dam - base_dam * pct / 100;
                auras += dam;
            }
        }
        for (j = 0; j < MAX_MON_AURAS; j++)
        {
            mon_effect_ptr aura = &r->auras[j];
            int dam;
            if (!aura->effect) continue;
            dam = _avg_dam_roll(aura->dd, aura->ds);
            if (aura->pct)
                dam = dam * aura->pct / 100;
            /* XXX resistance? need a map from GF_* -> RES_*. */
            auras += dam;
        }

        if (i%25 == 0)
            doc_printf(doc, "\n<color:G>%-30.30s Lvl    HP Speed  AC   Exp Damage   Damage   Auras </color>\n", "Name");

        if (r->flags9 & RF9_DEPRECATED)
            color = 'D';
        else if (r->flags3 & RF3_OLYMPIAN)
            color = 'U';
        else if (r->id > 1132)
            color = 'B';
        doc_printf(doc, "<color:%c>%-30.30s</color>", color, r_name + r->name);
        color = (r->flags1 & RF1_FORCE_DEPTH) ? 'r' : 'w';
        doc_printf(doc, " <color:%c>%3d</color> %5d", color, r->level, hp);
        _display_speed(doc, r->speed - 110);
        if (r->ac < 999)
            doc_printf(doc, " %3d", r->ac);
        else
            doc_insert(doc, " <color:y>***</color>"); /* metal babble */
        {
            int plev = spoiler_hack ? 50 : p_ptr->max_plv;
            int xp = r->mexp * r->level / (plev + 2);
            char buf[10];

            if (quickband) xp *= 2;
            big_num_display(xp, buf);
            doc_printf(doc, " %5.5s", buf);
        }
        _display_melee_dam(doc, melee1.reduced, melee2.reduced);
        _display_melee_dam(doc, melee1.effective, melee2.effective);
        if (auras)
            doc_printf(doc, " %5d", auras);
        doc_newline(doc);
    }
}

static bool _mon_dam_p(mon_race_ptr r)
{
    return !(r->flags9 & RF9_DEPRECATED);
    return TRUE;
    return r->d_char == 'C';
    return BOOL(r->flags2 & RF2_CAMELOT);
}

static void spoil_mon_spell_dam(void)
{
    doc_ptr doc = doc_alloc(120);
    vec_ptr v = _mon_table(_mon_dam_p); 

    doc_change_name(doc, "mon-spells.html");
    doc_insert(doc, "<style:table>");

    _spoil_mon_spell_dam_aux(doc, v);

    doc_insert(doc, "</style>");
    doc_printf(doc, "\n<color:D>Generated for PosChengband Version %d.%d.%d</color>\n\n",
                     VER_MAJOR, VER_MINOR, VER_PATCH);
    doc_display(doc, "Monster Tables", 0);
    doc_free(doc);
    vec_free(v);
}

static void spoil_mon_melee_dam(void)
{
    doc_ptr doc = doc_alloc(120);
    vec_ptr v = _mon_table(_mon_dam_p); 

    doc_change_name(doc, "mon-melee.html");
    doc_insert(doc, "<style:table>");

    _spoil_mon_melee_dam_aux(doc, v);

    doc_insert(doc, "</style>");
    doc_printf(doc, "\n<color:D>Generated for PosChengband Version %d.%d.%d</color>\n\n",
                     VER_MAJOR, VER_MINOR, VER_PATCH);
    doc_display(doc, "Monster Tables", 0);
    doc_free(doc);
    vec_free(v);
}

typedef struct {
    double mean;
    double variance;
    double sigma;
    int    max;
} _stat_t, *_stat_ptr;
_stat_t _calc_stats(vec_ptr v)
{
    _stat_t r;
    int n = vec_length(v);
    int i, max = 0;
    double tx2 = 0.0, tx = 0.0;
    for (i = 0; i < n; i++)
    {
        int x = vec_get_int(v, i);
        tx += (double)x;
        tx2 += (double)x*(double)x;
        if (x > max) max = x;
    }
    r.mean = tx/(double)n;
    r.variance = tx2/(double)n - r.mean;  /* XXX check this ... */
    r.sigma = sqrt(r.variance);
    r.max = max;
    return r;
}
static void spoil_mon_anger(void)
{
    /* I tried to spreadsheet this, but I must be too dumb to get accurate results! */
    doc_ptr doc = doc_alloc(80);
    doc_ptr cols[2];
    int boosts[] = { 0, 5, 7, 10, 12, 15, -1 };
    int freqs[] = { 10, 15, 20, 25, 30, 33, 35, 40, 50, -1 };
    int doc_idx = 1, boost_idx, freq_idx;
    cols[0] = doc_alloc(36);
    cols[1] = doc_alloc(36);
    for (boost_idx = 0;; boost_idx++)
    {
        int boost = boosts[boost_idx];
        if (boost < 0) break;
        doc_idx = !doc_idx;
        doc_printf(cols[doc_idx], "<color:R>Anger Boost: <color:B>%d</color></color>\n", boost);
        doc_insert(cols[doc_idx], "<color:G>Base Actual Mean  Std Max</color>\n");
        for (freq_idx = 0;; freq_idx++)
        {
            vec_ptr runs = vec_alloc(NULL);
            _stat_t stat;
            int i, j, cast = 0, total = 0;
            int freq = freqs[freq_idx];
            if (freq < 0) break;
            for (i = 0; i < 10000; i++)
            {
                int a = 0;    /* anger */
                for (j = 0; ; j++) /* loop until we cast a spell */
                {
                    total++;
                    if (randint0(100) < freq + a)
                    {
                        vec_add_int(runs, j+1);
                        cast++;
                        break;
                    }
                    /* angering each turn ... there are lots of ways to do
                     * this, but the following vastly decreases stat.max while
                     * only slightly affecting stat.mean and stat.sigma */
                    a += boost + a/2;
                }
            }
            stat = _calc_stats(runs);
            doc_printf(cols[doc_idx], "%4d  %2d.%d%% %.2f %.2f %3d\n",
                freq, cast*100/total, (cast*1000/total)%10, stat.mean, stat.sigma, stat.max);
            vec_free(runs);
        }
        doc_newline(cols[doc_idx]);
    }
    doc_insert_cols(doc, cols, 2, 0);
    doc_insert(doc, "The stats are on the number of turns it takes to actually cast a spell. "
                    "This is very important for mage-like distance tactics where very long runs "
                    "of no player damage are to be avoided.\n");
    doc_display(doc, "Dynamic Spell Frequencies", 0);
    doc_free(cols[0]);
    doc_free(cols[1]);
    doc_free(doc);
}

/************************************************************************
 * Devices
 ************************************************************************/
static char _fail_color(int fail)
{
    if (fail < 30) return 'w';
    if (fail < 80) return 'W';
    if (fail < 150) return 'U';
    if (fail < 250) return 'y';
    if (fail < 350) return 'o';
    if (fail < 500) return 'R';
    if (fail < 750) return 'r';
    return 'v';
}
static void spoil_device_fail()
{
    doc_ptr doc = doc_alloc(80);
    int     d, s;

    doc_change_name(doc, "devices.html");
    doc_insert(doc, "<style:table>");
    doc_insert(doc, "<topic:d_vs_s><color:r>Difficulty vs Skill</color>\n");
    for (d = 1; d <= 100; d++)
    {
        if (d%25 == 1)
        {
            doc_insert(doc, "\n<color:G>Dev ");
            for (s = 20; s <= 160; s+=10)
                doc_printf(doc, "%4d ", s);
            doc_insert(doc, "</color>\n");
        }
        doc_printf(doc, "<color:B>%3d</color> ", d);
        for (s = 20; s <= 160; s+=10)
        {
            int fail = device_calc_fail_rate_aux(s, d);
            doc_printf(doc, "<color:%c>%2d.%d</color> ", _fail_color(fail), fail/10, fail%10);
        }
        doc_newline(doc);
    }

    doc_insert(doc, "\n<topic:s_vs_d><color:r>Skill vs Difficulty</color>\n");
    for (s = 26; s <= 160; s++)
    {
        if (s%25 == 1)
        {
            doc_insert(doc, "\n<color:G>Skl ");
            for (d = 30; d <= 100; d+=5)
                doc_printf(doc, "%4d ", d);
            doc_insert(doc, "</color>\n");
        }
        doc_printf(doc, "<color:B>%3d</color> ", s);
        for (d = 30; d <= 100; d+=5)
        {
            int fail = device_calc_fail_rate_aux(s, d);
            doc_printf(doc, "<color:%c>%2d.%d</color> ", _fail_color(fail), fail/10, fail%10);
        }
        doc_newline(doc);
    }
    doc_insert(doc, "</style>");
    doc_printf(doc, "\n<color:D>Generated for PosChengband %d.%d.%d</color>\n",
                     VER_MAJOR, VER_MINOR, VER_PATCH);
    doc_display(doc, "Device Faile Rates", 0);
    doc_free(doc);
}

/************************************************************************
 * Monster Lore
 ************************************************************************/
static void spoil_mon_info(void)
{
    int     i;
    vec_ptr v = vec_alloc(NULL);
    doc_ptr doc = doc_alloc(80);

    spoiler_hack = TRUE;

    for (i = 1; i < max_r_idx; i++)
    {
        monster_race *r_ptr = &r_info[i];
        if (!r_ptr->name) continue;
        if (r_ptr->id == MON_MONKEY_CLONE) continue;
        if (r_ptr->id == MON_KAGE) continue;
        vec_add(v, r_ptr);
    }
    vec_sort(v, (vec_cmp_f)_compare_r_level_desc);

    for (i = 0; i < vec_length(v); i++)
    {
        monster_race *r_ptr = vec_get(v, i);
        doc_printf(doc, "<topic:%s><color:r>=====================================================================</color>\n", r_name + r_ptr->name);
        mon_display_doc(r_ptr, doc);
        doc_newline(doc);
    }
    vec_free(v);

    doc_display(doc, "Monster Spoilers", 0);
    doc_free(doc);

    spoiler_hack = FALSE;
}

/************************************************************************
 * Monster Evolution
 ************************************************************************/
static vec_ptr _evol_roots(void)
{
    vec_ptr     roots = vec_alloc(NULL);
    int_map_ptr targets = int_map_alloc(NULL);
    int         i;

    /* Find all the targets (monsters that may
     * be evolved into) */
    for (i = 1; i < max_r_idx; i++)
    {
        monster_race *r_ptr = &r_info[i];
        if (!r_ptr->next_exp) continue;
        int_map_add(targets, r_ptr->next_r_idx, NULL);
    }

    /* Now, the roots are simply monsters that
     * may evolve, but aren't themselves targets */
    for (i = 1; i < max_r_idx; i++)
    {
        monster_race *r_ptr = &r_info[i];
        if (!r_ptr->next_exp) continue;
        if (int_map_contains(targets, i)) continue;
        vec_add(roots, r_ptr);
    }
    int_map_free(targets);

    vec_sort(roots, (vec_cmp_f)_compare_r_level);
    return roots;
}

static void _evol_mon_line(doc_ptr doc, monster_race *r_ptr)
{
    doc_printf(doc, "[%d]: <color:B>%s</color> (Level <color:G>%d</color>, ", r_ptr->id, r_name + r_ptr->name, r_ptr->level);
    doc_printf(doc, "<color:%c>%c</color>", attr_to_attr_char(r_ptr->d_attr), r_ptr->d_char);
    if (use_graphics && (r_ptr->x_char != r_ptr->d_char || r_ptr->x_attr != r_ptr->d_attr))
    {
        doc_insert(doc, " / ");
        doc_insert_char(doc, r_ptr->x_attr, r_ptr->x_char);
    }
    doc_insert(doc, ")\n");
}

static void spoil_mon_evol(void)
{
    int     i, j;
    vec_ptr roots = _evol_roots();
    doc_ptr doc = doc_alloc(80);

    doc_change_name(doc, "mon-evol.html");
    doc_printf(doc, "<color:heading>Monster Evolution for PosChengband Version %d.%d.%d</color>\n",
                     VER_MAJOR, VER_MINOR, VER_PATCH);
    doc_insert(doc, "<style:table>");

    for (i = 0; i < vec_length(roots); i++)
    {
        monster_race *r_ptr = vec_get(roots, i);

        _evol_mon_line(doc, r_ptr);
        for (j = 1; r_ptr->next_exp; j++)
        {
            doc_printf(doc, "%*s<color:y>-<color:R>%d</color>-></color> ", j * 2, "", r_ptr->next_exp);
            r_ptr = &r_info[r_ptr->next_r_idx];
            _evol_mon_line(doc, r_ptr);
        }
        doc_newline(doc);
    }
    vec_free(roots);

    doc_insert(doc, "</style>");
    doc_display(doc, "Monster Evolution", 0);
    doc_free(doc);
}

/************************************************************************
 * Magic Realms
 ************************************************************************/
static bool _check_realm(int class_idx, int realm_idx)
{
    int bit = (1 << (realm_idx-1)); /* cf CH_LIFE and REALM_LIFE (etc) in defines.h */
    if (realm_choices1[class_idx] & bit)
        return TRUE;
    if (realm_choices2[class_idx] & bit)
        return TRUE;
    if (class_idx == CLASS_SORCERER || class_idx == CLASS_RED_MAGE || class_idx == CLASS_GRAY_MAGE || class_idx == CLASS_SKILLMASTER)
    {
        if (is_magic(realm_idx) && realm_idx != REALM_NECROMANCY)
            return TRUE;
    }
    if (class_idx == CLASS_SKILLMASTER && realm_idx == REALM_BURGLARY)
        return TRUE;
    return FALSE;
}

static void _spoil_spell_book(doc_ptr doc, int class_idx, int realm_idx, int book_idx)
{
    int           spell_idx;
    int           k_idx = lookup_kind(realm2tval(realm_idx), book_idx);
    player_magic *magic_ptr = &m_info[class_idx];

    doc_printf(doc, "<color:o>%-25.25s</color><color:G> Lvl Cst Fail  </color>\n", k_name + k_info[k_idx].name);
    for (spell_idx = book_idx*8; spell_idx < (book_idx+1)*8; spell_idx++)
    {
        magic_type *spell_ptr = NULL;
        if (is_magic(realm_idx))
            spell_ptr = &magic_ptr->info[realm_idx - 1][spell_idx];
        else
            spell_ptr = &technic_info[realm_idx - MIN_TECHNIC][spell_idx];

        if (0 < spell_ptr->slevel && spell_ptr->slevel <= PY_MAX_LEVEL)
        {
            doc_printf(doc, "%-25.25s %3d %3d %3d%%\n",
                do_spell(realm_idx, spell_idx, SPELL_NAME),
                spell_ptr->slevel,
                spell_ptr->smana,
                spell_ptr->sfail
            );
        }
        else
        {
            doc_printf(doc, "<color:D>%-26.26s</color>\n", "Illegible");
        }
    }
    doc_newline(doc);
}

static int  _cmp_class_name(int left_idx, int right_idx)
{
    class_t *l = get_class_aux(left_idx, 0);
    class_t *r = get_class_aux(right_idx, 0);
    return strcmp(l->name, r->name);
}

static void spoil_spells_by_class(void)
{
    int i, realm_idx;
    doc_ptr doc = doc_alloc(80);
    vec_ptr vec = vec_alloc(NULL);

    for (i = 0; i < MAX_CLASS; i++)
        vec_add_int(vec, i);

    vec_sort(vec, (vec_cmp_f)_cmp_class_name);

    for (i = 0; i < vec_length(vec); i++)
    {
        int           class_idx = vec_get_int(vec, i);
        class_t      *class_ptr = get_class_aux(class_idx, 0);
        bool          class_heading = FALSE;

        if (class_idx == CLASS_RAGE_MAGE) continue; /* broken */

        for (realm_idx = REALM_LIFE; realm_idx <= MAX_REALM; realm_idx++)
        {
            if (_check_realm(class_idx, realm_idx))
            {
                doc_ptr cols[2];

                cols[0] = doc_alloc(40);
                cols[1] = doc_alloc(40);

                _spoil_spell_book(cols[0], class_idx, realm_idx, 0);
                _spoil_spell_book(cols[1], class_idx, realm_idx, 1);
                if (class_idx != CLASS_RED_MAGE || realm_idx == REALM_ARCANE)
                {
                    _spoil_spell_book(cols[0], class_idx, realm_idx, 2);
                    _spoil_spell_book(cols[1], class_idx, realm_idx, 3);
                }

                if (!class_heading)
                {
                    doc_printf(doc, "<topic:%s><color:r>%s</color>\n", class_ptr->name, class_ptr->name);
                    doc_printf(doc, "%s\n\n", class_ptr->desc);
                    class_heading = TRUE;
                }
                doc_printf(doc, "<color:B>%s</color>\n", realm_names[realm_idx]);

                doc_insert_cols(doc, cols, 2, 0);

                doc_free(cols[0]);
                doc_free(cols[1]);
            }
        }
    }

    doc_display(doc, "Spells by Class", 0);
    doc_free(doc);
    vec_free(vec);
}

static void _spoil_spell_book2(doc_ptr doc, int class1_idx, int class2_idx, int realm_idx, int book_idx)
{
    int           spell_idx;
    int           k_idx = lookup_kind(realm2tval(realm_idx), book_idx);
    player_magic *magic1_ptr = &m_info[class1_idx];
    player_magic *magic2_ptr = &m_info[class2_idx];

    doc_printf(doc, "%-25.25s <color:G>%-12.12s</color>  <color:R>%-12.12s</color>\n",
        "", get_class_aux(class1_idx, 0)->name, get_class_aux(class2_idx, 0)->name);
    doc_printf(doc, "<color:o>%-25.25s</color><color:G> Lvl Cst Fail</color>  <color:R>Lvl Cst Fail</color>\n", k_name + k_info[k_idx].name);
    for (spell_idx = book_idx*8; spell_idx < (book_idx+1)*8; spell_idx++)
    {
        magic_type *spell1_ptr = &magic1_ptr->info[realm_idx - 1][spell_idx];
        magic_type *spell2_ptr = &magic2_ptr->info[realm_idx - 1][spell_idx];

        doc_printf(doc, "%-25.25s ", do_spell(realm_idx, spell_idx, SPELL_NAME));
        if (1 <= spell1_ptr->slevel && spell1_ptr->slevel <= PY_MAX_LEVEL)
            doc_printf(doc, "%3d %3d %3d%%  ", spell1_ptr->slevel, spell1_ptr->smana, spell1_ptr->sfail);
        else
            doc_insert(doc, "<color:D>Illegible</color>     ");
        if (1 <= spell2_ptr->slevel && spell2_ptr->slevel <= PY_MAX_LEVEL)
            doc_printf(doc, "%3d %3d %3d%%  ", spell2_ptr->slevel, spell2_ptr->smana, spell2_ptr->sfail);
        else
            doc_insert(doc, "<color:D>Illegible</color>     ");
        doc_newline(doc);
    }
    doc_newline(doc);
}

static void _spoil_spells_by_realm_aux3(int realm_idx, int class1_idx, int class2_idx)
{
    int i;
    doc_ptr doc = doc_alloc(80);

    for (i = 0; i < 4; i++)
        _spoil_spell_book2(doc, class1_idx, class2_idx, realm_idx, i);

    doc_display(doc, "Spells by Realm", 0);
    doc_free(doc);
}

static void _spoil_spells_by_realm_aux2(int realm_idx, int class1_idx)
{
    int i, row, col, class_idx, choice;
    vec_ptr vec = vec_alloc(NULL);

    for (class_idx = 0; class_idx < MAX_CLASS; class_idx++)
    {
        if (_check_realm(class_idx, realm_idx))
            vec_add_int(vec, class_idx);
    }

    vec_sort(vec, (vec_cmp_f)_cmp_class_name);

    while (1)
    {
        Term_clear();

        c_prt(TERM_L_BLUE, format("%s", realm_names[realm_idx]), 2, 0);
        c_prt(TERM_L_BLUE, format("First Class: %s", get_class_aux(class1_idx, 0)->name), 3, 0);

        /* Classes */
        row = 4;
        col = 2;
        c_prt(TERM_RED, "Second Class", row++, col - 2);

        for (i = 0; i < vec_length(vec); i++)
        {
            int      class_idx = vec_get_int(vec, i);
            class_t *class_ptr = get_class_aux(class_idx, 0);

            prt(format("(%c) %s", 'a' + i, class_ptr->name), row++, col);
        }

        i = inkey();
        if (i == ESCAPE) break;
        choice = i - 'a';

        if (0 <= choice && choice < vec_length(vec))
        {
            class_idx = vec_get_int(vec, choice);
            _spoil_spells_by_realm_aux3(realm_idx, class1_idx, class_idx);
        }
     }

    vec_free(vec);
}

static void _spoil_spells_by_realm_aux1(int realm_idx)
{
    int i, row, col, class_idx, choice;
    vec_ptr vec = vec_alloc(NULL);

    for (class_idx = 0; class_idx < MAX_CLASS; class_idx++)
    {
        if (_check_realm(class_idx, realm_idx))
            vec_add_int(vec, class_idx);
    }

    vec_sort(vec, (vec_cmp_f)_cmp_class_name);

    while (1)
    {
        Term_clear();

        c_prt(TERM_L_BLUE, format("%s", realm_names[realm_idx]), 2, 0);

        /* Classes */
        row = 4;
        col = 2;
        c_prt(TERM_RED, "First Class", row++, col - 2);

        for (i = 0; i < vec_length(vec); i++)
        {
            int      class_idx = vec_get_int(vec, i);
            class_t *class_ptr = get_class_aux(class_idx, 0);

            prt(format("(%c) %s", 'a' + i, class_ptr->name), row++, col);
        }

        i = inkey();
        if (i == ESCAPE) break;
        choice = i - 'a';

        if (0 <= choice && choice < vec_length(vec))
        {
            class_idx = vec_get_int(vec, choice);
            _spoil_spells_by_realm_aux2(realm_idx, class_idx);
        }
     }

    vec_free(vec);
}

static void spoil_spells_by_realm(void)
{
    int i, row, col, realm_idx;
    while (1)
    {
        Term_clear();

        prt("Realm Spoilers", 2, 0);

        /* Realms */
        row = 4;
        col = 2;
        c_prt(TERM_RED, "Realms", row++, col - 2);

        for (realm_idx = REALM_LIFE; realm_idx <= MAX_MAGIC; realm_idx++)
        {
            prt(format("(%c) %s", 'a' + realm_idx - 1, realm_names[realm_idx]), row++, col);
        }

        i = inkey();
        if (i == ESCAPE) break;
        realm_idx = i - 'a' + 1;

        if (REALM_LIFE <= realm_idx && realm_idx <= MAX_MAGIC)
            _spoil_spells_by_realm_aux1(realm_idx);
     }
}

/************************************************************************
 * Miscellaneous
 ************************************************************************/
static void spoil_option_bits(void)
{
    int     set, bit, seek;
    doc_ptr doc = doc_alloc(80);
    doc_ptr cols[2];

    doc_insert(doc, "Options are stored in the savefile using hard coded "
        "bits. When adding new options, it is hard to locate a free slot. "
        "Perhaps this will help?\n\n");
    cols[0] = doc_alloc(36);
    cols[1] = doc_alloc(36);
    for (set = 0; set < 8; set++) /* 8 is hardcoded ... */
    {
        doc_ptr col = cols[set < 4 ? 0 : 1];
        for (bit = 0; bit < 32; bit++) /* u32b ... */
        {
            doc_printf(col, "<color:R>%d.%2d:</color> ", set, bit);
            for (seek = 0; option_info[seek].o_desc; seek++)
            {
                if ( option_info[seek].o_set == set
                  && option_info[seek].o_bit == bit )
                {
                    doc_printf(col, "%s", option_info[seek].o_text);
                    break;
                }
            }
            doc_newline(col);
        }
        doc_newline(col);
    }
    doc_insert_cols(doc, cols, 2, 0);
    doc_free(cols[0]);
    doc_free(cols[1]);
    doc_display(doc, "Option Bits", 0);
    doc_free(doc);
}

/************************************************************************
 * Public
 ************************************************************************/
void do_cmd_spoilers(void)
{
    int i, row, col;

    screen_save();

    while (1)
    {
        Term_clear();

        prt("View Spoilers", 2, 0);

        /* Give some choices */
        row = 4;
        col = 2;
        c_prt(TERM_RED, "Object Spoilers", row++, col - 2);
        prt("(a) Artifact Descriptions", row++, col);
        prt("(A) Artifact Tables", row++, col);
        prt("(o) Objects", row++, col);
        prt("(O) Object Tables", row++, col);
        row++;

        c_prt(TERM_RED, "Monster Spoilers", row++, col - 2);
        prt("(m) Brief Descriptions", row++, col);
        prt("(M) Full Descriptions", row++, col);
        prt("(e) Evolution", row++, col);
        prt("(d) Damage by Resistance", row++, col);
        prt("(D) Damage by Melee", row++, col);
        prt("(f) Spell Frequency", row++, col);
        row++;

        c_prt(TERM_RED, "Class Spoilers", row++, col - 2);
        prt("(s) Spells by Class", row++, col);
        prt("(r) Spells by Realm", row++, col);
        row++;

        row = 4;
        col = 40;

        c_prt(TERM_RED, "Miscellaneous", row++, col - 2);
        prt("(1) Option Bits", row++, col);
        prt("(2) Device Fail Rates", row++, col);
        row++;

        /* Prompt */
        prt("ESC) Exit menu", 21, 1);
        prt("Command: ", 20, 0);

        /* Prompt */
        i = inkey();

        /* Done */
        if (i == ESCAPE) break;
        switch (i)
        {
        /* Object Spoilers */
        case 'a':
            spoil_artifact_desc();
            break;
        case 'A':
            spoil_artifact_tables();
            break;
        case 'O':
            spoil_object_tables();
            break;
        case 'o':
            spoil_obj_desc("obj-desc.spo");
            break;

        /* Monster Spoilers */
        case 'm':
            spoil_mon_desc();
            break;
        case 'M':
            spoil_mon_info();
            break;
        case 'e':
            spoil_mon_evol();
            break;
        case 'd':
            spoil_mon_spell_dam();
            break;
        case 'D':
            spoil_mon_melee_dam();
            break;
        case 'f':
            spoil_mon_anger();
            break;

        /* Class Spoilers */
        case 's':
            spoil_spells_by_class();
            break;
        case 'r':
            spoil_spells_by_realm();
            break;

        /* Miscellaneous */
        case '1':
            spoil_option_bits();
            break;
        case '2':
            spoil_device_fail();
            break;

        /* Oops */
        default:
            bell();
            break;
        }

        /* Flush messages */
        msg_print(NULL);
    }
    screen_load();
}


#else

#ifdef MACINTOSH
static int i = 0;
#endif

#endif
