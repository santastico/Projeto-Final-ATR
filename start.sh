#!/usr/bin/env bash
set -e

START_UI="${START_UI:-0}"
START_SIMULATOR="${START_SIMULATOR:-0}"
START_CAMINHAO="${START_CAMINHAO:-0}"
BROKER_HOST="${BROKER_HOST:-localhost}"

echo "[start] Config: SIM=$START_SIMULATOR, CAM=$START_CAMINHAO, HOST=$BROKER_HOST"

# --- INFRA: Broker + Simulador ---
if [ "$START_SIMULATOR" = "1" ]; then
    echo "[start] Iniciando Mosquitto..."
    mosquitto -c /etc/mosquitto/mosquitto.conf -d
    
    # Espera o broker subir localmente antes de lançar o simulador
    echo "[start] Aguardando broker local..."
    while ! mosquitto_sub -h localhost -p 1883 -t '$SYS/#' -C 1 -W 1 >/dev/null 2>&1; do
        sleep 0.5
    done
    echo "[start] Broker OK."

    if [ -f "/app/interface_unificada/simulator_view.py" ]; then
        echo "[start] Iniciando Simulador Python..."
        python3 /app/interface_unificada/simulator_view.py &
    fi
fi

# --- CAMINHÃO: Espera Broker Remoto e Inicia ---
if [ "$START_CAMINHAO" = "1" ]; then
    echo "[start] Aguardando conexão com Broker em $BROKER_HOST..."
    
    # Loop infinito (ou quase) até conectar
    # Usa mosquitto_sub para testar conexão TCP + MQTT real
    RETRIES=30
    while [ $RETRIES -gt 0 ]; do
        if mosquitto_sub -h "$BROKER_HOST" -p 1883 -t '$SYS/#' -C 1 -W 2 >/dev/null 2>&1; then
            echo "[start] Conexão com Broker estabelecida!"
            break
        fi
        echo "[start] Broker indisponível... tentando novamente ($RETRIES restantes)"
        sleep 2
        RETRIES=$((RETRIES-1))
    done

    if [ $RETRIES -eq 0 ]; then
        echo "[start] ERRO: Não foi possível conectar ao broker após 60s."
        exit 1
    fi

    # Inicia Binário
    BIN_CPP="/app/caminhao_cpp/build/caminhao_embarcado"
    if [ -x "$BIN_CPP" ]; then
        echo "[start] Iniciando núcleo C++ (ID=$CAMINHAO_ID)..."
        "$BIN_CPP" &
    else
        echo "[start][ERRO] Binário não encontrado."
    fi
fi

if [ "$START_UI" = "1" ]; then
    python3 /app/interface_unificada/main_ui.py
fi

tail -f /dev/null
