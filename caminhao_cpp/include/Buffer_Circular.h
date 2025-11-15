#ifndef BUFFERCIRCULAR_H
#define BUFFERCIRCULAR_H

#include <cstddef>  // para size_t
#include <stdexcept> // para exceções

<<<<<<< HEAD
// Classe template para Buffer Circular genérico
template <typename T>
=======
struct EstadoVeiculo {
    bool e_defeito    = false;
    bool e_automatico = false;
};

void set_estado_veiculo(const EstadoVeiculo& est);

>>>>>>> 9a292b71b7378cad101e77ac32d255a100cbf4e2
class BufferCircular {
private:
    T* buffer;        // Array dinâmico para armazenar os dados
    size_t capacidade; // Capacidade máxima do buffer
    size_t head;      // Índice para leitura
    size_t tail;      // Índice para escrita
    size_t tamanho;   // Quantidade de elementos atuais

public:
    // Construtor: inicializa buffer e índices
    explicit BufferCircular(size_t capacidade);

    // Destrutor: libera memória
    ~BufferCircular();

    // Escreve um elemento no buffer. Retorna true se sucesso, false se cheio
    bool escrever(const T& item);

    // Lê um elemento sem retirar. Retorna true se sucesso, false se vazio
    bool ler(T& item) const;

    // Retira elemento do buffer e armazena em item. Retorna true se sucesso, false se vazio
    bool retirar(T& item);

    // Retorna se o buffer está vazio
    bool estaVazio() const;

    // Retorna se o buffer está cheio
    bool estaCheio() const;

    // Retorna número atual de elementos
    size_t obterTamanho() const;

    // Retorna capacidade máxima
    size_t obterCapacidade() const;

    // Limpa o buffer, resetando índices e tamanho
    void limpar();
};

#include "BufferCircular.cpp"  // Incluir implementação do template

#endif // BUFFERCIRCULAR_H
