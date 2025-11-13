#include <vector>
#include <mutex>
#include <condition_variable>

BufferCircular::BufferCircular(std::size_t capacidade)
    : buffer_(capacidade), capacidade_(capacidade) {}

// ---------------------------------------------------------------------
// Escreve uma nova posição tratada no buffer circular
// ---------------------------------------------------------------------
void BufferCircular::set_posicao_tratada(const PosicaoData& pos)
{
    buffer_[fim_] = pos;
    fim_ = (fim_ + 1) % capacidade_;

    if (tamanho_ < capacidade_) {
        ++tamanho_;
    } else {
        // Sobrescreve o mais antigo
        inicio_ = (inicio_ + 1) % capacidade_;
    }
}

// ---------------------------------------------------------------------
// Retorna a posição mais recente (sem remover)
// ---------------------------------------------------------------------
BufferCircular::PosicaoData BufferCircular::get_posicao_recente() const
{
    if (tamanho_ == 0) {
        return PosicaoData{}; // vazio
    }
    std::size_t ultimo = (fim_ + capacidade_ - 1) % capacidade_;
    return buffer_[ultimo];
}

// ---------------------------------------------------------------------
// Retorna cópia de todas as posições armazenadas
// ---------------------------------------------------------------------
std::vector<BufferCircular::PosicaoData> BufferCircular::get_todas() const
{
    std::vector<PosicaoData> saida;
    saida.reserve(tamanho_);

    for (std::size_t i = 0; i < tamanho_; ++i) {
        std::size_t idx = (inicio_ + i) % capacidade_;
        saida.push_back(buffer_[idx]);
    }
    return saida;
}

// ---------------------------------------------------------------------
// Retorna referência ao mutex interno (para uso em lock externo)
// ---------------------------------------------------------------------
std::mutex& BufferCircular::get_mutex()
{
    return mutex_;
}

// ---------------------------------------------------------------------
// Notifica todos os consumidores esperando novos dados
// ---------------------------------------------------------------------
void BufferCircular::notify_all_consumers()
{
    cond_var_.notify_all();
}

// ---------------------------------------------------------------------
// Espera por novos dados (bloqueia thread consumidora)
// ---------------------------------------------------------------------
void BufferCircular::wait_for_new_data(std::unique_lock<std::mutex>& lock)
{
    cond_var_.wait(lock);
}
