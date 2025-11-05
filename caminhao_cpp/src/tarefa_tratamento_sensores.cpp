/**
 * @file tarefa_tratamento_sensores.cpp
 * @brief Implementação da thread Tratamento de Sensores.
 *
 * @objetivo Responsável por ler os dados brutos de posicionamento (com ruído) 
 * vindos do simulador, aplicar um filtro de média móvel para tratá-los 
 * e disponibilizar os dados limpos para as outras tarefas no Buffer_Circular.
 *
 * @entradas (Inputs)
 * 1. MQTT (subscribe): Assina os tópicos "i_pos_x", "i_pos_y" e "i_angulo_x" 
 * publicados pelo simulador.
 *
 * @saidas (Outputs)
 * 1. Buffer Circular (escrita): Escreve as variáveis tratadas (após o filtro)
 * de "i_pos_x", "i_pos_y" e "i_angulo_x" no buffer.
 */
#include "tarefas.h"
#include "Buffer_Circular.h"
#include "Notificador_Eventos.h"
#include <iostream>
#include <chrono>
#include <thread>

void tarefa_tratamento_sensores(int id, BufferCircular& buffer) {
    std::cout << "[Sensores " << id << "] Thread iniciada." << std::endl;
    while(true) {
        // Lógica (vazia)
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}