CXX := g++
PARENT_DIR := ..
CXXFLAGS += -funroll-loops -O3 -std=c++23 -I$(PARENT_DIR)
FLAGS := 
SUFFIX :=
PREPROCESS_ARCH ?= native

ifeq ($(OS), Windows_NT)
	SUFFIX := .exe
	CXXFLAGS += -static
else
	FLAGS := -pthread
endif

preprocess-$(PREPROCESS_ARCH)$(SUFFIX): preprocess.cpp ../src/nnue.h ../src/simd.h
	$(CXX) $(CXXFLAGS) $(NATIVE) -o $@ $< $(FLAGS)

clean:
	rm -f preprocess preprocess-* *.o