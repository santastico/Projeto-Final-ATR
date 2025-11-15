/**
 * @file tarefa_logica_comando.cpp
 * @brief Implementação da thread Lógica de Comando.
 *
 * @objetivo É o "cérebro" de decisão central do caminhão. Esta tarefa:
 * 1. Define o estado do veículo (manual, automático, defeito).
 * 2. Processa os comandos (do operador em modo manual, ou do 
 * controle em modo automático).
 * 3. Determina os valores finais dos atuadores a serem enviados 
 * para o simulador.
 *
 * @entradas (Inputs)
 * 1. Buffer Circular (leitura): Lê os dados de posição, os comandos do 
 * operador (ex: "c_man", "c_acelera") e os valores de controle 
 * (ex: "velocidade").
 * 2. Notificador de Eventos (recebimento): Recebe eventos de falha 
 * disparados pelo Monitoramento de Falhas.
 *
 * @saidas (Outputs)
 * 1. Buffer Circular (escrita): Escreve os estados atuais do 
 * caminhão: "e_defeito" e "e_automatico".
 * 2. MQTT (publish): Publica os comandos finais dos atuadores 
 * "o_aceleracao" e "o_direcao".
 */

#include "tarefas.h"
#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"
#include "config.h"

#include <mqtt/async_client.h>

#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <algorithm>

namespace atr {

using namespace std::chrono_literals;

// ---------------------------------------------------------------------
// Classe auxiliar para publicar aceleração e direção via MQTT
// ---------------------------------------------------------------------
class PublicadorAtuadores {
public:
    explicit PublicadorAtuadores(int caminhao_id)
        : m_client(BROKER_ADDRESS, "logica_cmd_" + std::to_string(caminhao_id))
    {
        // Tópicos que o simulador vai assinar
        std::string base = "caminhao/" + std::to_string(caminhao_id) + "/atuadores/";
        m_topic_acel = base + "o_aceleracao";
        m_topic_dir  = base + "o_direcao";

        mqtt::connect_options opts;
        opts.set_clean_session(true);

        try {
            m_client.connect(opts)->wait();
            std::cout << "[Logica " << caminhao_id
                      << "] Conectado ao broker MQTT em " << BROKER_ADDRESS << "\n";
        }
        catch (const std::exception& e) {
            std::cerr << "[Logica " << caminhao_id
                      << "] ERRO ao conectar MQTT: " << e.what() << "\n";
        }
    }

    void publicar(int aceleracao, int direcao)
    {
        try {
            // Publica aceleração
            auto msg_acel = mqtt::make_message(m_topic_acel,
                                               std::to_string(aceleracao));
            msg_acel->set_qos(0);
            m_client.publish(msg_acel);

            // Publica direção
            auto msg_dir = mqtt::make_message(m_topic_dir,
                                              std::to_string(direcao));
            msg_dir->set_qos(0);
            m_client.publish(msg_dir);
        }
        catch (const std::exception& e) {
            std::cerr << "[Logica] ERRO ao publicar atuadores: "
                      << e.what() << "\n";
        }
    }

private:
    mqtt::async_client m_client;
    std::string m_topic_acel;
    std::string m_topic_dir;
};

// ---------------------------------------------------------------------
// Tarefa Lógica de Comando
// ---------------------------------------------------------------------
void tarefa_logica_comando(int id, BufferCircular& buffer, NotificadorEventos& notificador)
{
    std::cout << "[Logica " << id << "] Tarefa iniciada.\n";

    // Publicador de saída
    PublicadorAtuadores publicador(id);

    // Estados internos da máquina de estados
    bool e_defeito    = false;  // começa SEM defeito
    bool e_automatico = true;   // começa em modo automático

    // Comandos finais dos atuadores
    int cmd_acel = 0;           // -100 a 100
    int cmd_dir  = 0;           // -180 a 180

    while (true) {
        // -------------------------------------------------------------
        // 1) Verifica se há evento de falha pendente (NÃO bloqueante)
        // -------------------------------------------------------------
    
        TipoEvento evento = notificador.verificar_sem_bloqueio();

        if (evento != TipoEvento::NENHUM) {
            std::cout << "[Logica " << id << "] Evento recebido (enum="
                      << static_cast<int>(evento) << ")\n";

            // Qualquer evento diferente de NENHUM coloca o sistema em defeito,
            // desliga o modo automático e zera atuadores.
            e_defeito    = true;
            e_automatico = false;
            cmd_acel     = 0;
            cmd_dir      = 0;
        }

        // -------------------------------------------------------------
        // 2) Lê a posição tratada mais recente do buffer
        // -------------------------------------------------------------
        BufferCircular::PosicaoData pos = buffer.get_posicao_recente();
        (void)pos; // evita warning de variável não usada

        // -------------------------------------------------------------
        // 3) Lógica de decisão dos atuadores
        // -------------------------------------------------------------
        if (e_defeito) {
            // Em defeito: parada segura
            cmd_acel = 0;
            cmd_dir  = 0;
        }
        else if (e_automatico) {
            // Modo automático: anda para frente em linha reta
            cmd_acel = 40;  // 40% de aceleração, 
            //vamos ja deixar definida a aceleração(valor_padrao) ou vamos usar do controle_navegação?
            cmd_dir  = 0;   // direção centralizada
        }
        else {
            // Modo manual (ainda não integrado com Interface Local):
            // por enquanto mantemos o veículo parado.
            cmd_acel = 0;
            cmd_dir  = 0;
        }

        // Garante limites
        cmd_acel = std::clamp(cmd_acel, -100, 100);
        cmd_dir  = std::clamp(cmd_dir,  -180, 180);

        // -------------------------------------------------------------
        // 4) Publica comandos para o simulador via MQTT
        // -------------------------------------------------------------
        publicador.publicar(cmd_acel, cmd_dir);

        // -------------------------------------------------------------
        // 5) Período da tarefa (~20 Hz -> 50 ms)
        // -------------------------------------------------------------
        std::this_thread::sleep_for(50ms);
    }
}

} // namespace atr
