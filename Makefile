
CFLAGS=-g -Wall -Werror -O0 -std=c11 -D_POSIX_C_SOURCE=2

all: tool libhuffman.a

tool: huffcode.o libhuffman.a
	$(CC) $(LDFLAGS) -o $@ huffcode.o libhuffman.a
	rm *.o && rm *.a

huffman.o: huffman.h

libhuffman.a: huffman.o
	$(AR) r $@ $<

clean:
	$(RM) -r *.o *~ core tool libhuffman.a
