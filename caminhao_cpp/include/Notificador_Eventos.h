#ifndef NOTIFICADOR_EVENTOS_H
#define NOTIFICADOR_EVENTOS_H

#include <mutex>
#include <condition_variable>

/**
 * @file NotificadorEventos.h
 * @brief Declaração da classe NotificadorEventos.
 *
 * @objetivo Fornecer um mecanismo de sincronização thread-safe para 
 * que a tarefa de Monitoramento de Falhas possa "acordar" 
 * ou "notificar" as outras tarefas (Lógica, Controle, Coletor) 
 * instantaneamente sobre a ocorrência de um evento de falha.
 */
class NotificadorEventos {
public:
    NotificadorEventos();
    
    // Bloqueia a thread até que disparar_evento() seja chamado
    void esperar_evento();
    
    // Acorda todas as threads que estão esperando
    void disparar_evento();

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

#endif