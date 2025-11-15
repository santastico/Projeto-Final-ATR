#ifndef BUFFER_CIRCULAR_H
#define BUFFER_CIRCULAR_H

#include <vector>
#include <mutex>
#include <condition_variable>

struct EstadoVeiculo {
    bool e_defeito    = false;
    bool e_automatico = false;
};

void set_estado_veiculo(const EstadoVeiculo& est);

class BufferCircular {
public:
    // Estrutura dos dados tratados (posição e ângulo)
    struct PosicaoData {
        int i_pos_x = 0;
        int i_pos_y = 0;
        int i_angulo_x = 0;
    };

private:
    std::vector<PosicaoData> buffer_;
    std::size_t capacidade_;
    std::size_t inicio_ = 0;  // índice de leitura
    std::size_t fim_ = 0;     // índice de escrita
    std::size_t tamanho_ = 0; // número atual de elementos

    std::mutex mutex_;
    std::condition_variable cond_var_;

public:
    explicit BufferCircular(std::size_t capacidade = 200);

    // Grava nova posição tratada (substitui mais antiga se cheio)
    void set_posicao_tratada(const PosicaoData& pos);

    // Lê a posição mais recente
    PosicaoData get_posicao_recente() const;

    // Lê todas as posições disponíveis (para debug ou histórico)
    std::vector<PosicaoData> get_todas() const;

    // Notificação e acesso ao mutex (para integração com outras tarefas)
    std::mutex& get_mutex();
    void notify_all_consumers();

    // Espera por novos dados (para threads consumidoras)
    void wait_for_new_data(std::unique_lock<std::mutex>& lock);
};

#endif // BUFFER_CIRCULAR_H
