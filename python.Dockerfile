# Use uma imagem base leve do Python
FROM python:3.10-slim

# Instala as dependências do Python
# - paho-mqtt: Cliente MQTT para Python [cite: 419]
# - pygame: Para as interfaces gráficas [cite: 433]
# - numpy: Útil para a simulação (ruídos, física)
RUN pip install paho-mqtt pygame numpy

# Cria o diretório de trabalho
WORKDIR /app