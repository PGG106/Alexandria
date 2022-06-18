EVALFILE=C:/Users/giuseppe/source/repos/PGG106/Alexandria/src/nn.net


ifeq ($(OS), Windows_NT)
    uname_S := Windows
else
	uname_S := $(shell uname -s)
endif
ifeq ($(uname_S), Darwin)
	TARGET = -mcpu=apple-a14
else
	TARGET = -march=native
endif

ifeq ($(build), release)
	CFLAGS =  -static -static-libgcc -static-libstdc++ 
endif

SRCS := $(wildcard *.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

Alexandria: $(TARGET)
$(TARGET): $(OBJS)
	g++ -static -static-libstdc++ -static-libgcc -o Alexandria $@ $^

%.o: %.cpp
	g++ -c -static -static-libstdc++ -static-libgcc $(TARGET) -DEVALFILE="\"$(EVALFILE)\"" -flto -O3 -std=gnu++2a  -Wall $<

clean:
	rm *.o *.exe