#pragma once

#include <string>
#include <mutex>
#include <condition_variable>
#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"
#include "config.h"

namespace atr {

// =====================================================================
// TRATAMENTO DE SENSORES
// =====================================================================

void tratamento_sensores(
    BufferCircular<std::string>* buffer_bruto,
    BufferCircular<std::string>* buffer_tratado,
    std::mutex&                  mtx_posicao_tratada,
    std::condition_variable&     cv_buffer_tratada,
    int                          caminhao_id);

void tarefa_tratamento_sensores_run(const std::string& broker_uri);

// =====================================================================
// PLANEJAMENTO DE ROTA
// =====================================================================

void planejamento_rota_config(
    BufferCircular<std::string>* buffer_tratada,
    std::mutex&                  mtx_tratada,
    BufferCircular<std::string>* buffer_setpoints,
    std::mutex*                  mtx_setpoints,
    std::condition_variable*     cv_setpoints_rota,
    int                          caminhao_id);

void tarefa_planejamento_rota_run(const std::string& broker_uri);

// =====================================================================
// COLETOR DE DADOS
// =====================================================================

void coletor_dados_config(
    BufferCircular<std::string>* buffer_tratada,
    std::mutex&                  mtx_tratada,
    std::condition_variable&     cv_tratada,
    int                          caminhao_id);

void tarefa_coletor_dados_run();

// =====================================================================
// CONTROLE DE NAVEGAÇÃO (SEM LEITURA DIRETA DE POSIÇÃO TRATADA)
// =====================================================================

void controle_navegacao_config(
    BufferCircular<std::string>* buffer_sp_rota,   // entrada: setpoints de rota
    std::mutex&                  mtx_sp_rota,
    std::condition_variable&     cv_sp_rota,       // acorda quando há SP novo
    BufferCircular<std::string>* buffer_sp_ctrl,   // saída: para lógica de comando
    std::mutex&                  mtx_sp_ctrl,
    NotificadorEventos&          notificador,
    int                          caminhao_id);

void tarefa_controle_navegacao_run();

// =====================================================================
// LÓGICA DE COMANDO
// =====================================================================

void logica_comando_config(
    BufferCircular<std::string>* buffer_setpoints,
    std::mutex&                  mtx_setpoints,
    BufferCircular<std::string>* buffer_estado,
    std::mutex&                  mtx_estado,
    NotificadorEventos&          notificador,
    int                          caminhao_id);

void tarefa_logica_comando_run(const std::string& broker_uri);

// =====================================================================
// MONITORAMENTO DE FALHAS
// =====================================================================

void tarefa_monitoramento_falhas(
    int                 caminhao_id,
    NotificadorEventos& notificador);

} // namespace atr