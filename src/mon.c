#include "angband.h"

#include "mon.h"
#include <assert.h>

int mon_ac(mon_ptr mon)
{
    mon_race_ptr race = &r_info[mon->r_idx];
    int          ac = race->ac;

    ac += mon->ac_adj;
    ac = ac * mon->mpower / 1000;
    if (ac < 0) ac = 0;
    /* XXX timed buffs */
    return ac;
}

bool mon_show_msg(mon_ptr mon)
{
    assert(mon);
    if (!mon->ml) return FALSE;
    if (p_ptr->blind) return FALSE;
    if (p_ptr->inside_battle) return TRUE;
    if (!ignore_unview) return TRUE;
    return player_can_see_bold(mon->fy, mon->fx)
        && projectable(py, px, mon->fy, mon->fx);
}

mon_race_ptr mon_race(mon_ptr mon)
{
    assert(mon);
    return &r_info[mon->ap_r_idx];
}

mon_race_ptr mon_true_race(mon_ptr mon)
{
    mon_race_ptr race;
    assert(mon);
    race = &r_info[mon->r_idx];
    /* XXX In general, mon->ap_r_idx == mon->r_idx. The only
     * exception is, I think, the Tanuki. However, I don't think
     * chameleons or shadowers should change r_idx ... Probably,
     * this was done solely to avoid changing the 1000+ places in
     * code that were using r_idx directly ... Reconsider. */
    if (mon->mflag2 & MFLAG2_CHAMELEON)
    {
        if (race->flags1 & RF1_UNIQUE) /* Chameleon lord only uses unique forms */
            return &r_info[MON_CHAMELEON_K];
        else                           /* other chameleons never use unique forms */
            return &r_info[MON_CHAMELEON];
    }
    return race;
}
