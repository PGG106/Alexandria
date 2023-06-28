
NETWORK_NAME = nn.net
_THIS     := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
_ROOT     := $(_THIS)
EVALFILE   = $(_ROOT)/$(NETWORK_NAME)
CXX       := g++
TARGET    := Alexandria
CXXFLAGS  :=  -funroll-loops -O3 -flto -Wall -Wcast-qual -fno-exceptions -std=gnu++2a -pedantic -Wextra -Wshadow -Wdouble-promotion -Wformat=2 -Wnull-dereference \
-Wlogical-op -Wunused -Wold-style-cast -Wundef -DNDEBUG
NATIVE     = -march=native


# engine name
NAME      := Alexandria

TMPDIR = .tmp

# Detect Windows
ifeq ($(OS), Windows_NT)
	MKDIR    := mkdir
else
ifeq ($(COMP), MINGW)
	MKDIR    := mkdir
else
	MKDIR   := mkdir -p
endif
endif


# Detect Windows
ifeq ($(OS), Windows_NT)
	uname_S := Windows
	SUFFIX  := .exe
	FLAGS    = -pthread -lstdc++ -static
	CXXFLAGS += -static -static-libgcc -static-libstdc++ -Wl,--whole-archive -lpthread -Wl,--no-whole-archive
else
	FLAGS   = -lpthread -lstdc++
	SUFFIX  :=
	uname_S := $(shell uname -s)
endif

# Different native flag for macOS
ifeq ($(uname_S), Darwin)
	NATIVE = -mcpu=apple-a14	
	FLAGS = 
endif

# Remove native for builds
ifdef build
	NATIVE =
endif

# SPECIFIC BUILDS
ifeq ($(build), native)
	NATIVE   = -march=native
	ARCH     = -x86-64-native
endif

ifeq ($(build), x86-64)
	NATIVE       = -mtune=znver1
	INSTRUCTIONS = -msse -msse2 -mpopcnt
	ARCH         = -x86-64
endif

ifeq ($(build), x86-64-modern)
	NATIVE       = -mtune=znver2
	INSTRUCTIONS = -m64 -msse -msse3 -mpopcnt
	ARCH         = -x86-64-modern
endif

ifeq ($(build), x86-64-avx2)
	NATIVE       = -mtune=znver3
	INSTRUCTIONS = -m64 -msse -msse3 -mpopcnt -mavx -mavx2 -mssse3 -msse2
	ARCH         = -x86-64-avx2
endif

ifeq ($(build), x86-64-bmi2)
	NATIVE       = -mtune=znver3
	INSTRUCTIONS = -m64 -msse -msse3 -mpopcnt -mavx -mavx2 -msse4.1 -mssse3 -msse2 -mbmi -mbmi2
	ARCH         = -x86-64-bmi2
endif

ifeq ($(build), debug)
	CXXFLAGS = -g3 -fno-omit-frame-pointer -std=gnu++2a
	NATIVE   = -msse -msse3 -mpopcnt
	FLAGS    = -lpthread -lstdc++
endif

# Add network name and Evalfile
CXXFLAGS += -DNETWORK_NAME=\"$(NETWORK_NAME)\" -DEVALFILE=\"$(EVALFILE)\"

SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(patsubst %.cpp,$(TMPDIR)/%.o,$(SOURCES))
DEPENDS := $(patsubst %.cpp,$(TMPDIR)/%.d,$(SOURCES))
EXE     := $(NAME)$(SUFFIX)

all: $(TARGET)
clean:
	@rm -rf $(TMPDIR) *.o  $(DEPENDS) *.d

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(NATIVE) -MMD -MP -o $(EXE) $^ $(FLAGS)

$(TMPDIR)/%.o: %.cpp | $(TMPDIR)
	$(CXX) $(CXXFLAGS) $(NATIVE) -MMD -MP -c $< -o $@ $(FLAGS)

$(TMPDIR):
	$(MKDIR) "$(TMPDIR)" "$(TMPDIR)/src"

-include $(DEPENDS)
