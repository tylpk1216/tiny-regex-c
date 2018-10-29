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

#ifdef __cplusplus
extern "C"{
#endif

#define MAX_RENODES    64  // Max number of regex nodes in expression.
#define MAX_REBUFLEN  128  // Max length of character-class buffer in.

typedef struct re_node_t
{
  unsigned char  type;   /* CHAR, STAR, etc.                      */
  union
  {
    unsigned char  ch;   /*      the character itself             */
    unsigned char* ccl;  /*  OR  a pointer to characters in class */
  };
} re_node;

typedef struct re_comp_t
{
  re_node nodes[MAX_RENODES];
  unsigned char buffer[MAX_REBUFLEN];
} re_comp;

/* Compile regex string pattern to a regex_t-array. */
int re_compile(const char *pattern, re_comp *compiled);


/* Find matches of the compiled pattern inside text. */
int re_matchp(const re_comp *compiled, const char *text, const char **next);


/* Find matches of the txt pattern inside text (will compile automatically first). */
int re_match(const char *pattern, const char *text, const char **next);


void re_print(const re_comp *compiled);


#ifdef __cplusplus
}
#endif
