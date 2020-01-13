INCLUDES= -I../../.. -I..
O=fnbounds-cache.o crypto-hash.o file-permissions.o

cache: $(O) 
	gcc -o cache $(O) -lmbedcrypto

fnbounds-cache.o: ../fnbounds-cache.c
	gcc -g -c $(INCLUDES) $<

file-permissions.o: ../file-permissions.c
	gcc -g -c $(INCLUDES) $<

crypto-hash.o: ../crypto-hash.c
	gcc -g -c $(INCLUDES) $<

clean:
	/bin/rm -rf cache $(O)

foo: ../file-permissions.h foo.c
	gcc -o foo $(INCLUDES) foo.c
