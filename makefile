
NETWORK_NAME = nn.net
_THIS       := $(realpath $(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
_ROOT       := $(_THIS)
EVALFILE     = $(NETWORK_NAME)
CXX         := g++
TARGET      := Alexandria
WARNINGS     = -Wall -Wcast-qual -Wextra -Wshadow -Wdouble-promotion -Wformat=2 -Wnull-dereference -Wlogical-op -Wold-style-cast -Wundef -pedantic
CXXFLAGS    :=  -funroll-loops -O3 -flto -fno-exceptions -std=gnu++2a -DNDEBUG $(WARNINGS)
NATIVE       = -march=native


# engine name
NAME        := Alexandria

TMPDIR = .tmp

# Detect Clang
ifeq ($(CXX), clang++)
CXXFLAGS = -funroll-loops -O3 -flto -fuse-ld=lld -fno-exceptions -std=gnu++2a -DNDEBUG
endif

# Detect Windows
ifeq ($(OS), Windows_NT)
	MKDIR   := mkdir
else
ifeq ($(COMP), MINGW)
	MKDIR   := mkdir
else
	MKDIR   := mkdir -p
endif
endif


# Detect Windows
ifeq ($(OS), Windows_NT)
	uname_S  := Windows
	SUFFIX   := .exe
	CXXFLAGS += -static
else
	FLAGS    = -pthread
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
else
	PROPERTIES = $(shell echo | $(CXX) -march=native -E -dM -)
	ifneq ($(findstring __AVX512F__, $(PROPERTIES)),)
		ifneq ($(findstring __AVX512BW__, $(PROPERTIES)),)
			CXXFLAGS += -DUSE_AVX512 -mavx512f -mavx512bw
		endif
	else ifneq ($(findstring __AVX2__, $(PROPERTIES)),)
		CXXFLAGS += -DUSE_AVX2 -mavx2 -mbmi
	endif
endif

# SPECIFIC BUILDS
ifeq ($(build), native)
	NATIVE     = -march=native
	ARCH       = -x86-64-native
	PROPERTIES = $(shell echo | $(CXX) -march=native -E -dM -)
	ifneq ($(findstring __AVX512F__, $(PROPERTIES)),)
		ifneq ($(findstring __AVX512BW__, $(PROPERTIES)),)
			CXXFLAGS += -DUSE_AVX512 -mavx512f -mavx512bw
		endif
	else ifneq ($(findstring __AVX2__, $(PROPERTIES)),)
		CXXFLAGS += -DUSE_AVX2 -mavx2 -mbmi
	endif	
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
	NATIVE    = -march=bdver4 -mno-tbm -mno-sse4a -mno-bmi2
	ARCH      = -x86-64-avx2
	CXXFLAGS += -DUSE_AVX2 -mavx2 -mbmi
endif

ifeq ($(build), x86-64-bmi2)
	NATIVE    = -march=haswell
	ARCH      = -x86-64-bmi2
	CXXFLAGS += -DUSE_AVX2 -mavx2 -mbmi
endif

ifeq ($(build), x86-64-avx512)
	NATIVE    = -march=x86-64-v4 -mtune=znver4
	ARCH      = -x86-64-avx512
	CXXFLAGS += -DUSE_AVX512 -mavx512f -mavx512bw
endif

ifeq ($(build), debug)
	CXXFLAGS = -O3 -g3 -fno-omit-frame-pointer -std=gnu++2a
	NATIVE   = -msse -msse3 -mpopcnt
	FLAGS    = -lpthread -lstdc++

	PROPERTIES = $(shell echo | $(CXX) -march=native -E -dM -)
	ifneq ($(findstring __AVX512F__, $(PROPERTIES)),)
		ifneq ($(findstring __AVX512BW__, $(PROPERTIES)),)
			CXXFLAGS += -DUSE_AVX512 -mavx512f -mavx512bw
		endif
	else ifneq ($(findstring __AVX2__, $(PROPERTIES)),)
		CXXFLAGS += -DUSE_AVX2 -mavx2 -mbmi
	endif
endif

# Get what pgo flags we should be using

ifneq ($(findstring gcc, $(CC)),)
	PGOGEN   = -fprofile-generate
	PGOUSE   = -fprofile-use
endif

ifneq ($(findstring clang, $(CC)),)
	PGOMERGE = llvm-profdata merge -output=alexandria.profdata *.profraw
	PGOGEN   = -fprofile-instr-generate
	PGOUSE   = -fprofile-instr-use=alexandria.profdata
endif

# Add network name and Evalfile
CXXFLAGS += -DNETWORK_NAME=\"$(NETWORK_NAME)\" -DEVALFILE=\"$(EVALFILE)\"

SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(patsubst %.cpp,$(TMPDIR)/%.o,$(SOURCES))
DEPENDS := $(patsubst %.cpp,$(TMPDIR)/%.d,$(SOURCES))
EXE	    := $(NAME)$(SUFFIX)

all: $(TARGET)
clean:
	@rm -rf $(TMPDIR) *.o  $(DEPENDS) *.d

$(TARGET): $(OBJECTS)
	$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(NATIVE) -MMD -MP -o $(EXE) $^ $(FLAGS)

$(TMPDIR)/%.o: %.cpp | $(TMPDIR)
	$(CXX) $(CXXFLAGS) $(NATIVE) -MMD -MP -c $< -o $@ $(FLAGS)

$(TMPDIR):
	$(MKDIR) "$(TMPDIR)" "$(TMPDIR)/src"

-include $(DEPENDS)


# Usual disservin yoink for makefile related stuff
pgo:
	$(CXX) $(CXXFLAGS) $(PGO_GEN) $(NATIVE) $(INSTRUCTIONS) -MMD -MP -o $(EXE) $(SOURCES) $(LDFLAGS)
	./$(EXE) bench
	$(PGO_MERGE)
	$(CXX) $(CXXFLAGS) $(NATIVE) $(INSTRUCTIONS) $(PGO_USE) -MMD -MP -o $(EXE) $(SOURCES) $(LDFLAGS)
	@rm -f *.gcda *.profraw *.o $(DEPENDS) *.d  profdata
