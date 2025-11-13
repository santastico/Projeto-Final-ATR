/**
 * @file tarefa_controle_navegacao.cpp
 * @brief Implementação da thread Controle de Navegação.
 *
 * @objetivo Ser o "piloto automático" do caminhão. Com base no 
 * estado lido (manual, automático, defeito), ele:
 * 1. (Automático): Executa o controle para seguir os setpoints.
 * 2. (Manual): Desliga o controle (bumpless transfer).
 * 3. (Defeito): Não executa movimentação e aguarda o rearme.
 *
 * @entradas (Inputs)
 * 1. Buffer Circular (leitura): Lê os estados "e_defeito", "e_automatico"
 * e os setpoints "setpoint_velocidade", "setpoint_posicao_angular".
 * 2. Notificador de Eventos (recebimento): Recebe eventos de falha 
 * disparados pelo Monitoramento de Falhas.
 *
 * @saidas (Outputs)
 * 1. Buffer Circular (escrita): Escreve as variáveis de controle 
 * calculadas "velocidade" e "posicao_angular".
 */
#include <iostream>
#include <chrono>
#include <thread>

namespace atr {

void tarefa_controle_navegacao(int id, BufferCircular& buffer, NotificadorEventos& notificador) {
    std::cout << "[Navegacao " << id << "] Thread iniciada." << std::endl;
    while(true) {
        // Lógica (vazia)
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace atr
