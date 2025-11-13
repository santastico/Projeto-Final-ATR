/**
 * @file tarefa_monitoramento_falhas.cpp
 * @brief Implementação da thread Monitoramento de Falhas.
 *
 * @objetivo Responsável por ler os dados brutos dos sensores de falha 
 * vindos do simulador, analisar esses dados (ex: verificar limites de 
 * temperatura) e disparar eventos de falha/alerta para as outras tarefas.
 *
 * @entradas (Inputs)
 * 1. MQTT (subscribe): Assina os tópicos:
 *      caminhao/<id>/sensores/i_temperatura
 *      caminhao/<id>/sensores/i_falha_hidraulica
 *      caminhao/<id>/sensores/i_falha_eletrica
 *
 * @saidas (Outputs)
 * 1. Notificador de Eventos (disparo): Dispara eventos 
 *    (ex: "alerta_termico", "falha_termica", "falha_eletrica",
 *     "falha_hidraulica", "falha_sensor_timeout", "normalizacao").
 */
#include <string>
#include <mqtt/async_client.h>
#include <chrono>
#include <string>
#include <iostream>
#include <string>

const std::string BROKER_ADRESS = "tcp://localhost:1883";
const int BROKER_PORT = 1883;

namespace atr {

using Clock      = std::chrono::steady_clock;
using TimePoint  = std::chrono::steady_clock::time_point;
using namespace std::chrono_literals;

// Configurações de histerese e timeout (mesma lógica do código original)
struct FaultConfig {
    int alerta_on  = 95;    // sobe alerta térmico
    int alerta_off = 90;    // desce alerta térmico
    int falha_on   = 120;   // sobe falha térmica
    int falha_off  = 115;   // desce falha térmica
    std::chrono::milliseconds timeout{1000}; // timeout de sensores
};

class MonitorMQTT {
public:
    MonitorMQTT(int id, NotificadorEventos& notificador, const FaultConfig& cfg)
        : m_id(id),
          m_notif(notificador),
          m_cfg(cfg),
          m_server_uri(build_server_uri()),
          m_client(m_server_uri, "monitor_" + std::to_string(id))
    {
        // Monta tópicos conforme comentário original
        std::string base = "caminhao/" + std::to_string(m_id) + "/sensores/";
        m_topico_temp = base + "i_temperatura";
        m_topico_elet = base + "i_falha_eletrica";
        m_topico_hidr = base + "i_falha_hidraulica";

        // Conexão MQTT usando paho.mqtt.cpp
        mqtt::connect_options opts;
        opts.set_clean_session(true);

        try {
            auto tok = m_client.connect(opts);
            tok->wait();
            std::cout << "[Monitor " << m_id << "] Conectado em " << m_server_uri << '\n';

            // modo consumer: permite usar consume_message_for()
            m_client.start_consuming();

            // Assina os três tópicos de sensores
            m_client.subscribe(m_topico_temp, 1)->wait();
            m_client.subscribe(m_topico_elet, 1)->wait();
            m_client.subscribe(m_topico_hidr, 1)->wait();

            m_last_msg = Clock::now();
        }
        catch (const std::exception& e) {
            std::cerr << "[Monitor " << m_id << "] ERRO conexao MQTT: " << e.what() << '\n';
            throw;
        }
    }

    ~MonitorMQTT() {
        try {
            m_client.stop_consuming();
            if (m_client.is_connected()) {
                m_client.unsubscribe(m_topico_temp)->wait();
                m_client.unsubscribe(m_topico_elet)->wait();
                m_client.unsubscribe(m_topico_hidr)->wait();
                m_client.disconnect()->wait();
            }
        } catch (...) {
            // evitar exceção em destrutor
        }
    }

    // Um passo de processamento:
    // - consome mensagem (se houver)
    // - aplica mesma lógica de falhas/alertas
    // - verifica watchdog de timeout
    void step() {
        // tenta ler uma msg com timeout curto
        mqtt::const_message_ptr msg;
        if (m_client.try_consume_message_for(&msg, 100ms) && msg) {
            m_last_msg = Clock::now();

            const std::string topic   = msg->get_topic();
            const std::string payload = msg->to_string();

            if (topic == m_topico_temp) {
                processar_temperatura(payload);
            } else if (topic == m_topico_elet) {
                processar_eletrica(payload);
            } else if (topic == m_topico_hidr) {
                processar_hidraulica(payload);
            }
        }

        verificar_watchdog();
    }

private:
    int m_id;
    NotificadorEventos& m_notif;
    FaultConfig m_cfg;

    // MQTT
    std::string m_server_uri;
    mqtt::async_client m_client;

    // Tópicos
    std::string m_topico_temp;
    std::string m_topico_elet;
    std::string m_topico_hidr;

    // Estado interno (mesma lógica anterior)
    TimePoint m_last_msg{};
    bool m_alerta_termico   = false;
    bool m_falha_termica    = false;
    bool m_falha_eletrica   = false;
    bool m_falha_hidraulica = false;
    bool m_falha_sensor     = false;

    static std::string build_server_uri() {
        // BROKER_ADDRESS e BROKER_PORT devem vir de config.h
        return "tcp://" + std::string(BROKER_ADDRESS) + ":" + std::to_string(BROKER_PORT);
    }

    void processar_temperatura(const std::string& payload) {
        try {
            const int temp = std::stoi(payload);

            // Falha térmica (histerese)
            if (!m_falha_termica && temp > m_cfg.falha_on) {
                m_falha_termica = true;
                std::cout << "[Monitor " << m_id << "] DEFEITO: Temp " << temp << "°C\n";
                m_notif.disparar_evento(TipoEvento::DEFEITO_TERMICO);
            } else if (m_falha_termica && temp < m_cfg.falha_off) {
                m_falha_termica = false;
                m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
            }

            // Alerta térmico (histerese), só se não estiver em falha
            if (!m_falha_termica) {
                if (!m_alerta_termico && temp > m_cfg.alerta_on) {
                    m_alerta_termico = true;
                    std::cout << "[Monitor " << m_id << "] ALERTA: Temp " << temp << "°C\n";
                    m_notif.disparar_evento(TipoEvento::ALERTA_TERMICO);
                } else if (m_alerta_termico && temp < m_cfg.alerta_off) {
                    m_alerta_termico = false;
                    m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
                }
            }
        } catch (...) {
            // ignora payload inválido
        }
    }

    void processar_eletrica(const std::string& payload) {
        const bool atual = (payload == "1" || payload == "true");
        if (atual != m_falha_eletrica) {
            m_falha_eletrica = atual;
            if (m_falha_eletrica) {
                std::cout << "[Monitor " << m_id << "] DEFEITO: Falha Elétrica!\n";
                m_notif.disparar_evento(TipoEvento::FALHA_ELETRICA);
            } else {
                m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
            }
        }
    }

    void processar_hidraulica(const std::string& payload) {
        const bool atual = (payload == "1" || payload == "true");
        if (atual != m_falha_hidraulica) {
            m_falha_hidraulica = atual;
            if (m_falha_hidraulica) {
                std::cout << "[Monitor " << m_id << "] DEFEITO: Falha Hidráulica!\n";
                m_notif.disparar_evento(TipoEvento::FALHA_HIDRAULICA);
            } else {
                m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
            }
        }
    }

    void verificar_watchdog() {
        const auto now = Clock::now();
        const auto dt  = now - m_last_msg;

        if (dt > m_cfg.timeout) {
            if (!m_falha_sensor) {
                std::cerr << "[Monitor " << m_id << "] TIMEOUT DOS SENSORES!\n";
                m_falha_sensor = true;
                m_notif.disparar_evento(TipoEvento::FALHA_SENSOR_TIMEOUT);
            }
        } else {
            if (m_falha_sensor) {
                std::cout << "[Monitor " << m_id << "] Sensores recuperados.\n";
                m_falha_sensor = false;
                m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
            }
        }
    }
};


// ============================
// Função principal da tarefa
// ============================

void tarefa_monitoramento_falhas(int id, NotificadorEventos& notificador) {
    std::cout << "[Monitor " << id << "] Iniciado.\n";

    FaultConfig cfg;
    // se a conexão falhar, o construtor já mostra o erro
    MonitorMQTT monitor(id, notificador, cfg);

    while (true) {
        monitor.step();
        // pequeno descanso já embutido no consume_message_for + watchdog
    }
}

} // namespace atr
