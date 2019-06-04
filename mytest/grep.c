#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "re.h"

struct Arg {
    bool printLine;
};

void removeEnter(char *line)
{
    line += strlen(line) - 1;
    while (*line == '\r' || *line == '\n') {
        *line = '\0';
        line--;
    }
}

void initArg(int argc, char *argv[], struct Arg *arg)
{
    arg->printLine = false;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            arg->printLine = true;
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        printf("Usage : grep.exe pattern file \n");
        printf(" e.g. grep \"[Hh]ello\\s+[Ww]orld\\s{1,3}\" input.txt \n");
        return -1;
    }

    char *pattern = argv[1];
    char *file = argv[2];

    struct Arg arg;
    initArg(argc, argv, &arg);

    tre_comp tregex;
    int ret = tre_compile(pattern, &tregex);
    if(ret == 0)
    {
        printf("pattern(%s) can't be compiled \n", pattern);
        return -1;
    }

    char buf[10240];
    FILE *f = fopen(file, "rb");
    if (!f) {
        printf("Can't open %s \n", file);
        return -1;
    }

    int line = 0;
    while (fgets(buf, sizeof(buf), f) != NULL) {
        removeEnter(buf);

        line++;

        if (strlen(buf) == 0) continue;

        const char *m = tre_match(&tregex, buf, NULL);
        if (m)
        {
            if (arg.printLine) {
                printf("%d:", line);
            }
            printf("%s\r\n", buf);
        }
    }

    fclose(f);
    return 0;
}
