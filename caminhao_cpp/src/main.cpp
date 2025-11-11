/**
 * @file main.cpp
 * @brief Ponto de entrada (entry point) do software embarcado do caminhão.
 *
 * Responsabilidades:
 * 1. Instanciar objetos de estado compartilhado (BufferCircular, NotificadorEventos).
 * 2. Criar e lançar as 6 threads de tarefas principais.
 * 3. Passar referências às threads.
 * 4. Mantener o processo vivo (join nas threads).
 */

#include <iostream>
#include <thread>
#include <string>

#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"
#include "tarefas.h"  // declarações das tarefas no namespace atr

int main(int argc, char* argv[]) {
    // 1) Lê ID do caminhão (opcional). Se não vier, usa 1 para não falhar no Docker.
    int caminhao_id = 1;
    if (argc >= 2) {
        try {
            caminhao_id = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "[Main] ID inválido recebido. Usando ID=1 por padrão.\n";
            caminhao_id = 1;
        }
    } else {
        std::cout << "[Main] ID do caminhão não fornecido. Usando ID=1 por padrão.\n";
    }

    std::cout << "--- Iniciando Caminhao Embarcado ID: " << caminhao_id << " ---\n";

    // Buffer de estado compartilhado entre as tarefas desse caminhão
    BufferCircular buffer_principal;

    // Notificador de eventos de falha (AGORA no namespace atr)
    atr::NotificadorEventos notificador_falhas;

    // Vincula buffer + id para a tarefa de tratamento de sensores
    atr::tratamento_sensores(&buffer_principal, caminhao_id);

    // Thread 1: Tratamento de Sensores
    std::thread t_sens(
        atr::tarefa_tratamento_sensores_run,
        std::string("localhost")   // broker dentro do container
    );

    // Thread 2: Monitoramento de Falhas
    std::thread t_monitor_falhas(
        atr::tarefa_monitoramento_falhas,
        caminhao_id,
        std::ref(notificador_falhas)
    );

    // Thread 3: Lógica de Comando
    std::thread t_logica_comando(
        atr::tarefa_logica_comando,
        caminhao_id,
        std::ref(buffer_principal),
        std::ref(notificador_falhas)
    );

    // Thread 4: Coletor de Dados
    std::thread t_coletor(
        atr::tarefa_coletor_dados,
        caminhao_id,
        std::ref(buffer_principal),
        std::ref(notificador_falhas)
    );

    // Thread 5: Controle de Navegação
    std::thread t_navegacao(
        atr::tarefa_controle_navegacao,
        caminhao_id,
        std::ref(buffer_principal),
        std::ref(notificador_falhas)
    );

    // Thread 6: Planejamento de Rota
    std::thread t_planejamento(
        atr::tarefa_planejamento_rota,
        caminhao_id,
        std::ref(buffer_principal)
    );

    std::cout << "[Main " << caminhao_id << "] 6 threads de tarefas iniciadas.\n";

    // 4) Espera todas as threads (mantém o processo vivo)
    t_sens.join();
    t_monitor_falhas.join();
    t_logica_comando.join();
    t_coletor.join();
    t_navegacao.join();
    t_planejamento.join();

    std::cout << "[Main " << caminhao_id << "] Processo encerrado.\n";
    return 0;
}
