import paho.mqtt.client as mqtt
import json
import os
import time
import threading
import sys

# Configuração do Broker via Variável de Ambiente
BROKER_HOST = os.environ.get("BROKER_HOST", "localhost")
BROKER_PORT = 1883

# Estado global da frota (atualizado via MQTT)
frota_status = {} # { "1": { "x": 10, "y": 20, "status": "ativo" }, ... }
lock_frota = threading.Lock()

# IDs definidos no docker-compose
CAMINHOES_DISPONIVEIS = [1, 2, 3, 4, 5]

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[MQTT] Conectado ao broker {BROKER_HOST}!")
        # Assina atualizações de sensores de TODOS os caminhões
        client.subscribe("atr/+/sensor/raw")
        # Assina respostas de lista do simulador
        client.subscribe("atr/sim/list/response")
    else:
        print(f"[MQTT] Falha na conexão. Código: {rc}")

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode()

    try:
        data = json.loads(payload)
        
        # Atualização de sensor (heartbeat do caminhão)
        if "sensor/raw" in topic:
            truck_id = str(data.get("truck_id"))
            with lock_frota:
                if truck_id not in frota_status:
                    frota_status[truck_id] = {}
                
                frota_status[truck_id].update({
                    "x": data.get("i_posicao_x"),
                    "y": data.get("i_posicao_y"),
                    "ang": data.get("i_angulo_x"),
                    "temp": data.get("i_temperatura"),
                    "last_seen": time.time()
                })

        # Resposta da lista de ativos do simulador
        elif topic == "atr/sim/list/response":
            # data é uma lista de objetos
            pass # Podemos usar isso para sincronizar status "real"

    except Exception as e:
        pass
        # print(f"Erro ao processar msg: {e}")

def clrscr():
    os.system('cls' if os.name == 'nt' else 'clear')

def exibir_menu():
    clrscr()
    print("==========================================")
    print(f"   GESTÃO DA MINA (Broker: {BROKER_HOST})")
    print("==========================================")
    print("Caminhões no Docker: 1 a 5")
    print("------------------------------------------")
    print("STATUS DA FROTA (Últimos dados recebidos):")
    
    with lock_frota:
        if not frota_status:
            print(" [Nenhum caminhão ativo ou transmitindo]")
        else:
            print(f" {'ID':<4} | {'Pos (X,Y)':<12} | {'Ang':<6} | {'Temp':<6} | {'Status'}")
            print("-" * 50)
            for tid in sorted(frota_status.keys(), key=lambda x: int(x)):
                info = frota_status[tid]
                # Verifica se está 'vivo' (dados recentes < 5s)
                is_alive = (time.time() - info.get("last_seen", 0)) < 5.0
                status = "ONLINE" if is_alive else "OFFLINE"
                
                print(f" {tid:<4} | {info.get('x'):>4}, {info.get('y'):<5} | {info.get('ang'):>4} | {info.get('temp'):>4} | {status}")
    
    print("==========================================")
    print("1. ATIVAR Caminhão (Spawn no Simulador)")
    print("2. REMOVER Caminhão (Stop no Simulador)")
    print("3. INJETAR Falha Elétrica")
    print("4. LIMPAR Falhas")
    print("5. Atualizar Lista (Refresh)")
    print("0. Sair")
    print("==========================================")

def comando_spawn(client):
    try:
        tid = input("Digite o ID do caminhão para ATIVAR (1-5): ")
        if int(tid) not in CAMINHOES_DISPONIVEIS:
            print("ID inválido! Use IDs definidos no Docker Compose.")
            input("Pressione Enter...")
            return

        # Manda spawn para o simulador
        payload = {"cmd": "spawn", "truck_id": tid}
        client.publish("atr/sim/spawn", json.dumps(payload), qos=1)
        print(f"Comando enviado: ATIVAR Caminhão {tid}")
        
        # Manda reset de posição opcional
        time.sleep(0.2)
        x = input("Posição X inicial (padrão 0): ") or "0"
        y = input("Posição Y inicial (padrão 0): ") or "0"
        payload_pos = {"cmd": "reset_position", "x": float(x), "y": float(y), "ang": 0}
        client.publish(f"atr/{tid}/sim/cmd", json.dumps(payload_pos), qos=1)
        print(f"Posição definida para {tid}.")
        
    except ValueError:
        print("Entrada inválida.")
    
    time.sleep(1)

def comando_remove(client):
    tid = input("Digite o ID do caminhão para REMOVER: ")
    payload = {"cmd": "remove", "truck_id": tid}
    client.publish("atr/sim/remove", json.dumps(payload), qos=1)
    print(f"Comando enviado: REMOVER Caminhão {tid}")
    
    # Remove da lista local também para limpar a tela
    with lock_frota:
        if tid in frota_status:
            del frota_status[tid]
            
    time.sleep(1)

def comando_falha(client):
    tid = input("Digite o ID do caminhão: ")
    payload = {"cmd": "set_fault", "eletrica": True, "hidraulica": False}
    client.publish(f"atr/{tid}/sim/cmd", json.dumps(payload), qos=1)
    print(f"FALHA ELÉTRICA injetada no Caminhão {tid}!")
    time.sleep(1)

def comando_limpar(client):
    tid = input("Digite o ID do caminhão: ")
    payload = {"cmd": "clear_faults"}
    client.publish(f"atr/{tid}/sim/cmd", json.dumps(payload), qos=1)
    print(f"Falhas limpas no Caminhão {tid}.")
    time.sleep(1)

def main():
    client = mqtt.Client("cli_gestao_central")
    client.on_connect = on_connect
    client.on_message = on_message

    try:
        print(f"Conectando ao broker {BROKER_HOST}...")
        client.connect(BROKER_HOST, BROKER_PORT, 60)
        client.loop_start()
    except Exception as e:
        print(f"Erro crítico ao conectar no broker: {e}")
        return

    while True:
        exibir_menu()
        opcao = input("Escolha uma opção: ")

        if opcao == "1":
            comando_spawn(client)
        elif opcao == "2":
            comando_remove(client)
        elif opcao == "3":
            comando_falha(client)
        elif opcao == "4":
            comando_limpar(client)
        elif opcao == "5":
            print("Atualizando...")
            time.sleep(0.5)
        elif opcao == "0":
            print("Saindo...")
            break
        else:
            print("Opção inválida!")
            time.sleep(1)

    client.loop_stop()
    client.disconnect()

if __name__ == "__main__":
    main()
