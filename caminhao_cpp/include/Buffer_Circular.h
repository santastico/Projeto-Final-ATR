#ifndef BUFFER_CIRCULAR_H
#define BUFFER_CIRCULAR_H

#include <mutex> // Necessário para std::mutex

/**
 * @file BufferCircular.h
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
 */
class BufferCircular {
public:
    // --- Estruturas de Dados para Agrupar Variáveis ---
    // (Facilita a passagem de dados entre as threads)

    /**
     * @brief Dados escritos pela tarefa tratamento_sensores
     */
    struct PosicaoData {
        double i_pos_x = 0.0;
        double i_pos_y = 0.0;
        double i_angulo_x = 0.0;
    };

    /**
     * @brief Dados escritos pela tarefa coletor_dados
     */
    struct ComandosOperador {
        bool c_automatico = false;
        bool c_man = true; // Começa em manual por padrão
        bool c_rearme = false;
        bool c_direita = false;
        bool c_esquerda = false;
        bool c_acelera = false;
    };

    /**
     * @brief Dados escritos pela tarefa logica_comando
     */
    struct EstadoVeiculo {
        bool e_defeito = false;
        bool e_automatico = false; // Começa em manual por padrão
    };

    /**
     * @brief Dados escritos pela tarefa planejamento_rota
     */
    struct SetpointsNavegacao {
        double setpoint_velocidade = 0.0;
        double setpoint_posicao_angular = 0.0;
    };

    /**
     * @brief Dados escritos pela tarefa controle_navegacao
     */
    struct SaidaControle {
        double velocidade = 0.0;
        double posicao_angular = 0.0;
    };


    // --- Interface Pública (Métodos Thread-Safe) ---

    /**
     * @brief Construtor
     */
    BufferCircular();

    /**
     * @brief Define a posição tratada (Usado por tratamento_sensores)
     */
    void set_posicao_tratada(const PosicaoData& pos);

    /**
     * @brief Lê a posição tratada (Usado por logica_comando, coletor_dados, planejamento_rota)
     */
    PosicaoData get_posicao_tratada();

    /**
     * @brief Define os comandos do operador (Usado por coletor_dados)
     */
    void set_comandos_operador(const ComandosOperador& cmds);

    /**
     * @brief Lê os comandos do operador (Usado por logica_comando)
     */
    ComandosOperador get_comandos_operador();

    /**
     * @brief Define o estado do veículo (Usado por logica_comando)
     */
    void set_estado_veiculo(const EstadoVeiculo& estado);

    /**
     * @brief Lê o estado do veículo (Usado por coletor_dados, controle_navegacao)
     */
    EstadoVeiculo get_estado_veiculo();

    /**
     * @brief Define os setpoints de navegação (Usado por planejamento_rota)
     */
    void set_setpoints_navegacao(const SetpointsNavegacao& sp);

    /**
     * @brief Lê os setpoints de navegação (Usado por controle_navegacao)
     */
    SetpointsNavegacao get_setpoints_navegacao();

    /**
     * @brief Define a saída do controle (Usado por controle_navegacao)
     */
    void set_saida_controle(const SaidaControle& out);

    /**
     * @brief Lê a saída do controle (Usado por logica_comando)
     */
    SaidaControle get_saida_controle();


private:
    // --- Membros Privados ---

    /**
     * @brief O "cadeado" para proteger o acesso a todas as 
     * variáveis de estado abaixo.
     */
    std::mutex m_mutex;

    // --- Variáveis de Estado (Protegidas pelo Mutex) ---

    PosicaoData         m_posicao;
    ComandosOperador    m_comandos;
    EstadoVeiculo       m_estado;
    SetpointsNavegacao  m_setpoints;
    SaidaControle       m_saida_controle;
};

#endif // BUFFER_CIRCULAR_H