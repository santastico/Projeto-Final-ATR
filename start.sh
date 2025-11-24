#!/usr/bin/env bash

set -e

START_UI="${START_UI:-0}"
START_SIMULATOR="${START_SIMULATOR:-0}"
START_CAMINHAO="${START_CAMINHAO:-0}"
BROKER_HOST="${BROKER_HOST:-localhost}"

echo "[start] Config: SIM=$START_SIMULATOR, CAM=$START_CAMINHAO, UI=$START_UI, HOST=$BROKER_HOST"

# --- INFRA: Broker + Simulador ---
if [ "$START_SIMULATOR" = "1" ]; then
  echo "[start] Iniciando Mosquitto..."
  mosquitto -c /etc/mosquitto/mosquitto.conf -d

  echo "[start] Aguardando broker local..."
  while ! mosquitto_sub -h localhost -p 1883 -t '$SYS/#' -C 1 -W 1 >/dev/null 2>&1; do
    sleep 0.5
  done
  echo "[start] Broker OK."

  if [ -f "/app/interface_unificada/simulator_view.py" ]; then
    echo "[start] Iniciando Simulador Python..."
    python3 /app/interface_unificada/simulator_view.py &
  else
    echo "[start][AVISO] simulator_view.py nÃ£o encontrado."
  fi
fi

# --- CAMINHÃƒO: Espera Broker Remoto e Inicia ---
if [ "$START_CAMINHAO" = "1" ]; then
  echo "[start] Aguardando conexÃ£o com Broker em $BROKER_HOST..."

  RETRIES=30
  while [ $RETRIES -gt 0 ]; do
    if mosquitto_sub -h "$BROKER_HOST" -p 1883 -t '$SYS/#' -C 1 -W 2 >/dev/null 2>&1; then
      echo "[start] ConexÃ£o com Broker estabelecida!"
      break
    fi
    echo "[start] Broker indisponÃ­vel... tentando novamente ($RETRIES restantes)"
    sleep 2
    RETRIES=$((RETRIES-1))
  done

  if [ $RETRIES -eq 0 ]; then
    echo "[start] ERRO: NÃ£o foi possÃ­vel conectar ao broker apÃ³s 60s."
    exit 1
  fi

  BIN_CPP="/app/caminhao_cpp/build/caminhao_embarcado"
  if [ -x "$BIN_CPP" ]; then
    echo "[start] Iniciando nÃºcleo C++ (ID=$CAMINHAO_ID)..."
    "$BIN_CPP" &
  else
    echo "[start][ERRO] BinÃ¡rio nÃ£o encontrado em $BIN_CPP."
  fi
fi

# --- UI (GestÃ£o da Mina / outra UI Python) ---
if [ "$START_UI" = "1" ]; then
  if [ -f "/app/interface_unificada/gestao_mina.py" ]; then
    echo "[start] Iniciando GestÃ£o da Mina (gestao_mina.py)..."
    python3 /app/interface_unificada/gestao_mina.py
  elif [ -f "/app/interface_unificada/main_ui.py" ]; then
    echo "[start] Iniciando UI principal (main_ui.py)..."
    python3 /app/interface_unificada/main_ui.py
  else
    echo "[start][AVISO] Nenhuma UI encontrada (gestao_mina.py ou main_ui.py)."
  fi
fi

# MantÃ©m o container vivo (especialmente para infra_mina)
tail -f /dev/null