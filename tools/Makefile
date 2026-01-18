CXX := g++
PARENT_DIR := ..
CXXFLAGS := -funroll-loops -O3 -std=c++23 -I$(PARENT_DIR)
NATIVE := -march=native
FLAGS := 
SUFFIX :=

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