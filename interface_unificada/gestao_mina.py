#!/usr/bin/env python3
"""
gestao_mina.py

Interface central de Gestão da Mina (GUI Pygame + MQTT).

Responsabilidades (conforme arquitetura):
- Assinar atr/<id>/posicao_inicial, publicados pelo Planejamento de Rota de
  cada caminhao, para conhecer a Posição atual (tratada) da frota.
- Renderizar um mapa 100x100 em Pygame, com um Ã­cone por caminhao.
- Permitir ao operador:
    * Criar caminhÃµes no simulador (spawn/reset_position) escolhendo Posição
      e Ã¢ngulo iniciais.
    * Selecionar um caminhao e clicar no mapa para definir o destino final
      (setpoint_posicao_final) enviado ao Planejamento de Rota.
"""

import json
import os
import threading
import time
import math

import pygame
import paho.mqtt.client as mqtt

# -----------------------------------------------------------
# ConfiguraÃ§Ã£o MQTT (igual ao cli_gestao)
# -----------------------------------------------------------

BROKER_HOST = os.environ.get("BROKER_HOST", "localhost")
BROKER_PORT = int(os.environ.get("BROKER_PORT", "1883"))

# -----------------------------------------------------------
# EspaÃ§o da mina (100x100)
# -----------------------------------------------------------

WORLD_SIZE = 100.0  # eixo X e Y em [0, 100]

# -----------------------------------------------------------
# Estado da frota
# -----------------------------------------------------------
# estado_frota["1"] = { "x": 10.0, "y": 20.0, "ang": 90.0, "last_ts": 123.0 }

estado_frota = {}
estado_lock = threading.Lock()
selected_truck_id = None

# -----------------------------------------------------------
# ConversÃ£o mundo â†” tela
# -----------------------------------------------------------


def world_to_screen(x: float, y: float, width: int, height: int):
    """
    Converte (x,y) em [0, WORLD_SIZE] para coordenadas em pixels.

    (0,0) da mina = canto inferior esquerdo.
    (0,0) Pygame   = canto superior esquerdo â†’ inverter Y.
    """
    sx = int((x / WORLD_SIZE) * width)
    sy = height - int((y / WORLD_SIZE) * height)
    return sx, sy


def screen_to_world(px: int, py: int, width: int, height: int):
    """
    Converte clique de tela (px,py) em coordenadas do mundo [0, WORLD_SIZE].
    """
    x = (px / float(width)) * WORLD_SIZE
    y = ((height - py) / float(height)) * WORLD_SIZE
    return x, y


# -----------------------------------------------------------
# Callbacks MQTT
# -----------------------------------------------------------


def on_connect(client, userdata, flags, rc):
    print(f"[gestao_mina] Conectado a {BROKER_HOST}:{BROKER_PORT} rc={rc}")
    # Posição tratada publicada pelo Planejamento de Rota:
    # atr/<truck_id>/posicao_inicial
    client.subscribe("atr/+/posicao_inicial", qos=1)
    print("[gestao_mina] Assinado tópico atr/+/posicao_inicial")


def on_message(client, userdata, msg):
    """
    Espera payload JSON no formato:
      { "truck_id": <int/str>, "x": <float>, "y": <float>, "ang": <float> }
    no tÃ³pico:
      atr/<truck_id>/posicao_inicial
    """
    try:
        parts = msg.topic.split("/")
        if len(parts) != 3 or parts[0] != "atr" or parts[2] != "posicao_inicial":
            return

        truck_id = parts[1]

        dados = json.loads(msg.payload.decode())
        x = float(dados.get("x", dados.get("f_posicao_x", 0.0)))
        y = float(dados.get("y", dados.get("f_posicao_y", 0.0)))
        ang = float(dados.get("ang", dados.get("f_angulo_x", 0.0)))

        with estado_lock:
            estado_frota[truck_id] = {
                "x": x,
                "y": y,
                "ang": ang,
                "last_ts": time.time(),
            }

    except Exception as e:
        print(f"[gestao_mina] Erro ao processar mensagem '{msg.topic}': {e}")


# -----------------------------------------------------------
# PublicaÃ§Ã£o MQTT
# -----------------------------------------------------------


def publicar_setpoint_final(client: mqtt.Client, truck_id: str, x_dest: float, y_dest: float):
    """
    Publica o destino final (ponto final) para o caminhao:
      atr/<id>/setpoint_posicao_final
    Payload: { "x": x_dest, "y": y_dest }
    """
    payload = {"x": float(x_dest), "y": float(y_dest)}
    topico = f"atr/{truck_id}/setpoint_posicao_final"
    client.publish(topico, json.dumps(payload), qos=1)
    print(f"[gestao_mina] Novo destino para caminhao {truck_id}: "
          f"({x_dest:.2f}, {y_dest:.2f}) -> {topico}")


def criar_caminhao_simulador(client: mqtt.Client, truck_id: str, x_ini: float, y_ini: float, ang_ini: float):
    """
    Cria caminhao no simulador e define Posição inicial, espelhando lÃ³gica do cli_gestao. [spawn + reset_position]
    """
    # 1) Spawn
    payload_spawn = {"cmd": "spawn", "truck_id": str(truck_id)}
    client.publish("atr/sim/spawn", json.dumps(payload_spawn), qos=1)
    print(f"[gestao_mina] Spawn solicitado para caminhao {truck_id}")
    time.sleep(0.2)

    # 2) Reset de Posição
    payload_pos = {
        "cmd": "reset_position",
        "x": float(x_ini),
        "y": float(y_ini),
        "ang": float(ang_ini),
    }
    client.publish(f"atr/{truck_id}/sim/cmd", json.dumps(payload_pos), qos=1)
    print(f"[gestao_mina] Posição inicial definida p/ {truck_id}: "
          f"({x_ini}, {y_ini}, ang={ang_ini})")

    # 3) Atualiza estado local imediatamente (sem esperar Planejamento)
    with estado_lock:
        estado_frota[str(truck_id)] = {
            "x": float(x_ini),
            "y": float(y_ini),
            "ang": float(ang_ini),
            "last_ts": time.time(),
        }


# -----------------------------------------------------------
# Interface Pygame
# -----------------------------------------------------------


def loop_pygame(client: mqtt.Client):
    global selected_truck_id

    pygame.init()
    largura, altura = 800, 500
    screen = pygame.display.set_mode((largura, altura))
    pygame.display.set_caption("Gestão da Mina - Mapa 100x100")

    clock = pygame.time.Clock()
    font = pygame.font.SysFont("Consolas", 16)

    rodando = True
    while rodando:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                rodando = False

            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    rodando = False

                elif event.key == pygame.K_c:
                    # Criar caminhao via interface
                    try:
                        tid = input("\nID do caminhao para criar (1-5): ").strip()
                        if not tid:
                            print("[gestao_mina] Criação cancelada.")
                        else:
                            x_ini = float(input("Posição X inicial (0-100): ").strip())
                            y_ini = float(input("Posição Y inicial (0-100): ").strip())
                            ang_ini = float(input("Ângulo inicial em graus (0=lestes): ").strip() or "0")
                            criar_caminhao_simulador(client, tid, x_ini, y_ini, ang_ini)
                    except Exception as e:
                        print(f"[gestao_mina] Erro ao criar caminhao: {e}")

                elif event.key == pygame.K_t:
                    # Selecionar caminhao e definir setpoint de destino via terminal
                    with estado_lock:
                        ids = sorted(estado_frota.keys())

                    print("\n=== Selecionar caminhao para setpoint ===")
                    if not ids:
                        print("Nenhum caminhao conhecido ainda (aguarde posicao_inicial).")
                        continue

                    print(f"IDs de caminhoes conhecidos ({len(ids)}): " + ", ".join(ids))
                    tid = input("ID do caminhao (ENTER para cancelar): ").strip()
                    if not tid:
                        print("[gestao_mina] Selecao cancelada.")
                        continue
                    if tid not in ids:
                        print(f"[gestao_mina] ID {tid} nÃ£o estÃ¡ na lista de caminhoes conhecidos.")
                        continue

                    selected_truck_id = tid
                    print(f"[gestao_mina] Caminhao selecionado: {selected_truck_id}")

                    try:
                        x_dest = float(input("Destino X (0-100): ").strip())
                        y_dest = float(input("Destino Y (0-100): ").strip())
                        publicar_setpoint_final(client, selected_truck_id, x_dest, y_dest)
                    except Exception as e:
                        print(f"[gestao_mina] Erro ao ler destino: {e}")



        # Fundo e grade
        screen.fill((15, 15, 25))
        cor_grade_fina = (40, 40, 60)
        cor_grade_grossa = (70, 70, 100)
        for i in range(0, 101):
            x_px, _ = world_to_screen(i, 0, largura, altura)
            _, y_px = world_to_screen(0, i, largura, altura)
            if i % 10 == 0:
                pygame.draw.line(screen, cor_grade_grossa, (0, y_px), (largura, y_px), 1)
                pygame.draw.line(screen, cor_grade_grossa, (x_px, 0), (x_px, altura), 1)
            else:
                pygame.draw.line(screen, cor_grade_fina, (0, y_px), (largura, y_px), 1)
                pygame.draw.line(screen, cor_grade_fina, (x_px, 0), (x_px, altura), 1)

        # Snapshot da frota
        with estado_lock:
            itens = list(estado_frota.items())
        agora = time.time()

        # Desenhar caminhÃµes
        for truck_id, st in itens:
            x = st["x"]
            y = st["y"]
            ang = st["ang"]
            age = agora - st["last_ts"]

            sx, sy = world_to_screen(x, y, largura, altura)

            if truck_id == selected_truck_id:
                cor = (0, 255, 0)
                raio = 8
            else:
                cor = (0, 180, 255)
                raio = 6

            pygame.draw.circle(screen, cor, (sx, sy), raio)

            # Vetor de direÃ§Ã£o
            ang_rad = math.radians(ang)
            dx = 8 * math.cos(ang_rad)
            dy = 8 * math.sin(ang_rad)
            ex, ey = world_to_screen(x + dx, y + dy, largura, altura)
            pygame.draw.line(screen, (255, 255, 0), (sx, sy), (ex, ey), 2)

            # Label com ID + idade da amostra
            label = font.render(f"{truck_id} ({age:0.1f}s)", True, (255, 255, 255))
            screen.blit(label, (sx + 10, sy - 10))

        # Painel de instruÃ§Ãµes
        painel = [
            "Gestao da Mina - Mapa 100x100",
            "ESC= sair  |  C= criar caminhao  |  T= selecionar caminhao para destino",
            f"Caminhao selecionado: {selected_truck_id if selected_truck_id else '(nenhum)'}",
            f"Caminhoes visiveis: {len(itens)}",
        ]
        y_txt = 10
        for linha in painel:
            txt = font.render(linha, True, (230, 230, 230))
            screen.blit(txt, (10, y_txt))
            y_txt += 20

        pygame.display.flip()
        clock.tick(30)

    pygame.quit()


# -----------------------------------------------------------
# main()
# -----------------------------------------------------------


def main():
    client = mqtt.Client(client_id=f"gestao_mina_{int(time.time())}")
    client.on_connect = on_connect
    client.on_message = on_message

    print(f"[gestao_mina] Conectando em {BROKER_HOST}:{BROKER_PORT} ...")
    client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
    client.loop_start()

    try:
        loop_pygame(client)
    finally:
        client.loop_stop()
        client.disconnect()
        print("[gestao_mina] Encerrado.")


if __name__ == "__main__":
    main()