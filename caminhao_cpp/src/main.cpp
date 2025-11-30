/**
 * @file main.cpp
 * @brief Ponto de entrada (entry point) do software embarcado do caminhão.
 */

#include <iostream>
#include <cstdlib>
#include <thread>
#include <string>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"
#include "tarefas.h"
#include "config.h"

int main() {
    // -----------------------------------------------------------------
    // 1. Identificação do Caminhão
    // -----------------------------------------------------------------
    int caminhao_id = 1;
    const char* env_id = std::getenv("CAMINHAO_ID");
    if (env_id) {
        try {
            caminhao_id = std::stoi(env_id);
        } catch (...) {
            std::cerr << "[Main] Erro ao ler CAMINHAO_ID, usando padrao 1.\n";
        }
    }
    
    std::cout << "==========================================\n";
    std::cout << " INICIANDO CAMINHAO EMBARCADO - ID: " << caminhao_id << "\n";
    std::cout << " Broker MQTT: " << obter_broker_uri() << "\n";
    std::cout << "==========================================\n";

    // -----------------------------------------------------------------
    // 2. Instanciação de Recursos Compartilhados
    // -----------------------------------------------------------------

    std::mutex mtx_posicao_tratada;
    std::condition_variable cv_buffer_tratada; // NOVO: notifica leitores
    
    BufferCircular<std::string> buffer_posicao_bruta(10);
    BufferCircular<std::string> buffer_posicao_tratada(100);
    // Navegação (planejamento -> controle)
    BufferCircular<std::string> buffer_setpoints_rota(50);
    std::mutex mtx_setpoints_rota;
    std::condition_variable cv_setpoints_rota;   // NOVO: acorda controle_nav


    // Saída do controle (controle -> lógica de comando)
    BufferCircular<std::string> buffer_setpoints_ctrl(50);
    std::mutex mtx_setpoints_ctrl;

    // Estado para interface local (lógica -> UI futura)
    BufferCircular<std::string> buffer_estado_logica(50);
    std::mutex mtx_estado_logica;
    
    atr::NotificadorEventos notificador;
    std::mutex mtx_setpoints_nav;

    // -----------------------------------------------------------------
    // 3. Configuração das Tarefas
    // -----------------------------------------------------------------

    // Tratamento de Sensores
    atr::tratamento_sensores(
        &buffer_posicao_bruta, 
        &buffer_posicao_tratada, 
        mtx_posicao_tratada,
        cv_buffer_tratada, 
        caminhao_id
    );

    // Planejamento de Rota
    atr::planejamento_rota_config(
        &buffer_posicao_tratada,
        mtx_posicao_tratada,
        &buffer_setpoints_rota,
        &mtx_setpoints_rota,
        &cv_setpoints_rota,
        caminhao_id
    );

    // Coletor de Dados (NOVO)
    atr::coletor_dados_config(
        &buffer_posicao_tratada,
        mtx_posicao_tratada,
        cv_buffer_tratada,
        caminhao_id
    );

    // Controle de Navegação
    atr::controle_navegacao_config(
        &buffer_setpoints_rota,    // lê SP da rota
        mtx_setpoints_rota,
        cv_setpoints_rota,         // acorda quando há SP novo
        &buffer_setpoints_ctrl,    // escreve saída para lógica
        mtx_setpoints_ctrl,
        notificador,
        caminhao_id
    );

    // Lógica de Comando
    atr::logica_comando_config(
        &buffer_setpoints_ctrl,    // lê saída do controle
        mtx_setpoints_ctrl,
        &buffer_estado_logica,     // escreve estado para UI local
        mtx_estado_logica,
        notificador,
        caminhao_id
    );

    // -----------------------------------------------------------------
    // 4. Lançamento das Threads
    // -----------------------------------------------------------------

    std::thread t_sens(
        atr::tarefa_tratamento_sensores_run,
        obter_broker_uri()
    );

    std::thread t_monitor(
        atr::tarefa_monitoramento_falhas,
        caminhao_id,
        std::ref(notificador)
    );

    std::thread t_plan(
        atr::tarefa_planejamento_rota_run,
        obter_broker_uri()
    );

    std::thread t_coletor(atr::tarefa_coletor_dados_run); // NOVO

    std::thread t_ctrl_nav(atr::tarefa_controle_navegacao_run);

    std::thread t_logica(
        atr::tarefa_logica_comando_run,
        obter_broker_uri()
    );

    std::cout << "[Main " << caminhao_id << "] Todas as threads iniciadas.\n";

    // -----------------------------------------------------------------
    // 5. Loop Principal
    // -----------------------------------------------------------------
    
    if (t_sens.joinable()) t_sens.join();
    if (t_monitor.joinable()) t_monitor.join();
    if (t_plan.joinable()) t_plan.join();
    if (t_coletor.joinable()) t_coletor.join();
    if (t_ctrl_nav.joinable()) t_ctrl_nav.join();
    if (t_logica.joinable())   t_logica.join();

    std::cout << "[Main " << caminhao_id << "] Processo encerrado.\n";
    return 0;
}
