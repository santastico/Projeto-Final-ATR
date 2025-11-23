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
#include <cstdlib> // para std::getenv
#include <thread>
#include <string>
#include <mutex>
#include "Buffer_Circular.h"
#include "tarefas.h"  // declarações das tarefas no namespace atr
#include <functional>
#include "Notificador_Eventos.h"


int main() {
    // 1) Lê ID do caminhão (opcional). Se não vier, usa 1 para não falhar no Docker.
    // Padrão: 1 (para teste local rápido)
    int caminhao_id = 1;
    // 1. Tenta ler de variável de ambiente (Melhor para Docker)
    const char* env_id = std::getenv("CAMINHAO_ID");
    if (env_id) {
        caminhao_id = std::stoi(env_id);
    } 

    std::mutex mtx_posicao_tratada;
    std::cout << "--- Iniciando Caminhao Embarcado ID: " << caminhao_id << " ---\n";

    // Buffer de posição bruta e tratada
    BufferCircular<std::string> buffer_posicao_bruta(10);
    BufferCircular<std::string> buffer_posicao_tratada(100);


    // Vincula buffer + id para a tarefa de tratamento de sensores
    atr::tratamento_sensores(&buffer_posicao_bruta, &buffer_posicao_tratada, mtx_posicao_tratada, caminhao_id);
    atr::leitura_posicao_config(&buffer_posicao_bruta,
                            &buffer_posicao_tratada,
                            mtx_posicao_tratada);

   // Cria o notificador de eventos (compartilhado entre as tarefas)
   atr::NotificadorEventos notificador;

    // Thread 1: Tratamento de Sensores
    std::thread t_sens(
        atr::tarefa_tratamento_sensores_run,
        std::string("tcp://localhost:1883")   // URI completa para Paho C++
    );

    std::thread t_leitura(atr::tarefa_leitura_posicao_run);

    std::thread t_monitor(
        atr::tarefa_monitoramento_falhas,
        caminhao_id,
        std::ref(notificador)             // passa por referência
    );

    std::cout << "[Main " << caminhao_id << "]  thread de tratamento_sensores.\n";

    // 4) Espera todas as threads (mantém o processo vivo)
    t_sens.join();
    t_leitura.join();
    t_monitor.join();

    std::cout << "[Main " << caminhao_id << "] Processo encerrado.\n";
    return 0;
}
