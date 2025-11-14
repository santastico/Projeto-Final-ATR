#include <iostream>
#include <string>
#include <chrono>
#include <thread>

#include "config.h"              // BROKER_ADDRESS, BROKER_PORT
#include "Notificador_Eventos.h" // TipoEvento, NotificadorEventos
#include "tarefas.h"             // protótipo de tarefa_monitoramento_falhas

#include <mqtt/async_client.h>

namespace atr {

// ------------------------------------------------------------------
// Constantes de MQTT
// ----------------------------------------------- ------------------
static std::string build_server_uri()
{
    std::string addr = BROKER_ADDRESS;

    // Remove tcp:// se já existir
    if (addr.rfind("tcp://", 0) == 0) {  
        addr = addr.substr(6);          // remove "tcp://"
    }

    return "tcp://" + addr + ":" + std::to_string(BROKER_PORT);
}


static const std::string TOPIC_TEMP     = "atr/+/fault/temperature";
static const std::string TOPIC_ELETRICA = "atr/+/fault/electrical";
static const std::string TOPIC_HIDRAUL  = "atr/+/fault/hydraulic";

static const int QOS = 1;

// ------------------------------------------------------------------
// Classe simples para monitorar as falhas via MQTT
// ------------------------------------------------------------------
class MonitorMQTT
{
public:
    MonitorMQTT(int caminhao_id, NotificadorEventos& notificador)
    : m_id(caminhao_id)
    , m_notif(notificador)
    , m_server_uri(build_server_uri())
    , m_client(m_server_uri, "monitor_" + std::to_string(caminhao_id))
    {
        std::cout << "[Monitor " << m_id << "] Iniciado." << std::endl;
    }

    // Tenta conectar e assinar os tópicos de falha
    bool conectar()
    {
        try {
            mqtt::connect_options connOpts;
            connOpts.set_clean_session(true);

            std::cout << "[Monitor " << m_id << "] Conectando em " 
                      << m_server_uri << "..." << std::endl;

            auto tok = m_client.connect(connOpts);
            tok->wait();

            // Assina todos os tópicos de falha
            m_client.subscribe(TOPIC_TEMP,     QOS)->wait();
            m_client.subscribe(TOPIC_ELETRICA, QOS)->wait();
            m_client.subscribe(TOPIC_HIDRAUL,  QOS)->wait();

            std::cout << "[Monitor " << m_id << "] Conectado e assinando tópicos de falhas."
                      << std::endl;

            return true;
        }
        catch (const mqtt::exception& e) {
            std::cerr << "[Monitor " << m_id
                      << "] ERRO MQTT na inicialização: " << e.what() << std::endl;
            return false;
        }
    }

    // Loop principal: consome mensagens e dispara eventos
    void loop()
    {
        using namespace std::chrono_literals;

        while (true) {
            try {
                auto msg = m_client.try_consume_message_for(100ms);
                if (!msg) {
                    // Sem mensagem: só dorme um pouquinho e volta
                    std::this_thread::sleep_for(50ms);
                    continue;
                }

                const std::string topic = msg->get_topic();
                const std::string payload = msg->to_string();

                if (topic.find("fault/temperature") != std::string::npos) {
                    processar_temperatura(payload);
                }
                else if (topic.find("fault/electrical") != std::string::npos) {
                    processar_eletrica(payload);
                }
                else if (topic.find("fault/hydraulic") != std::string::npos) {
                    processar_hidraulica(payload);
                }
            }
            catch (const mqtt::exception& e) {
                std::cerr << "[Monitor " << m_id
                          << "] Erro ao consumir mensagem: " << e.what() << std::endl;
                std::this_thread::sleep_for(200ms);
            }
        }
    }

private:
    int m_id;
    NotificadorEventos& m_notif;

    std::string m_server_uri;
    mqtt::async_client m_client;

    // ---- Tratamento das mensagens de falha (mantém tua lógica atual) ----
    void processar_temperatura(const std::string& payload)
    {
        // Exemplo de lógica: adapte para a tua enum / thresholds
        int temp = std::stoi(payload);

        if (temp > 90) {
            m_notif.disparar_evento(TipoEvento::DEFEITO_TERMICO);
        }
        else if (temp > 80) {
            m_notif.disparar_evento(TipoEvento::ALERTA_TERMICO);
        }
        else {
            m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
        }
    }

    void processar_eletrica(const std::string& payload)
    {
        int estado = std::stoi(payload);

        if (estado != 0) {
            m_notif.disparar_evento(TipoEvento::FALHA_ELETRICA);
        } else {
            m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
        }
    }

    void processar_hidraulica(const std::string& payload)
    {
        int estado = std::stoi(payload);

        if (estado != 0) {
            m_notif.disparar_evento(TipoEvento::FALHA_HIDRAULICA);
        } else {
            m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
        }
    }
};

// ------------------------------------------------------------------
// Função de entrada da thread
// ------------------------------------------------------------------
void tarefa_monitoramento_falhas(int id, NotificadorEventos& notificador)
{
    MonitorMQTT monitor(id, notificador);

    // Se não conectar, apenas sai da tarefa (não derruba o resto do sistema)
    if (!monitor.conectar()) {
        std::cerr << "[Monitor " << id
                  << "] Encerrando tarefa por erro de conexão MQTT." << std::endl;
        return;
    }

    monitor.loop();
}
} // namespace atr
