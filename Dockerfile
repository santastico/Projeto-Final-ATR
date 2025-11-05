# Estágio 1: Base com todas as dependências
# Usamos Ubuntu 22.04 como base universal
FROM ubuntu:22.04

# Evita prompts interativos durante a instalação
ENV DEBIAN_FRONTEND=noninteractive

# Instala todas as dependências de C++, Python, MQTT e GUI de uma vez
RUN apt-get update && apt-get install -y \
    # Dependências C++
    build-essential \
    cmake \
    libmosquittopp-dev \
    # Dependências Python
    python3-pip \
    # Dependências MQTT (O Broker + Clientes de Teste)
    mosquitto \
    mosquitto-clients \
    # Dependências para GUI (Pygame precisa delas)
    libsdl2-dev \
    && rm -rf /var/lib/apt/lists/*

# Define o diretório de trabalho
WORKDIR /app

# --- Compilação do C++ ---
# Copia o código C++ para dentro do contêiner
COPY ./caminhao_cpp /app/caminhao_cpp
# Compila o C++
RUN cmake -S /app/caminhao_cpp -B /app/caminhao_cpp/build && \
    cmake --build /app/caminhao_cpp/build

# --- Instalação do Python ---
# Copia o código Python
COPY ./interface_unificada /app/interface_unificada
# Instala as dependências Python
RUN pip3 install -r /app/interface_unificada/requirements.txt

# --- Script de Inicialização ---
# Copia e dá permissão de execução para o script start.sh
COPY ./start.sh /app/start.sh
RUN chmod +x /app/start.sh

# Comando final que o docker-compose irá rodar
ENTRYPOINT ["/app/start.sh"]