
#include "re.h"

#define TRE_MAXQUANT  1024  // Max b in {a,b}. must be < ushrt_max
#define TRE_MAXPLUS  40000  // For + and *,  > 32768 for test2

#define TRE_TYPES_X  X(NONE) X(BEGIN) X(END) \
        X(QUANT) X(LQUANT) X(QMARK) X(LQMARK) X(STAR) X(LSTAR) X(PLUS) X(LPLUS) \
        X(DOT) X(CHAR) X(CLASS) X(NCLASS) X(DIGIT) X(NDIGIT) X(ALPHA) X(NALPHA) X(SPACE) X(NSPACE)

#define X(A) TRE_##A,
enum { TRE_TYPES_X };
#undef X

#ifndef TRE_SILENT
#include "stdio.h"
#endif

static int tre_err(const char *msg)
{
#ifndef TRE_SILENT
    fprintf(stderr, "%s\n", msg);
#endif
    return 0;
}

TRE_DEF const char *tre_compile_match(const char *pattern, const char *text, const char **end)
{
    tre_comp tregex = {0};
    if (!tre_compile(pattern, &tregex))
    {
        tre_err("Compiling pattern failed");
        return 0;
    }

    return tre_match(&tregex, text, end);
}

static const char *matchpattern(const tre_node *nodes, const char *text);

TRE_DEF const char *tre_match(const tre_comp *tregex, const char *text, const char **end)
{
    if (!tregex || !text)
    {
        tre_err("NULL text or tre_comp");
        return 0;
    }

    const char *mend;
    const tre_node *nodes = tregex->nodes;

    if (nodes[0].type == TRE_BEGIN)
    {
        mend = matchpattern(nodes + 1, text);
        if (mend)
        {
            if (end) { *end = mend; }
            return text;
        }
        return 0;
    }

    do
    {
        mend = matchpattern(nodes, text);
        if (mend)
        {
            //if (!*text) //Fixme: ???
            //    return 0;
            if (end) { *end = mend; }
            return text;
        }
    }
    while (*text++);

    return 0;
}

TRE_DEF int tre_compile(const char *pattern, tre_comp *tregex)
{
    if (!tregex || !pattern || !*pattern)
        return tre_err("NULL/empty string or tre_comp");
    
    tre_node *tnode = tregex->nodes;
    unsigned char *buf = tregex->buffer;
    unsigned char ischar = 0; // is the last node quantifiable

    int bufidx = 0;

    unsigned long val; // for parsing numbers in {m,n}
    char c;       // current char in pattern
    int i = 0;    // index into pattern
    int j = 0;    // index into tnode

    while (pattern[i] != '\0' && (j + 1 < TRE_MAX_NODES))
    {
        c = pattern[i];

        switch (c)
        {
        // Meta-characters
        case '^': ischar = 0; tnode[j].type = TRE_BEGIN; break;
        case '$': ischar = 0; tnode[j].type = TRE_END;   break;
        case '.': ischar = 1; tnode[j].type = TRE_DOT;   break;
        case '*':
            if (ischar == 0) { return tre_err("Non-quantifiable before *"); }
            ischar = 0;
            tnode[j].type = (pattern[i + 1] == '?') ? (i++, TRE_LSTAR) : TRE_STAR; break;
        case '+':
            if (ischar == 0) { return tre_err("Non-quantifiable before +"); }
            ischar = 0;
            tnode[j].type = (pattern[i + 1] == '?') ? (i++, TRE_LPLUS) : TRE_PLUS; break;
        case '?':
            if (ischar == 0) { return tre_err("Non-quantifiable before ?"); }
            ischar = 0;
            tnode[j].type = (pattern[i + 1] == '?') ? (i++, TRE_LQMARK) : TRE_QMARK; break;

        // Escaped characters
        case '\\':
        {
            ischar = 1;
            i++;
            // dangling?
            if (pattern[i] == '\0') { return tre_err("Dangling \\"); }

            switch (pattern[i])
            {
            // Meta-character:
            case 'd': tnode[j].type = TRE_DIGIT;  break;
            case 'D': tnode[j].type = TRE_NDIGIT; break;
            case 'w': tnode[j].type = TRE_ALPHA;  break;
            case 'W': tnode[j].type = TRE_NALPHA; break;
            case 's': tnode[j].type = TRE_SPACE;  break;
            case 'S': tnode[j].type = TRE_NSPACE; break;

            // Not in [dDwWsS]
            default: tnode[j].type = TRE_CHAR; tnode[j].ch = pattern[i]; break;
            }
        } break;

        // Character class
        case '[':
        {
            ischar = 1;
            int buf_begin = bufidx;
            char nc = pattern[i + 1];

            // Look-ahead to determine if negated
            tnode[j].type = (nc == '^') ? (i++, TRE_NCLASS) : TRE_CLASS;

            // Copy characters inside [..] to buffer
            while (pattern[++i] != ']' && pattern[i] != '\0')
            {
                if (pattern[i] == '\\')
                {
                    nc = pattern[i + 1];
                    if (!nc) 
                        return tre_err("Dangling \\ in class");

                    // skip slash for non-meta characters and slash
                    if (nc == 's' || nc == 'S' || nc == 'w' || nc == 'W' ||
                            nc == 'd' || nc == 'D' || nc == '\\')
                    {
                        if (bufidx > TRE_MAX_BUFLEN - 3)
                            return tre_err("Buffer overflow at meta in class");
                        buf[bufidx++] = pattern[i++];
                    }
                    else
                    {
                        if (bufidx > TRE_MAX_BUFLEN - 2)
                            return tre_err("Buffer overflow at escaped-char in class");
                        i++;
                    }
                    buf[bufidx++] = pattern[i];
                }
                else if (pattern[i + 1] == '-' && pattern[i + 2] != '0')
                {
                    if (pattern[i] > pattern[i + 2])
                        return tre_err("Incorrect range in class");
                    if (bufidx > TRE_MAX_BUFLEN - 4)
                        return tre_err("Buffer overflow at range in class");
                    buf[bufidx++] = pattern[i++];
                    buf[bufidx++] = pattern[i++];
                    buf[bufidx++] = pattern[i];
                }
                else
                {
                    if (bufidx > TRE_MAX_BUFLEN - 2)
                        return tre_err("Buffer overflow at char in class");
                    buf[bufidx++] = pattern[i];
                }
            }

            if (pattern[i] != ']')
                return tre_err("Non terminated class");
            // Nul-terminated string
            buf[bufidx++] = 0;
            tnode[j].ccl = &buf[buf_begin];
        } break;

        // Quantifier
        case '{':
        {
            if (ischar == 0)
                return tre_err("Non-quantifiable before {m,n}");
            ischar = 0;

            // Use a char for each min and max (<= 255)
            //if (bufidx > TRE_MAX_BUFLEN - 2)
            //    return tre_err("Buffer overflow for quantifier");
            i++;
            val = 0;
            do
            {
                if (pattern[i] < '0' || pattern[i] > '9')
                    return tre_err("Non-digit in quantifier min value");
                val = 10 * val + (pattern[i++] - '0');
            }
            while (pattern[i] != ',' && pattern[i] != '}');

            if (val > TRE_MAXQUANT)
                return tre_err("Quantifier min value too big");
            tnode[j].mn[0] = val;

            if (pattern[i] == ',')
            {
                i++;
                if (pattern[i] == '}')
                {
                    val = TRE_MAXQUANT;
                }
                else
                {
                    val = 0;
                    while (pattern[i] != '}')
                    {
                        if (pattern[i] < '0' || pattern[i] > '9')
                            return tre_err("Non-digit in quantifier max value");
                        val = 10 * val + (pattern[i++] - '0');
                    }

                    if (val > TRE_MAXQUANT || val < tnode[j].mn[0])
                        return tre_err("Quantifier max value too big or less than min value");
                }
            }
            tnode[j].type = (pattern[i + 1] == '?') ? (i++, TRE_LQUANT) : TRE_QUANT;
            tnode[j].mn[1] = val;
        } break;

        // Regular characters
        default: ischar = 1; tnode[j].type = TRE_CHAR; tnode[j].ch = c; break;
        }
        i += 1;
        j += 1;
    }
    // 'TRE_NONE' is a sentinel used to indicate end-of-pattern
    tnode[j].type = TRE_NONE;

    return 1;
}

#define TRE_MATCHDIGIT(c) ((c >= '0') && (c <= '9'))
#define TRE_MATCHALPHA(c) ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'))
#define TRE_MATCHSPACE(c) ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') || (c == '\f') || (c == '\v'))
#define TRE_MATCHALNUM(c) ((c == '_') || TRE_MATCHALPHA(c) || TRE_MATCHDIGIT(c))

static int matchmetachar(char c, char mc)
{
    switch (mc)
    {
    case 'd': return  TRE_MATCHDIGIT(c);
    case 'D': return !TRE_MATCHDIGIT(c);
    case 'w': return  TRE_MATCHALNUM(c);
    case 'W': return !TRE_MATCHALNUM(c);
    case 's': return  TRE_MATCHSPACE(c);
    case 'S': return !TRE_MATCHSPACE(c);
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
            if (matchmetachar(c, str[1])) { return 1; }
            str += 2;
        }
        else if (str[1] == '-' && str[2] != '\0')
        {
            if (c >= str[0] && c <= str[2]) { return 1; }
            str += 3;
        }
        else
        {
            if (c == *str) { return 1; }
            str += 1;
        }
    }

    return 0;
}


#ifndef TRE_DOTANY
#define TRE_MATCHDOT(c)   ((c != '\n') && (c != '\r'))
#else
#define TRE_MATCHDOT(c)   (1)
#endif

static int matchone(const tre_node *tnode, char c)
{
    switch (tnode->type)
    {
    case TRE_DOT:    return  TRE_MATCHDOT(c);
    case TRE_CLASS:  return  matchcharclass(c, tnode->ccl);
    case TRE_NCLASS: return !matchcharclass(c, tnode->ccl);
    case TRE_DIGIT:  return  TRE_MATCHDIGIT(c);
    case TRE_NDIGIT: return !TRE_MATCHDIGIT(c);
    case TRE_ALPHA:  return  TRE_MATCHALNUM(c);
    case TRE_NALPHA: return !TRE_MATCHALNUM(c);
    case TRE_SPACE:  return  TRE_MATCHSPACE(c);
    case TRE_NSPACE: return !TRE_MATCHSPACE(c);
    default:         return (tnode->ch == c);
    }
}

#undef TRE_MATCHDIGIT
#undef TRE_MATCHALPHA
#undef TRE_MATCHSPACE
#undef TRE_MATCHALNUM
#undef TRE_MATCHDOT

static const char *matchquant_lazy(const tre_node *tnode, const char *text, unsigned min, unsigned max)
{
    const char *end;
    max = max - min + 1;
    while (min && *text && matchone(tnode, *text)) { text++; min--; }
    if (min) { return 0; }

    do
    {
        end = matchpattern(tnode + 2, text);
        if (end) { return end; }
        max--;
    }
    while (max && *text && matchone(tnode, *text++));

    return 0;
}

static const char *matchquant(const tre_node *tnode, const char *text, unsigned min, unsigned max)
{
    const char *end, *start = text;
    while (max && *text && matchone(tnode, *text)) { text++; max--; }

    while (text - start >= min)
    {
        end = matchpattern(tnode + 2, text--);
        if (end) { return end; }
    }

    return 0;
}

// Iterative matching
static const char *matchpattern(const tre_node *tnode, const char *text)
{
    do
    {
        if (tnode[0].type == TRE_NONE)
        {
            return text;
        }
        else if ((tnode[0].type == TRE_END) && tnode[1].type == TRE_NONE)
        {
            return (*text == '\0') ? text : 0;
        }

        switch (tnode[1].type)
        {
        case TRE_QMARK:
            return matchquant(tnode, text, 0, 1);
        case TRE_LQMARK:
            return matchquant_lazy(tnode, text, 0, 1);
        case TRE_QUANT:
            return matchquant(tnode, text, tnode[1].mn[0], tnode[1].mn[1]);
        case TRE_LQUANT:
            return matchquant_lazy(tnode, text, tnode[1].mn[0], tnode[1].mn[1]);
        case TRE_STAR:
            return matchquant(tnode, text, 0, TRE_MAXPLUS);
        case TRE_LSTAR:
            return matchquant_lazy(tnode, text, 0, TRE_MAXPLUS);
        case TRE_PLUS:
            return matchquant(tnode, text, 1, TRE_MAXPLUS);
        case TRE_LPLUS:
            return matchquant_lazy(tnode, text, 1, TRE_MAXPLUS);
        default: break; // w/e
        }
    }
    while (*text && matchone(tnode++, *text++));

    return 0;
}

#ifndef TRE_SILENT

void tre_print(const tre_comp *tregex)
{
#define X(A) #A,
    static const char *tre_typenames[] = { TRE_TYPES_X };
#undef X

    if (!tregex)
    {
        printf("NULL compiled regex detected\n");
        return;
    }

    const tre_node *tnode = tregex->nodes;
    int i;
    for (i = 0; i < TRE_MAX_NODES; ++i)
    {
        if (tnode[i].type == TRE_NONE)
            break;

        printf("type: %s", tre_typenames[tnode[i].type]);
        if (tnode[i].type == TRE_CLASS || tnode[i].type == TRE_NCLASS)
        {
            printf(tnode[i].type == TRE_CLASS ? " [" : " [^");
            int j;
            char c;
            for (j = 0; j < TRE_MAX_BUFLEN; ++j)
            {
                c = tnode[i].ccl[j];
                if (c == '\0') break;
                printf(c == ']' ? "\\%c" : "%c", c);
            }
            printf("]");
        }
        else if (tnode[i].type == TRE_QUANT || tnode[i].type == TRE_LQUANT)
        {
            printf(" {%d,%d}", tnode[i].mn[0], tnode[i].mn[1]);
        }
        else if (tnode[i].type == TRE_CHAR)
        {
            printf(" '%c'", tnode[i].ch);
        }
        printf("\n");
    }
}
#endif // TRE_DISABLE_PRINT

#undef TRE_TYPES_X

