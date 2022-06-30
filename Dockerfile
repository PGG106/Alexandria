FROM gcc:11.2.0 as GCC

COPY ["/src", "/src"]
WORKDIR /src
RUN make
