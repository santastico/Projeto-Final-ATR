#!/usr/bin/env bash
set -e

# Variáveis com defaults
START_UI="${START_UI:-0}"
START_SIMULATOR="${START_SIMULATOR:-1}"
BROKER_HOST="${BROKER_HOST:-localhost}"

echo "[start] Iniciando broker Mosquitto..."
mosquitto -d
sleep 0.8

# Checa binário C++
BIN_CPP="/app/caminhao_cpp/build/caminhao_embarcado"
if [ -x "$BIN_CPP" ]; then
  echo "[start] Iniciando núcleo C++: $BIN_CPP"
  "$BIN_CPP" &
else
  echo "[start][AVISO] Binário C++ não encontrado em $BIN_CPP"
fi

# (opcional) Inicia simulador Python para gerar sensores brutos via MQTT
if [ "$START_SIMULATOR" = "1" ]; then
  if [ -f "/app/interface_unificada/simulator_view.py" ]; then
    echo "[start] Iniciando simulador Python (BROKER=$BROKER_HOST)..."
    # O simulador conecta em localhost:1883 (dentro do container)
    # e aceita 'spawn' dinâmico via tópico atr/sim/spawn
    python3 /app/interface_unificada/simulator_view.py &
  else
    echo "[start][AVISO] simulator_view.py não encontrado."
  fi
fi

# (opcional) Inicia UI unificada quando estiver pronta
if [ "$START_UI" = "1" ]; then
  if [ -f "/app/interface_unificada/main_ui.py" ]; then
    echo "[start] Iniciando Interface Unificada Python..."
    python3 /app/interface_unificada/main_ui.py
  else
    echo "[start][AVISO] main_ui.py não encontrado. Mantendo container ativo..."
    tail -f /dev/null
  fi
else
  echo "[start] Container ativo. Logs do broker no syslog. Pressione Ctrl+C para sair."
  # Mantém o container vivo quando UI não é iniciada
  tail -f /dev/null
fi
