#include "huffman.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

static void
usage(FILE* out)
{
    fputs("Usage: huffcode [-i<input file>] [-o<output file>] [-d|-c]\n"
          "-i - input file (default is standard input)\n"
          "-o - output file (default is standard output)\n"
          "-d - unescape\n"
          "-c - escape (default)\n",
          out);
}

int
main(int argc, char** argv)
{
    char compress = 1;
    int opt;
    const char *file_in = NULL, *file_out = NULL;
    FILE *in = stdin;
    FILE *out = stdout;
    int close_in = 0;
    int close_out = 0;
    int rc = 0;

    /* Get the command line arguments. */
    while((opt = getopt(argc, argv, "i:o:cdhvm")) != -1)
    {
        switch(opt)
        {
        case 'i':
            file_in = optarg;
            break;
        case 'o':
            file_out = optarg;
            break;
        case 'c':
            compress = 1;
            break;
        case 'd':
            compress = 0;
            break;
        case 'h':
            usage(stdout);
            return 0;
        default:
            usage(stderr);
            return 1;
        }
    }

    /* If an input file is given then open it. */
    if(file_in)
    {
        in = fopen(file_in, "rb");
        if(!in)
        {
            fprintf(stderr,
                    "Can't open input file '%s': %s\n",
                    file_in, strerror(errno));
            return 1;
        }
        close_in = 1;
    }

    /* If an output file is given then create it. */
    if(file_out)
    {
        out = fopen(file_out, "wb");
        if(!out)
        {
            fprintf(stderr,
                    "Can't open output file '%s': %s\n",
                    file_out, strerror(errno));
            return 1;
        }
        close_out = 1;
    }

    if (compress)
    {
        rc = huffman_encode_file(in, out);
    }
    else
    {
        rc = huffman_decode_file(in, out);
    }

    if (close_in)
    {
        fclose(in);
    }

    if (close_out)
    {
        fclose(out);
    }

    return rc;
}


