#include "BufferCircular.h"

/**
 * @file BufferCircular.cpp
 * @brief Implementação da classe BufferCircular (Quadro de Estado Gerenciado).
 *
 * @mecanismo Esta classe implementa o padrão "Blackboard" com 
 * **TRAVAMENTO EXTERNO (EXTERNAL LOCKING)**.
 * * Os métodos 'set_' e 'get_' NÃO travam o mutex; eles apenas 
 * acessam diretamente as variáveis de membro.
 * * A thread que chama (ex: 'logica_comando') DEVE travar o mutex 
 * (obtido via 'get_mutex()') ANTES de chamar 'get_' ou 'set_'.
 */

// --- Construtor ---

/**
 * @brief Construtor padrão.
 * As variáveis de membro (structs) são inicializadas com seus 
 * valores padrão (definidos no .h) automaticamente.
 */
BufferCircular::BufferCircular() {
    // O construtor está vazio, pois os valores padrão (ex: c_man = true)
    // já foram definidos nas declarações das structs no arquivo .h.
}


// --- Métodos de Escrita (Produtores) ---
// (Estes métodos são "burros" - eles assumem que o mutex JÁ FOI TRAVADO 
// pela thread que os chamou)

void BufferCircular::set_posicao_tratada(const PosicaoData& pos_tratada) {
    this->m_posicao = pos_tratada;
}

void BufferCircular::set_comandos_operador(const ComandosOperador& cmds) {
    this->m_comandos = cmds;
}

void BufferCircular::set_estado_veiculo(const EstadoVeiculo& estado) {
    this->m_estado = estado;
}

void BufferCircular::set_setpoints_navegacao(const SetpointsNavegacao& sp) {
    this->m_setpoints = sp;
}

void BufferCircular::set_saida_controle(const SaidaControle& out) {
    this->m_saida_controle = out;
}


// --- Métodos de Leitura (Consumidores) ---
// (Estes métodos são "burros" - eles assumem que o mutex JÁ FOI TRAVADO 
// pela thread que os chamou)

PosicaoData BufferCircular::get_posicao_tratada() const {
    return this->m_posicao;
}

ComandosOperador BufferCircular::get_comandos_operador() const {
    return this->m_comandos;
}

EstadoVeiculo BufferCircular::get_estado_veiculo() const {
    return this->m_estado;
}

SetpointsNavegacao BufferCircular::get_setpoints_navegacao() const {
    return this->m_setpoints;
}

SaidaControle BufferCircular::get_saida_controle() const {
    return this->m_saida_controle;
}


// --- Métodos de Sincronização (Acesso Público) ---

/**
 * @brief Retorna uma referência ao mutex privado.
 * Permite que as threads executem o travamento externo.
 */
std::mutex& BufferCircular::get_mutex() {
    return this->m_mutex;
}

/**
 * @brief Faz a thread chamadora dormir, esperando por um 'notify_all_consumers()'.
 * @param lock Um 'std::unique_lock' que a thread já deve possuir.
 * O 'wait' libera o lock atomicamente e dorme; ao acordar, 
 * ele re-adquire o lock.
 */
void BufferCircular::esperar_por_dados(std::unique_lock<std::mutex>& lock) {
    this->m_cv.wait(lock);
}

/**
 * @brief Acorda todas as threads que estão "dormindo" em 'esperar_por_dados()'.
 * As threads produtoras devem chamar isso APÓS liberar o mutex.
 */
void BufferCircular::notify_all_consumers() {
    this->m_cv.notify_all();
}