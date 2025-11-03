/**
 * @file main.cpp
 * @brief Ponto de entrada (entry point) do software embarcado do caminhão.
 *
 * @objetivo Orquestrar o início do processo do caminhão. 
 * É responsável por:
 * 1. Instanciar os objetos de estado compartilhado (BufferCircular, 
 * NotificadorEventos).
 * 2. Criar e lançar as 6 threads de tarefas principais 
 * (sensores, falhas, logica, etc.).
 * 3. Passar as referências dos objetos compartilhados para as threads.
 * 4. Manter o processo principal vivo (aguardando as threads com .join()).
 */

#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"
#include "tarefas.h"
#include <iostream>
#include <thread>
#include <string>

// --- Declarações das Funções de Tarefa ---
// (Estas devem ser implementadas em seus respectivos arquivos .cpp)
// (Por enquanto, apenas tê-las declaradas em tarefas.h é o suficiente 
// para o linker nos próximos passos, mas elas precisam existir.)

/*
// Exemplo do que deve estar em tarefas.h
#pragma once
#include "BufferCircular.h"
#include "NotificadorEventos.h"
void tarefa_tratamento_sensores(int id, BufferCircular& buffer);
void tarefa_monitoramento_falhas(int id, NotificadorEventos& notificador);
void tarefa_logica_comando(int id, BufferCircular& buffer, NotificadorEventos& notificador);
void tarefa_coletor_dados(int id, BufferCircular& buffer, NotificadorEventos& notificador);
void tarefa_controle_navegacao(int id, BufferCircular& buffer, NotificadorEventos& notificador);
void tarefa_planejamento_rota(int id, BufferCircular& buffer);
*/


#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"
#include "tarefas.h" // Inclui as declarações das tarefas
#include <iostream>
#include <thread>
#include <string>

// Ponto de Entrada Principal
int main(int argc, char *argv[]) {
    // 1. Verifica se o ID do caminhão foi passado
    if (argc < 2) {
        std::cerr << "Erro: ID do caminhao nao fornecido." << std::endl;
        std::cerr << "Uso: ./caminhao_embarcado <ID>" << std::endl;
        return 1;
    }

    int caminhao_id = std::stoi(argv[1]);
    std::cout << "--- Iniciando Caminhao Embarcado ID: " << caminhao_id << " ---" << std::endl;

    // 2. Criar os Recursos Compartilhados
    BufferCircular buffer_principal;
    NotificadorEventos notificador_falhas;

    std::cout << "[Main " << caminhao_id << "] Recursos compartilhados criados." << std::endl;

    // 3. Criar as Threads e passar os recursos por referência
    std::thread t_sensores(tarefa_tratamento_sensores, caminhao_id, std::ref(buffer_principal));
    std::thread t_monitor_falhas(tarefa_monitoramento_falhas, caminhao_id, std::ref(notificador_falhas));
    std::thread t_logica_comando(tarefa_logica_comando, caminhao_id, std::ref(buffer_principal), std::ref(notificador_falhas));
    std::thread t_coletor(tarefa_coletor_dados, caminhao_id, std::ref(buffer_principal), std::ref(notificador_falhas));
    std::thread t_navegacao(tarefa_controle_navegacao, caminhao_id, std::ref(buffer_principal), std::ref(notificador_falhas));
    std::thread t_planejamento(tarefa_planejamento_rota, caminhao_id, std::ref(buffer_principal));
    
    std::cout << "[Main " << caminhao_id << "] 6 threads de tarefas iniciadas." << std::endl;

    // 4. Esperar que as threads terminem (manter o programa vivo)
    t_sensores.join();
    t_monitor_falhas.join();
    t_logica_comando.join();
    t_coletor.join();
    t_navegacao.join();
    t_planejamento.join();

    std::cout << "[Main " << caminhao_id << "] Processo encerrado." << std::endl;
    return 0;
}