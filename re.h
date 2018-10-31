// Public Domain Tiny Regular Expressions Library
// Forked from https://github.com/kokke/tiny-regex-c
//
// Supports:
// ---------
//   '^'        Start anchor, matches start of string
//   '$'        End anchor, matches end of string
// ---------
//   '*'        Asterisk, match zero or more (greedy, *? lazy)
//   '+'        Plus, match one or more (greedy, +? lazy)
//   '{m,n}'    Quantifier, match min. 'm' and max. 'n' (greedy, {m,n}? lazy)
//   '{m}'                  exactly 'm'
//   '{m,}'                 match min 'm' and max. MAX_QUANT
//   '?'        Question, match zero or one (greedy, ?? lazy)
// ---------
//   '.'        Dot, matches any character except newline (\r, \n)
//   '[abc]'    Character class, match if one of {'a', 'b', 'c'}
//   '[^abc]'   Inverted class, match if NOT one of {'a', 'b', 'c'}
//   '[a-zA-Z]' Character ranges, the character set of the ranges { a-z | A-Z }
//   '\s'       Whitespace, \t \f \r \n \v and spaces
//   '\S'       Non-whitespace
//   '\w'       Alphanumeric, [a-zA-Z0-9_]
//   '\W'       Non-alphanumeric
//   '\d'       Digits, [0-9]
//   '\D'       Non-digits
//   '\X'       Character itself where X matches [^sSwWdD] (e.g. '\\' is '\')
// ---------


#ifndef TRE_H_INCLUDE
#define TRE_H_INCLUDE

#ifndef TRE_STATIC
#define TRE_DEF extern
#else
#define TRE_DEF static
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define TRE_MAX_NODES    64  // Max number of regex nodes in expression.
#define TRE_MAX_BUFLEN  128  // Max length of character-class buffer in.

typedef struct tre_node tre_node;
typedef struct tre_comp tre_comp;

struct tre_node
{
    unsigned char  type;
    union
    {
        unsigned char  ch;  // character itself
        unsigned char *ccl; // for class buffer and quantifier min max
        unsigned short mn[2];
    };
};

struct tre_comp
{
    tre_node nodes[TRE_MAX_NODES];
    unsigned char buffer[TRE_MAX_BUFLEN];
};

// Compile regex string pattern as tre_comp struct tregex
TRE_DEF int tre_compile(const char *pattern, tre_comp *tregex);

// Match tregex in text and return the match start or null if there is no match
// If end is not null set it to the match end
TRE_DEF const char *tre_match(const tre_comp *tregex, const char *text, const char **end);

// Same but compiles pattern then matches
TRE_DEF const char *tre_compile_match(const char *pattern, const char *text, const char **end);

// Print the pattern
TRE_DEF void tre_print(const tre_comp *tregex);

#ifdef __cplusplus
}
#endif

#endif // TRE_H_INCLUDE
