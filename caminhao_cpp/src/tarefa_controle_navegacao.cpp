#include "tarefas.h" 
#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <algorithm>    // std::clamp

namespace atr {

using json = nlohmann::json;

// ---------------------------------------------------------------------
// Globais da tarefa
// ---------------------------------------------------------------------

// Setpoints de rota (entrada do controle)
static BufferCircular<std::string>* g_buffer_setpoints_rota = nullptr;
static std::mutex*                  g_mtx_setpoints_rota    = nullptr;
static std::condition_variable*     g_cv_setpoints_rota     = nullptr;

// Saída do controle (para Lógica de Comando)
static BufferCircular<std::string>* g_buffer_ctrl_saida = nullptr;
static std::mutex*                  g_mtx_ctrl_saida    = nullptr;

// Eventos de falha
static NotificadorEventos*          g_notificador_ptr   = nullptr;

// Identificação
static int                          g_caminhao_id       = 1;

// Flag para indicar defeito ativo
static std::atomic<bool>            g_em_defeito{false};

// Últimos setpoints recebidos
static double g_sp_vel_atual = 0.0;   // 0 = parado, 1 = andar
static double g_sp_ang_atual = 0.0;   // ângulo desejado (graus)

// ---------------------------------------------------------------------
// Configuração chamada pelo main
// ---------------------------------------------------------------------

void controle_navegacao_config(
    BufferCircular<std::string>* buffer_sp_rota,
    std::mutex&                  mtx_sp_rota,
    std::condition_variable&     cv_sp_rota,
    BufferCircular<std::string>* buffer_sp_ctrl,
    std::mutex&                  mtx_sp_ctrl,
    NotificadorEventos&          notificador,
    int                          caminhao_id)
{
    g_buffer_setpoints_rota = buffer_sp_rota;
    g_mtx_setpoints_rota    = &mtx_sp_rota;
    g_cv_setpoints_rota     = &cv_sp_rota;

    g_buffer_ctrl_saida     = buffer_sp_ctrl;
    g_mtx_ctrl_saida        = &mtx_sp_ctrl;

    g_notificador_ptr       = &notificador;
    g_caminhao_id           = caminhao_id;

    std::cout << "[controle_nav " << g_caminhao_id
              << "] Configurado (modo AUTOMATICO, dirigido por setpoints).\n";
}

// ---------------------------------------------------------------------
// Thread auxiliar para escutar eventos de falha
// ---------------------------------------------------------------------

static void thread_eventos_controle()
{
    if (!g_notificador_ptr) {
        std::cerr << "[controle_nav/eventos] Notificador nao configurado!\n";
        return;
    }

    std::cout << "[controle_nav/eventos " << g_caminhao_id
              << "] Thread de eventos iniciada.\n";

    while (true) {
        TipoEvento ev = g_notificador_ptr->esperar_evento();

        switch (ev) {
        case TipoEvento::DEFEITO_TERMICO:
        case TipoEvento::FALHA_ELETRICA:
        case TipoEvento::FALHA_HIDRAULICA:
        case TipoEvento::FALHA_SENSOR_TIMEOUT:
            g_em_defeito.store(true);
            std::cout << "[controle_nav " << g_caminhao_id
                      << "] DEFEITO ativo (evento).\n";
            break;

        case TipoEvento::NORMALIZACAO:
            g_em_defeito.store(false);
            std::cout << "[controle_nav " << g_caminhao_id
                      << "] DEFEITO desligado (NORMALIZACAO).\n";
            break;

        default:
            // ALERTA_TERMICO ou NENHUM -> ignorados aqui
            break;
        }
    }
}

// ---------------------------------------------------------------------
// Função principal da tarefa de Controle de Navegação
// ---------------------------------------------------------------------

void tarefa_controle_navegacao_run()
{
    if (!g_buffer_setpoints_rota || !g_mtx_setpoints_rota ||
        !g_cv_setpoints_rota     ||
        !g_buffer_ctrl_saida     || !g_mtx_ctrl_saida     ||
        !g_notificador_ptr) {
        std::cerr << "[controle_nav] ERRO: tarefa nao configurada.\n";
        return;
    }

    // Thread para escutar eventos de falha
    std::thread th_eventos(thread_eventos_controle);
    th_eventos.detach();

    std::cout << "[controle_nav " << g_caminhao_id
              << "] Thread iniciada (MODO AUTOMATICO, sem sleep de controle).\n";

    while (true) {
        // 0) Em defeito, não gera novos comandos (mantém último).
        if (g_em_defeito.load()) {
            // Pequeno sleep aqui é aceitável: estamos em estado de falha.
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        // 1) Espera até chegar NOVO setpoint de rota
        std::string dado_sp;
        {
            std::unique_lock<std::mutex> lock(*g_mtx_setpoints_rota);

            g_cv_setpoints_rota->wait(lock, [] {
                return !g_buffer_setpoints_rota->estaVazio();
            });

            if (!g_buffer_setpoints_rota->retirar(dado_sp)) {
                // Acordou mas não conseguiu retirar (raro) -> recomeça
                continue;
            }
        }

        // 2) Processa o setpoint recebido
        try {
            json j_sp = json::parse(dado_sp, nullptr, false);
            if (j_sp.is_discarded()) {
                continue;
            }

            g_sp_vel_atual = j_sp.value("setpoint_velocidade",      0.0);
            g_sp_ang_atual = j_sp.value("setpoint_posicao_angular", 0.0);
        }
        catch (...) {
            continue;
        }

        // 3) Traduz setpoints de rota em comandos de atuador (versão simples)
        //    - Se sp_vel > 0 => aceleração constante
        //    - sp_ang é usado diretamente como soma angular

        double cmd_acel;
        if (g_sp_vel_atual > 0.5) {
            cmd_acel = 30.0;    // acelera para andar
        } else {
            cmd_acel = -30.0;   // freio ativo quando queremos parar
        }
        double cmd_soma_ang = g_sp_ang_atual;

        cmd_acel     = std::clamp(cmd_acel, -100.0, 100.0);
        cmd_soma_ang = std::clamp(cmd_soma_ang, -180.0, 180.0);

        // 4) Monta JSON de saída para a Lógica de Comando
        json j_out;
        j_out["truck_id"]              = g_caminhao_id;
        j_out["setpoint_aceleracao"]   = cmd_acel;
        j_out["setpoint_soma_angular"] = cmd_soma_ang;

        const std::string saida = j_out.dump();

        {
            std::lock_guard<std::mutex> lock(*g_mtx_ctrl_saida);
            if (!g_buffer_ctrl_saida->escrever(saida)) {
                std::cerr << "[controle_nav " << g_caminhao_id
                          << "] buffer_ctrl_saida CHEIO ao escrever saida.\n";
            }
        }

        // 5) Log opcional (1 linha por setpoint recebido)
        std::cout << "[controle_nav " << g_caminhao_id << "] "
                  << "sp_vel="  << g_sp_vel_atual
                  << " acel="   << cmd_acel
                  << " sp_ang=" << g_sp_ang_atual
                  << " soma_ang=" << cmd_soma_ang << "\n";
    }
}

} // namespace atr