#include "tarefas.h"
#include "Buffer_Circular.h"

#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace atr {

using json = nlohmann::json;

// ---------------------------------------------------------------------
// Globais da tarefa
// ---------------------------------------------------------------------

static BufferCircular<std::string>* g_buffer_pos_tratada = nullptr;
static std::mutex*                  g_mtx_pos_tratada    = nullptr;

static BufferCircular<std::string>* g_buffer_setpoints   = nullptr;
static std::mutex*                  g_mtx_setpoints      = nullptr;

static int             g_caminhao_id       = 1;

// Destino final (setpoint_posicao_final) vindo da Gestão da Mina
static std::atomic<double> g_dest_x{0.0};
static std::atomic<double> g_dest_y{0.0};
static std::atomic<bool>   g_tem_destino{false};

// ---------------------------------------------------------------------
// Configuração (chamada pelo main.cpp)
// ---------------------------------------------------------------------

void planejamento_rota_config(
    BufferCircular<std::string>* buffer_tratada,
    std::mutex&     mtx_tratada,
    BufferCircular<std::string>* buffer_setpoints,
    std::mutex*     mtx_setpoints,
    int             caminhao_id)
{
    g_buffer_pos_tratada = buffer_tratada;
    g_mtx_pos_tratada    = &mtx_tratada;

    g_buffer_setpoints   = buffer_setpoints;
    g_mtx_setpoints      = mtx_setpoints;

    g_caminhao_id        = caminhao_id;

    std::cout << "[planejamento_rota] Configurado para caminhao_id = "
              << g_caminhao_id << std::endl;
}

// ---------------------------------------------------------------------
// Processa payload de setpoint_posicao_final vindo via MQTT
// ---------------------------------------------------------------------

static void processar_setpoint_final(const std::string& payload)
{
    try {
        json j = json::parse(payload, nullptr, false);
        if (j.is_discarded()) return;

        if (j.contains("x") && j.contains("y")) {
            double x = j["x"].get<double>();
            double y = j["y"].get<double>();

            g_dest_x.store(x);
            g_dest_y.store(y);
            g_tem_destino.store(true);

            std::cout << "[planejamento_rota] Novo destino para caminhao "
                      << g_caminhao_id << ": x=" << x << " y=" << y << "\n";
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[planejamento_rota] Erro ao processar setpoint_final: "
                  << e.what() << "\n";
    }
}

// ---------------------------------------------------------------------
// Thread principal da tarefa de Planejamento de Rota
// ---------------------------------------------------------------------

void tarefa_planejamento_rota_run(const std::string& broker_uri)
{
    if (!g_buffer_pos_tratada || !g_mtx_pos_tratada) {
        std::cerr << "[planejamento_rota] ERRO: tarefa nao configurada.\n";
        return;
    }

    const std::string client_id = "planejamento_rota_" + std::to_string(g_caminhao_id);
    const std::string topico_destino =
        "atr/" + std::to_string(g_caminhao_id) + "/setpoint_posicao_final";
    const std::string topico_posicao =
        "atr/" + std::to_string(g_caminhao_id) + "/posicao_inicial";

    mqtt::async_client cli(broker_uri, client_id);
    mqtt::connect_options opts;
    opts.set_clean_session(true);

    try {
        std::cout << "[planejamento_rota] Conectando em " << broker_uri << "...\n";
        cli.connect(opts)->wait();
        std::cout << "[planejamento_rota] Conectado.\n";

        cli.start_consuming();
        cli.subscribe(topico_destino, 1)->wait();
        std::cout << "[planejamento_rota] Assinado topico " << topico_destino << "\n";

        constexpr auto PERIODO_PLANEJ = std::chrono::milliseconds(500);

        std::string dado_tratado;

        while (true) {
            // 1) Verifica se chegou novo destino via MQTT (não bloqueante)
            auto msg = cli.try_consume_message_for(std::chrono::milliseconds(10));
            if (msg) {
                processar_setpoint_final(msg->to_string());
            }

            // 2) Lê uma amostra tratada do buffer (posição atual)
            bool tem_amostra = false;
            {
                std::lock_guard<std::mutex> lock(*g_mtx_pos_tratada);
                if (g_buffer_pos_tratada->retirar(dado_tratado)) {
                    tem_amostra = true;
                }
            }

            if (tem_amostra) {
                try {
                    json j = json::parse(dado_tratado, nullptr, false);
                    if (j.is_discarded()) {
                        continue;
                    }

                    const double x   = j.value("f_posicao_x", 0.0);
                    const double y   = j.value("f_posicao_y", 0.0);
                    const double ang = j.value("f_angulo_x", 0.0);

                    // 2.a) Publica posição atual para a Gestão da Mina
                    json pub_pos;
                    pub_pos["truck_id"] = g_caminhao_id;
                    pub_pos["x"]        = x;
                    pub_pos["y"]        = y;
                    pub_pos["ang"]      = ang;

                    auto msg_pos = mqtt::make_message(topico_posicao, pub_pos.dump());
                    msg_pos->set_qos(1);
                    cli.publish(msg_pos);

                    // 2.b) Calcula setpoints imediatos para o Controle
                    double sp_vel = 0.0;
                    double sp_ang = ang;

                    if (g_tem_destino.load()) {
                        const double dx   = g_dest_x.load() - x;
                        const double dy   = g_dest_y.load() - y;
                        const double dist = std::sqrt(dx * dx + dy * dy);

                        if (dist > 1.0) {
                            // Velocidade desejada (constante simples)
                            sp_vel = 10.0;

                            double ang_desejado = std::atan2(dy, dx) * 180.0 / M_PI;
                            double erro = ang_desejado - ang;

                            // Normaliza erro para [-180, 180]
                            while (erro > 180.0)  erro -= 360.0;
                            while (erro < -180.0) erro += 360.0;

                            sp_ang = ang + erro;
                        }
                        else {
                            // Chegou próximo do destino
                            sp_vel = 0.0;
                        }
                    }

                    if (g_buffer_setpoints && g_mtx_setpoints) {
                        json j_sp;
                        j_sp["truck_id"]             = g_caminhao_id;
                        j_sp["setpoint_velocidade"]  = sp_vel;
                        j_sp["setpoint_posicao_angular"] = sp_ang;

                        std::lock_guard<std::mutex> lock2(*g_mtx_setpoints);
                        if (!g_buffer_setpoints->escrever(j_sp.dump())) {
                            std::cerr << "[planejamento_rota] buffer_setpoints CHEIO.\n";
                        }
                    }
                }
                catch (const std::exception& e) {
                    std::cerr << "[planejamento_rota] Erro ao processar dado tratado: "
                              << e.what() << "\n";
                }
            }

            std::this_thread::sleep_for(PERIODO_PLANEJ);
        }

        cli.stop_consuming();
        cli.disconnect()->wait();
    }
    catch (const std::exception& e) {
        std::cerr << "[planejamento_rota] ERRO MQTT: " << e.what() << "\n";
    }

    std::cout << "[planejamento_rota] Thread encerrada.\n";
}

} // namespace atr
