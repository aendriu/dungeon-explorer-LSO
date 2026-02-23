FROM ubuntu:24.04

RUN apt-get update && \
    apt-get install -y gcc make libncurses-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

# Compila server e client
RUN make clean && make
