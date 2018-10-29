/*
    This program prints out a verbose explanation of a given regular expression.
*/

#include <stdio.h>
#include "re.h"

int main(int argc, char** argv)
{
  if (argc >= 2)
  {
    re_comp regex;
    int ret = re_compile(argv[1], &regex);
    if (ret)
    {
      printf("regexp: '%s'\n", argv[1]);
      re_print(&regex);
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
    const char *m = re_matchp(&regex, argv[2], &end);
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

