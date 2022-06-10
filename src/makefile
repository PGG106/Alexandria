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
	CFLAGS = -static-libgcc -static-libstdc++ -static
endif

SRCS := $(wildcard *.cpp)
OBJS := $(patsubst %.cpp,%.o,$(SRCS))

Alexandria: $(TARGET)
$(TARGET): $(OBJS)
	g++ -o Alexandria $@ $^

%.o: %.cpp
	g++ -c -flto -O3 $(TARGET) $(CFLAGS)  -std=gnu++2a  -static -Wall $<

clean:
	rm *.o *.exe