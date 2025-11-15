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
#include "tarefas.h"  // declarações das tarefas no namespace atr

int main() {
    // 1) Lê ID do caminhão (opcional). Se não vier, usa 1 para não falhar no Docker.
    int caminhao_id = 1;

    std::cout << "--- Iniciando Caminhao Embarcado ID: " << caminhao_id << " ---\n";

    // Buffer de posição bruta
    BufferCircular buffer_posicao_bruta(10);
    BufferCircular buffer_posicao_tratada(100);

    // Vincula buffer + id para a tarefa de tratamento de sensores
    atr::tratamento_sensores(&buffer_posicao_bruta, buffer_posicao_tratada,caminhao_id);

    // Thread 1: Tratamento de Sensores
    std::thread t_sens(
        atr::tarefa_tratamento_sensores_run,
        std::string("tcp://localhost:1883")   // URI completa para Paho C++
    );

    std::cout << "[Main " << caminhao_id << "]  thread de tratamento_sensores.\n";

    // 4) Espera todas as threads (mantém o processo vivo)
    t_sens.join();

    std::cout << "[Main " << caminhao_id << "] Processo encerrado.\n";
    return 0;
}
