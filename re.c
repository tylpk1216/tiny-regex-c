/*
 *
 * Mini regex-module inspired by Rob Pike's regex code described in:
 *
 * http://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html
 *
 *
 *
 * Supports:
 * ---------
 *   '.'        Dot, matches any character
 *   '^'        Start anchor, matches beginning of string
 *   '$'        End anchor, matches end of string
 *   '*'        Asterisk, match zero or more (greedy)
 *   '+'        Plus, match one or more (greedy)
 *   '?'        Question, match zero or one (non-greedy)
 *   '[abc]'    Character class, match if one of {'a', 'b', 'c'}
 *   '[^abc]'   Inverted class, match if NOT one of {'a', 'b', 'c'} -- NOTE: feature is currently broken!
 *   '[a-zA-Z]' Character ranges, the character set of the ranges { a-z | A-Z }
 *   '\s'       Whitespace, \t \f \r \n \v and spaces
 *   '\S'       Non-whitespace
 *   '\w'       Alphanumeric, [a-zA-Z0-9_]
 *   '\W'       Non-alphanumeric
 *   '\d'       Digits, [0-9]
 *   '\D'       Non-digits
 *
 *
 */



#include "re.h"
#include <stdio.h>


/* Definitions: */
#define MAX_QUANT           255    /* Max b in {a,b}. 255 since a & b are char   */
#define MAX_COUNT         40000    /* for + and *,  above 32768 for test2        */



#define X_RE_TYPES  X(UNUSED) X(DOT) X(BEGIN) X(END) X(QUANT) X(QUANT_LAZY) \
        X(QMARK) X(QMARK_LAZY) X(STAR) X(STAR_LAZY) X(PLUS) X(PLUS_LAZY) \
        X(CHAR) X(CCLASS) X(INV_CCLASS) X(DIGIT) X(NOT_DIGIT) X(ALPHA) X(NOT_ALPHA) \
        X(WSPACE) X(NOT_WSPACE) X(BRANCH)

#define X(A) A,
enum { X_RE_TYPES };
#undef X



/* Private function declarations: */
static const char *matchpattern(const re_node *pattern, const char *text);

/* Public functions: */
int re_match(const char *pattern, const char *text, const char **next)
{
    re_comp compiled = {0};
    int ret = re_compile(pattern, &compiled);
    if (!ret)
    {
        printf("compiling pattern '%s' failed\n", pattern);
        return -1;
    }
    return re_matchp(&compiled, text, next);
}

int re_matchp(const re_comp *compiled, const char *text, const char **next)
{
    if (!compiled)
        return -1;

    const char *end;

    const re_node *pattern = compiled->nodes;

    if (pattern[0].type == BEGIN)
    {
        if ((end = matchpattern(&pattern[1], text)))
        {
            if (next) *next = end;
            return 0;
        }
        return -1;
    }

    int idx = -1;

    do
    {
        idx += 1;

        if ((end = matchpattern(pattern, text)))
        {
            if (text[0] == '\0') //Fixme: ???
                return -1;
            if (next) *next = end;
            return idx;
        }
    }
    while (*text++ != '\0');

    return -1;
}

int re_compile(const char *pattern, re_comp *compiled)
{
    if (!compiled)
        return 0;
    re_node *re_compiled = compiled->nodes;
    unsigned char *ccl_buf = compiled->buffer;

    int ccl_bufidx = 1;

    char c;     /* current char in pattern   */
    int i = 0;  /* index into pattern        */
    int j = 0;  /* index into re_compiled    */

    while (pattern[i] != '\0' && (j + 1 < MAX_RENODES))
    {
        c = pattern[i];

        switch (c)
        {
        /* Meta-characters: */
        case '^': {    re_compiled[j].type = BEGIN;           } break;
        case '$': {    re_compiled[j].type = END;             } break;
        case '.': {    re_compiled[j].type = DOT;             } break;
        case '*':
            re_compiled[j].type = (pattern[i + 1] == '?') ? (i++, STAR_LAZY) : STAR; break;
        case '+':
            re_compiled[j].type = (pattern[i + 1] == '?') ? (i++, PLUS_LAZY) : PLUS; break;
        case '?':
            re_compiled[j].type = (pattern[i + 1] == '?') ? (i++, QMARK_LAZY) : QMARK; break;
        /*    case '|': {    re_compiled[j].type = BRANCH;          } break; <-- not working properly */

        /* Escaped character-classes (\s \w ...): */
        case '\\':
        {
            i++;
            if (pattern[i] == '\0')
            {
                // dangling '\'
                return 0;
            }

            switch (pattern[i])
            {
            /* Meta-character: */
            case 'd': { re_compiled[j].type = DIGIT;      } break;
            case 'D': { re_compiled[j].type = NOT_DIGIT;  } break;
            case 'w': { re_compiled[j].type = ALPHA;      } break;
            case 'W': { re_compiled[j].type = NOT_ALPHA;  } break;
            case 's': { re_compiled[j].type = WSPACE;     } break;
            case 'S': { re_compiled[j].type = NOT_WSPACE; } break;

            /* Escaped character, e.g. '.' or '$' */
            default:
            {
                re_compiled[j].type = CHAR;
                re_compiled[j].ch = pattern[i];
            } break;
            }
        } break;

        /* Character class: */
        case '[':
        {
            /* Remember where the char-buffer starts. */
            int buf_begin = ccl_bufidx;
            char nc = pattern[i + 1];

            /* Look-ahead to determine if negated */
            re_compiled[j].type = (nc == '^') ? (i++, INV_CCLASS) : CCLASS;

            /* Copy characters inside [..] to buffer */
            while (pattern[++i] != ']' && pattern[i] != '\0') /* Missing ] */
            {
                if (pattern[i] == '\\')
                {
                    if (!(nc = pattern[i + 1]))
                        return 0;

                    // skip slash for non meta chars and slash
                    if (nc == 's' || nc == 'S' || nc == 'w' || nc == 'W' ||
                            nc == 'd' || nc == 'D' || nc == '\\')
                    {
                        if (ccl_bufidx >= MAX_REBUFLEN - 2)
                            return 0;
                        ccl_buf[ccl_bufidx++] = pattern[i];
                    }
                    i++;
                }
                else if (pattern[i + 1] == '-' && pattern[i + 2] != '0')
                {
                    if (pattern[i] > pattern[i + 2])
                        return 0;
                }
                if (ccl_bufidx >= MAX_REBUFLEN - 1)
                {
                    //fputs("exceeded internal buffer!\n", stderr);
                    return 0;
                }
                ccl_buf[ccl_bufidx++] = pattern[i];
            }
            if (pattern[i] != ']') // pattern[i] == '\0'
            {
                //printf("char class missing ] at %i\n", i);
                return 0;
            }
            /* Null-terminate string end */
            ccl_buf[ccl_bufidx++] = 0;
            re_compiled[j].ccl = &ccl_buf[buf_begin];
        } break;

        /* Quantifier: */
        case '{':
        {
            //re_compiled[j].type = QUANT;
            // consumes 2 chars (one char for each min and max <= 255)
            if (ccl_bufidx >= MAX_REBUFLEN - 1)
            {
                return 0;
            }
            i++;
            unsigned val = 0;
            do
            {
                if (pattern[i] < '0' || pattern[i] > '9')
                    return 0;
                val = 10 * val + (pattern[i++] - '0');
            }
            while (pattern[i] != ',' && pattern[i] != '}');
            if (val > MAX_QUANT)
            {
                return 0;
            }
            ccl_buf[ccl_bufidx] = val;
            if (pattern[i] == ',')
            {
                i++;
                if (pattern[i] == '}')
                {
                    val = MAX_QUANT;
                }
                else
                {
                    val = 0;
                    while (pattern[i] != '}')
                    {
                        if (pattern[i] < '0' || pattern[i] > '9')
                            return 0;
                        val = 10 * val + (pattern[i++] - '0');
                    }

                    if (val > MAX_QUANT || val < ccl_buf[ccl_bufidx])
                    {
                        return 0;
                    }
                }
            }
            re_compiled[j].type = (pattern[i + 1] == '?') ? (i++, QUANT_LAZY) : QUANT;
            ccl_buf[ccl_bufidx + 1] = val;
            re_compiled[j].ccl = &ccl_buf[ccl_bufidx];
            ccl_bufidx += 2;
        } break;

        /* Other characters: */
        default:
        {
            re_compiled[j].type = CHAR;
            re_compiled[j].ch = c;
        } break;
        }
        i += 1;
        j += 1;
    }
    /* 'UNUSED' is a sentinel used to indicate end-of-pattern */
    re_compiled[j].type = UNUSED;

    return 1;
}

/* Private functions: */
static int matchdigit(char c)
{
    return ((c >= '0') && (c <= '9'));
}
static int matchalpha(char c)
{
    return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'));
}
static int matchwhitespace(char c)
{
    return ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') || (c == '\f') || (c == '\v'));
}
static int matchalphanum(char c)
{
    return ((c == '_') || matchalpha(c) || matchdigit(c));
}

static int matchmetachar(char c, char mc)
{
    switch (mc)
    {
    case 'd': return  matchdigit(c);
    case 'D': return !matchdigit(c);
    case 'w': return  matchalphanum(c);
    case 'W': return !matchalphanum(c);
    case 's': return  matchwhitespace(c);
    case 'S': return !matchwhitespace(c);
    default:  return (c == mc);
    }
}

static int matchcharclass(char c, const unsigned char *str)
{
    while (*str != '\0')
    {
        if (str[0] == '\\')
        {
            // if (str[1] == '\0') { return 0; } // shouldn't happen; compiling would also fail
            if (matchmetachar(c, str[1]))
            {
                return 1;
            }
            str += 2;
        }
        else if (str[1] == '-' && str[2] != '\0')
        {
            if (c >= str[0] && c <= str[2])
            {
                return 1;
            }
            str += 3;
        }
        else
        {
            if (c == *str)
            {
                return 1;
            }
            str += 1;
        }
    }

    return 0;
}

static int matchone(const re_node *p, char c)
{
    switch (p->type)
    {
    case DOT:        return (c != '\n' && c != '\r');
    case CCLASS:     return  matchcharclass(c, p->ccl);
    case INV_CCLASS: return !matchcharclass(c, p->ccl);
    case DIGIT:      return  matchdigit(c);
    case NOT_DIGIT:  return !matchdigit(c);
    case ALPHA:      return  matchalphanum(c);
    case NOT_ALPHA:  return !matchalphanum(c);
    case WSPACE:     return  matchwhitespace(c);
    case NOT_WSPACE: return !matchwhitespace(c);
    default:         return (p->ch == c);
    }
}

static const char *matchquant_lazy(const re_node *p, const char *text, int min, int max)
{
    const char *end;
    max -= min;
    while (min > 0 && *text && matchone(p, *text)) { text++; min--; }
    if (min > 0)
        return 0;
    do
    {
        if ((end = matchpattern(p + 2, text)))
            return end;
        max--;
    }
    while (max >= 0 && *text && matchone(p, *text++));

    return 0;
}

static const char *matchquant(const re_node *p, const char *text, int min, int max)
{
    const char *end, *start = text;
    while (max > 0 && *text && matchone(p, *text)) { text++; max--; }

    while (text - start >= min)
    {
        if ((end = matchpattern(p + 2, text--)))
            return end;
    }

    return 0;
}

/* Iterative matching */
static const char *matchpattern(const re_node *pattern, const char *text)
{
    do
    {
        if (pattern[0].type == UNUSED)
        {
            return text;
        }
        else if ((pattern[0].type == END) && pattern[1].type == UNUSED)
        {
            return (*text == '\0') ? text : 0;
        }
        else
        {
            switch (pattern[1].type)
            {
            case QMARK:
                return matchquant(pattern, text, 0, 1);
            case QMARK_LAZY:
                return matchquant_lazy(pattern, text, 0, 1);
            case QUANT:
                return matchquant(pattern, text, pattern[1].ccl[0], pattern[1].ccl[1]);
            case QUANT_LAZY:
                return matchquant_lazy(pattern, text, pattern[1].ccl[0], pattern[1].ccl[1]);
            case STAR:
                return matchquant(pattern, text, 0, MAX_COUNT);
            case STAR_LAZY:
                return matchquant_lazy(pattern, text, 0, MAX_COUNT);
            case PLUS:
                return matchquant(pattern, text, 1, MAX_COUNT);
            case PLUS_LAZY:
                return matchquant_lazy(pattern, text, 1, MAX_COUNT);
            default: break; // w/e
            }
        }
    }
    while (*text && matchone(pattern++, *text++));

    return 0;
}

#ifndef RE_DISABLE_PRINT

#define X(A) #A,
static const char *types[] = { X_RE_TYPES };
#undef X

#include <stdio.h>
void re_print(const re_comp *compiled)
{
    if (!compiled)
    {
        printf("NULL compiled regex detected\n");
        return;
    }

    const re_node *pattern = compiled->nodes;
    int i;
    for (i = 0; i < MAX_RENODES; ++i)
    {
        if (pattern[i].type == UNUSED)
        {
            break;
        }

        printf("type: %s", types[pattern[i].type]);
        if (pattern[i].type == CCLASS || pattern[i].type == INV_CCLASS)
        {
            printf(pattern[i].type == CCLASS ? " [" : " [^");
            int j;
            char c;
            for (j = 0; j < MAX_REBUFLEN; ++j)
            {
                c = pattern[i].ccl[j];
                if (c == '\0') break;
                printf(c == ']' ? "\\%c" : "%c", c);
            }
            printf("]");
        }
        else if (pattern[i].type == QUANT || pattern[i].type == QUANT_LAZY)
        {
            printf(" {%d,%d}", pattern[i].ccl[0], pattern[i].ccl[1]);
        }
        else if (pattern[i].type == CHAR)
        {
            printf(" '%c'", pattern[i].ch);
        }
        printf("\n");
    }
}
#endif

#undef X_RE_TYPES
