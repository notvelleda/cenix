OUTPUT = $(BINARY) $(BINARY).gdb

all: $(BINARY)

$(BINARY): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) $(LIBRARIES) -o $(BINARY)

.include <$(PROJECT_ROOT)/makefiles/common-targets.mk>
