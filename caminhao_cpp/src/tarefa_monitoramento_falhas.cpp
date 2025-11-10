/**
 * @file tarefa_monitoramento_falhas.cpp
 * @brief Implementação da thread Monitoramento de Falhas.
 *
 * @objetivo Responsável por ler os dados brutos dos sensores de falha 
 * vindos do simulador, analisar esses dados (ex: verificar limites de 
 * temperatura) e disparar eventos de falha/alerta para as outras tarefas.
 *
 * @entradas (Inputs)
 * 1. MQTT (subscribe): Assina os tópicos "i_temperatura", "i_falha_hidraulica"
 * e "i_falha_eletrica".
 *
 * @saidas (Outputs)
 * 1. Notificador de Eventos (disparo): Dispara eventos 
 * (ex: "alerta_termico", "falha_termica") para as threads 
 * Logica de Comando, Controle de Navegação e Coletor de Dados.
 */

#include "tarefas.h"
#include "Notificador_Eventos.h"
#include "config.h"
#include <mosquittopp.h>
#include <atomic>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>

// Configurações de Histerese e Timeout
struct FaultConfig {
    int alerta_on = 95;  
    int alerta_off = 90;
    int falha_on = 120; 
    int falha_off = 115;
    std::chrono::milliseconds timeout{1000};
};

class MonitorMQTT : public mosqpp::mosquittopp {
public:
    MonitorMQTT(int id, NotificadorEventos& notificador, const FaultConfig& cfg)
        : mosqpp::mosquittopp(("monitor_" + std::to_string(id)).c_str()),
          m_id(id), m_notif(notificador), m_cfg(cfg) {
        
        // Define os tópicos específicos para este caminhão
        std::string base = "caminhao/" + std::to_string(m_id) + "/sensores/";
        m_topico_temp = base + "i_temperatura";
        m_topico_elet = base + "i_falha_eletrica";
        m_topico_hidr = base + "i_falha_hidraulica";

        connect_async(BROKER_ADDRESS.c_str(), BROKER_PORT, 60);
        loop_start(); // Usa thread interna do Mosquitto para rede
        
        // Inicializa watchdog
        auto now = std::chrono::steady_clock::now();
        m_last_msg = now;
    }

    ~MonitorMQTT() {
        loop_stop(true);
    }

    void on_connect(int rc) override {
        if (rc == 0) {
            std::cout << "[Monitor " << m_id << "] Conectado. Assinando sensores." << std::endl;
            subscribe(nullptr, m_topico_temp.c_str(), 1);
            subscribe(nullptr, m_topico_elet.c_str(), 1);
            subscribe(nullptr, m_topico_hidr.c_str(), 1);
        } else {
            std::cerr << "[Monitor " << m_id << "] Erro conexao MQTT: " << rc << std::endl;
        }
    }

    void on_message(const mosquitto_message* m) override {
        if (!m->topic) return;
        std::string topic = m->topic;
        std::string payload((char*)m->payload, m->payloadlen);
        m_last_msg = std::chrono::steady_clock::now(); // Reset watchdog

        if (topic == m_topico_temp) {
            processar_temperatura(payload);
        } else if (topic == m_topico_elet) {
            processar_eletrica(payload);
        } else if (topic == m_topico_hidr) {
            processar_hidraulica(payload);
        }
    }

    // Chamado periodicamente pela thread principal da tarefa
    void verificar_watchdog() {
        auto now = std::chrono::steady_clock::now();
        if (now - m_last_msg.load() > m_cfg.timeout) {
            if (!m_falha_sensor) {
                std::cerr << "[Monitor " << m_id << "] TIMEOUT DOS SENSORES!" << std::endl;
                m_falha_sensor = true;
                m_notif.disparar_evento(TipoEvento::FALHA_SENSOR_TIMEOUT);
            }
        } else {
            if (m_falha_sensor) {
                std::cout << "[Monitor " << m_id << "] Sensores recuperados." << std::endl;
                m_falha_sensor = false;
                m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
            }
        }
    }

private:
    int m_id;
    NotificadorEventos& m_notif;
    FaultConfig m_cfg;
    
    // Tópicos completos
    std::string m_topico_temp, m_topico_elet, m_topico_hidr;

    // Estado interno
    std::atomic<std::chrono::steady_clock::time_point> m_last_msg;
    bool m_alerta_termico = false;
    bool m_falha_termica = false;
    bool m_falha_eletrica = false;
    bool m_falha_hidraulica = false;
    bool m_falha_sensor = false;

    void processar_temperatura(const std::string& payload) {
        try {
            int temp = std::stoi(payload);
            // Lógica de Histerese para Falha Térmica (>120)
            if (!m_falha_termica && temp > m_cfg.falha_on) {
                m_falha_termica = true;
                std::cout << "[Monitor " << m_id << "] DEFEITO: Temp " << temp << "°C" << std::endl;
                m_notif.disparar_evento(TipoEvento::DEFEITO_TERMICO);
            } else if (m_falha_termica && temp < m_cfg.falha_off) {
                 m_falha_termica = false;
                 m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
            }

            // Lógica de Histerese para Alerta Térmico (>95) - só se não for falha
            if (!m_falha_termica) {
                if (!m_alerta_termico && temp > m_cfg.alerta_on) {
                    m_alerta_termico = true;
                    std::cout << "[Monitor " << m_id << "] ALERTA: Temp " << temp << "°C" << std::endl;
                    m_notif.disparar_evento(TipoEvento::ALERTA_TERMICO);
                } else if (m_alerta_termico && temp < m_cfg.alerta_off) {
                    m_alerta_termico = false;
                    m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
                }
            }
        } catch (...) {}
    }

    void processar_eletrica(const std::string& payload) {
        bool atual = (payload == "1" || payload == "true");
        if (atual != m_falha_eletrica) {
            m_falha_eletrica = atual;
            if (m_falha_eletrica) {
                std::cout << "[Monitor " << m_id << "] DEFEITO: Falha Eletrica!" << std::endl;
                m_notif.disparar_evento(TipoEvento::FALHA_ELETRICA);
            } else {
                m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
            }
        }
    }

    void processar_hidraulica(const std::string& payload) {
        bool atual = (payload == "1" || payload == "true");
        if (atual != m_falha_hidraulica) {
            m_falha_hidraulica = atual;
            if (m_falha_hidraulica) {
                 std::cout << "[Monitor " << m_id << "] DEFEITO: Falha Hidraulica!" << std::endl;
                 m_notif.disparar_evento(TipoEvento::FALHA_HIDRAULICA);
            } else {
                m_notif.disparar_evento(TipoEvento::NORMALIZACAO);
            }
        }
    }
};

// Função principal da tarefa
void tarefa_monitoramento_falhas(int id, NotificadorEventos& notificador) {
    std::cout << "[Monitor " << id << "] Iniciado." << std::endl;
    mosqpp::lib_init();
    {
        FaultConfig cfg;
        MonitorMQTT monitor(id, notificador, cfg);

        // Loop principal: mantém a thread viva e checa o watchdog
        while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            monitor.verificar_watchdog();
        }
    }
    mosqpp::lib_cleanup();
}
