.include "$(PROJECT_ROOT)/makefiles/out-of-source.mk"

# this is the set of common C compiler flags that are enforced for all compilations in this project, as these warnings are Very Important and shouldn't be forgotten
CFLAGS += -Wall -Wextra -Wsign-conversion

.PHONY: clean

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

.S.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-rm $(OBJECTS) $(DEBUG_OBJECTS) $(OUTPUT)
