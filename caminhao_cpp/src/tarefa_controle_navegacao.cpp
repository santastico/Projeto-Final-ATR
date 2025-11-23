#include "tarefas.h"
#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"

#include <iostream>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>

namespace atr {

// ====================================================================
// Globais usados pela tarefa
// ====================================================================

// Buffer de onde o controle lê (posição tratada / estado)
static BufferCircular<std::string>* g_buffer_controle_nav = nullptr;
static std::mutex*                  g_mtx_controle_nav    = nullptr;

// Notificador de eventos compartilhado com tarefa_monitoramento_falhas
static NotificadorEventos*          g_notificador_ctrl    = nullptr;

// Flag de defeito "travada": uma vez true, fica true até reiniciar
static std::atomic<bool>            g_defeito_latch{false};

// ====================================================================
// Thread interna de eventos: fica bloqueada em esperar_evento()
// e atualiza g_defeito_latch quando necessário.
// ====================================================================

static void thread_eventos_controle()
{
    if (!g_notificador_ctrl) {
        std::cerr << "[controle_nav/eventos] Notificador não configurado!\n";
        return;
    }

    while (true) {
        // Bloqueante: acorda quando o Monitoramento de Falhas dispara_evento(...)
        TipoEvento ev = g_notificador_ctrl->esperar_evento();

        switch (ev) {
        case TipoEvento::DEFEITO_TERMICO:
        case TipoEvento::FALHA_ELETRICA:
        case TipoEvento::FALHA_HIDRAULICA:
        case TipoEvento::FALHA_SENSOR_TIMEOUT:
            // Travamos o defeito de forma permanente (até reiniciar o processo)
            g_defeito_latch.store(true);
            std::cout << "[controle_nav/eventos] Entrou em DEFEITO (evento="
                      << static_cast<int>(ev) << ")\n";
            break;

        case TipoEvento::NORMALIZACAO:
            // Por enquanto só logamos; não limpamos o defeito.
            // Isso garante que o controle "veja" a falha.
            std::cout << "[controle_nav/eventos] NORMALIZACAO recebida "
                         "(mas defeito continua travado ate rearmar no futuro)\n";
            break;

        case TipoEvento::ALERTA_TERMICO:
            std::cout << "[controle_nav/eventos] ALERTA TERMICO recebido\n";
            break;

        case TipoEvento::NENHUM:
        default:

            break;
        }
    }
}

// ====================================================================
// Configuração da tarefa de Controle de Navegação
// ====================================================================

void controle_navegacao_config(BufferCircular<std::string>* buffer_tratada,
                               std::mutex& mtx,
                               NotificadorEventos& notificador)
{
    g_buffer_controle_nav = buffer_tratada;
    g_mtx_controle_nav    = &mtx;
    g_notificador_ctrl    = &notificador;

    // Inicia thread interna que escuta eventos de falha
    std::thread t_eventos(thread_eventos_controle);
    t_eventos.detach(); // rodará até o fim do processo

    std::cout << "[controle_nav] Tarefa configurada (modo automatico + eventos).\n";
}

// ====================================================================
// Thread principal da tarefa de Controle de Navegação
// ====================================================================

void tarefa_controle_navegacao_run()
{
    if (!g_buffer_controle_nav || !g_mtx_controle_nav || !g_notificador_ctrl) {
        std::cerr << "[controle_nav] ERRO: tarefa não configurada "
                  << "(run chamado antes do config?).\n";
        return;
    }

    constexpr auto PERIODO_CONTROLE = std::chrono::milliseconds(500);

    std::string dado;

    while (true) {
        bool tem_dado = false;

        // 1) Tenta ler uma mensagem do buffer (posição tratada / estado)
        {
            std::lock_guard<std::mutex> lock(*g_mtx_controle_nav);
            if (g_buffer_controle_nav->retirar(dado)) {
                tem_dado = true;
            }
        }

        if (tem_dado) {
            std::cout << "\n[controle_nav] Entrada para controle: "
                      << dado << "\n";
        }

        // 2) Verifica estado de defeito travado
        const bool defeito_ativo = g_defeito_latch.load();

        int cmd_acel = 0; // [-100, 100]
        int cmd_dir  = 0; // [-180, 180]

        if (defeito_ativo) {
            // Em defeito: zera comandos SEMPRE
            cmd_acel = 0;
            cmd_dir  = 0;
            std::cout << "[controle_nav] EM DEFEITO -> comandos zerados "
                      << "(acel=" << cmd_acel << ", dir=" << cmd_dir << ")\n";
        } else {
            // MODO AUTOMÁTICO SIMPLES: anda pra frente devagar e reto
            cmd_acel = 20;
            cmd_dir  = 0;
            std::cout << "[controle_nav] AUTO -> cmd_acel=" << cmd_acel
                      << " cmd_dir=" << cmd_dir << "\n";
        }

        std::this_thread::sleep_for(PERIODO_CONTROLE);
    }
}

} // namespace atr
