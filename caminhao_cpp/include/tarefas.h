#pragma once

#include <string>

class BufferCircular; // forward global (definida em outro header)

namespace atr {

class NotificadorEventos; // forward dentro do namespace atr

// Tratamento de sensores
void tratamento_sensores(BufferCircular* buffer_ptr, int caminhao_id);
void tarefa_tratamento_sensores_run(const std::string& broker = "localhost");

// Monitoramento de falhas
void tarefa_monitoramento_falhas(int id, NotificadorEventos& notificador);

// Demais tarefas (mantém a mesma ideia)
void tarefa_logica_comando(int id, BufferCircular& buffer, NotificadorEventos& notificador);
void tarefa_coletor_dados(int id, BufferCircular& buffer, NotificadorEventos& notificador);
void tarefa_controle_navegacao(int id, BufferCircular& buffer, NotificadorEventos& notificador);
void tarefa_planejamento_rota(int id, BufferCircular& buffer);

} // namespace atr
