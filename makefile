SOURCES := $(wildcard sources/*.cpp)

OBJECTS := $(SOURCES:%.cpp=%.o)
DEPENDS := $(SOURCES:%.cpp=%.d)
native = no

CFLAGS += -Wall -Wextra -Wcast-qual -Wshadow -Werror -O3 -flto
CPPFLAGS += -MMD -I include
LDFLAGS += -lpthread -lm

# If no arch is specified, we select a prefetch+popcnt build by default.
# If you want to build on a different architecture, please specify it in the
# command with ARCH=something. If you want to disable all arch-specific
# optimizations, use ARCH=unknown.

ifeq ($(ARCH),)
	ARCH=x86-64-modern
endif

# Add .exe to the executable name if we are on Windows

ifeq ($(OS),Windows_NT)
	EXE = Alexandria.exe
else
	EXE = Alexandria-bot
endif

# Enable use of PREFETCH instruction

ifeq ($(ARCH),x86-64)
    CFLAGS += -DUSE_PREFETCH
    ifneq ($(native),yes)
        CFLAGS += -msse
    endif
endif

ifeq ($(ARCH),x86-64-modern)
    CFLAGS += -DUSE_PREFETCH -DUSE_POPCNT
    ifneq ($(native),yes)
        CFLAGS += -msse -msse3 -mpopcnt
    endif
endif

ifeq ($(ARCH),x86-64-bmi2)
    CFLAGS += -DUSE_PREFETCH -DUSE_POPCNT -DUSE_PEXT
    ifneq ($(native),yes)
        CFLAGS += -msse -msse3 -mpopcnt -msse4 -mbmi2
    endif
endif

# If native is specified, build will try to use all available CPU instructions

ifeq ($(native),yes)
    CFLAGS += -march=native
endif

all: $(EXE)

$(EXE): $(OBJECTS)
	+$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

-include $(DEPENDS)

clean:
	rm -f $(OBJECTS) $(DEPENDS)

fclean: clean
	rm -f $(EXE)

re:
	$(MAKE) fclean
	+$(MAKE) all CFLAGS="$(CFLAGS)" CPPFLAGS="$(CPPFLAGS)" LDFLAGS="$(LDFLAGS)"

.PHONY: all clean fclean re