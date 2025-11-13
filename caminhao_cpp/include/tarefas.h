#pragma once
#include <string>

// As classes estão no namespace global (pelos seus headers atuais)
class BufferCircular;
class NotificadorEventos;

namespace atr {

// Espelha os tipos globais dentro de atr para não duplicar declarações
using ::BufferCircular;
using ::NotificadorEventos;

/**
 * @brief Thread do Tratamento de Sensores
 *  - Assina MQTT (atr/+/sensor/raw)
 *  - Filtra (média móvel) e publica no(s) buffer(es)
 */
void tarefa_tratamento_sensores_run(const std::string& broker = "localhost");

/**
 * @brief Demais tarefas do núcleo embarcado (mantidas em atr)
 */
void tarefa_monitoramento_falhas(int id, NotificadorEventos& notificador);
void tarefa_logica_comando(int id, BufferCircular& buffer, NotificadorEventos& notificador);
void tarefa_coletor_dados(int id, BufferCircular& buffer, NotificadorEventos& notificador);
void tarefa_controle_navegacao(int id, BufferCircular& buffer, NotificadorEventos& notificador);
void tarefa_planejamento_rota(int id, BufferCircular& buffer);
// vincula o buffer e o id local para a thread de sensores (chamar no main antes de criar a thread)
void tratamento_sensores(BufferCircular* buffer_ptr, int caminhao_id);


} // namespace atr
