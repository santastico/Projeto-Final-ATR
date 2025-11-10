#ifndef BUFFER_CIRCULAR_H
#define BUFFER_CIRCULAR_H

// --- 1. Imports (Dependências) ---

#include <mutex>                // Para o std::mutex (o "cadeado" único)
#include <condition_variable>   // Para o std::condition_variable (o "sino" de notificação)

/**
 * @file BufferCircular.h
 * @brief Declaração da classe BufferCircular (Quadro de Estado Gerenciado).
 *
 * @objetivo Servir como o "quadro de avisos" central e thread-safe 
 * para o caminhão.
 *
 * @mecanismo Esta classe implementa o padrão "Blackboard" com 
 * **TRAVAMENTO EXTERNO (EXTERNAL LOCKING)**.
 * * As threads (ex: 'logica_comando') são responsáveis por travar 
 * o mutex antes de chamar os métodos 'get'/'set'. Isso permite 
 * "transações atômicas" (ex: ler 3 valores de uma vez) e 
 * previne "snapshots inconsistentes".
 *
 * A classe gerencia 5 "buffers de tamanho 1" (estado mais recente) 
 * e os protege com um único mutex (para evitar deadlocks) e 
 * uma única variável de condição (para notificação).
 */
 
class BufferCircular {
public:
    // --- 2. Definição Pública dos Tipos de Dados (Structs) ---
    // (Qualquer thread que incluir "BufferCircular.h" pode usar estes tipos)

    /**
     * @brief Dados TRATADOS (pós-média móvel) do 'tarefa_tratamento_sensores'.
     */
    struct PosicaoData {
        int i_pos_x = 0;
        int i_pos_y = 0;
        int i_angulo_x = 0;
    };

    /**
     * @brief Comandos vindos da 'tarefa_coletor_dados'.
     */
    struct ComandosOperador {
        bool c_automatico = false;
        bool c_man = true; 
        bool c_rearme = false;
        int c_acelera = 0;
        int c_soma_angulo = 0;
    };

    /**
     * @brief Estado do veículo, definido pela 'tarefa_logica_comando'.
     */
    struct EstadoVeiculo {
        bool e_defeito = false;
        bool e_automatico = false;
    };

    /**
     * @brief Setpoints definidos pela 'tarefa_planejamento_rota'.
     */
    struct SetpointsNavegacao {
        int set_velocidade = 0;
        int set_pos_angular = 0;
    };

    /**
     * @brief Saída do controle, definida pela 'tarefa_controle_navegacao'.
     */
    struct SaidaControle {
        int a_acelera = 0;
        int a_soma_ang = 0;
    };


    // --- 3. Métodos Públicos (A "API" da Classe) ---

    /**
     * @brief Construtor padrão.
     * Utilizado por: 'main.cpp'.
     */
    BufferCircular();

    
    // --- Métodos de Escrita (Produtores) ---
    // ATENÇÃO: Estes métodos NÃO travam o mutex.
    // A thread que os chama DEVE ter travado o 'm_mutex' antes.

    /**
     * @brief Escreve o valor de posição (já tratado com média móvel).
     * @quem_usa (Escritor): 'tarefa_tratamento_sensores' (DENTRO de um lock).
     */
    void set_posicao_tratada(const PosicaoData& pos_tratada);

    /**
     * @brief Atualiza o buffer de comandos do operador.
     * @quem_usa (Escritor): 'tarefa_coletor_dados' (DENTRO de um lock).
     */
    void set_comandos_operador(const ComandosOperador& cmds);

    /**
     * @brief Atualiza o buffer de estado do veículo.
     * @quem_usa (Escritor): 'tarefa_logica_comando' (DENTRO de um lock).
     */
    void set_estado_veiculo(const EstadoVeiculo& estado);

    /**
     * @brief Atualiza o buffer de setpoints.
     * @quem_usa (Escritor): 'tarefa_planejamento_rota' (DENTRO de um lock).
     */
    void set_setpoints_navegacao(const SetpointsNavegacao& sp);

    /**
     * @brief Atualiza o buffer da saída do controle.
     * @quem_usa (Escritor): 'tarefa_controle_navegacao' (DENTRO de um lock).
     */
    void set_saida_controle(const SaidaControle& out);


    // --- Métodos de Leitura (Consumidores) ---
    // ATENÇÃO: Estes métodos NÃO travam o mutex.
    // A thread que os chama DEVE ter travado o 'm_mutex' antes.

    /**
     * @brief Obtém o último valor de posição tratado.
     * @quem_usa (Leitor): 'logica_comando', 'coletor_dados', etc. (DENTRO de um lock).
     */
    PosicaoData get_posicao_tratada() const; // 'const' indica que não altera o objeto

    /**
     * @brief Obtém os últimos comandos do operador.
     * @quem_usa (Leitor): 'logica_comando' (DENTRO de um lock).
     */
    ComandosOperador get_comandos_operador() const;

    /**
     * @brief Obtém o estado atual do veículo.
     * @quem_usa (Leitor): 'coletor_dados', 'controle_navegacao' (DENTRO de um lock).
     */
    EstadoVeiculo get_estado_veiculo() const;

    /**
     * @brief Obtém os setpoints de navegação atuais.
     * @quem_usa (Leitor): 'controle_navegacao' (DENTRO de um lock).
     */
    SetpointsNavegacao get_setpoints_navegacao() const;

    /**
     * @brief Obtém a última saída calculada pelo controle.
     * @quem_usa (Leitor): 'logica_comando' (DENTRO de um lock).
     */
    SaidaControle get_saida_controle() const;


    // --- Métodos de Sincronização (Acesso Público) ---

    /**
     * @brief Obtém uma referência direta ao mutex (para travamento externo).
     * @quem_usa Todas as threads, para usar com 'std::lock_guard'
     * ou 'std::unique_lock'.
     */
    std::mutex& get_mutex();

    /**
     * @brief Faz a thread consumidora "dormir" até que novos dados cheguem.
     * @param lock Um 'unique_lock' do 'm_mutex' (que já deve estar travado).
     * @quem_usa (Leitor): Qualquer thread que queira esperar por uma atualização.
     */
    void esperar_por_dados(std::unique_lock<std::mutex>& lock);

    /**
     * @brief Acorda TODOS os consumidores que estão "dormindo" (esperando).
     * @quem_usa (Produtor): Qualquer thread após chamar um 'set_...'.
     * (Deve ser chamado APÓS o 'mutex' ser liberado).
     */
    void notify_all_consumers();


private:
    // --- 4. Membros Privados ---

    // O "cadeado" único que protege TODOS os dados abaixo.
    // É 'mutable' para que 'get_mutex()' possa retorná-lo.
    mutable std::mutex m_mutex; 
    
    // O "sino" único para notificar consumidores.
    std::condition_variable m_cv; 

    // Os 5 Buffers de "Tamanho 1" (Estado Mais Recente)
    PosicaoData         m_posicao;
    ComandosOperador    m_comandos;
    EstadoVeiculo       m_estado;
    SetpointsNavegacao  m_setpoints;
    SaidaControle       m_saida_controle;
};

#endif // BUFFER_CIRCULAR_H