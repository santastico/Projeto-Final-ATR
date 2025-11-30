// tarefa_logica_comando.cpp
#include "tarefas.h"
#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"

#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <algorithm> // std::clamp

namespace atr {

using json = nlohmann::json;

// ---------------------------------------------------------------------
// Globais da tarefa
// ---------------------------------------------------------------------

// Buffer que recebe os comandos do Controle de Navegação
static BufferCircular<std::string>* g_buffer_setpoints = nullptr;
static std::mutex*                  g_mtx_setpoints    = nullptr;

// Buffer de estado (vai para a interface_local)
static BufferCircular<std::string>* g_buffer_estado    = nullptr;
static std::mutex*                  g_mtx_estado       = nullptr;

// Notificador de eventos vindo do monitoramento_falhas
static NotificadorEventos*          g_notificador      = nullptr;

static int                          g_caminhao_id      = 1;

// Flags de estado
static std::atomic<bool>           g_em_defeito{false};
static std::atomic<bool>           g_modo_automatico{true}; // por enquanto só AUTO

// ---------------------------------------------------------------------
// Configuração (chamada no main.cpp)
// ---------------------------------------------------------------------

void logica_comando_config(
    BufferCircular<std::string>* buffer_setpoints,
    std::mutex&                  mtx_setpoints,
    BufferCircular<std::string>* buffer_estado,
    std::mutex&                  mtx_estado,
    NotificadorEventos&          notificador,
    int                          caminhao_id)
{
    g_buffer_setpoints = buffer_setpoints;
    g_mtx_setpoints    = &mtx_setpoints;

    g_buffer_estado    = buffer_estado;
    g_mtx_estado       = &mtx_estado;

    g_notificador      = &notificador;
    g_caminhao_id      = caminhao_id;

    std::cout << "[logica_comando] Configurado para caminhao_id = "
              << g_caminhao_id << "\n";
}

// ---------------------------------------------------------------------
// Thread auxiliar: escuta eventos de falha / normalização
// ---------------------------------------------------------------------

static void thread_eventos_logica()
{
    if (!g_notificador) {
        std::cerr << "[logica_comando/eventos] Notificador nao configurado!\n";
        return;
    }

    std::cout << "[logica_comando/eventos] Thread de eventos iniciada.\n";

    while (true) {
        TipoEvento ev = g_notificador->esperar_evento();

        switch (ev) {
        case TipoEvento::DEFEITO_TERMICO:
        case TipoEvento::FALHA_ELETRICA:
        case TipoEvento::FALHA_HIDRAULICA:
        case TipoEvento::FALHA_SENSOR_TIMEOUT:
            g_em_defeito.store(true);
            std::cout << "[logica_comando] DEFEITO ativo (evento).\n";
            break;

        case TipoEvento::NORMALIZACAO:
            g_em_defeito.store(false);
            std::cout << "[logica_comando] NORMALIZACAO recebida, limpando defeito.\n";
            break;

        default:
            // ALERTA_TERMICO ou NENHUM -> ignorados aqui
            break;
        }
    }
}

// ---------------------------------------------------------------------
// Thread principal: Lógica de Comando (modo automático)
// ---------------------------------------------------------------------

void tarefa_logica_comando_run(const std::string& broker_uri)
{
    if (!g_buffer_setpoints || !g_mtx_setpoints ||
        !g_buffer_estado    || !g_mtx_estado) {
        std::cerr << "[logica_comando] ERRO: tarefa nao configurada.\n";
        return;
    }

    // Thread que escuta eventos de falha
    std::thread th_eventos(thread_eventos_logica);
    th_eventos.detach();

    // 1) Configura MQTT
    std::string client_id = "logica_comando_" + std::to_string(g_caminhao_id);
    mqtt::async_client cli(broker_uri, client_id);

    mqtt::connect_options opts;
    opts.set_clean_session(true);

    try {
        std::cout << "[logica_comando] Conectando em " << broker_uri << "...\n";
        cli.connect(opts)->wait();
        std::cout << "[logica_comando] Conectado.\n";
    }
    catch (const mqtt::exception& e) {
        std::cerr << "[logica_comando] ERRO ao conectar: " << e.what() << "\n";
        return;
    }

    // Tópicos de atuação esperados pelo simulador
    std::string base        = "atr/" + std::to_string(g_caminhao_id) + "/";
    std::string topic_acel  = base + "o_aceleracao"; // [-100, 100]
    std::string topic_dir   = base + "o_direcao";    // [-180, 180]

    constexpr auto PERIODO = std::chrono::milliseconds(100);

    double ultimo_acel = 0.0;
    double ultimo_dir  = 0.0;

    std::cout << "[logica_comando] Thread iniciada (MODO AUTOMATICO).\n";

    while (true) {
        double cmd_acel = ultimo_acel;
        double cmd_dir  = ultimo_dir;

        // -------------------------------------------------------------
        // 1) Le um comando vindo do Controle de Navegação (se tiver)
        //    Só considera mensagens que tenham 'setpoint_aceleracao'
        //    == saídas do controle_navigation.
        // -------------------------------------------------------------
        std::string msg_sp;
        bool tem_sp = false;
        {
            std::lock_guard<std::mutex> lock(*g_mtx_setpoints);
            if (g_buffer_setpoints->retirar(msg_sp)) {
                tem_sp = true;
            }
        }

        if (tem_sp) {
            try {
                json j = json::parse(msg_sp, nullptr, false);
                if (!j.is_discarded() && j.contains("setpoint_aceleracao")) {

                    cmd_acel = j.value("setpoint_aceleracao", 0.0);

                    double soma_ang = j.value("setpoint_soma_angular", 0.0);
                    // O simulador espera direcao em graus [-180, 180]
                    cmd_dir = soma_ang;
                }
                // Se for uma mensagem do Planejamento de Rota (só setpoint_vel/ang),
                // ignoramos aqui.
            }
            catch (...) {
                // Em caso de erro, mantemos os últimos comandos válidos
            }
        }

        // -------------------------------------------------------------
        // 2) Aplica lógica de falha: se em defeito, zera comandos
        // -------------------------------------------------------------
        if (g_em_defeito.load()) {
            cmd_acel = 0.0;
            cmd_dir  = 0.0;
        }

        // -------------------------------------------------------------
        // 3) Publica nos tópicos MQTT do simulador
        // -------------------------------------------------------------
        try {
            auto msg1 = mqtt::make_message(
                topic_acel,
                std::to_string(static_cast<int>(cmd_acel))
            );
            msg1->set_qos(1);
            cli.publish(msg1);

            auto msg2 = mqtt::make_message(
                topic_dir,
                std::to_string(static_cast<int>(cmd_dir))
            );
            msg2->set_qos(1);
            cli.publish(msg2);
        }
        catch (const mqtt::exception& e) {
            std::cerr << "[logica_comando] ERRO ao publicar: "
                      << e.what() << "\n";
        }

        ultimo_acel = cmd_acel;
        ultimo_dir  = cmd_dir;

        // -------------------------------------------------------------
        // 4) Atualiza buffer de estado para a interface_local
        // -------------------------------------------------------------
        if (g_buffer_estado && g_mtx_estado) {
            json j_est;
            j_est["truck_id"]        = g_caminhao_id;
            j_est["modo_automatico"] = g_modo_automatico.load();
            j_est["em_defeito"]      = g_em_defeito.load();
            j_est["cmd_aceleracao"]  = cmd_acel;
            j_est["cmd_direcao"]     = cmd_dir;

            std::lock_guard<std::mutex> lock_est(*g_mtx_estado);
            // Se estiver cheio, simplesmente não grava (estado seguinte virá depois)
            g_buffer_estado->escrever(j_est.dump());
        }

        std::this_thread::sleep_for(PERIODO);
    }

    // Em operação normal, nunca chega aqui
    // (se quiser um dia encerrar a thread, pode colocar um break e desconectar)
    // cli.disconnect()->wait();
    // std::cout << "[logica_comando] Thread encerrada.\n";
}

} // namespace atr
