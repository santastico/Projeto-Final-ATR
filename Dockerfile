FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config git \
    libssl-dev \
    mosquitto mosquitto-clients \
    python3 python3-pip \
    nlohmann-json3-dev \
 && rm -rf /var/lib/apt/lists/*

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

# Torna visível os .pc instalados em /usr/local
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig

# Compila seu projeto C++
WORKDIR /app
COPY ./caminhao_cpp /app/caminhao_cpp
RUN cmake -S /app/caminhao_cpp -B /app/caminhao_cpp/build && cmake --build /app/caminhao_cpp/build


# ------------------------------
# Instala dependências Python
# ------------------------------
COPY ./interface_unificada /app/interface_unificada
RUN pip install --no-cache-dir -r /app/interface_unificada/requirements.txt

# Script de inicialização
COPY ./start.sh /app/start.sh
RUN chmod +x /app/start.sh

EXPOSE 1883
ENTRYPOINT ["/app/start.sh"]
