#include "huffman.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <netinet/in.h>

typedef struct huffman_node_tag
{
    unsigned char isLeaf;
    unsigned long count;
    struct huffman_node_tag *parent;

    union
    {
        struct
        {
            struct huffman_node_tag *zero, *one;
        };
        unsigned char symbol;
    };
} huffman_node;

typedef struct huffman_code_tag
{
    unsigned long numbits;
    unsigned char *bits;
} huffman_code;

static unsigned long
numbytes_from_numbits(unsigned long numbits)
{
    return numbits / 8 + (numbits % 8 ? 1 : 0);
}

/*
 * get_bit returns the ith bit in the bits array
 * in the 0th position of the return value.
 */
static unsigned char
get_bit(unsigned char* bits, unsigned long i)
{
    return (bits[i / 8] >> i % 8) & 1;
}

static void
reverse_bits(unsigned char* bits, unsigned long numbits)
{
    unsigned long numbytes = numbytes_from_numbits(numbits);
    unsigned char *tmp =
        (unsigned char*)malloc(numbytes);
    unsigned long curbit;
    long curbyte = 0;

    memset(tmp, 0, numbytes);

    for(curbit = 0; curbit < numbits; ++curbit)
    {
        unsigned int bitpos = curbit % 8;

        if(curbit > 0 && curbit % 8 == 0)
            ++curbyte;

        tmp[curbyte] |= (get_bit(bits, numbits - curbit - 1) << bitpos);
    }

    memcpy(bits, tmp, numbytes);
    free(tmp);
}

/*
 * new_code builds a huffman_code from a leaf in
 * a Huffman tree.
 */
static huffman_code*
new_code(const huffman_node* leaf)
{
    unsigned long numbits = 0;
    unsigned char* bits = NULL;
    huffman_code *p;

    while(leaf && leaf->parent)
    {
        huffman_node *parent = leaf->parent;
        unsigned char cur_bit = (unsigned char)(numbits % 8);
        unsigned long cur_byte = numbits / 8;

        if(cur_bit == 0)
        {
            size_t newSize = cur_byte + 1;
            bits = (unsigned char*)realloc(bits, newSize);
            bits[newSize - 1] = 0;
        }

        if(leaf == parent->one)
            bits[cur_byte] |= 1 << cur_bit;

        ++numbits;
        leaf = parent;
    }

    if(bits)
        reverse_bits(bits, numbits);

    p = (huffman_code*)malloc(sizeof(huffman_code));
    p->numbits = numbits;
    p->bits = bits;
    return p;
}

#define MAX_SYMBOLS 256
typedef huffman_node* SymbolFrequencies[MAX_SYMBOLS];
typedef huffman_code* SymbolEncoder[MAX_SYMBOLS];

static huffman_node*
new_leaf_node(unsigned char symbol)
{
    huffman_node *p = (huffman_node*)malloc(sizeof(huffman_node));
    p->isLeaf = 1;
    p->symbol = symbol;
    p->count = 0;
    p->parent = 0;
    return p;
}

static huffman_node*
new_nonleaf_node(unsigned long count, huffman_node *zero, huffman_node *one)
{
    huffman_node *p = (huffman_node*)malloc(sizeof(huffman_node));
    p->isLeaf = 0;
    p->count = count;
    p->zero = zero;
    p->one = one;
    p->parent = 0;

    return p;
}

static void
free_huffman_tree(huffman_node *subtree)
{
    if(subtree == NULL)
        return;

    if(!subtree->isLeaf)
    {
        free_huffman_tree(subtree->zero);
        free_huffman_tree(subtree->one);
    }

    free(subtree);
}

static void
free_code(huffman_code* p)
{
    free(p->bits);
    free(p);
}

static void
free_encoder(SymbolEncoder *pSE)
{
    unsigned long i;
    for(i = 0; i < MAX_SYMBOLS; ++i)
    {
        huffman_code *p = (*pSE)[i];
        if(p)
            free_code(p);
    }

    free(pSE);
}

static void
init_frequencies(SymbolFrequencies pSF)
{
    memset(pSF, 0, sizeof(SymbolFrequencies));
}

typedef struct buf_cache_tag
{
    unsigned char *cache;
    unsigned int cache_len;
    unsigned int cache_cur;
    unsigned char **pbufout;
    unsigned int *pbufoutlen;
} buf_cache;

static int init_cache(buf_cache* pc,
                      unsigned int cache_size,
                      unsigned char **pbufout,
                      unsigned int *pbufoutlen)
{
    assert(pc && pbufout && pbufoutlen);
    if(!pbufout || !pbufoutlen)
        return 1;

    pc->cache = (unsigned char*)malloc(cache_size);
    pc->cache_len = cache_size;
    pc->cache_cur = 0;
    pc->pbufout = pbufout;
    *pbufout = NULL;
    pc->pbufoutlen = pbufoutlen;
    *pbufoutlen = 0;

    return pc->cache ? 0 : 1;
}

static void free_cache(buf_cache* pc)
{
    assert(pc);
    if(pc->cache)
    {
        free(pc->cache);
        pc->cache = NULL;
    }
}

static int flush_cache(buf_cache* pc)
{
    assert(pc);

    if(pc->cache_cur > 0)
    {
        unsigned int newlen = pc->cache_cur + *pc->pbufoutlen;
        unsigned char* tmp = realloc(*pc->pbufout, newlen);
        if(!tmp)
            return 1;

        memcpy(tmp + *pc->pbufoutlen, pc->cache, pc->cache_cur);

        *pc->pbufout = tmp;
        *pc->pbufoutlen = newlen;
        pc->cache_cur = 0;
    }

    return 0;
}

static int write_cache(buf_cache* pc,
                       const void *to_write,
                       unsigned int to_write_len)
{
    unsigned char* tmp;

    assert(pc && to_write);
    assert(pc->cache_len >= pc->cache_cur);

    if(to_write_len > pc->cache_len - pc->cache_cur)
    {
        unsigned int newlen;
        flush_cache(pc);
        newlen = *pc->pbufoutlen + to_write_len;
        tmp = realloc(*pc->pbufout, newlen);
        if(!tmp)
            return 1;
        memcpy(tmp + *pc->pbufoutlen, to_write, to_write_len);
        *pc->pbufout = tmp;
        *pc->pbufoutlen = newlen;
    }
    else
    {
        memcpy(pc->cache + pc->cache_cur, to_write, to_write_len);
        pc->cache_cur += to_write_len;
    }

    return 0;
}

static unsigned int
get_symbol_frequencies(SymbolFrequencies pSF, FILE *in)
{
    int c;
    unsigned int total_count = 0;

    init_frequencies(pSF);

    while((c = fgetc(in)) != EOF)
    {
        unsigned char uc = c;
        if(!pSF[uc])
            pSF[uc] = new_leaf_node(uc);
        ++(pSF[uc]->count);
        ++total_count;
    }

    return total_count;
}

static unsigned int
get_symbol_frequencies_from_memory(SymbolFrequencies pSF,
                                   const unsigned char *bufin,
                                   unsigned int bufinlen)
{
    unsigned int i;
    unsigned int total_count = 0;

    init_frequencies(pSF);

    for(i = 0; i < bufinlen; ++i)
    {
        unsigned char uc = bufin[i];
        if(!pSF[uc])
            pSF[uc] = new_leaf_node(uc);
        ++(pSF[uc]->count);
        ++total_count;
    }

    return total_count;
}

static int
SFComp(const void *p1, const void *p2)
{
    const huffman_node *hn1 = *(const huffman_node**)p1;
    const huffman_node *hn2 = *(const huffman_node**)p2;

    /* Sort all NULLs to the end. */
    if(hn1 == NULL && hn2 == NULL)
        return 0;
    if(hn1 == NULL)
        return 1;
    if(hn2 == NULL)
        return -1;

    if(hn1->count > hn2->count)
        return 1;
    else if(hn1->count < hn2->count)
        return -1;

    return 0;
}

/*
 * build_symbol_encoder builds a SymbolEncoder by walking
 */
static void
build_symbol_encoder(huffman_node *subtree, SymbolEncoder *pSF)
{
    if(subtree == NULL)
        return;

    if(subtree->isLeaf)
        (*pSF)[subtree->symbol] = new_code(subtree);
    else
    {
        build_symbol_encoder(subtree->zero, pSF);
        build_symbol_encoder(subtree->one, pSF);
    }
}

/*
 * calculate_huffman_codes turns pSF
 */
static SymbolEncoder*
calculate_huffman_codes(SymbolFrequencies pSF)
{
    unsigned int i = 0;
    unsigned int n = 0;
    huffman_node *m1 = NULL, *m2 = NULL;
    SymbolEncoder *pSE = NULL;

    /* Sort the symbol frequency array by ascending frequency. */
    qsort(pSF, MAX_SYMBOLS, sizeof(pSF[0]), SFComp);

    /* Get the number of symbols. */
    for(n = 0; n < MAX_SYMBOLS && pSF[n]; ++n)
        ;

    for(i = 1; i < n; ++i)
    {
        /* Set m1 and m2 to the two subsets of least probability. */
        m1 = pSF[0];
        m2 = pSF[1];

        /* Replace m1 and m2 with a set {m1, m2} whose probability
         * is the sum of that of m1 and m2. */
        pSF[0] = m1->parent = m2->parent =
                                  new_nonleaf_node(m1->count + m2->count, m1, m2);
        pSF[1] = NULL;

        /* Put newSet into the correct count position in pSF. */
        qsort(pSF, n, sizeof(pSF[0]), SFComp);
    }

    /* Build the SymbolEncoder array from the tree. */
    pSE = (SymbolEncoder*)malloc(sizeof(SymbolEncoder));
    memset(pSE, 0, sizeof(SymbolEncoder));
    build_symbol_encoder(pSF[0], pSE);
    return pSE;
}

/*
 * Write the huffman code table.
 */
static int
write_code_table(FILE* out, SymbolEncoder *se, uint32_t symbol_count)
{
    uint32_t i, count = 0;

    /* Determine the number of entries in se. */
    for(i = 0; i < MAX_SYMBOLS; ++i)
    {
        if((*se)[i])
            ++count;
    }

    /* Write the number of entries in network byte order. */
    i = htonl(count);
    if(fwrite(&i, sizeof(i), 1, out) != 1)
        return 1;

    /* Write the number of bytes that will be encoded. */
    symbol_count = htonl(symbol_count);
    if(fwrite(&symbol_count, sizeof(symbol_count), 1, out) != 1)
        return 1;

    /* Write the entries. */
    for(i = 0; i < MAX_SYMBOLS; ++i)
    {
        huffman_code *p = (*se)[i];
        if(p)
        {
            unsigned int numbytes;
            fputc((unsigned char)i, out);
            fputc(p->numbits, out);
            numbytes = numbytes_from_numbits(p->numbits);
            if(fwrite(p->bits, 1, numbytes, out) != numbytes)
                return 1;
        }
    }

    return 0;
}

/*
 * Allocates memory and sets *pbufout to point to it. The memory
 * contains the code table.
 */
static int
write_code_table_to_memory(buf_cache *pc,
                           SymbolEncoder *se,
                           uint32_t symbol_count)
{
    uint32_t i, count = 0;

    /* Determine the number of entries in se. */
    for(i = 0; i < MAX_SYMBOLS; ++i)
    {
        if((*se)[i])
            ++count;
    }

    /* Write the number of entries in network byte order. */
    i = htonl(count);

    if(write_cache(pc, &i, sizeof(i)))
        return 1;

    /* Write the number of bytes that will be encoded. */
    symbol_count = htonl(symbol_count);
    if(write_cache(pc, &symbol_count, sizeof(symbol_count)))
        return 1;

    /* Write the entries. */
    for(i = 0; i < MAX_SYMBOLS; ++i)
    {
        huffman_code *p = (*se)[i];
        if(p)
        {
            unsigned int numbytes;
            /* The value of i is < MAX_SYMBOLS (256), so it can
            be stored in an unsigned char. */
            unsigned char uc = (unsigned char)i;
            /* Write the 1 byte symbol. */
            if(write_cache(pc, &uc, sizeof(uc)))
                return 1;
            /* Write the 1 byte code bit length. */
            uc = (unsigned char)p->numbits;
            if(write_cache(pc, &uc, sizeof(uc)))
                return 1;
            /* Write the code bytes. */
            numbytes = numbytes_from_numbits(p->numbits);
            if(write_cache(pc, p->bits, numbytes))
                return 1;
        }
    }

    return 0;
}

/*
 * read_code_table builds a Huffman tree
 */
static bool
read_code_table(FILE* in, huffman_node** rootOut, unsigned int *dataBytesOut)
{
    huffman_node *root = NULL;
    uint32_t count = 0;

    if(fread(&count, sizeof(count), 1, in) != 1)
    {
        return false;
    }

    count = ntohl(count);

    if (count > MAX_SYMBOLS)
    {
        return false;
    }

    unsigned int dataBytes = 0;
    if(fread(&dataBytes, sizeof(dataBytes), 1, in) != 1)
    {
        return false;
    }

    dataBytes = ntohl(dataBytes);

    if (count == 0 && dataBytes > 0)
    {
        return false;
    }

    /* Read the entries. */
    while(count-- > 0)
    {
        int c = 0;

        if((c = fgetc(in)) == EOF)
        {
            free_huffman_tree(root);
            return false;
        }
        unsigned char symbol = (unsigned char)c;

        if((c = fgetc(in)) == EOF)
        {
            free_huffman_tree(root);
            return false;
        }
        unsigned char numbits = (unsigned char)c;

        if (root == NULL)
        {
            if (numbits == 0)
            {
                if (count != 0) {
                    // Invalid code table. Abort processing.
                    return false;
                }

                *rootOut = new_leaf_node(symbol);
                *dataBytesOut = dataBytes;
                return true;
            }

            root = new_nonleaf_node(0, NULL, NULL);
        }

        if (numbits == 0) {
            // Valid code tables only have 0 bit length codes if they encode only 1 symbol.
            free_huffman_tree(root);
            return false;

        }

        assert(root != NULL);
        huffman_node *p = root;
        unsigned char numbytes = (unsigned char)numbytes_from_numbits(numbits);
        unsigned char *bytes = (unsigned char*)malloc(numbytes);
        if(fread(bytes, 1, numbytes, in) != numbytes)
        {
            free(bytes);
            free_huffman_tree(root);
            return false;
        }

        for(unsigned int curbit = 0; curbit < numbits; ++curbit)
        {
            if (p->isLeaf)
            {
                // Invalid input.
                free(bytes);
                free_huffman_tree(root);
                return false;
            }

            if(get_bit(bytes, curbit))
            {
                assert(p != NULL);
                if(p->one == NULL)
                {
                    p->one = curbit == (unsigned char)(numbits - 1)
                             ? new_leaf_node(symbol)
                             : new_nonleaf_node(0, NULL, NULL);
                    assert(p->one != NULL);
                    p->one->parent = p;
                }
                assert(p->one != NULL);
                p = p->one;
            }
            else
            {
                assert(p != NULL);
                if(p->zero == NULL)
                {
                    p->zero = curbit == (unsigned char)(numbits - 1)
                              ? new_leaf_node(symbol)
                              : new_nonleaf_node(0, NULL, NULL);
                    assert(p->zero != NULL);
                    p->zero->parent = p;
                }
                assert(p->zero != NULL);
                p = p->zero;
            }
        }

        free(bytes);
    }

    *rootOut = root;
    *dataBytesOut = dataBytes;
    return true;
}

static int
memread(const unsigned char* buf,
        unsigned int buflen,
        unsigned int *pindex,
        void* bufout,
        unsigned int readlen)
{
    assert(buf && pindex && bufout);
    assert(buflen >= *pindex);
    if(buflen < *pindex)
        return 1;
    if(readlen + *pindex >= buflen)
        return 1;
    memcpy(bufout, buf + *pindex, readlen);
    *pindex += readlen;
    return 0;
}

static huffman_node*
read_code_table_from_memory(const unsigned char* bufin,
                            unsigned int bufinlen,
                            unsigned int *pindex,
                            uint32_t *pDataBytes)
{
    huffman_node *root = new_nonleaf_node(0, NULL, NULL);
    uint32_t count;

    /* Read the number of entries.
       (it is stored in network byte order). */
    if(memread(bufin, bufinlen, pindex, &count, sizeof(count)))
    {
        free_huffman_tree(root);
        return NULL;
    }

    count = ntohl(count);

    /* Read the number of data bytes this encoding represents. */
    if(memread(bufin, bufinlen, pindex, pDataBytes, sizeof(*pDataBytes)))
    {
        free_huffman_tree(root);
        return NULL;
    }

    *pDataBytes = ntohl(*pDataBytes);

    /* Read the entries. */
    while(count-- > 0)
    {
        unsigned int curbit;
        unsigned char symbol;
        unsigned char numbits;
        unsigned char numbytes;
        unsigned char *bytes;
        huffman_node *p = root;

        if(memread(bufin, bufinlen, pindex, &symbol, sizeof(symbol)))
        {
            free_huffman_tree(root);
            return NULL;
        }

        if(memread(bufin, bufinlen, pindex, &numbits, sizeof(numbits)))
        {
            free_huffman_tree(root);
            return NULL;
        }

        numbytes = (unsigned char)numbytes_from_numbits(numbits);
        bytes = (unsigned char*)malloc(numbytes);
        if(memread(bufin, bufinlen, pindex, bytes, numbytes))
        {
            free(bytes);
            free_huffman_tree(root);
            return NULL;
        }

        for(curbit = 0; curbit < numbits; ++curbit)
        {
            if(get_bit(bytes, curbit))
            {
                if(p->one == NULL)
                {
                    p->one = curbit == (unsigned char)(numbits - 1)
                             ? new_leaf_node(symbol)
                             : new_nonleaf_node(0, NULL, NULL);
                    p->one->parent = p;
                }
                p = p->one;
            }
            else
            {
                if(p->zero == NULL)
                {
                    p->zero = curbit == (unsigned char)(numbits - 1)
                              ? new_leaf_node(symbol)
                              : new_nonleaf_node(0, NULL, NULL);
                    p->zero->parent = p;
                }
                p = p->zero;
            }
        }

        free(bytes);
    }

    return root;
}

static int
do_file_encode(FILE* in, FILE* out, SymbolEncoder *se)
{
    unsigned char curbyte = 0;
    unsigned char curbit = 0;
    int c;

    while((c = fgetc(in)) != EOF)
    {
        unsigned char uc = (unsigned char)c;
        huffman_code *code = (*se)[uc];
        unsigned long i;

        for(i = 0; i < code->numbits; ++i)
        {
            /* Add the current bit to curbyte. */
            curbyte |= get_bit(code->bits, i) << curbit;

            /* If this byte is filled up then write it
             * out and reset the curbit and curbyte. */
            if(++curbit == 8)
            {
                fputc(curbyte, out);
                curbyte = 0;
                curbit = 0;
            }
        }
    }

    if(curbit > 0)
        fputc(curbyte, out);

    return 0;
}

static int
do_memory_encode(buf_cache *pc,
                 const unsigned char* bufin,
                 unsigned int bufinlen,
                 SymbolEncoder *se)
{
    unsigned char curbyte = 0;
    unsigned char curbit = 0;
    unsigned int i;

    for(i = 0; i < bufinlen; ++i)
    {
        unsigned char uc = bufin[i];
        huffman_code *code = (*se)[uc];
        unsigned long i;

        for(i = 0; i < code->numbits; ++i)
        {
            /* Add the current bit to curbyte. */
            curbyte |= get_bit(code->bits, i) << curbit;

            /* If this byte is filled up then write it
             * out and reset the curbit and curbyte. */
            if(++curbit == 8)
            {
                if(write_cache(pc, &curbyte, sizeof(curbyte)))
                    return 1;
                curbyte = 0;
                curbit = 0;
            }
        }
    }

    /*
     * If there is data in curbyte that has not been
     * output yet, which means that the last encoded
     * character did not fall on a byte boundary,
     * then output it.
     */
    return curbit > 0 ? write_cache(pc, &curbyte, sizeof(curbyte)) : 0;
}

/*
 * huffman_encode_file huffman encodes in to out.
 */
int
huffman_encode_file(FILE *in, FILE *out)
{
    SymbolFrequencies sf;
    SymbolEncoder *se;
    huffman_node *root = NULL;
    int rc;
    unsigned int symbol_count;

    /* Get the frequency of each symbol in the input file. */
    symbol_count = get_symbol_frequencies(sf, in);

    /* Build an optimal table from the symbolCount. */
    se = calculate_huffman_codes(sf);
    root = sf[0];

    rewind(in);
    rc = write_code_table(out, se, symbol_count);
    if(rc == 0)
        rc = do_file_encode(in, out, se);

    /* Free the Huffman tree. */
    free_huffman_tree(root);
    free_encoder(se);
    return rc;
}

int
huffman_decode_file(FILE *in, FILE *out)
{
    huffman_node *root = NULL, *p = NULL;
    int c = 0;
    unsigned int data_count = 0;

    /* Read the Huffman code table. */
    if (!read_code_table(in, &root, &data_count))
    {
        return 1;
    }

    if (root == NULL && data_count == 0) {
        return 0;
    }

    if (root->isLeaf) {
        while(data_count-- > 0) {
            fputc(root->symbol, out);
        }
        free_huffman_tree(root);
        return 0;
    }

    // This is a multi-symbol, non-empty file.
    p = root;
    while(data_count > 0 && (c = fgetc(in)) != EOF)
    {
        unsigned char byte = (unsigned char)c;
        unsigned char mask = 1;
        while(data_count > 0 && mask)
        {
            p = byte & mask ? p->one : p->zero;
            if (p == NULL)
            {
                // Invalid file.
                free_huffman_tree(root);
                return 1;
            }

            mask <<= 1;

            if(p->isLeaf)
            {
                fputc(p->symbol, out);
                p = root;
                --data_count;
            }
        }
    }

    free_huffman_tree(root);
    return 0;
}

#define CACHE_SIZE 1024

int huffman_encode_memory(const unsigned char *bufin,
                          unsigned int bufinlen,
                          unsigned char **pbufout,
                          unsigned int *pbufoutlen)
{
    SymbolFrequencies sf;
    SymbolEncoder *se;
    huffman_node *root = NULL;
    int rc;
    unsigned int symbol_count;
    buf_cache cache;

    /* Ensure the arguments are valid. */
    if(!pbufout || !pbufoutlen)
        return 1;

    if(init_cache(&cache, CACHE_SIZE, pbufout, pbufoutlen))
        return 1;

    symbol_count = get_symbol_frequencies_from_memory(sf, bufin, bufinlen);

    se = calculate_huffman_codes(sf);
    root = sf[0];

    rc = write_code_table_to_memory(&cache, se, symbol_count);
    if(rc == 0)
        rc = do_memory_encode(&cache, bufin, bufinlen, se);

    /* Flush the cache. */
    flush_cache(&cache);

    /* Free the Huffman tree. */
    free_huffman_tree(root);
    free_encoder(se);
    free_cache(&cache);
    return rc;
}

int huffman_decode_memory(const unsigned char *bufin,
                          unsigned int bufinlen,
                          unsigned char **pbufout,
                          unsigned int *pbufoutlen)
{
    huffman_node *root, *p;
    unsigned int data_count;
    unsigned int i = 0;
    unsigned char *buf;
    unsigned int bufcur = 0;

    /* Ensure the arguments are valid. */
    if(!pbufout || !pbufoutlen)
        return 1;

    /* Read the Huffman code table. */
    root = read_code_table_from_memory(bufin, bufinlen, &i, &data_count);
    if(!root)
        return 1;

    buf = (unsigned char*)malloc(data_count);

    /* Decode the memory. */
    p = root;
    for(; i < bufinlen && data_count > 0; ++i)
    {
        unsigned char byte = bufin[i];
        unsigned char mask = 1;
        while(data_count > 0 && mask)
        {
            p = byte & mask ? p->one : p->zero;
            mask <<= 1;

            if(p->isLeaf)
            {
                buf[bufcur++] = p->symbol;
                p = root;
                --data_count;
            }
        }
    }

    free_huffman_tree(root);
    *pbufout = buf;
    *pbufoutlen = bufcur;
    return 0;
}
