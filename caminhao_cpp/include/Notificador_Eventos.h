#ifndef NOTIFICADOR_EVENTOS_H
#define NOTIFICADOR_EVENTOS_H

#include <mutex>
#include <condition_variable>

/**
 * @file Notificador_Eventos.h
 * @brief Declaração da classe NotificadorEventos.
 *
 * @objetivo Fornecer um mecanismo de sincronização thread-safe para 
 * que a tarefa de Monitoramento de Falhas possa "acordar" 
 * ou "notificar" as outras tarefas (Lógica, Controle, Coletor) 
 * instantaneamente sobre a ocorrência de um evento de falha.
 * Define os tipos de eventos que podem ocorrer no sistema.
 * Isso permite que a Lógica de Comando saiba exatamente qual foi a falha.
 */
namespace atr {
enum class TipoEvento {
    NENHUM,
    ALERTA_TERMICO,       // T > 95°C
    DEFEITO_TERMICO,      // T > 120°C
    FALHA_ELETRICA,       // i_falha_eletrica = true
    FALHA_HIDRAULICA,     // i_falha_hidraulica = true
    FALHA_SENSOR_TIMEOUT, // sensores pararam de responder
    NORMALIZACAO          // sistema voltou ao normal
};

class NotificadorEventos {
public:
    NotificadorEventos();
    
    /**
     * @brief Bloqueia a thread chamadora até que um evento ocorra.
     * @return O tipo do evento que causou o desbloqueio.
     */
    TipoEvento esperar_evento();
    
    /**
     * @brief Acorda as threads esperando e informa o tipo do evento.
     * @param tipo O tipo de evento a ser reportado.
     */
    void disparar_evento(TipoEvento tipo);

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    
    bool m_evento_ativo;
    TipoEvento m_tipo_atual;
};
}//namespace atr
#endif