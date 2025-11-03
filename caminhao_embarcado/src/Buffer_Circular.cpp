/**
 * @file BufferCircular.cpp
 * @brief Declaração da classe BufferCircular (Estado Compartilhado).
 *
 * @objetivo Servir como o "quadro de estado" central e thread-safe para 
 * um único caminhão. Ele armazena e sincroniza o acesso a 
 * todos os dados compartilhados (posição, comandos, estados, setpoints) 
 * entre as 6 threads.
 *
 * @mecanismo (Interno)
 * Encapsula um 'std::mutex' para garantir que todas as leituras 
 * (get) e escritas (set) de variáveis sejam atômicas e seguras 
 * contra "race conditions".
 *
 * @entradas (Inputs) - (Métodos de Escrita: 'set_...')
 * - 'tratamento_sensores' (escreve posição tratada)
 * - 'logica_comando' (escreve estados 'e_defeito', 'e_automatico')
 * - 'coletor_dados' (escreve comandos 'c_man', 'c_rearme')
 * - 'controle_navegacao' (escreve 'velocidade', 'posicao_angular')
 * - 'planejamento_rota' (escreve 'setpoint_velocidade')
 *
 * @saidas (Outputs) - (Métodos de Leitura: 'get_...')
 * - 'logica_comando' (lê posição, comandos, valores de controle)
 * - 'coletor_dados' (lê estados, posição)
 * - 'controle_navegacao' (lê estados, setpoints)
 * - 'planejamento_rota' (lê posição)
 */

#include "Buffer_Circular.h"

// Implementações "vazias" dos métodos, apenas com a lógica 
// de lock/unlock do mutex para garantir thread-safety.

BufferCircular::BufferCircular() {
    // Construtor (pode inicializar valores padrão aqui)
}

void BufferCircular::set_posicao_tratada(const PosicaoData& pos) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_posicao = pos;
}

BufferCircular::PosicaoData BufferCircular::get_posicao_tratada() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_posicao;
}

void BufferCircular::set_comandos_operador(const ComandosOperador& cmds) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_comandos = cmds;
}

BufferCircular::ComandosOperador BufferCircular::get_comandos_operador() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_comandos;
}

void BufferCircular::set_estado_veiculo(const EstadoVeiculo& estado) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_estado = estado;
}

BufferCircular::EstadoVeiculo BufferCircular::get_estado_veiculo() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_estado;
}

void BufferCircular::set_setpoints_navegacao(const SetpointsNavegacao& sp) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_setpoints = sp;
}

BufferCircular::SetpointsNavegacao BufferCircular::get_setpoints_navegacao() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_setpoints;
}

void BufferCircular::set_saida_controle(const SaidaControle& out) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_saida_controle = out;
}

BufferCircular::SaidaControle BufferCircular::get_saida_controle() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_saida_controle;
}