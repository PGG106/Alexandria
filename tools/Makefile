CXX := g++
PARENT_DIR := ..
CXXFLAGS := -funroll-loops -O3 -std=c++23 -I$(PARENT_DIR)
NATIVE := -march=native
FLAGS := 
SUFFIX :=

# Import architecture flags from parent
ifdef build
include $(PARENT_DIR)/makefile
CXXFLAGS += $(FLAGS_DETECTED)
NATIVE :=
endif

ifeq ($(OS), Windows_NT)
	SUFFIX := .exe
	CXXFLAGS += -static
else
	FLAGS := -pthread
endif

preprocess$(SUFFIX): preprocess.cpp
	$(CXX) $(CXXFLAGS) $(NATIVE) -o $@ $< $(FLAGS)

clean:
	rm -f preprocess$(SUFFIX) *.o