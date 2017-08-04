#include "angband.h"

#include <assert.h>

/************************************************************************
 * Utilities
 ************************************************************************/
char *_str_copy_aux(cptr s)
{
    char *copy = malloc(strlen(s) + 1);
    strcpy(copy, s);
    return copy;
}
char *_str_copy(cptr s)
{
    if (!s) return NULL;
    return _str_copy_aux(s);
}
char *_timestamp(void)
{
    time_t  now = time(NULL);
    char    buf[20];
    strftime(buf, 20, "%Y-%m-%d", localtime(&now));
    return _str_copy(buf);
}
char *_version(void)
{
    char buf[20];
    sprintf(buf, "%d.%d.%d", VER_MAJOR, VER_MINOR, VER_PATCH);
    return _str_copy(buf);
}
static char *_status(void)
{
    cptr status = 
        p_ptr->total_winner ? "Winner" : (p_ptr->is_dead ? "Dead" : "Alive");
    return _str_copy(status);
}
static char *_killer(void)
{
    if (p_ptr->is_dead)
        return _str_copy(p_ptr->died_from);
    return NULL;
}

/************************************************************************
 * Score Entries
 ************************************************************************/
score_ptr score_alloc(void)
{
    score_ptr score = malloc(sizeof(score_t));
    memset(score, 0, sizeof(score_t));
    return score;
}

score_ptr score_current(void)
{
    score_ptr       score = score_alloc();
    race_t         *race = get_true_race();
    class_t        *class_ = get_class();
    personality_ptr personality = get_personality();

    score->id = p_ptr->id;
    score->uid = player_uid;
    score->date = _timestamp();
    score->version = _version();
    score->score = p_ptr->max_max_exp;  /* XXX */

    score->name = _str_copy(player_name);
    score->race = _str_copy(race->name);
    score->subrace = _str_copy(race->subname);
    score->class_ = _str_copy(class_->name);
    score->subclass = _str_copy(class_->subname);

    score->sex = _str_copy(p_ptr->psex == SEX_MALE ? "Male" : "Female");
    score->personality = _str_copy(personality->name);

    score->gold = p_ptr->au;
    score->turns = player_turn;
    score->clvl = p_ptr->lev;
    score->dlvl = dun_level;
    score->dungeon = _str_copy(map_name());
    score->killer = _killer();
    score->status = _status();

    return score;
}

static int _parse_int(vec_ptr v, int i)
{
    if (i < vec_length(v))
    {
        string_ptr s = vec_get(v, i);
        return atoi(string_buffer(s));
    }
    return 0;
}
static char *_parse_string(vec_ptr v, int i)
{
    if (i < vec_length(v))
    {
        string_ptr s = vec_get(v, i);
        return _str_copy(string_buffer(s));
    }
    return NULL;
}
#define _FIELD_COUNT 19
static score_ptr score_read(FILE *fp)
{
    score_ptr  score = NULL;
    string_ptr line = string_alloc();
    vec_ptr    fields;

    string_read_line(line, fp);
    fields = string_split(line, '|');
    string_free(line);
    if (vec_length(fields))
    {
        score = score_alloc();
        score->id = _parse_int(fields, 0);
        score->uid = _parse_int(fields, 1);
        score->date = _parse_string(fields, 2);
        score->version = _parse_string(fields, 3);
        score->score = _parse_int(fields, 4);

        score->name = _parse_string(fields, 5);
        score->race = _parse_string(fields, 6);
        score->subrace = _parse_string(fields, 7);
        score->class_ = _parse_string(fields, 8);
        score->subclass = _parse_string(fields, 9);

        score->sex = _parse_string(fields, 10);
        score->personality = _parse_string(fields, 11);

        score->gold = _parse_int(fields, 12);
        score->turns = _parse_int(fields, 13);
        score->clvl = _parse_int(fields, 14);
        score->dlvl = _parse_int(fields, 15);
        score->dungeon = _parse_string(fields, 16);
        score->killer = _parse_string(fields, 17);
        score->status = _parse_string(fields, 18);
    }
    vec_free(fields);
    return score;
}

static cptr _opt(cptr s) { return s ? s : ""; }
static void score_write(score_ptr score, FILE* fp)
{
    string_ptr line = string_alloc();

    string_printf(line, "%d|%d|%s|%s|%d|",
        score->id, score->uid, score->date, score->version, score->score);

    string_printf(line, "%s|%s|%s|%s|%s|",
        score->name, score->race, _opt(score->subrace), score->class_, _opt(score->subclass));

    string_printf(line, "%s|%s|%d|%d|%d|%d|%s|%s|%s\n",
        score->sex, score->personality, score->gold, score->turns,
        score->clvl, score->dlvl, _opt(score->dungeon), _opt(score->killer),
        _opt(score->status));

    fputs(string_buffer(line), fp);
    string_free(line);
}

void score_free(score_ptr score)
{
    if (score)
    {
        if (score->date) free(score->date);
        if (score->version) free(score->version);
        if (score->name) free(score->name);
        if (score->race) free(score->race);
        if (score->subrace) free(score->subrace);
        if (score->class_) free(score->class_);
        if (score->subclass) free(score->subclass);
        if (score->sex) free(score->sex);
        if (score->personality) free(score->personality);
        if (score->dungeon) free(score->dungeon);
        if (score->killer) free(score->killer);
        if (score->status) free(score->status);
        free(score);
    }
}

static int score_cmp(score_ptr l, score_ptr r)
{
    if (l->score < r->score) return 1;
    if (l->score > r->score) return -1;
    if (l->id < r->id) return 1;
    if (l->id > r->id) return -1;
    return 0;
}

static bool score_is_winner(score_ptr score)
{
    return score->status && strcmp(score->status, "Winner") == 0;
}
static bool score_is_dead(score_ptr score)
{
    return score->status && strcmp(score->status, "Dead") == 0;
}

/************************************************************************
 * Scores File (scores.txt)
 ************************************************************************/
static FILE *_scores_fopen(cptr name, cptr mode)
{
    char buf[1024];
    path_build(buf, sizeof(buf), ANGBAND_DIR_APEX, name);
    return my_fopen(buf, mode);
}

vec_ptr scores_load(score_p filter)
{
    FILE   *fp = _scores_fopen("scores.txt", "r");
    vec_ptr v = vec_alloc((vec_free_f)score_free);

    if (fp)
    {
        for (;;)
        {
            score_ptr score = score_read(fp);
            if (!score) break;
            if (filter && !filter(score))
            {
                score_free(score);
                continue;
            }
            vec_add(v, score);
        }
        fclose(fp);
    }
    vec_sort(v, (vec_cmp_f)score_cmp);
    return v;
}

void scores_save(vec_ptr scores)
{
    int i;
    FILE *fp = _scores_fopen("scores.txt", "w");

    if (!fp)
    {
        msg_print("<color:v>Error:</color> Unable to open scores.txt");
        return;
    }
    vec_sort(scores, (vec_cmp_f)score_cmp);
    for (i = 0; i < vec_length(scores); i++)
    {
        score_ptr score = vec_get(scores, i);
        score_write(score, fp);
    }
    fclose(fp);
}

int scores_next_id(void)
{
    FILE *fp = _scores_fopen("next", "r");
    int   next;

    if (!fp)
    {
        fp = _scores_fopen("next", "w");
        if (!fp)
        {
            msg_print("<color:v>Error:</color> Unable to open next file in scores directory.");
            return 1;
        }
        fputc('2', fp);
        fclose(fp);
        return 1;
    }
   
    fscanf(fp, "%d", &next);
    fclose(fp);
    fp = _scores_fopen("next", "w");
    fprintf(fp, "%d", next + 1);
    fclose(fp);
    return next;
}

void scores_update(void)
{
    int       i;
    vec_ptr   scores = scores_load(NULL);
    score_ptr current = score_current();
    FILE     *fp;
    char      name[100];

    for (i = 0; i < vec_length(scores); i++)
    {
        score_ptr score = vec_get(scores, i);
        if (score->id == current->id)
        {
            vec_set(scores, i, current);
            break;
        }
    }
    if (i == vec_length(scores))
        vec_add(scores, current);
    scores_save(scores);
    vec_free(scores); /* current is now in scores[] and need not be freed */

    sprintf(name, "dump%d.doc", p_ptr->id);
    fp = _scores_fopen(name, "w");
    if (fp)
    {
        doc_ptr doc = doc_alloc(80);
        py_display_character_sheet(doc);
        doc_write_file(doc, fp, DOC_FORMAT_DOC);
        doc_free(doc);
        fclose(fp);
    } 
}

/************************************************************************
 * User Interface
 ************************************************************************/
void scores_display(vec_ptr scores)
{
    doc_ptr   doc = doc_alloc(80);
    int       top = 0, i, j, cmd;
    int       page_size = ui_screen_rect().cy - 5;
    score_ptr score;

    if (page_size > 26)
        page_size = 26;

    Term_clear();
    for (;;)
    {
        doc_clear(doc);
        doc_insert(doc, "<color:R>                High Score Listing</color>\n");
        doc_printf(doc, "<color:G>    %-20.20s %8.8s Rank Status</color>\n", "Name", "Score");
        for (i = 0; i < page_size; i++)
        {
            j = top + i;
            if (j >= vec_length(scores)) break;
            score = vec_get(scores, j);
            doc_printf(doc, " <color:y>%c)</color> %-20.20s", I2A(i), score->name);
            doc_printf(doc, " %8d %4d", score->score, j + 1);
            if (score_is_winner(score))
                doc_insert(doc, " <color:v>Winner</color>");
            else if (score_is_dead(score))
                doc_insert(doc, " <color:r>Dead</color>");
            else
                doc_insert(doc, " Alive");
            doc_newline(doc);
        }
        doc_insert(doc, "\n <color:U>Press corresponding letter to view last character sheet.</color>\n");
        if (page_size < vec_length(scores))
            doc_insert(doc, " <color:U>Use <color:keypress>PageUp</color> and <color:keypress>PageDown</color> to scroll.</color>\n");
        doc_sync_menu(doc);
        cmd = inkey_special(TRUE);
        if (cmd == ESCAPE || cmd == 'Q') break;
        if (islower(cmd))
        {
            j = top + A2I(cmd);
            if (0 <= j && j < vec_length(scores))
            {
                char name[30];
                FILE *fp;
                score = vec_get(scores, j);
                sprintf(name, "dump%d.doc", score->id);
                fp = _scores_fopen(name, "r");
                if (fp)
                {
                    doc_ptr d2 = doc_alloc(80);
                    doc_read_file(d2, fp);
                    fclose(fp);
                    doc_display(d2, name, 0);
                    doc_free(d2);
                    Term_clear();
                }
            }
        }
        else
        {
            switch (cmd)
            {
            case SKEY_PGDOWN: case '3': case ' ':
                if (top + page_size < vec_length(scores))
                    top += page_size;
                break;
            case SKEY_PGUP: case '9': case '-':
                if (top >= page_size)
                    top -= page_size;
                break;
            }
        }
    }
    Term_clear();
    doc_free(doc);
}

