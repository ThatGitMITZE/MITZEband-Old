/* File: mspells2.c */

/*
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 *
 * This software may be copied and distributed for educational, research,
 * and not for profit purposes provided that this copyright and statement
 * are included in all such copies. Other copyrights may also apply.
 */

/* Purpose: Monster spells (attack monster) */

#include "angband.h"


/*
 * Monster casts a breath (or ball) attack at another monster.
 * Pass over any monsters that may be in the way
 * Affect grids, objects, monsters, and the player
 */
static void monst_breath_monst(int m_idx, int y, int x, int typ, int dam_hp, int rad, bool breath, int monspell, bool learnable)
{
    int flg = PROJECT_GRID | PROJECT_ITEM | PROJECT_KILL;

    monster_type *m_ptr = &m_list[m_idx];
    monster_race *r_ptr = &r_info[m_ptr->r_idx];

    /* Determine the radius of the blast */
    if (rad < 1 && breath) rad = 2;

    /* Handle breath attacks */
    if (breath) rad = 0 - rad;

    switch (typ)
    {
    case GF_ROCKET:
        flg |= PROJECT_STOP;
        break;
    case GF_DRAIN_MANA:
    case GF_MIND_BLAST:
    case GF_BRAIN_SMASH:
    case GF_CAUSE_1:
    case GF_CAUSE_2:
    case GF_CAUSE_3:
    case GF_CAUSE_4:
    case GF_HAND_DOOM:
        flg |= (PROJECT_HIDE | PROJECT_AIMED);
        break;
    }

    (void)project(m_idx, rad, y, x, dam_hp, typ, flg, (learnable ? monspell : -1));
}


/*
 * Monster casts a bolt at another monster
 * Stop if we hit a monster
 * Affect monsters and the player
 */
static void monst_bolt_monst(int m_idx, int y, int x, int typ, int dam_hp, int monspell, bool learnable)
{
    int flg = PROJECT_STOP | PROJECT_KILL | PROJECT_REFLECTABLE;

    (void)project(m_idx, 0, y, x, dam_hp, typ, flg, (learnable ? monspell : -1));
}

static void monst_beam_monst(int m_idx, int y, int x, int typ, int dam_hp, int monspell, bool learnable)
{
    int flg = PROJECT_BEAM | PROJECT_KILL | PROJECT_THRU;

    (void)project(m_idx, 0, y, x, dam_hp, typ, flg, (learnable ? monspell : -1));
}

/*
 * Determine if a beam spell will hit the target.
 */
static bool direct_beam(int y1, int x1, int y2, int x2, monster_type *m_ptr)
{
    bool hit2 = FALSE;
    int i, y, x;

    int grid_n = 0;
    u16b grid_g[512];

    bool friend = is_pet(m_ptr);

    /* Check the projection path */
    grid_n = project_path(grid_g, MAX_RANGE, y1, x1, y2, x2, PROJECT_THRU);

    /* No grid is ever projectable from itself */
    if (!grid_n) return (FALSE);

    for (i = 0; i < grid_n; i++)
    {
        y = GRID_Y(grid_g[i]);
        x = GRID_X(grid_g[i]);

        if (y == y2 && x == x2)
            hit2 = TRUE;
        else if (friend && cave[y][x].m_idx > 0 &&
             !are_enemies(m_ptr, &m_list[cave[y][x].m_idx]))
        {
            /* Friends don't shoot friends */
            return FALSE;
        }

        if (friend && player_bold(y, x))
            return FALSE;
    }
    if (!hit2)
        return FALSE;
    return TRUE;
}

static bool breath_direct(int y1, int x1, int y2, int x2, int rad, int typ, bool friend)
{
    /* Must be the same as projectable() */

    int i;

    /* Initial grid */
    int y = y1;
    int x = x1;

    int grid_n = 0;
    u16b grid_g[512];

    int grids = 0;
    byte gx[1024], gy[1024];
    byte gm[32];
    int gm_rad = rad;

    bool hit2 = FALSE;
    bool hityou = FALSE;

    int flg;

    switch (typ)
    {
    case GF_LITE:
    case GF_LITE_WEAK:
        flg = PROJECT_LOS;
        break;
    case GF_DISINTEGRATE:
        flg = PROJECT_DISI;
        break;
    default:
        flg = 0;
        break;
    }

    /* Check the projection path */
    grid_n = project_path(grid_g, MAX_RANGE, y1, x1, y2, x2, flg);

    /* Project along the path */
    for (i = 0; i < grid_n; ++i)
    {
        int ny = GRID_Y(grid_g[i]);
        int nx = GRID_X(grid_g[i]);

        if (flg & PROJECT_DISI)
        {
            /* Hack -- Balls explode before reaching walls */
            if (cave_stop_disintegration(ny, nx)) break;
        }
        else if (flg & PROJECT_LOS)
        {
            /* Hack -- Balls explode before reaching walls */
            if (!cave_los_bold(ny, nx)) break;
        }
        else
        {
            /* Hack -- Balls explode before reaching walls */
            if (!cave_have_flag_bold(ny, nx, FF_PROJECT)) break;
        }

        /* Save the "blast epicenter" */
        y = ny;
        x = nx;
    }

    grid_n = i;

    if (!grid_n)
    {
        if (flg & PROJECT_DISI)
        {
            if (in_disintegration_range(y1, x1, y2, x2) && (distance(y1, x1, y2, x2) <= rad)) hit2 = TRUE;
            if (in_disintegration_range(y1, x1, py, px) && (distance(y1, x1, py, px) <= rad)) hityou = TRUE;
        }
        else if (flg & PROJECT_LOS)
        {
            if (los(y1, x1, y2, x2) && (distance(y1, x1, y2, x2) <= rad)) hit2 = TRUE;
            if (los(y1, x1, py, px) && (distance(y1, x1, py, px) <= rad)) hityou = TRUE;
        }
        else
        {
            if (projectable(y1, x1, y2, x2) && (distance(y1, x1, y2, x2) <= rad)) hit2 = TRUE;
            if (projectable(y1, x1, py, px) && (distance(y1, x1, py, px) <= rad)) hityou = TRUE;
        }
    }
    else
    {
        breath_shape(grid_g, grid_n, &grids, gx, gy, gm, &gm_rad, rad, y1, x1, y, x, typ);

        for (i = 0; i < grids; i++)
        {
            /* Extract the location */
            y = gy[i];
            x = gx[i];

            if ((y == y2) && (x == x2)) hit2 = TRUE;
            if (player_bold(y, x)) hityou = TRUE;
        }
    }

    if (!hit2) return FALSE;
    if (friend && hityou) return FALSE;

    return TRUE;
}

/*
 * Get the actual center point of ball spells (rad > 1) (originally from TOband)
 */
void get_project_point(int sy, int sx, int *ty, int *tx, int flg)
{
    u16b path_g[128];
    int  path_n, i;

    path_n = project_path(path_g, MAX_RANGE, sy, sx, *ty, *tx, flg);

    *ty = sy;
    *tx = sx;

    /* Project along the path */
    for (i = 0; i < path_n; i++)
    {
        sy = GRID_Y(path_g[i]);
        sx = GRID_X(path_g[i]);

        /* Hack -- Balls explode before reaching walls */
        if (!cave_have_flag_bold(sy, sx, FF_PROJECT)) break;

        *ty = sy;
        *tx = sx;
    }
}

/*
 * Check should monster cast dispel spell at other monster.
 */
static bool dispel_check_monster(int m_idx, int t_idx)
{
    monster_type *t_ptr = &m_list[t_idx];

    /* Invulnabilty */
    if (MON_INVULNER(t_ptr)) return TRUE;

    /* Speed */
    if (t_ptr->mspeed < 135)
    {
        if (MON_FAST(t_ptr)) return TRUE;
    }

    /* Riding monster */
    if (t_idx == p_ptr->riding)
    {
        if (dispel_check(m_idx)) return TRUE;
    }

    /* No need to cast dispel spell */
    return FALSE;
}

/*
 * Monster tries to 'cast a spell' (or breath, etc)
 * at another monster.
 *
 * The player is only disturbed if able to be affected by the spell.
 */
bool mon_spell_mon(int m_idx, int options)
{
    int y = 0, x = 0;
    int i, k, t_idx = 0;
    int thrown_spell, count = 0;
    int rlev;
    int dam = 0;
    int start;
    int plus = 1;
    u32b u_mode = 0L;
    int s_num_6 = 6;
    int s_num_4 = 4;

    byte spell[96], num = 0;

    char m_name[MAX_NLEN];
    char t_name[MAX_NLEN];
    char tmp[MAX_NLEN];
    char m_poss[MAX_NLEN];

    monster_type *m_ptr = &m_list[m_idx];
    monster_type *t_ptr = NULL;

    monster_race *r_ptr = &r_info[m_ptr->r_idx];
    monster_race *tr_ptr = NULL;

    u32b f4, f5, f6;

    bool wake_up = FALSE;
    bool fear = FALSE;

    bool blind = (p_ptr->blind ? TRUE : FALSE);

    bool see_m = is_seen(m_ptr);
    bool maneable = player_has_los_bold(m_ptr->fy, m_ptr->fx);
    bool learnable = (m_ptr->ml && maneable && !world_monster);
    bool see_t;
    bool see_either;
    bool known;

    bool pet = is_pet(m_ptr);

    bool in_no_magic_dungeon = py_in_dungeon() && (d_info[dungeon_type].flags1 & DF1_NO_MAGIC);

    bool can_use_lite_area = FALSE;

    bool can_remember;

    bool resists_tele = FALSE;

    /* Prepare flags for summoning */
    if (!pet) u_mode |= PM_ALLOW_UNIQUE;

    /* Cannot cast spells when confused */
    if (MON_CONFUSED(m_ptr)) return (FALSE);

    /* Extract the racial spell flags */
    f4 = r_ptr->flags4;
    f5 = r_ptr->flags5;
    f6 = r_ptr->flags6;


    /* Dragonrider guided breaths will piggy back the pet target for now. I'd like to be
       able to guide the breath to a chosen grid location in addition to a monster, but
       this doesn't look easy to accomplish */
    if (options & DRAGONRIDER_HACK)
    {
        f4 &= RF4_BREATH_MASK;
        f5 &= RF5_BREATH_MASK;
        f6 &= RF6_BREATH_MASK;
        if (!target_okay()) return FALSE;
        y = target_row;
        x = target_col;
        t_idx = cave[y][x].m_idx;
        if (t_idx)
        {
            t_ptr = &m_list[t_idx];
            tr_ptr = &r_info[t_ptr->r_idx];
        }
    }
    else
    {
        /* Target is given for pet? */
        if (pet_t_m_idx && pet)
        {
            t_idx = pet_t_m_idx;
            t_ptr = &m_list[t_idx];

            /* Cancel if not projectable (for now) */
            if ((m_idx == t_idx) || !projectable(m_ptr->fy, m_ptr->fx, t_ptr->fy, t_ptr->fx))
            {
                t_idx = 0;
            }
        }
        /* Is there counter attack target? */
        if (!t_idx && m_ptr->target_y)
        {
            t_idx = cave[m_ptr->target_y][m_ptr->target_x].m_idx;

            if (t_idx)
            {
                t_ptr = &m_list[t_idx];

                /* Cancel if neither enemy nor a given target */
                if ((m_idx == t_idx) ||
                    ((t_idx != pet_t_m_idx) && !are_enemies(m_ptr, t_ptr)))
                {
                    t_idx = 0;
                }

                /* Allow only summoning etc.. if not projectable */
                else if (!projectable(m_ptr->fy, m_ptr->fx, t_ptr->fy, t_ptr->fx))
                {
                    f4 &= (RF4_INDIRECT_MASK);
                    f5 &= (RF5_INDIRECT_MASK);
                    f6 &= (RF6_INDIRECT_MASK);
                }
            }
        }

        /* Look for enemies normally */
        if (!t_idx)
        {
            bool success = FALSE;

            if (p_ptr->inside_battle)
            {
                start = randint1(m_max-1) + m_max;
                if (randint0(2)) plus = -1;
            }
            else start = m_max + 1;

            /* Scan thru all monsters */
            for (i = start; ((i < start + m_max) && (i > start - m_max)); i += plus)
            {
                int dummy = (i % m_max);
                if (!dummy) continue;

                t_idx = dummy;
                t_ptr = &m_list[t_idx];

                /* Skip dead monsters */
                if (!t_ptr->r_idx) continue;

                /* Monster must be 'an enemy' */
                if ((m_idx == t_idx) || !are_enemies(m_ptr, t_ptr)) continue;

                /* Monster must be projectable */
                if (!projectable(m_ptr->fy, m_ptr->fx, t_ptr->fy, t_ptr->fx)) continue;

                /* Get it */
                success = TRUE;
                break;
            }

            /* No enemy found */
            if (!success) return FALSE;
        }
        /* OK -- we've got a target */
        y = t_ptr->fy;
        x = t_ptr->fx;
        tr_ptr = &r_info[t_ptr->r_idx];

        /* No monster fighting (option) except involving pets */
        if ( !p_ptr->inside_battle
          && !allow_hostile_monster
          && !(pet || is_pet(t_ptr)) )
        {
            return (FALSE);
        }

        /* Forget old counter attack target */
        reset_target(m_ptr);
    }

    /* Extract the monster level */
    rlev = ((r_ptr->level >= 1) ? r_ptr->level : 1);

    /* Remove unimplemented spells */
    f6 &= ~(RF6_WORLD | RF6_TRAPS | RF6_FORGET);

    if (f4 & RF4_BR_LITE)
    {
        if (!los(m_ptr->fy, m_ptr->fx, y, x))
            f4 &= ~(RF4_BR_LITE);
    }

    /* Remove unimplemented special moves */
    if (f6 & RF6_SPECIAL)
    {
        if ((m_ptr->r_idx != MON_ROLENTO) && (r_ptr->d_char != 'B'))
            f6 &= ~(RF6_SPECIAL);
    }

    if (f6 & RF6_DARKNESS)
    {
        bool vs_ninja = (p_ptr->pclass == CLASS_NINJA) && !is_hostile(t_ptr);

        if (vs_ninja &&
            !(r_ptr->flags3 & (RF3_UNDEAD | RF3_HURT_LITE)) &&
            !(r_ptr->flags7 & RF7_DARK_MASK))
            can_use_lite_area = TRUE;

        if (!(r_ptr->flags2 & RF2_STUPID))
        {
            if (d_info[dungeon_type].flags1 & DF1_DARKNESS) f6 &= ~(RF6_DARKNESS);
            else if (vs_ninja && !can_use_lite_area) f6 &= ~(RF6_DARKNESS);
        }
    }

    if (in_no_magic_dungeon && !(r_ptr->flags2 & RF2_STUPID))
    {
        f4 &= (RF4_NOMAGIC_MASK);
        f5 &= (RF5_NOMAGIC_MASK);
        f6 &= (RF6_NOMAGIC_MASK);
    }

    if (p_ptr->inside_arena || p_ptr->inside_battle || !(pet || is_pet(t_ptr)))
    {
        f4 &= ~(RF4_SUMMON_MASK);
        f5 &= ~(RF5_SUMMON_MASK);
        f6 &= ~(RF6_SUMMON_MASK | RF6_TELE_LEVEL);

        if (m_ptr->r_idx == MON_ROLENTO) f6 &= ~(RF6_SPECIAL);
    }

    if (p_ptr->inside_battle && !one_in_(3))
    {
        f6 &= ~(RF6_HEAL);
    }

    if (m_idx == p_ptr->riding)
    {
        f4 &= ~(RF4_RIDING_MASK);
        f5 &= ~(RF5_RIDING_MASK);
        f6 &= ~(RF6_RIDING_MASK);
    }

    if (pet)
    {
        f4 &= ~(RF4_SHRIEK);
        f6 &= ~(RF6_DARKNESS | RF6_TRAPS);

        if (!(p_ptr->pet_extra_flags & PF_TELEPORT))
        {
            f6 &= ~(RF6_BLINK | RF6_TPORT | RF6_TELE_TO | RF6_TELE_AWAY | RF6_TELE_LEVEL);
        }

        if (!(p_ptr->pet_extra_flags & PF_ATTACK_SPELL))
        {
            f4 &= ~(RF4_ATTACK_MASK);
            f5 &= ~(RF5_ATTACK_MASK);
            f6 &= ~(RF6_ATTACK_MASK);
        }

        if (!(p_ptr->pet_extra_flags & PF_SUMMON_SPELL))
        {
            f4 &= ~(RF4_SUMMON_MASK);
            f5 &= ~(RF5_SUMMON_MASK);
            f6 &= ~(RF6_SUMMON_MASK);
        }

        /* Prevent collateral damage */
        if (!(p_ptr->pet_extra_flags & PF_BALL_SPELL) && (m_idx != p_ptr->riding))
        {
            if ((f4 & (RF4_BALL_MASK & ~(RF4_ROCKET))) ||
                (f5 & RF5_BALL_MASK) ||
                (f6 & RF6_BALL_MASK))
            {
                int real_y = y;
                int real_x = x;

                get_project_point(m_ptr->fy, m_ptr->fx, &real_y, &real_x, 0L);

                if (projectable(real_y, real_x, py, px))
                {
                    int dist = distance(real_y, real_x, py, px);

                    if (dist <= 2)
                    {
                        f4 &= ~(RF4_BALL_MASK & ~(RF4_ROCKET));
                        f5 &= ~(RF5_BALL_MASK);
                        f6 &= ~(RF6_BALL_MASK);
                    }
                    else if (dist <= 4)
                    {
                        f4 &= ~(RF4_BIG_BALL_MASK);
                        f5 &= ~(RF5_BIG_BALL_MASK);
                        f6 &= ~(RF6_BIG_BALL_MASK);
                    }
                }
                else if (f5 & RF5_BA_LITE)
                {
                    if ((distance(real_y, real_x, py, px) <= 4) && los(real_y, real_x, py, px))
                        f5 &= ~(RF5_BA_LITE);
                }
            }

            if (f4 & RF4_ROCKET)
            {
                int real_y = y;
                int real_x = x;

                get_project_point(m_ptr->fy, m_ptr->fx, &real_y, &real_x, PROJECT_STOP);
                if (projectable(real_y, real_x, py, px) && (distance(real_y, real_x, py, px) <= 2))
                    f4 &= ~(RF4_ROCKET);
            }

            if (((f4 & RF4_BEAM_MASK) ||
                 (f5 & RF5_BEAM_MASK) ||
                 (f6 & RF6_BEAM_MASK)) &&
                !direct_beam(m_ptr->fy, m_ptr->fx, y, x, m_ptr))
            {
                f4 &= ~(RF4_BEAM_MASK);
                f5 &= ~(RF5_BEAM_MASK);
                f6 &= ~(RF6_BEAM_MASK);
            }

            if ((f4 & RF4_BREATH_MASK) ||
                (f5 & RF5_BREATH_MASK) ||
                (f6 & RF6_BREATH_MASK))
            {
                /* Expected breath radius */
                int rad = 2;

                if (!breath_direct(m_ptr->fy, m_ptr->fx, y, x, rad, 0, TRUE))
                {
                    f4 &= ~(RF4_BREATH_MASK);
                    f5 &= ~(RF5_BREATH_MASK);
                    f6 &= ~(RF6_BREATH_MASK);
                }
                else if ((f4 & RF4_BR_LITE) &&
                     !breath_direct(m_ptr->fy, m_ptr->fx, y, x, rad, GF_LITE, TRUE))
                {
                    f4 &= ~(RF4_BR_LITE);
                }
                else if ((f4 & RF4_BR_DISI) &&
                     !breath_direct(m_ptr->fy, m_ptr->fx, y, x, rad, GF_DISINTEGRATE, TRUE))
                {
                    f4 &= ~(RF4_BR_DISI);
                }
            }
        }

        /* Special moves restriction */
        if (f6 & RF6_SPECIAL)
        {
            if (m_ptr->r_idx == MON_ROLENTO)
            {
                if ((p_ptr->pet_extra_flags & (PF_ATTACK_SPELL | PF_SUMMON_SPELL)) != (PF_ATTACK_SPELL | PF_SUMMON_SPELL))
                    f6 &= ~(RF6_SPECIAL);
            }
            else if (r_ptr->d_char == 'B')
            {
                if ((p_ptr->pet_extra_flags & (PF_ATTACK_SPELL | PF_TELEPORT)) != (PF_ATTACK_SPELL | PF_TELEPORT))
                    f6 &= ~(RF6_SPECIAL);
            }
            else f6 &= ~(RF6_SPECIAL);
        }
    }

    /* Remove some spells if necessary */

    if (!(r_ptr->flags2 & RF2_STUPID))
    {
        /* Check for a clean bolt shot */
        if (((f4 & RF4_BOLT_MASK) ||
             (f5 & RF5_BOLT_MASK) ||
             (f6 & RF6_BOLT_MASK)) &&
            !clean_shot(m_ptr->fy, m_ptr->fx, y, x, pet))
        {
            f4 &= ~(RF4_BOLT_MASK);
            f5 &= ~(RF5_BOLT_MASK);
            f6 &= ~(RF6_BOLT_MASK);
        }

        /* Check for a possible summon */
        if (((f4 & RF4_SUMMON_MASK) ||
             (f5 & RF5_SUMMON_MASK) ||
             (f6 & RF6_SUMMON_MASK)) &&
            !(summon_possible(y, x)))
        {
            /* Remove summoning spells */
            f4 &= ~(RF4_SUMMON_MASK);
            f5 &= ~(RF5_SUMMON_MASK);
            f6 &= ~(RF6_SUMMON_MASK);
        }

        /* Dispel magic */
        if ((f4 & RF4_DISPEL) && (!t_idx || !dispel_check_monster(m_idx, t_idx)))
        {
            /* Remove dispel spell */
            f4 &= ~(RF4_DISPEL);
        }

        /* Anti-magic only vs. Player for now */
        if (f4 & RF4_ANTI_MAGIC)
        {
            f4 &= ~(RF4_ANTI_MAGIC);
        }

        /* Check for a possible raise dead */
        if ((f6 & RF6_RAISE_DEAD) && !raise_possible(m_ptr))
        {
            /* Remove raise dead spell */
            f6 &= ~(RF6_RAISE_DEAD);
        }

        /* Special moves restriction */
        if (f6 & RF6_SPECIAL)
        {
            if ((m_ptr->r_idx == MON_ROLENTO) && !summon_possible(y, x))
            {
                f6 &= ~(RF6_SPECIAL);
            }
        }
    }

    if (r_ptr->flags2 & RF2_SMART)
    {
        /* Hack -- allow "desperate" spells */
        if ((m_ptr->hp < m_ptr->maxhp / 10) &&
            (randint0(100) < 50))
        {
            /* Require intelligent spells */
            f4 &= (RF4_INT_MASK);
            f5 &= (RF5_INT_MASK);
            f6 &= (RF6_INT_MASK);
        }

        /* Hack -- decline "teleport level" in some case */
        if ((f6 & RF6_TELE_LEVEL) && TELE_LEVEL_IS_INEFF((t_idx == p_ptr->riding) ? 0 : t_idx))
        {
            f6 &= ~(RF6_TELE_LEVEL);
        }
    }

    /* No spells left */
    if (!f4 && !f5 && !f6) return FALSE;

    /* Extract the "inate" spells */
    for (k = 0; k < 32; k++)
    {
        if (f4 & (1L << k)) spell[num++] = k + 32 * 3;
    }

    /* Extract the "normal" spells */
    for (k = 0; k < 32; k++)
    {
        if (f5 & (1L << k)) spell[num++] = k + 32 * 4;
    }

    /* Extract the "bizarre" spells */
    for (k = 0; k < 32; k++)
    {
        if (f6 & (1L << k)) spell[num++] = k + 32 * 5;
    }

    /* No spells left */
    if (!num) return (FALSE);

    /* Stop if player is dead or gone */
    if (!p_ptr->playing || p_ptr->is_dead) return (FALSE);

    /* Handle "leaving" */
    if (p_ptr->leaving) return (FALSE);

    /* Get the monster name (or "it") */
    monster_desc(tmp, m_ptr, 0x00);
    tmp[0] = toupper(tmp[0]);
    sprintf(m_name, "<color:B>%s</color>", tmp);

    /* Get the monster possessive ("his"/"her"/"its") */
    monster_desc(m_poss, m_ptr, MD_PRON_VISIBLE | MD_POSSESSIVE);

    /* Get the target's name (or "it") */
    if (t_ptr)
    {
        monster_desc(tmp, t_ptr, 0x00);
        sprintf(t_name, "<color:o>%s</color>", tmp);
    }
    else
        sprintf(t_name, "<color:o>%s</color>", "the target");

    /* Choose a spell to cast */
    thrown_spell = spell[randint0(num)];

    if (t_ptr)
        see_t = is_seen(t_ptr);
    else
        see_t = los(y, x, py, px);

    see_either = (see_m || see_t);

    /* Can the player be aware of this attack? */
    if (t_ptr)
        known = (m_ptr->cdis <= MAX_SIGHT) || (t_ptr->cdis <= MAX_SIGHT);
    else
        known = see_t;

    if (disturb_minor && p_ptr->riding && (m_idx == p_ptr->riding)) disturb(1, 0);

    /* Check for spell failure (inate attacks never fail)
    if (!spell_is_innate(thrown_spell) && (in_no_magic_dungeon || (MON_STUNNED(m_ptr) && one_in_(2))))
    {
        if (see_m) msg_format("%^s tries to cast a spell, but fails.", m_name);

        return (TRUE);
    } */

    /* Hex: Anti Magic Barrier
    if (!spell_is_innate(thrown_spell) && magic_barrier(m_idx))
    {
        if (see_m) msg_format("Your anti-magic barrier blocks the spell which %^s casts.");
        return (TRUE);
    }

    if (!spell_is_innate(thrown_spell) && psion_check_disruption(m_idx))
    {
        msg_format("Your psionic disruption blocks the spell which %^s casts.", m_name);
        return TRUE;
    }*/

    can_remember = is_original_ap_and_seen(m_ptr);
return FALSE;
}
