#ifndef IPC_MANAGER_H
#define IPC_MANAGER_H

#include <string>

/**
 * @file IpcManager.h
 * @brief Declaração da classe IpcManager.
 *
 * @objetivo Abstrair e gerenciar a Comunicação entre Processos (IPC) 
 * entre esta instância do caminhão e o processo 'interface_local'.
 * (Implementação simulada por enquanto).
 */
class IpcManager {
public:
    IpcManager(int id);

    // Métodos simulados (vazios)
    std::string receber_comando();
    void enviar_estado(const std::string& estado);

private:
    int m_id;
};

#endif