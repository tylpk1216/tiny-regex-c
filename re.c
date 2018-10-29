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



#define X_RE_TYPES  X(NONE) X(DOT) X(BEGIN) X(END) X(QUANT) X(QUANT_LAZY) \
        X(QMARK) X(QMARK_LAZY) X(STAR) X(STAR_LAZY) X(PLUS) X(PLUS_LAZY) \
        X(CHAR) X(CCLASS) X(INV_CCLASS) X(DIGIT) X(NOT_DIGIT) X(ALPHA) X(NOT_ALPHA) \
        X(WSPACE) X(NOT_WSPACE) X(BRANCH)

#define X(A) A,
enum { X_RE_TYPES };
#undef X



/* Private function declarations: */
static const char *matchpattern(const re_node *pattern, const char *text);

/* Public functions: */
const char *re_match(const char *pattern, const char *text, const char **end)
{
    re_comp compiled = {0};
    int ret = re_compile(pattern, &compiled);
    if (!ret)
    {
        printf("compiling pattern '%s' failed\n", pattern);
        return 0;
    }
    return re_matchp(&compiled, text, end);
}

const char *re_matchp(const re_comp *compiled, const char *text, const char **end)
{
    if (!compiled || !text || !*text)
        return 0;

    const char *me;
    const re_node *node = compiled->nodes;

    if (node[0].type == BEGIN)
    {
        if ((me = matchpattern(&node[1], text)))
        {
            if (end) *end = me;
            return text;
        }
        return 0;
    }

    do
    {
        if ((me = matchpattern(node, text)))
        {
            if (!*text) //Fixme: ???
                return 0;
            if (end) *end = me;
            return text;
        }
    }
    while (*text++);

    return 0;
}

int re_compile(const char *pattern, re_comp *compiled)
{
    if (!compiled)
        return 0;
    re_node *nodes = compiled->nodes;
    unsigned char *buf = compiled->buffer;
    unsigned char ischar = 0;

    int bufidx = 0;

    char c;     /* current char in pattern   */
    int i = 0;  /* index into pattern        */
    int j = 0;  /* index into nodes    */

    while (pattern[i] != '\0' && (j + 1 < MAX_RENODES))
    {
        c = pattern[i];

        switch (c)
        {
        /* Meta-characters: */
        case '^': { ischar = 0; nodes[j].type = BEGIN; } break;
        case '$': { ischar = 0; nodes[j].type = END;   } break;
        case '.': { ischar = 1; nodes[j].type = DOT;   } break;
        case '*':
            if (ischar == 0) return 0;
            ischar = 0;
            nodes[j].type = (pattern[i + 1] == '?') ? (i++, STAR_LAZY) : STAR; break;
        case '+':
            if (ischar == 0) return 0;
            ischar = 0;
            nodes[j].type = (pattern[i + 1] == '?') ? (i++, PLUS_LAZY) : PLUS; break;
        case '?':
            if (ischar == 0) return 0;
            ischar = 0;
            nodes[j].type = (pattern[i + 1] == '?') ? (i++, QMARK_LAZY) : QMARK; break;
        /*    case '|': {    nodes[j].type = BRANCH;          } break; <-- not working properly */

        /* Escaped character-classes (\s \w ...): */
        case '\\':
        {
            ischar = 1;
            i++;
            if (pattern[i] == '\0')
            {
                // dangling '\'
                return 0;
            }

            switch (pattern[i])
            {
            /* Meta-character: */
            case 'd': { nodes[j].type = DIGIT;      } break;
            case 'D': { nodes[j].type = NOT_DIGIT;  } break;
            case 'w': { nodes[j].type = ALPHA;      } break;
            case 'W': { nodes[j].type = NOT_ALPHA;  } break;
            case 's': { nodes[j].type = WSPACE;     } break;
            case 'S': { nodes[j].type = NOT_WSPACE; } break;

            /* Escaped character, e.g. '.' or '$' */
            default:
                nodes[j].type = CHAR;
                nodes[j].ch = pattern[i];
                break;
            }
        } break;

        /* Character class: */
        case '[':
        {
            ischar = 1;
            /* Remember where the char-buffer starts. */
            int buf_begin = bufidx;
            char nc = pattern[i + 1];

            /* Look-ahead to determine if negated */
            nodes[j].type = (nc == '^') ? (i++, INV_CCLASS) : CCLASS;

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
                        if (bufidx >= MAX_REBUFLEN - 2)
                            return 0;
                        buf[bufidx++] = pattern[i];
                    }
                    i++;
                }
                else if (pattern[i + 1] == '-' && pattern[i + 2] != '0')
                {
                    if (pattern[i] > pattern[i + 2])
                        return 0;
                }
                if (bufidx >= MAX_REBUFLEN - 1)
                {
                    //fputs("exceeded internal buffer!\n", stderr);
                    return 0;
                }
                buf[bufidx++] = pattern[i];
            }
            if (pattern[i] != ']') // pattern[i] == '\0'
            {
                //printf("char class missing ] at %i\n", i);
                return 0;
            }
            /* Null-terminate string end */
            buf[bufidx++] = 0;
            nodes[j].ccl = &buf[buf_begin];
        } break;

        /* Quantifier: */
        case '{':
        {
            if (ischar == 0) return 0;
            ischar = 0;
            //nodes[j].type = QUANT;
            // consumes 2 chars (one char for each min and max <= 255)
            if (bufidx >= MAX_REBUFLEN - 1)
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
            buf[bufidx] = val;
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

                    if (val > MAX_QUANT || val < buf[bufidx])
                    {
                        return 0;
                    }
                }
            }
            nodes[j].type = (pattern[i + 1] == '?') ? (i++, QUANT_LAZY) : QUANT;
            buf[bufidx + 1] = val;
            nodes[j].ccl = &buf[bufidx];
            bufidx += 2;
        } break;

        /* Other characters: */
        default:
        {
            ischar = 1;
            nodes[j].type = CHAR;
            nodes[j].ch = c;
        } break;
        }
        i += 1;
        j += 1;
    }
    /* 'NONE' is a sentinel used to indicate end-of-pattern */
    nodes[j].type = NONE;

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
        if (pattern[0].type == NONE)
        {
            return text;
        }
        else if ((pattern[0].type == END) && pattern[1].type == NONE)
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
        if (pattern[i].type == NONE)
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
