OUTPUT = $(LIBRARY)

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(AR) $(ARFLAGS) $(LIBRARY) $(OBJECTS)

.include <../common-targets.mk>
