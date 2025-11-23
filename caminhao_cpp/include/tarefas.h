#pragma once

#include <string>
#include <mutex>
#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"

namespace atr {

// configura ponteiros globais
void tratamento_sensores(BufferCircular<std::string>* buffer_bruto,
                         BufferCircular<std::string>* buffer_tratado,
                         std::mutex& mtx_posicao_tratada,
                         int caminhao_id);

// função que roda na thread
void tarefa_tratamento_sensores_run(const std::string& broker_uri);

// configuração
void leitura_posicao_config(BufferCircular<std::string>* buffer_bruta,
                            BufferCircular<std::string>* buffer_tratada,
                            std::mutex& mtx);

// thread
void tarefa_leitura_posicao_run();

void tarefa_monitoramento_falhas(int caminhao_id,
                                 NotificadorEventos& notificador);

// configuração da tarefa de controle de navegação
void controle_navegacao_config(BufferCircular<std::string>* buffer_tratada,
                               std::mutex& mtx,
                               NotificadorEventos& notificador);

// thread do controle de navegação
void tarefa_controle_navegacao_run();


} // namespace atr