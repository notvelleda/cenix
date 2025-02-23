OUTPUT = $(LIBRARY)

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(AR) $(ARFLAGS) $(LIBRARY) $(OBJECTS)

.include "$(PROJECT_ROOT)/makefiles/common-targets.mk"
