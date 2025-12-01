// tarefa_monitoramento_falhas.cpp
#include <iostream>
#include <string>
#include <chrono>
#include <mqtt/async_client.h>
#include "Notificador_Eventos.h"
#include "tarefas.h"
#include "config.h" // Inclui para usar obter_broker_uri()

namespace atr {

void tarefa_monitoramento_falhas(int caminhao_id, NotificadorEventos& notificador)
{
    using namespace std::chrono_literals;

    // 1) Endereço do broker MQTT (agora dinâmico via config.h)
    const std::string broker_uri = obter_broker_uri();

    // 2) Cria o cliente MQTT com um client_id único para evitar conflitos
    std::string client_id = "monitor_falhas_" + std::to_string(caminhao_id);
    mqtt::async_client cli(broker_uri, client_id);

    // Variáveis de estado para evitar flood de eventos de Normalização
    bool estado_alerta_termico = false;
    bool estado_defeito_termico = false;
    bool estado_falha_eletrica = false;
    bool estado_falha_hidraulica = false;

    // ---------------- Conexão MQTT ----------------
    try {
        mqtt::connect_options opts;
        opts.set_clean_session(true);

        std::cout << "[Monitor " << caminhao_id << "] Conectando em "
                  << broker_uri << "...\n";

        cli.connect(opts)->wait();

        // modo "consumer" para usar try_consume_message_for()
        cli.start_consuming();

        std::cout << "[Monitor " << caminhao_id << "] Conectado.\n";
    }
    catch (const mqtt::exception& e) {
        std::cerr << "[Monitor " << caminhao_id
                  << "] ERRO ao conectar: " << e.what() << "\n";
        return; // não derruba o processo todo, só essa tarefa
    }

    // ---------------- Tópicos dos sensores ----------------
    // Segue o padrão atr/{id}/sensor/...
    std::string base = "atr/" + std::to_string(caminhao_id) + "/sensor/";
    std::string topic_temp     = base + "i_temperatura";
    std::string topic_eletrica = base + "i_falha_eletrica";
    std::string topic_hidraul  = base + "i_falha_hidraulica";

    try {
        cli.subscribe(topic_temp,     1)->wait();
        cli.subscribe(topic_eletrica, 1)->wait();
        cli.subscribe(topic_hidraul,  1)->wait();

        std::cout << "[Monitor " << caminhao_id << "] Assinando:\n"
                  << "  - " << topic_temp     << "\n"
                  << "  - " << topic_eletrica << "\n"
                  << "  - " << topic_hidraul  << "\n";
    }
    catch (const mqtt::exception& e) {
        std::cerr << "[Monitor " << caminhao_id
                  << "] ERRO ao assinar tópicos: " << e.what() << "\n";
        return;
    }

    // ---------------- Loop principal ----------------
    while (true) {
        mqtt::const_message_ptr msg;

        // Espera no máx. 200 ms por uma mensagem
        if (!cli.try_consume_message_for(&msg, 200ms) || !msg)
            continue;

        const std::string topic   = msg->get_topic();
        const std::string payload = msg->to_string();

        // --------- Temperatura ---------
        if (topic == topic_temp) {
            try {
                int temp = std::stoi(payload);

                if (temp > 120) {
                    if (!estado_defeito_termico) {
                        notificador.disparar_evento(TipoEvento::DEFEITO_TERMICO);
                        std::cout << "[Monitor " << caminhao_id << "] DEFEITO TERMICO (T=" << temp << "°C)\n";
                        estado_defeito_termico = true;
                    }
                }
                else if (temp > 95) {
                    if (!estado_alerta_termico) {
                        notificador.disparar_evento(TipoEvento::ALERTA_TERMICO);
                        std::cout << "[Monitor " << caminhao_id << "] ALERTA TERMICO (T=" << temp << "°C)\n";
                        estado_alerta_termico = true;
                    }
                    estado_defeito_termico = false; // Saiu do defeito
                }
                else {
                    // Se estava em alerta ou defeito, normalizou
                    if (estado_alerta_termico || estado_defeito_termico) {
                        notificador.disparar_evento(TipoEvento::NORMALIZACAO);
                        std::cout << "[Monitor " << caminhao_id << "] Temp normalizada (T=" << temp << "°C)\n";
                        estado_alerta_termico = false;
                        estado_defeito_termico = false;
                    }
                }
            }
            catch (...) {
                std::cerr << "[Monitor " << caminhao_id
                          << "] Temperatura inválida: '" << payload << "'\n";
            }
        }
        // --------- Falha elétrica ---------
        else if (topic == topic_eletrica) {
            bool falha = (payload == "1" || payload == "true");

            if (falha) {
                if (!estado_falha_eletrica) {
                    notificador.disparar_evento(TipoEvento::FALHA_ELETRICA);
                    std::cout << "[Monitor " << caminhao_id << "] FALHA ELETRICA detectada.\n";
                    registrar_evento_log(caminhao_id, "falha", "FALHA_ELETRICA=1");
                    estado_falha_eletrica = true;
                }
            } else {
                if (estado_falha_eletrica) {
                    notificador.disparar_evento(TipoEvento::NORMALIZACAO);
                    std::cout << "[Monitor " << caminhao_id << "] Falha eletrica normalizada.\n";
                    registrar_evento_log(caminhao_id, "falha", "FALHA_ELETRICA=0");
                    estado_falha_eletrica = false;
                }
            }
        }
        // --------- Falha hidráulica ---------
        else if (topic == topic_hidraul) {
            bool falha = (payload == "1" || payload == "true");

            if (falha) {
                if (!estado_falha_hidraulica) {
                    notificador.disparar_evento(TipoEvento::FALHA_HIDRAULICA);
                    std::cout << "[Monitor " << caminhao_id << "] FALHA HIDRAULICA detectada\n";
                    estado_falha_hidraulica = true;
                }
            } else {
                if (estado_falha_hidraulica) {
                    notificador.disparar_evento(TipoEvento::NORMALIZACAO);
                    std::cout << "[Monitor " << caminhao_id << "] Falha hidraulica normalizada\n";
                    estado_falha_hidraulica = false;
                }
            }
        }
    }
}

} // namespace atr
