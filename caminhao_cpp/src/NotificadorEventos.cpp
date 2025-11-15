#include <mutex>
#include <condition_variable>
#include "NotificadorEventos.h"

namespace atr{
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
}//namespace atr