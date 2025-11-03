#ifndef TAREFAS_H
#define TAREFAS_H

// Forward declarations para evitar inclusões circulares
class BufferCircular;
class NotificadorEventos;

/**
 * @brief Declarações das 6 funções de tarefa que serão lançadas como threads
 */
void tarefa_tratamento_sensores(int id, BufferCircular& buffer);
void tarefa_monitoramento_falhas(int id, NotificadorEventos& notificador);
void tarefa_logica_comando(int id, BufferCircular& buffer, NotificadorEventos& notificador);
void tarefa_coletor_dados(int id, BufferCircular& buffer, NotificadorEventos& notificador);
void tarefa_controle_navegacao(int id, BufferCircular& buffer, NotificadorEventos& notificador);
void tarefa_planejamento_rota(int id, BufferCircular& buffer);

#endif