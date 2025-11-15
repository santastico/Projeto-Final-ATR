#ifndef BUFFERCIRCULAR_CPP
#define BUFFERCIRCULAR_CPP

#include "BufferCircular.h"

// Construtor: aloca array e inicializa índices
template <typename T>
BufferCircular<T>::BufferCircular(size_t cap) : capacidade(cap), head(0), tail(0), tamanho(0) {
    if (capacidade == 0) {
        throw std::invalid_argument("Capacidade deve ser maior que zero.");
    }
    buffer = new T[capacidade];
}

// Destrutor: libera memória alocada
template <typename T>
BufferCircular<T>::~BufferCircular() {
    delete[] buffer;
}

// Escreve item na posição tail, se não estiver cheio
template <typename T>
bool BufferCircular<T>::escrever(const T& item) {
    if (estaCheio()) {
        return false; // Buffer cheio, não pode escrever
    }
    buffer[tail] = item;
    tail = (tail + 1) % capacidade;
    tamanho++;
    return true;
}

// Lê item na posição head sem remover
template <typename T>
bool BufferCircular<T>::ler(T& item) const {
    if (estaVazio()) {
        return false; // Buffer vazio, nada a ler
    }
    item = buffer[head];
    return true;
}

// Retira item da posição head e avança head
template <typename T>
bool BufferCircular<T>::retirar(T& item) {
    if (estaVazio()) {
        return false; // Buffer vazio
    }
    item = buffer[head];
    head = (head + 1) % capacidade;
    tamanho--;
    return true;
}

// Verifica se buffer está vazio
template <typename T>
bool BufferCircular<T>::estaVazio() const {
    return (tamanho == 0);
}

// Verifica se buffer está cheio
template <typename T>
bool BufferCircular<T>::estaCheio() const {
    return (tamanho == capacidade);
}

// Retorna tamanho atual
template <typename T>
size_t BufferCircular<T>::obterTamanho() const {
    return tamanho;
}

// Retorna capacidade máxima
template <typename T>
size_t BufferCircular<T>::obterCapacidade() const {
    return capacidade;
}

// Reseta buffer, descartando dados
template <typename T>
void BufferCircular<T>::limpar() {
    head = 0;
    tail = 0;
    tamanho = 0;
}

#endif // BUFFERCIRCULAR_CPP
