OUTPUT = lib$(LIBRARY).a

all: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	$(AR) $(ARFLAGS) $(OUTPUT) $(OBJECTS)

.include "$(PROJECT_ROOT)/makefiles/common-targets.mk"
