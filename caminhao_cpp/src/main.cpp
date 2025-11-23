/**
 * @file main.cpp
 * @brief Ponto de entrada (entry point) do software embarcado do caminhão.
 *
 * Responsabilidades:
 * 1. Instanciar objetos de estado compartilhado (BufferCircular, NotificadorEventos).
 * 2. Criar e lançar as threads das tarefas principais.
 * 3. Passar referências e configurações às tarefas.
 * 4. Manter o processo vivo (join nas threads).
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
    // Padrão: 1 (para teste local rápido)
    int caminhao_id = 1;
    
    // Tenta ler de variável de ambiente (Configuração Docker)
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

    // Mutex para proteger o acesso ao buffer de dados tratados (Sincronização Externa)
    std::mutex mtx_posicao_tratada;

    // Buffer Bruto: Capacidade 10 (FUNDAMENTAL para lógica de batch/decimação)
    // O Tratamento enche 10 posições -> Processa -> Esvazia -> Escreve 1 no Tratado
    BufferCircular<std::string> buffer_posicao_bruta(10);

    // Buffer Tratado: Capacidade maior para acomodar os resultados filtrados
    BufferCircular<std::string> buffer_posicao_tratada(100);

    // Notificador de eventos (compartilhado entre Monitoramento e outras tarefas)
    atr::NotificadorEventos notificador;

    // -----------------------------------------------------------------
    // 3. Configuração das Tarefas (Injeção de Dependência)
    // -----------------------------------------------------------------

    // Configura Tratamento de Sensores
    atr::tratamento_sensores(
        &buffer_posicao_bruta, 
        &buffer_posicao_tratada, 
        mtx_posicao_tratada, 
        caminhao_id
    );

    // Configura Coletor de Dados (antigo leitura_posicao)
    // Note: Coletor só precisa ler do tratado, então ignoramos o bruto na config interna
    atr::leitura_posicao_config(
        nullptr,                // Coletor não lê bruto
        &buffer_posicao_tratada,
        mtx_posicao_tratada
    );

    // -----------------------------------------------------------------
    // 4. Lançamento das Threads
    // -----------------------------------------------------------------

    // Thread 1: Tratamento de Sensores (Produtor)
    std::thread t_sens(
        atr::tarefa_tratamento_sensores_run,
        obter_broker_uri() // URI dinâmica (localhost ou infra_mina)
    );

    // Thread 2: Monitoramento de Falhas
    std::thread t_monitor(
        atr::tarefa_monitoramento_falhas,
        caminhao_id,
        std::ref(notificador) // passa por referência
    );

    // Thread 3: Coletor de Dados (Consumidor - Log no Terminal)
    std::thread t_coletor(atr::tarefa_leitura_posicao_run);

    std::cout << "[Main " << caminhao_id << "] Todas as threads iniciadas.\n";

    // -----------------------------------------------------------------
    // 5. Loop Principal (Bloqueante)
    // -----------------------------------------------------------------
    
    // Aguarda threads terminarem (na prática, rodam para sempre)
    if (t_sens.joinable()) t_sens.join();
    if (t_monitor.joinable()) t_monitor.join();
    if (t_coletor.joinable()) t_coletor.join();

    std::cout << "[Main " << caminhao_id << "] Processo encerrado.\n";
    return 0;
}
