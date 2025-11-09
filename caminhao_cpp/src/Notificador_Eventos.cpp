/**
 * @file NotificadorEventos.cpp
 * @brief Declaração da classe NotificadorEventos.
 *
 * @objetivo Fornecer um mecanismo de sincronização thread-safe para 
 * que a tarefa de Monitoramento de Falhas possa "acordar" 
 * ou "notificar" as outras tarefas (Lógica, Controle, Coletor) 
 * instantaneamente sobre a ocorrência de um evento de falha.
 *
 * @mecanismo (Interno)
 * Esta classe encapsula um 'std::mutex' e uma 'std::condition_variable'
 * para implementar um padrão "publish-subscribe" de eventos.
 *
 * @entradas (Inputs) - Para a thread que espera
 * 1. Chamada de 'esperar_evento()' pelas threads assinantes 
 * (Lógica de Comando, Controle de Navegação, Coletor de Dados).
 *
 * @saidas (Outputs) - Para a thread que dispara
 * 1. Chamada de 'disparar_evento()' pela thread publicadora 
 * (Monitoramento de Falhas).
 */

#include "Notificador_Eventos.h"

NotificadorEventos::NotificadorEventos() {} // Construtor vazio

void NotificadorEventos::esperar_evento() {
    std::unique_lock<std::mutex> lock(m_mutex);
    // Espera até que disparar_evento() seja chamado
    m_cv.wait(lock);
}

void NotificadorEventos::disparar_evento() {
    // Acorda todas as threads que estão esperando
    m_cv.notify_all();
}