MAKEFILE_NAME ?= Makefile
SUBDIRECTORIES_FILTER ?=

# list of subdirectories containing makefiles that should be built
SUBDIRECTORIES != find * -maxdepth 1 -name "$(MAKEFILE_NAME)" -not \( -path "Makefile*" -prune \) $(SUBDIRECTORIES_FILTER) | xargs -n 1 dirname

.PHONY: subdirectories $(SUBDIRECTORIES) # phony is used here since the subdirectory targets are 

subdirectories: $(SUBDIRECTORIES)

$(SUBDIRECTORIES):
	$(MAKE) -C $@ -f $(MAKEFILE_NAME) PROJECT_ROOT="$(PROJECT_ROOT)/.." CROSS="$(CROSS)" CC="$(CROSS)cc" PLATFORM="$(PLATFORM)" DEBUG="$(DEBUG)"
