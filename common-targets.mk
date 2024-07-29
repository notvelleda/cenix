.PHONY: clean

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

.S.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-rm $(OBJECTS) $(DEBUG_OBJECTS) $(OUTPUT)
