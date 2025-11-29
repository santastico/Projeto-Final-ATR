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

// Posição tratada
static BufferCircular<std::string>* g_buffer_pos_tratada = nullptr;
static std::mutex*                  g_mtx_pos_tratada    = nullptr;

// Setpoints de rota (entrada do controle)
static BufferCircular<std::string>* g_buffer_setpoints_rota = nullptr;
static std::mutex*                  g_mtx_setpoints_rota    = nullptr;

// Saída do controle (para Lógica de Comando)
static BufferCircular<std::string>* g_buffer_ctrl_saida = nullptr;
static std::mutex*                  g_mtx_ctrl_saida    = nullptr;

static NotificadorEventos*          g_notificador_ptr   = nullptr;

static int                          g_caminhao_id       = 1;

// Flag para indicar defeito ativo (vinda via eventos)
static std::atomic<bool>           g_em_defeito{false};

// Para estimar velocidade aproximada
static bool                         g_tem_pos_anterior  = false;
static double                       g_ult_x             = 0.0;
static double                       g_ult_y             = 0.0;
static std::chrono::steady_clock::time_point g_t_ult;

// Últimos setpoints usados pelo controle (para não depender de novo SP a cada ciclo)
static double g_sp_vel_atual = 0.0;
static double g_sp_ang_atual = 0.0;

// Ganhos simples de controle (podem ser ajustados depois)
constexpr double KP_VEL = 5.0;   // ganho do controle de velocidade
constexpr double KP_ANG = 1.0;   // ganho do controle de ângulo

// ---------------------------------------------------------------------
// Configuração chamada pelo main
// ---------------------------------------------------------------------

void controle_navegacao_config(
    BufferCircular<std::string>* buffer_tratada,
    std::mutex&                  mtx_tratada,
    BufferCircular<std::string>* buffer_sp_rota,
    std::mutex&                  mtx_sp_rota,
    BufferCircular<std::string>* buffer_sp_ctrl,
    std::mutex&                  mtx_sp_ctrl,
    NotificadorEventos&          notificador)
{
    g_buffer_pos_tratada    = buffer_tratada;
    g_mtx_pos_tratada       = &mtx_tratada;

    g_buffer_setpoints_rota = buffer_sp_rota;
    g_mtx_setpoints_rota    = &mtx_sp_rota;

    g_buffer_ctrl_saida     = buffer_sp_ctrl;
    g_mtx_ctrl_saida        = &mtx_sp_ctrl;

    g_notificador_ptr       = &notificador;

    std::cout << "[controle_nav] Configurado (modo AUTOMATICO).\n";
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

    std::cout << "[controle_nav/eventos] Thread de eventos iniciada.\n";

    while (true) {
        TipoEvento ev = g_notificador_ptr->esperar_evento();

        switch (ev) {
        case TipoEvento::DEFEITO_TERMICO:
        case TipoEvento::FALHA_ELETRICA:
        case TipoEvento::FALHA_HIDRAULICA:
        case TipoEvento::FALHA_SENSOR_TIMEOUT:
            g_em_defeito.store(true);
            std::cout << "[controle_nav] DEFEITO ativo (evento).\n";
            break;

        case TipoEvento::NORMALIZACAO:
            g_em_defeito.store(false);
            std::cout << "[controle_nav] DEFEITO desligado (NORMALIZACAO).\n";
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
    if (!g_buffer_pos_tratada    || !g_mtx_pos_tratada ||
        !g_buffer_setpoints_rota || !g_mtx_setpoints_rota ||
        !g_buffer_ctrl_saida     || !g_mtx_ctrl_saida) {
        std::cerr << "[controle_nav] ERRO: tarefa nao configurada.\n";
        return;
    }

    // Thread para escutar eventos de falha
    std::thread th_eventos(thread_eventos_controle);
    th_eventos.detach();

    constexpr auto PERIODO_CTRL = std::chrono::milliseconds(100);

    std::cout << "[controle_nav] Thread iniciada (MODO AUTOMATICO).\n";

    while (true) {
        // ---------------------------------------------------------
        // 0) Se estiver em defeito, não gera novos comandos
        // ---------------------------------------------------------
        if (g_em_defeito.load()) {
            std::this_thread::sleep_for(PERIODO_CTRL);
            continue;
        }

        // ---------------------------------------------------------
        // 1) Lê posição tratada mais recente
        // ---------------------------------------------------------
        double x_atual   = g_ult_x;
        double y_atual   = g_ult_y;
        double ang_atual = 0.0;

        {
            std::lock_guard<std::mutex> lock(*g_mtx_pos_tratada);
            std::string dado_tratado;
            if (g_buffer_pos_tratada->ler(dado_tratado)) {
                try {
                    json j = json::parse(dado_tratado, nullptr, false);
                    if (!j.is_discarded()) {
                        x_atual   = j.value("f_posicao_x", 0.0);
                        y_atual   = j.value("f_posicao_y", 0.0);
                        ang_atual = j.value("f_angulo_x",  0.0);
                    }
                } catch (...) {
                    // Se der erro no parse, mantém últimos valores.
                }
            }
        }

        // ---------------------------------------------------------
        // 2) Estima velocidade pela variação de posição
        // ---------------------------------------------------------
        double vel_atual = 0.0;
        auto   t_now     = std::chrono::steady_clock::now();

        if (g_tem_pos_anterior) {
            double dx = x_atual - g_ult_x;
            double dy = y_atual - g_ult_y;
            double dist = std::sqrt(dx * dx + dy * dy);
            double dt   = std::chrono::duration_cast<std::chrono::duration<double>>(
                              t_now - g_t_ult
                          ).count();

            if (dt > 0.0) {
                vel_atual = dist / dt;
            }
        }

        g_ult_x            = x_atual;
        g_ult_y            = y_atual;
        g_t_ult            = t_now;
        g_tem_pos_anterior = true;

        // ---------------------------------------------------------
        // 3) Lê setpoint do Planejamento de Rota (se tiver)
        //    - Se não tiver nada novo, usa último setpoint
        // ---------------------------------------------------------
        {
            std::lock_guard<std::mutex> lock(*g_mtx_setpoints_rota);
            std::string dado_sp;
            if (g_buffer_setpoints_rota->retirar(dado_sp)) {
                try {
                    json j_sp = json::parse(dado_sp, nullptr, false);
                    if (!j_sp.is_discarded()) {
                        g_sp_vel_atual = j_sp.value("setpoint_velocidade",      0.0);
                        g_sp_ang_atual = j_sp.value("setpoint_posicao_angular", ang_atual);
                    }
                } catch (...) {
                    // setpoint inválido -> mantém antigos
                }
            }
        }

        // ---------------------------------------------------------
        // 4) Controle de velocidade -> setpoint_aceleracao
        // ---------------------------------------------------------
        double erro_vel = g_sp_vel_atual - vel_atual;
        double cmd_acel = KP_VEL * erro_vel;
        cmd_acel = std::clamp(cmd_acel, -100.0, 100.0);

        // ---------------------------------------------------------
        // 5) Controle de ângulo -> setpoint_soma_angular
        // ---------------------------------------------------------
        double erro_ang = g_sp_ang_atual - ang_atual;
        while (erro_ang > 180.0)  erro_ang -= 360.0;
        while (erro_ang < -180.0) erro_ang += 360.0;

        double cmd_soma_ang = KP_ANG * erro_ang;
        cmd_soma_ang = std::clamp(cmd_soma_ang, -180.0, 180.0);

        // ---------------------------------------------------------
        // 6) Monta JSON de saída para a Lógica de Comando
        // ---------------------------------------------------------
        json j_out;
        j_out["truck_id"]              = g_caminhao_id;
        j_out["setpoint_aceleracao"]   = cmd_acel;
        j_out["setpoint_soma_angular"] = cmd_soma_ang;

        const std::string saida = j_out.dump();

        {
            std::lock_guard<std::mutex> lock(*g_mtx_ctrl_saida);
            if (!g_buffer_ctrl_saida->escrever(saida)) {
                std::cerr << "[controle_nav] buffer_ctrl_saida CHEIO ao escrever saida.\n";
            }
        }

        // (se quiser log, descomenta)
        std::cout << "[controle_nav] vel=" << vel_atual
                   << " sp_vel=" << g_sp_vel_atual
                   << " acel=" << cmd_acel
                   << " ang=" << ang_atual
                   << " sp_ang=" << g_sp_ang_atual
                   << " soma_ang=" << cmd_soma_ang << "\n";

        std::this_thread::sleep_for(PERIODO_CTRL);
    }
}

} // namespace atr
