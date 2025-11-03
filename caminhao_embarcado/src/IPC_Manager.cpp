/**
 * @file IpcManager.cpp
 * @brief Declaração da classe IpcManager.
 *
 * @objetivo Abstrair e gerenciar a Comunicação entre Processos (IPC) 
 * (ex: Sockets, Named Pipes) entre esta instância do caminhão 
 * e o processo 'interface_local' correspondente.
 * É usado pela 'coletor_dados' para trocar informações com 
 * o operador.
 *
 * @entradas (Inputs) - (Comandos do operador -> para o caminhão)
 * 1. Chamada de 'receber_comando()': Usado pela 'coletor_dados' para 
 * obter os comandos (ex: "c_man", "c_rearme") enviados pela 
 * 'interface_local'.
 *
 * @saidas (Outputs) - (Estado do caminhão -> para o operador)
 * 1. Chamada de 'enviar_estado()': Usado pela 'coletor_dados' para 
 * enviar o "dicionário" de estados (posição, modo, falhas) 
 * para a 'interface_local'.
 */

 #include "IPC_Manager.h"
#include <iostream>

// Implementação simulada/vazia
IpcManager::IpcManager(int id) : m_id(id) {
    std::cout << "[IPC " << m_id << "] Gerenciador IPC inicializado (simulado)." << std::endl;
}

std::string IpcManager::receber_comando() {
    // Simula não receber nada
    return "";
}

void IpcManager::enviar_estado(const std::string& estado) {
    // Simula o envio
}