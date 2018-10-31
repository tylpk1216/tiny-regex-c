/*
    This program prints out a verbose explanation of a given regular expression.
*/

#include <stdio.h>
#include "re.h"

int main(int argc, char** argv)
{
  if (argc >= 2)
  {
    tre_comp tregex;
    int ret = tre_compile(argv[1], &tregex);
    if (ret)
    {
      printf("regexp: '%s'\n", argv[1]);
      tre_print(&tregex);
    }
    else
    {
      printf("error compiling %s!\n", argv[1]);
      return -2;
    }
    
    if (argc == 2)
    {
      return 0;
    }
    
    const char *end = argv[2];
    const char *m = tre_match(&tregex, argv[2], &end);
    if (m)
    {
      printf("match at %li and length %li\n", m - argv[2], end - m);
    }
    else
    {
      printf("nomatch\n");
      return 0;
    }
    return 0;
    
  }
  else
  {
    printf("\nUsage: %s <PATTERN> [TEXT]\n", argv[0]);
  }   
  return -2; 
}

