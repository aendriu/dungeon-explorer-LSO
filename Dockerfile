# Immagine base: Ubuntu con gcc e librerie necessarie
FROM ubuntu:24.04

# Installa compilatore e dipendenze
RUN apt-get update && \
    apt-get install -y gcc make libncurses-dev && \
    rm -rf /var/lib/apt/lists/*

# Cartella di lavoro dentro il container
WORKDIR /app

# Copia tutti i sorgenti nel container
COPY . .

# Compila server e client
RUN make clean && make
