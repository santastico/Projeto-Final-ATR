# Use uma imagem base do Ubuntu com ferramentas de build
FROM ubuntu:22.04 AS build-stage

# Evita que a instalação de pacotes peça inputs interativos
ENV DEBIAN_FRONTEND=noninteractive

# Instala as dependências de build e runtime
# - build-essential: Compilador (g++), make, etc.
# - cmake: Para construir o projeto
# - libmosquittopp-dev: Biblioteca C++ do Mosquitto [cite: 421]
RUN apt-get update && apt-get install -y \
    #inutil comment
    build-essential \
    cmake \
    libmosquittopp-dev \
    && rm -rf /var/lib/apt/lists/*

# Cria o diretório de trabalho dentro do contêiner
WORKDIR /app

# Copia todo o código-fonte do C++ para o contêiner
COPY . .

# Compila o projeto
# 1. Cria a pasta 'build'
# 2. Roda o CMake para configurar
# 3. Roda o 'make' (ou 'cmake --build') para compilar
RUN cmake -B build -S . && cmake --build build

# Define o ponto de entrada. O executável compilado.
# O 'docker-compose' irá adicionar os argumentos (ex: "1", "2", "3"...)
ENTRYPOINT ["/app/build/caminhao_embarcado"]