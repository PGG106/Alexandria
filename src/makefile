EVALFILE=nn.net


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

ifeq ($(OS), Windows_NT)
    CFLAGS =  -static -static-libstdc++ -static-libgcc -pthread 
else
	CFLAGS =  -pthread 
endif

ifeq ($(build), release)
	
endif

SRCS := $(wildcard *.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

Alexandria: $(TARGET)
$(TARGET): $(OBJS)
	g++ $(CFLAGS) -o Alexandria $@ $^

%.o: %.cpp
	g++ -c $(CFLAGS)  $(TARGET) -DEVALFILE="\"$(EVALFILE)\"" -flto -O3 -std=gnu++2a  -Wall $<

clean:
	del *.o *.exe