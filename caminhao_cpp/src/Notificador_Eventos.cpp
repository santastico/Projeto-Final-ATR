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
#include <mutex>
#include <condition_variable>

NotificadorEventos::NotificadorEventos() 
    : m_evento_ativo(false), m_tipo_atual(TipoEvento::NENHUM) {}

TipoEvento NotificadorEventos::esperar_evento() {
    std::unique_lock<std::mutex> lock(m_mutex);
    
    // Predicado protege contra 'spurious wakeups' (acordar sem sinal real)
    m_cv.wait(lock, [this]{ return m_evento_ativo; });
    
    // Captura o evento e reseta o estado
    TipoEvento evento_recebido = m_tipo_atual;
    m_evento_ativo = false;
    m_tipo_atual = TipoEvento::NENHUM;
    
    return evento_recebido;
}

void NotificadorEventos::disparar_evento(TipoEvento tipo) {
    {
        // Bloqueia apenas o tempo suficiente para definir a flag
        std::lock_guard<std::mutex> lock(m_mutex);
        m_evento_ativo = true;
        m_tipo_atual = tipo;
    }
    // Notifica todas as threads interessadas (Logica, Controle, Coletor)
    m_cv.notify_all();
}