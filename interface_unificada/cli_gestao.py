import paho.mqtt.client as mqtt
import json
import os
import time

BROKER_HOST = os.environ.get("BROKER_HOST", "localhost")
BROKER_PORT = 1883

def criar_caminhao(client, truck_id, x, y, ang=0.0):
    # 1) Cria caminhão
    client.publish("atr/sim/spawn", json.dumps({"cmd": "spawn", "truck_id": truck_id}), qos=1)
    print(f"Comando de criação enviado para caminhão {truck_id}")
    time.sleep(0.2)  # Dá tempo do simulador instanciar
    # 2) Seta posição inicial
    client.publish(f"atr/{truck_id}/sim/cmd", json.dumps({
        "cmd": "reset_position",
        "x": float(x),
        "y": float(y),
        "ang": float(ang)
    }), qos=1)
    print(f"Comando de posição enviado para caminhão {truck_id} para (x={x}, y={y}, ang={ang})")

def remover_caminhao(client, truck_id):
    payload = {"cmd": "remove", "truck_id": truck_id}
    client.publish("atr/sim/remove", json.dumps(payload), qos=1)
    print(f"Comando de remoção enviado para caminhão {truck_id}")

def main():
    client = mqtt.Client(client_id=f"cli_gestao_{int(time.time())}")
    client.connect(BROKER_HOST, BROKER_PORT, 60)
    client.loop_start()
    print("\nInterface de criação/remover de caminhão\n")

    while True:
        try:
            print("1 - Criar caminhão")
            print("2 - Remover caminhão")
            print("ENTER - Sair")
            op = input("Escolha: ").strip()
            if not op:
                break
            if op == "1":
                truck_id = input("ID do caminhão: ").strip()
                x = float(input("  Posição X: "))
                y = float(input("  Posição Y: "))
                ang = float(input("  Ângulo inicial (padrão 0): ") or "0")
                criar_caminhao(client, truck_id, x, y, ang)
            elif op == "2":
                truck_id = input("ID do caminhão para remover: ").strip()
                remover_caminhao(client, truck_id)
        except Exception as e:
            print(f"Erro: {e}")
    client.disconnect()
    print("Encerrado.")

if __name__ == "__main__":
    main()
