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
    
def listar_caminhoes_ativos(client, timeout=1.0):
    lista_recebida = []

    def on_message(_client, _userdata, msg):
        try:
            dados = json.loads(msg.payload.decode())
            lista_recebida.append(dados)
        except Exception as e:
            print(f"[CLI] Erro ao decodificar resposta: {e}")

    # Subscreve temporariamente ao tópico de resposta
    client.subscribe("atr/sim/list/response", qos=1)
    client.on_message = on_message  # sobrescreve callback só para este momento

    # Limpa fila MQTT antiga (importante em ambientes concorrentes)
    client.loop_start()
    # Envia comando para listar caminhões ativos
    client.publish("atr/sim/list", "")

    # Aguarda resposta (timeout)
    esperou = 0
    while not lista_recebida and esperou < timeout:
        time.sleep(0.1)
        esperou += 0.1

    client.loop_stop()
    client.unsubscribe("atr/sim/list/response")
    client.on_message = None  # Remove handler após uso

    if not lista_recebida:
        print("\n[CLI] Nenhuma resposta recebida (simulador pode não estar ativo ou sem caminhões).\n")
        return

    # Exibe de forma amigável os caminhões ativos
    lista = lista_recebida[0]
    if not lista:
        print("\n[CLI] Nenhum caminhão ativo.\n")
        return
    print("\n  Caminhões ativos:")
    for cam in lista:
        print(
            f"    - ID={cam['id']:<4} Pos=({cam['x']:.1f}, {cam['y']:.1f}) "
            f"Ang={cam['ang']:<5.1f}°  Temp={cam['temp']:.1f}°C "
            f"V={cam['v']:.2f}  FalhaEle={cam['f_eletrica']}  FalhaHid={cam['f_hidraulica']}"
        )
    print("")
    
def injetar_falha_eletrica(client, truck_id):
    payload = {"cmd": "set_fault", "eletrica": True}
    topico = f"atr/{truck_id}/sim/cmd"
    client.publish(topico, json.dumps(payload), qos=1)
    print(f"[CLI] Falha elétrica injetada no caminhão {truck_id}")

def injetar_falha_hidraulica(client, truck_id):
    payload = {"cmd": "set_fault", "hidraulica": True}
    topico = f"atr/{truck_id}/sim/cmd"
    client.publish(topico, json.dumps(payload), qos=1)
    print(f"[CLI] Falha hidráulica injetada no caminhão {truck_id}")

def main():
    client = mqtt.Client(client_id=f"cli_gestao_{int(time.time())}")
    client.connect(BROKER_HOST, BROKER_PORT, 60)
    client.loop_start()
    print("\nInterface de criação/remover de caminhão\n")

    while True:
        try:
            print("1 - Criar caminhão")
            print("2 - Remover caminhão")
            print("3 - Listar caminhões ativos")
            print("4 - Injetar falha elétrica")
            print("5 - Injetar falha hidráulica")
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
            elif op == "3":
                listar_caminhoes_ativos(client)
            elif op == "4":
                truck_id = input("ID do caminhão: ").strip()
                injetar_falha_eletrica(client, truck_id)

            elif op == "5":
                truck_id = input("ID do caminhão: ").strip()
                injetar_falha_hidraulica(client, truck_id)

        except Exception as e:
            print(f"Erro: {e}")
    client.disconnect()
    print("Encerrado.")

if __name__ == "__main__":
    main()
