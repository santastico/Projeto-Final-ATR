/**
 * @file tarefa_monitoramento_falhas.cpp
 * @brief Implementação da thread Monitoramento de Falhas.
 */

#include "tarefas.h"
#include "Notificador_Eventos.h"
#include "config.h"

#include <mqtt/async_client.h>
#include <chrono>
#include <string>
#include <iostream>

namespace atr {

using Clock     = std::chrono::steady_clock;
using TimePoint = std::chrono::steady_clock::time_point;
using namespace std::chrono_literals;

struct FaultConfig {
    int alerta_on  = 95;
    int alerta_off = 90;
    int falha_on   = 120;
    int falha_off  = 115;
    std::chrono::milliseconds timeout{1000};
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
        std::string base = "caminhao/" + std::to_string(m_id) + "/sensores/";
        m_topico_temp = base + "i_temperatura";
        m_topico_elet = base + "i_falha_eletrica";
        m_topico_hidr = base + "i_falha_hidraulica";

        mqtt::connect_options opts;
        opts.set_clean_session(true);

        auto tok = m_client.connect(opts);
        tok->wait();
        std::cout << "[Monitor " << m_id << "] Conectado em " << m_server_uri << '\n';

        m_client.start_consuming();
        m_client.subscribe(m_topico_temp, 1)->wait();
        m_client.subscribe(m_topico_elet, 1)->wait();
        m_client.subscribe(m_topico_hidr, 1)->wait();

        m_last_msg = Clock::now();
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
        }
    }

    void step() {
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

    std::string m_server_uri;
    mqtt::async_client m_client;

    std::string m_topico_temp;
    std::string m_topico_elet;
    std::string m_topico_hidr;

    TimePoint m_last_msg{};
    bool m_alerta_termico   = false;
    bool m_falha_termica    = false;
    bool m_falha_eletrica   = false;
    bool m_falha_hidraulica = false;
    bool m_falha_sensor     = false;

    static std::string build_server_uri() {
        return "tcp://" + std::string(BROKER_ADDRESS) + ":" +
               std::to_string(BROKER_PORT);
    }

    void processar_temperatura(const std::string& payload) {
        int temp = 0;
        try {
            temp = std::stoi(payload);
        } catch (...) {
            return;
        }

        if (!m_falha_termica && temp > m_cfg.falha_on) {
            m_falha_termica = true;
            std::cout << "[Monitor " << m_id << "] DEFEITO: Temp " << temp << "°C\n";
            m_notif.disparar_evento(TipoEvento::DEFEITO_TERMICO);
        } else if (m_falha_termica && temp < m_cfg.falha_off) {
            m_falha_termica = false;
            m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
        }

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

void tarefa_monitoramento_falhas(int id, NotificadorEventos& notificador) {
    std::cout << "[Monitor " << id << "] Iniciado.\n";

    FaultConfig cfg;
    MonitorMQTT monitor(id, notificador, cfg);

    while (true) {
        monitor.step();
    }
}

} // namespace atr
