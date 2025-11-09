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
#include <iostream>
#include <chrono>
#include <thread>

namespace atr {

void tarefa_logica_comando(int id, BufferCircular& buffer, NotificadorEventos& notificador) {
    std::cout << "[Logica " << id << "] Thread iniciada." << std::endl;
    while(true) {
        // Lógica (vazia)
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace atr
