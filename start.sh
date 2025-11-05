#!/bin/bash

echo "[Start Script] Iniciando o Broker Mosquitto em background..."
# Inicia o broker MQTT em segundo plano
mosquitto &

# Aguarda 1 segundo para o broker iniciar antes de lançar a UI
sleep 1

echo "[Start Script] Iniciando a Interface Unificada Python..."
# Inicia a interface unificada (Pygame/Python)
# Este é o processo principal que mantém o contêiner vivo
python3 /app/interface_unificada/main_ui.py

echo "[Start Script] Interface encerrada. Desligando."