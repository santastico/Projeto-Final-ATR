FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Instala dependÃªncias do sistema
RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config git \
    libssl-dev \
    mosquitto mosquitto-clients \
    python3 python3-pip python3-dev \
    nlohmann-json3-dev \
    fontconfig \
    libx11-6 libxext6 libxrender1 libxrandr2 libxcursor1 \
    libxinerama1 libxi6 libgl1 libsm6 \
    libsdl2-2.0-0 libsdl2-image-2.0-0 libsdl2-mixer-2.0-0 libsdl2-ttf-2.0-0 \
 && rm -rf /var/lib/apt/lists/*

# ------------------------------
# ConfiguraÃ§Ã£o do Mosquitto (CRÃTICO PARA ACESSO EXTERNO)
# ------------------------------
# Certifique-se de ter criado o arquivo 'mosquitto.conf' na raiz do projeto
COPY mosquitto.conf /etc/mosquitto/mosquitto.conf

# ------------------------------
# Instala o Paho MQTT C e C++ manualmente
# ------------------------------
WORKDIR /tmp

# 1. Biblioteca C
RUN git clone https://github.com/eclipse/paho.mqtt.c.git && \
    cd paho.mqtt.c && \
    cmake -Bbuild -H. -DPAHO_BUILD_STATIC=TRUE -DPAHO_WITH_SSL=TRUE && \
    cmake --build build/ --target install && \
    ldconfig

# 2. Biblioteca C++
RUN git clone https://github.com/eclipse/paho.mqtt.cpp.git && \
    cd paho.mqtt.cpp && \
    cmake -Bbuild -H. -DPAHO_BUILD_STATIC=TRUE -DPAHO_WITH_SSL=TRUE && \
    cmake --build build/ --target install && \
    ldconfig

# Torna visÃ­vel os .pc instalados em /usr/local
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig

# ------------------------------
# Compila seu projeto C++
# ------------------------------
WORKDIR /app
COPY ./caminhao_cpp /app/caminhao_cpp
# Compila o binÃ¡rio
RUN cmake -S /app/caminhao_cpp -B /app/caminhao_cpp/build && cmake --build /app/caminhao_cpp/build

# ------------------------------
# Instala dependÃªncias Python
# ------------------------------
COPY ./interface_unificada /app/interface_unificada
RUN pip install --no-cache-dir -r /app/interface_unificada/requirements.txt pygame

# ------------------------------
# Script de inicializaÃ§Ã£o
# ------------------------------
COPY ./start.sh /app/start.sh
RUN chmod +x /app/start.sh

EXPOSE 1883
ENTRYPOINT ["/app/start.sh"]