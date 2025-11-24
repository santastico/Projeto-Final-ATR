#pragma once

#include <string>
#include <mutex>
#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"
#include "config.h"

namespace atr {

// configura ponteiros globais
void tratamento_sensores(BufferCircular<std::string>* buffer_bruto,
                         BufferCircular<std::string>* buffer_tratado,
                         std::mutex& mtx_posicao_tratada,
                         int caminhao_id);

// função que roda na thread
void tarefa_tratamento_sensores_run(const std::string& broker_uri);

// Planejamento de Rota
void planejamento_rota_config(
    BufferCircular<std::string>* buffer_tratada,
    std::mutex&     mtx_tratada,
    BufferCircular<std::string>* buffer_setpoints,
    std::mutex*     mtx_setpoints,
    int             caminhao_id);

// configuração
void leitura_posicao_config(BufferCircular<std::string>* buffer_bruta,
                            BufferCircular<std::string>* buffer_tratada,
                            std::mutex& mtx);


// thread do controle de navegação
void tarefa_controle_navegacao_run();
// thread
void tarefa_leitura_posicao_run();

void tarefa_monitoramento_falhas(int caminhao_id,
                                 NotificadorEventos& notificador);

// configuração da tarefa de controle de navegação
void controle_navegacao_config(BufferCircular<std::string>* buffer_tratada,
                               std::mutex& mtx,
                               NotificadorEventos& notificador);

// thread do planejamento de rota
void tarefa_planejamento_rota_run(const std::string& broker_uri);


} // namespace atr