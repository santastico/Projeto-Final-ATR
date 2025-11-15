import json
import time
import os

import paho.mqtt.client as mqtt


def ler_int(mensagem: str) -> int:
    """Lê um inteiro do terminal, reaproveitando o prompt até o usuário digitar certo."""
    while True:
        try:
            valor = int(input(mensagem))
            return valor
        except ValueError:
            print("Valor inválido, digite um número inteiro.")


def ler_float(mensagem: str) -> float:
    """Lê um número de ponto flutuante do terminal."""
    while True:
        try:
            valor = float(input(mensagem))
            return valor
        except ValueError:
            print("Valor inválido, digite um número (aceita casas decimais).")


def menu_injecao_falhas(client: mqtt.Client, lista_caminhoes: list[str]) -> None:
    """
    Menu interativo para o usuário injetar falhas nos caminhões já criados.
    Permite:
      - Aumentar temperatura (temp_step)
      - Ativar/desativar falha elétrica
      - Ativar/desativar falha hidráulica
      - Limpar falhas
    """
    if not lista_caminhoes:
        print("\n[management_view] Nenhum caminhão registrado nesta sessão para injetar falhas.")
        return

    print("\n=== MODO DE INJEÇÃO DE FALHAS ===")
    print("Você pode, a qualquer momento, gerar falhas nos caminhões enquanto eles se deslocam.")
    print("Caminhões conhecidos nesta sessão:", ", ".join(lista_caminhoes))

    while True:
        print("\n----- MENU DE FALHAS -----")
        print("1) Aumentar temperatura do motor (temp_step)")
        print("2) Ativar/Desativar falha elétrica")
        print("3) Ativar/Desativar falha hidráulica")
        print("4) Limpar todas as falhas do caminhão")
        print("0) Voltar ao menu principal")
        opc = ler_int("Selecione uma opção: ")

        if opc == 0:
            print("[management_view] Saindo do modo de injeção de falhas.")
            break

        # Escolhe caminhão
        truck_id = input("ID do caminhão alvo (ENTER para listar os conhecidos): ").strip()
        if not truck_id:
            print("Caminhões conhecidos:", ", ".join(lista_caminhoes))
            truck_id = input("Digite o ID de um caminhão: ").strip()

        if not truck_id:
            print("ID de caminhão vazio. Abortando operação.")
            continue

        sim_cmd_topic = f"atr/{truck_id}/sim/cmd"

        if opc == 1:
            # Aumentar temperatura
            delta = ler_float("Valor de incremento de temperatura (ΔT em °C, ex: 10): ")
            payload = {
                "cmd": "temp_step",
                "delta": delta,
            }
            client.publish(sim_cmd_topic, json.dumps(payload), qos=1)
            print(f"[MQTT] temp_step publicado em '{sim_cmd_topic}': {payload}")

        elif opc == 2:
            # Falha elétrica on/off
            estado = input("Ativar falha elétrica? (s/N): ").strip().lower()
            falha = (estado == "s")
            # Mantemos apenas o campo elétrico; o simulador preserva o hidráulico atual
            payload = {
                "cmd": "set_fault",
                "eletrica": falha,
            }
            client.publish(sim_cmd_topic, json.dumps(payload), qos=1)
            print(f"[MQTT] set_fault (eletrica={falha}) publicado em '{sim_cmd_topic}': {payload}")

        elif opc == 3:
            # Falha hidráulica on/off
            estado = input("Ativar falha hidráulica? (s/N): ").strip().lower()
            falha = (estado == "s")
            payload = {
                "cmd": "set_fault",
                "hidraulica": falha,
            }
            client.publish(sim_cmd_topic, json.dumps(payload), qos=1)
            print(f"[MQTT] set_fault (hidraulica={falha}) publicado em '{sim_cmd_topic}': {payload}")

        elif opc == 4:
            # Limpar falhas
            payload = {
                "cmd": "clear_faults",
            }
            client.publish(sim_cmd_topic, json.dumps(payload), qos=1)
            print(f"[MQTT] clear_faults publicado em '{sim_cmd_topic}': {payload}")

        else:
            print("Opção inválida. Tente novamente.")


def main():
    # Descobre o broker (igual aos outros módulos Python)
    broker_host = os.environ.get("BROKER_HOST", "localhost")
    broker_port = 1883

    client = mqtt.Client(
        client_id=f"management_{int(time.time())}",
        clean_session=True,
    )

    print(f"[management_view] Conectando ao broker MQTT em {broker_host}:{broker_port} ...")
    client.connect(broker_host, broker_port, 60)
    client.loop_start()
    print("[management_view] Conectado.\n")

    print("=== GERENCIAMENTO DE CAMINHÕES (management_view.py) ===")
    print("Este módulo permite:")
    print("  - Definir quantos caminhões criar na simulação;")
    print("  - Definir a ORIGEM de cada caminhão (x, com y = 0);")
    print("  - Definir o DESTINO (x, y) de cada caminhão;")
    print("  - Injetar FALHAS (temperatura, elétrica, hidráulica) enquanto os caminhões se deslocam.\n")

    # Lista de IDs de caminhões criados nesta sessão
    created_trucks: list[str] = []

    while True:
        print("\n===== MENU PRINCIPAL =====")
        print("1) Configurar / criar caminhões (origem e destino)")
        print("2) Injetar falhas em caminhões em operação")
        print("0) Sair")
        opc_principal = ler_int("Selecione uma opção: ")

        if opc_principal == 0:
            print("\n[management_view] Encerrando...")
            break

        elif opc_principal == 1:
            # Configuração de caminhões
            n_trucks = ler_int("\nQuantos caminhões deseja configurar nesta rodada? (0 para voltar): ")
            if n_trucks <= 0:
                continue

            for idx in range(1, n_trucks + 1):
                print("\n---------------------------------------------")
                print(f"Configuração do caminhão #{idx}")

                # ID do caminhão (padrão = idx)
                truck_id_input = input(
                    f"ID do caminhão (ENTER para usar '{idx}'): "
                ).strip()
                truck_id = truck_id_input if truck_id_input else str(idx)

                if truck_id not in created_trucks:
                    created_trucks.append(truck_id)

                # ORIGEM: sempre no eixo x, com y = 0
                print(f"\nOrigem do caminhão {truck_id}:")
                origem_x = ler_float("  -> Coordenada x de origem (y será fixo em 0): ")
                origem_y = 0.0
                origem_ang = 0.0  # apontando no eixo x positivo

                print(f"  Origem definida: ({origem_x}, {origem_y}), angulo = {origem_ang}°")

                # DESTINO: x,y arbitrários definidos pelo usuário
                print(f"\nDestino do caminhão {truck_id}:")
                destino_x = ler_float("  -> Coordenada x de destino: ")
                destino_y = ler_float("  -> Coordenada y de destino: ")

                print(f"  Destino definido: ({destino_x}, {destino_y})")

                # 1) Solicita ao simulator_view.py para criar o caminhão via 'atr/sim/spawn'
                spawn_topic = "atr/sim/spawn"
                spawn_payload = {
                    "cmd": "spawn",
                    "truck_id": truck_id,
                }
                client.publish(spawn_topic, json.dumps(spawn_payload), qos=1)
                print(f"\n[MQTT] Publicado spawn do caminhão {truck_id} em '{spawn_topic}':")
                print(f"       {spawn_payload}")

                # 2) Envia comando de reset de posição (origem) para o simulador
                sim_cmd_topic = f"atr/{truck_id}/sim/cmd"
                reset_payload = {
                    "cmd": "reset_position",
                    "x": origem_x,
                    "y": origem_y,
                    "ang": origem_ang,
                }
                client.publish(sim_cmd_topic, json.dumps(reset_payload), qos=1)
                print(f"[MQTT] Publicado reset de posição em '{sim_cmd_topic}':")
                print(f"       {reset_payload}")

                # 3) Envia DESTINO lógico para o módulo de Planejamento de Rota (C++)
                setpoint_topic = f"atr/{truck_id}/gestao/setpoint_posicao_final"
                setpoint_payload = {
                    "x": destino_x,
                    "y": destino_y,
                }
                client.publish(setpoint_topic, json.dumps(setpoint_payload), qos=1)
                print(f"[MQTT] Publicado destino para planejamento de rota em '{setpoint_topic}':")
                print(f"       {setpoint_payload}")

                print(f"\n[management_view] Caminhão {truck_id} configurado com sucesso.")
                print("  -> Simulador irá posicionar o caminhão na origem especificada;")
                print("  -> Planejamento de rota irá reagir ao destino enviado (quando o controle estiver implementado).")

        elif opc_principal == 2:
            # Modo de falhas
            menu_injecao_falhas(client, created_trucks)

        else:
            print("Opção inválida. Tente novamente.")

    # Encerra MQTT
    client.loop_stop()
    client.disconnect()


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n[management_view] Encerrado pelo usuário (Ctrl+C).")
