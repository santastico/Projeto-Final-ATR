# simulator_view.py (VERSÃO REFORMULADA)
# Simulação da Mina (somente dinâmica + falhas + ruído padrão)
#
# Responsabilidades:
#  - recebe atuadores via MQTT (atr/{id}/act) [o_aceleracao, o_direcao]
#  - aplica modelo dinâmico simples de veículo
#  - publica sensores brutos com RUÍDO PADRÃO (atr/{id}/sensor/raw)
#  - publica sinais individuais de falha/temperatura para Monitoramento de Falhas:
#       atr/{id}/sensor/i_temperatura      (int, °C)
#       atr/{id}/sensor/i_falha_eletrica   ("0"/"1")
#       atr/{id}/sensor/i_falha_hidraulica ("0"/"1")
#  - aceita comandos de simulação (falhas, reset, etc) via atr/{id}/sim/cmd
#  - suporta criação/remoção dinâmica de caminhões via:
#       atr/sim/spawn  { "cmd": "spawn", "truck_id": "X" }
#       atr/sim/remove { "cmd": "remove", "truck_id": "X" }

import json
import time
import math
import random
import threading
import os

import paho.mqtt.client as mqtt

# ==========================
# Parâmetros globais
# ==========================

DT_DEFAULT      = 2.0  # 20 Hz
V_MAX           = 2.0         # velocidade máxima (unidades/s)
A_MAX           = 2.0         # aceleração máxima (unidades/s²)
FRIC            = 0.99        # atrito simples sobre a velocidade

# Ruído PADRÃO dos sensores (usuário não altera)
POS_NOISE_STD   = 0.30        # desvio-padrão da posição (unidades)
ANG_NOISE_STD   = 2.0         # desvio-padrão do ângulo (graus)
TEMP_NOISE_STD  = 0.5         # desvio-padrão da temperatura (°C)

def round_i(v: float) -> int:
    return int(round(v))


# ==========================
# Classe de simulação de um caminhão
# ==========================

class TruckSim:
    def __init__(self, client: mqtt.Client, truck_id: str, hz: float = 20.0):
        self.client = client
        self.id = str(truck_id)
        self.dt = DT_DEFAULT
        self.seq = 0

        # estado "real"
        self.x = 0.0
        self.y = 0.0
        self.ang = 0.0      # direção (graus, 0=leste)
        self.v = 0.0        # velocidade linear (unid/s)
        self.temp = 70.0    # temperatura inicial do "motor"

        # falhas de sistema
        self.f_eletrica = False
        self.f_hidraulica = False

        # atuadores recebidos do controle (C++)
        # o_aceleracao: -100..100 (%)
        # o_direcao: -180..180 (graus) — direção desejada
        self.o_aceleracao = 0.0
        self.o_direcao = 0.0

        # controle interno
        self._stop = False
        self._lock = threading.Lock()
        self._last_cell = (round_i(self.x), round_i(self.y))

        # tópicos principais
        self.topic_cmd    = f"atr/{self.id}/sim/cmd"
        self.topic_act    = f"atr/{self.id}/act"
        self.topic_log    = f"atr/{self.id}/sim/log"
        self.topic_sensor = f"atr/{self.id}/sensor/raw"

        # tópicos individuais de temperatura e falhas (para Monitoramento de Falhas)
        self.topic_temp   = f"atr/{self.id}/sensor/i_temperatura"
        self.topic_fele   = f"atr/{self.id}/sensor/i_falha_eletrica"
        self.topic_fhid   = f"atr/{self.id}/sensor/i_falha_hidraulica"

        # assina comandos de simulação (falhas, reset, etc)
        self.client.message_callback_add(self.topic_cmd, self._on_cmd)
        self.client.subscribe(self.topic_cmd, qos=1)

        # assina atuadores do controle de navegação C++
        self.client.message_callback_add(self.topic_act, self._on_act)
        self.client.subscribe(self.topic_act, qos=1)

        # log inicial
        self._pub_log(f"caminhão {self.id}: criado")

    # ---------- publicação ----------

    def _pub_log(self, text: str):
        self.client.publish(self.topic_log, text, qos=1)

    def _pub_fault_and_temp(self):
        """
        Publica temperatura e flags de falha em tópicos individuais,
        no formato esperado pelo módulo de Monitoramento de Falhas.
        """
        # Temperatura como inteiro (°C)
        temp_int = int(round(self.temp))
        self.client.publish(self.topic_temp, str(temp_int), qos=1)

        # Falhas como "0"/"1"
        self.client.publish(self.topic_fele, "1" if self.f_eletrica   else "0", qos=1)
        self.client.publish(self.topic_fhid, "1" if self.f_hidraulica else "0", qos=1)

    def _pub_sensor_once(self):
        """
        Publica pacote de sensores brutos com RUÍDO padronizado em um único JSON,
        compatível com tarefa_tratamento_sensores.cpp.
        """
        pos_x = self.x + random.gauss(0.0, POS_NOISE_STD)
        pos_y = self.y + random.gauss(0.0, POS_NOISE_STD)
        ang   = self.ang + random.gauss(0.0, ANG_NOISE_STD)
        temp  = self.temp + random.gauss(0.0, TEMP_NOISE_STD)

        def r3(v):
            return round(v, 3)   # 3 casas decimais

        payload = {
            "truck_id": self.id,
            "seq": self.seq,
            "ts": time.time(),
            "i_posicao_x": r3(pos_x),
            "i_posicao_y": r3(pos_y),
            "i_angulo_x": r3(ang),
            "i_temperatura": r3(temp),
            "i_falha_eletrica": self.f_eletrica,
            "i_falha_hidraulica": self.f_hidraulica,
            "dt": self.dt,
        }
        self.seq += 1

        # Publicação do pacote bruto (posição + temperatura + falhas)
        self.client.publish(self.topic_sensor, json.dumps(payload), qos=1)

        # Publica também nos tópicos individuais consumidos por tarefa_monitoramento_falhas
        self._pub_fault_and_temp()

    # ---------- comandos de simulação ----------

    def _on_cmd(self, _c, _u, msg):
        try:
            cmd = json.loads(msg.payload.decode())
            c = cmd.get("cmd", "").strip()

            with self._lock:
                # IMPORTANTE: comando de ruído foi removido (ruído é fixo)
                # if c == "set_noise":  --> NÃO EXISTE MAIS

                if c == "set_fault":
                    # Injeção manual de falhas em caminhão específico
                    if "eletrica" in cmd:
                        self.f_eletrica = bool(cmd["eletrica"])
                    if "hidraulica" in cmd:
                        self.f_hidraulica = bool(cmd["hidraulica"])
                    self._pub_log(
                        f"caminhão {self.id}: falhas -> eletrica={self.f_eletrica}, "
                        f"hidraulica={self.f_hidraulica}"
                    )
                    # Atualiza imediatamente os tópicos de falha/temperatura
                    self._pub_fault_and_temp()

                elif c == "clear_faults":
                    self.f_eletrica = False
                    self.f_hidraulica = False
                    self._pub_log(f"caminhão {self.id}: falhas limpas")
                    self._pub_fault_and_temp()

                elif c == "temp_step":
                    delta = float(cmd.get("delta", 0.0))
                    self.temp += delta
                    self._pub_log(
                        f"caminhão {self.id}: temperatura {round(self.temp,1)}°C"
                    )
                    # Publica sensores + falhas/temperatura atualizados
                    self._pub_sensor_once()

                elif c == "reset_position":
                    self.x = float(cmd.get("x", 0.0))
                    self.y = float(cmd.get("y", 0.0))
                    self.ang = float(cmd.get("ang", 0.0))
                    self.v = 0.0
                    self.o_aceleracao = 0.0
                    self.o_direcao = self.ang
                    self._last_cell = (round_i(self.x), round_i(self.y))
                    self._pub_log(
                        f"caminhão {self.id}: posição ({round_i(self.x)},{round_i(self.y)})"
                    )
                    self._pub_sensor_once()

                elif c == "stop":
                    self.o_aceleracao = 0.0
                    self.v = 0.0
                    self._pub_log(
                        f"caminhão {self.id}: stop em ({round_i(self.x)},{round_i(self.y)})"
                    )
                    self._pub_sensor_once()

                # (intencionalmente NÃO há 'setpoint' aqui:
                #  setpoints/planejamento são responsabilidade do C++)

        except Exception as e:
            print(f"[SIM CMD {self.id}] erro:", e)

    # ---------- atuadores vindos do C++ ----------

    def _on_act(self, _c, _u, msg):
        try:
            data = json.loads(msg.payload.decode())
            with self._lock:
                if "o_aceleracao" in data:
                    self.o_aceleracao = float(data["o_aceleracao"])
                if "o_direcao" in data:
                    self.o_direcao = float(data["o_direcao"])
        except Exception as e:
            print(f"[SIM ACT {self.id}] erro:", e)

    # ---------- dinâmica ----------

    def _step(self):
        # aplica aceleração percentual em [-A_MAX, A_MAX]
        a = (self.o_aceleracao / 100.0) * A_MAX
        if a > A_MAX:
            a = A_MAX
        if a < -A_MAX:
            a = -A_MAX

        # integra velocidade
        self.v += a * self.dt
        self.v *= FRIC  # atrito simples

        # limita velocidade
        if self.v < 0.0:
            self.v = 0.0
        if self.v > V_MAX:
            self.v = V_MAX

        # direção: assumir o_direcao como ângulo absoluto do veículo
        # (o controle de navegação C++ decide esse valor)
        self.ang = float(self.o_direcao)
        # normaliza
        while self.ang > 180.0:
            self.ang -= 360.0
        while self.ang < -180.0:
            self.ang += 360.0

        # integra posição
        ang_rad = math.radians(self.ang)
        self.x += self.v * self.dt * math.cos(ang_rad)
        self.y += self.v * self.dt * math.sin(ang_rad)

        # dinâmica simplificada de temperatura:
        # - aumenta um pouco com a velocidade
        # - resfria lentamente quando parado
        self.temp += 0.01 * self.v * self.dt
        if self.v < 0.1:
            self.temp -= 0.005 * self.dt
        self.temp = max(-50.0, min(self.temp, 250.0))

        # publica somente em mudança de célula (reduz tráfego)
        cell_now = (round_i(self.x), round_i(self.y))
        if cell_now != self._last_cell:
            self._last_cell = cell_now
            self._pub_log(
                f"caminhão {self.id} deslocando ({cell_now[0]},{cell_now[1]}) , "
                f"angulo: {round(self.ang,1)}°"
            )
            

    # ---------- loop da thread ----------

    def run(self):
        while not self._stop:
            t0 = time.time()
            with self._lock:
                self._step()
                self._pub_sensor_once()
            dt_real = time.time() - t0
            time.sleep(max(0.0, self.dt - dt_real))

    def stop(self):
        self._stop = True


# ==========================
# Supervisão / gerência
# ==========================

def start_simulator(broker_host: str = "localhost", trucks=()):
    """
    Inicia o simulador de mina.

    :param broker_host: host do broker MQTT (normalmente 'localhost' dentro do container)
    :param trucks: tupla/lista de IDs iniciais. Use () para começar vazio.
    """
    client = mqtt.Client(
        client_id=f"sim_{int(time.time())}",
        clean_session=True,
    )
    client.connect(broker_host, 1883, 60)
    client.loop_start()

    sims = []

    # cria caminhões iniciais
    for tid in trucks:
        sim = TruckSim(client, str(tid), hz=1.0 / DT_DEFAULT)
        sims.append(sim)
        th = threading.Thread(target=sim.run, daemon=True)
        th.start()

    # callback SPAWN dinâmico
    def on_spawn(_c, _u, msg):
        try:
            data = json.loads(msg.payload.decode())
            if data.get("cmd") == "spawn" and "truck_id" in data:
                tid = str(data["truck_id"])
                for s in sims:
                    if s.id == tid:
                        client.publish(
                            f"atr/{tid}/sim/log",
                            f"caminhão {tid}: já existe",
                            qos=1,
                        )
                        return
                sim = TruckSim(client, tid, hz=1.0 / DT_DEFAULT)
                sims.append(sim)
                threading.Thread(target=sim.run, daemon=True).start()
        except Exception as e:
            print("[SPAWN] erro:", e)

    client.message_callback_add("atr/sim/spawn", on_spawn)
    client.subscribe("atr/sim/spawn", qos=1)

    # callback REMOVE dinâmico
    def on_remove(_c, _u, msg):
        print(f"DEBUG: [on_remove] callback chamado! payload: {msg.payload}")
        try:
            data = json.loads(msg.payload.decode())
            print(f"DEBUG: [on_remove] JSON recebido: {data}")
            print(f"DEBUG: [on_remove] caminhões ativos: {[s.id for s in sims]}")
            if data.get("cmd") == "remove":
                tids = data.get("truck_id")
                if isinstance(tids, str):
                    tids = [tids]
                for tid in tids:
                    tid = str(tid)
                    for sim in list(sims):
                        print(f"DEBUG: analisando sim.id={sim.id}, candidato para remoção={tid}")
                        if sim.id == tid:
                            print(f"DEBUG: Removendo caminhão {tid}")
                            sim.stop()
                            sims.remove(sim)
                            print(f"DEBUG: Lista de caminhões após remoção: {[s.id for s in sims]}")
                            client.publish(
                                f"atr/{tid}/sim/log",
                                f"caminhão {tid}: removido",
                                qos=1,
                            )
        except Exception as e:
            print("[REMOVE] erro:", e)

    client.message_callback_add("atr/sim/remove", on_remove)
    client.subscribe("atr/sim/remove", qos=1)
    print("DEBUG: [simulator] Registrado callback on_remove para atr/sim/remove")

    # callback LIST - listar todos caminhões ativos
    def on_list(_c, _u, msg):
        # Monta estado de todos caminhões ativos
        lista = []
        for s in sims:
            estado = {
                "id": s.id,
                "x": s.x,
                "y": s.y,
                "ang": s.ang,
                "temp": s.temp,
                "f_eletrica": s.f_eletrica,
                "f_hidraulica": s.f_hidraulica,
                "v": s.v,
            }
            lista.append(estado)
        # Publica lista única (JSON array) em tópico de resposta
        client.publish("atr/sim/list/response", json.dumps(lista), qos=1)
        print(f"[LIST] Respondeu lista de {len(lista)} caminhão(ões) em atr/sim/list/response")

    client.message_callback_add("atr/sim/list", on_list)
    client.subscribe("atr/sim/list", qos=1)
    return client, sims


# ==========================
# Execução direta
# ==========================

if __name__ == "__main__":
    broker = os.environ.get("BROKER_HOST", "localhost")
    # padrão: inicia com 1 caminhão; pode iniciar vazio usando trucks=()
    client, sims = start_simulator(broker_host=broker, trucks=())

    try:
        while True:
            time.sleep(1.0)
    except KeyboardInterrupt:
        print("\n[SIM] Encerrando simulador...")
        for s in sims:
            s.stop()
        time.sleep(0.5)
        client.loop_stop()
        client.disconnect()
