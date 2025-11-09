/**
 * @file tarefa_planejamento_rota.cpp
 * @brief Implementação da thread Planejamento de Rota.
 *
 * @objetivo Responsável por calcular a rota do caminhão. Lê a posição atual 
 * (do Buffer Circular) e o destino final (da Gestão da Mina via MQTT) 
 * e, com base nisso, define os setpoints imediatos (velocidade e ângulo) 
 * para a tarefa de Controle de Navegação.
 *
 * @entradas (Inputs)
 * 1. Buffer Circular (leitura): Lê as variáveis tratadas "i_pos_x", "i_pos_y", 
 * "i_angulo_x".
 * 2. MQTT (subscribe): Assina o tópico "setpoint_posicao_final" 
 * publicado pela Gestão da Mina.
 *
 * @saidas (Outputs)
 * 1. Buffer Circular (escrita): Escreve as variáveis 
 * "setpoint_velocidade" e "setpoint_posicao_angular".
 * 2. MQTT (publish): Publica no tópico "posicao_inicial" para 
 * informar a Gestão da Mina.
 */
#include "tarefas.h"
#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"

#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <cmath>
#include <iostream>
#include <string>
#include <semaphore.h>

namespace atr {

using json = nlohmann::json;

static double wrap_angle(double ang_deg) {
    while (ang_deg > 180.0) ang_deg -= 360.0;
    while (ang_deg < -180.0) ang_deg += 360.0;
    return ang_deg;
}

// Estrutura compartilhada entre a thread MQTT (callback) e a thread de planejamento
struct AlvoCompartilhado {
    double x = 0.0;
    double y = 0.0;
    bool ativo = false;
    std::mutex mtx;
    sem_t sem;  // semáforo usado para sinalizar novo destino
};

// ======================================
// Thread principal de planejamento de rota
// ======================================

void tarefa_planejamento_rota(int id, BufferCircular& buffer)
{
    const std::string broker   = "tcp://localhost:1883";
    const std::string clientId = "planner_" + std::to_string(id);
    const std::string topic_sp = "atr/" + std::to_string(id) + "/gestao/setpoint";
    const std::string topic_log= "atr/" + std::to_string(id) + "/planner/log";

    // Objeto compartilhado de sincronização
    AlvoCompartilhado alvo;
    sem_init(&alvo.sem, 0, 0);  // semáforo começa em 0

    // --- MQTT setup ---
    mqtt::async_client cli(broker, clientId);
    mqtt::connect_options connOpts;
    connOpts.set_clean_session(true);

    try {
        cli.connect(connOpts)->wait();
        std::cout << "[Planejamento " << id << "] Conectado ao broker.\n";
    } catch (const std::exception& e) {
        std::cerr << "[Planejamento " << id << "] ERRO conectando MQTT: " << e.what() << "\n";
        return;
    }

    // ---------- CALLBACK MQTT ----------
    class CallbackSetpoint : public virtual mqtt::callback {
        mqtt::async_client& cli_;
        AlvoCompartilhado& alvo_;
        std::string topic_log_;
    public:
        CallbackSetpoint(mqtt::async_client& c, AlvoCompartilhado& alvo, std::string log)
            : cli_(c), alvo_(alvo), topic_log_(std::move(log)) {}

        void message_arrived(mqtt::const_message_ptr msg) override {
            try {
                auto j = json::parse(msg->to_string());
                if (j.contains("x") && j.contains("y")) {
                    std::lock_guard<std::mutex> lk(alvo_.mtx);
                    alvo_.x = j["x"].get<double>();
                    alvo_.y = j["y"].get<double>();
                    alvo_.ativo = true;
                    sem_post(&alvo_.sem); // sinaliza nova rota
                    cli_.publish(topic_log_, "Novo destino recebido", 1, false);
                }
            } catch (std::exception& e) {
                std::cerr << "[Planejamento MQTT] erro: " << e.what() << "\n";
            }
        }
    };

    auto cb = std::make_shared<CallbackSetpoint>(cli, alvo, topic_log);
    cli.set_callback(*cb);
    cli.subscribe(topic_sp, 1)->wait();

    std::cout << "[Planejamento " << id << "] Thread iniciada.\n";

    // Constantes de controle
    const double V_MAX   = 2.0;
    const double KP_DIST = 0.8;
    const double KP_ANG  = 2.0;

    // Thread loop principal
    while (true) {
        try {
            // espera até haver um destino ativo
            {
                std::unique_lock<std::mutex> lk(alvo.mtx);
                if (!alvo.ativo) {
                    lk.unlock();
                    sem_wait(&alvo.sem);
                    continue;
                }
            }

            // lê posição tratada do buffer
            double x=0.0, y=0.0, ang=0.0;
            bool ok = buffer.lerPosicaoTratada(id, x, y, ang);
            if (!ok) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                continue;
            }

            // lê o alvo sob mutex
            double gx, gy;
            {
                std::lock_guard<std::mutex> lk(alvo.mtx);
                gx = alvo.x;
                gy = alvo.y;
            }

            // cálculo de erro e controle
            double dx = gx - x;
            double dy = gy - y;
            double dist = std::sqrt(dx*dx + dy*dy);

            double desired_ang = std::atan2(dy, dx) * 180.0 / M_PI;
            double ang_err = wrap_angle(desired_ang - ang);

            double sp_vel   = std::min(V_MAX, KP_DIST * dist);
            double sp_angulo= wrap_angle(ang + KP_ANG * ang_err);

            buffer.escreverSetpointsNavegacao(id, sp_vel, sp_angulo);

            // chegou ao destino?
            if (dist < 0.25 && std::fabs(ang_err) < 2.0) {
                std::lock_guard<std::mutex> lk(alvo.mtx);
                alvo.ativo = false;
                cli.publish(topic_log, "Destino atingido", 1, false);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 20 Hz

        } catch (const std::exception& e) {
            std::cerr << "[Planejamento " << id << "] erro loop: " << e.what() << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    }

    sem_destroy(&alvo.sem);
}

} // namespace atr

