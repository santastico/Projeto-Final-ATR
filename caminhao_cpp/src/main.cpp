/**
 * @file main.cpp
 * @brief Ponto de entrada (entry point) do software embarcado do caminhão.
 */

#include <iostream>
#include <cstdlib> // para std::getenv
#include <thread>
#include <string>
#include <mutex>
#include <functional>

#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"
#include "tarefas.h"  // declarações das tarefas no namespace atr
#include "config.h"   // para obter_broker_uri()

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
    BufferCircular<std::string> buffer_posicao_bruta(10);
    BufferCircular<std::string> buffer_posicao_tratada(100);
    BufferCircular<std::string> buffer_setpoints_nav(100);
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
        caminhao_id
    );

    // Planejamento de Rota (novo)
    atr::planejamento_rota_config(
        &buffer_posicao_tratada,   // entrada: posição tratada
        mtx_posicao_tratada,
        &buffer_setpoints_nav,     // saída: setpoints para o controle
        &mtx_setpoints_nav,
        caminhao_id
    );

    // Coletor de Dados
    atr::leitura_posicao_config(
        nullptr,
        &buffer_posicao_tratada,
        mtx_posicao_tratada
    );

    // Controle de Navegação (NOVO)
    atr::controle_navegacao_config(
        &buffer_setpoints_nav,
        mtx_setpoints_nav,
        notificador
    );

    // -----------------------------------------------------------------
    // 4. Lançamento das Threads
    // -----------------------------------------------------------------

    // Thread 1: Tratamento de Sensores
    std::thread t_sens(
        atr::tarefa_tratamento_sensores_run,
        obter_broker_uri()
    );

    // Thread 2: Monitoramento de Falhas
    std::thread t_monitor(
        atr::tarefa_monitoramento_falhas,
        caminhao_id,
        std::ref(notificador)
    );

    std::thread t_plan(
        atr::tarefa_planejamento_rota_run,
        obter_broker_uri()
    );

    // Thread 3: Coletor de Dados
    std::thread t_coletor(atr::tarefa_leitura_posicao_run);

    // Thread 4: Controle de Navegação (NOVO)
    std::thread t_ctrl_nav(atr::tarefa_controle_navegacao_run);

    std::cout << "[Main " << caminhao_id << "] Todas as threads iniciadas.\n";

    // -----------------------------------------------------------------
    // 5. Loop Principal
    // -----------------------------------------------------------------
    
    if (t_sens.joinable()) t_sens.join();
    if (t_monitor.joinable()) t_monitor.join();
    if (t_coletor.joinable()) t_coletor.join();
    if (t_ctrl_nav.joinable()) t_ctrl_nav.join();
    if (t_plan.joinable()) t_plan.join();

    std::cout << "[Main " << caminhao_id << "] Processo encerrado.\n";
    return 0;
}
