/**
 * @file tarefa_coletor_dados.cpp
 * @brief Coletor de dados: lê do buffer de posições tratadas e registra em arquivo.
 */

#include "tarefas.h"
#include "Buffer_Circular.h"

#include <iostream>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <ctime>
#include <string>

namespace atr {

// ---------------------------------------------------------------------
// Variáveis globais usadas pela tarefa
// ---------------------------------------------------------------------

static BufferCircular<std::string>* g_buffer_posicao_tratada = nullptr;
static std::mutex* g_mtx_posicao_tratada = nullptr;
static std::condition_variable* g_cv_buffer_tratada = nullptr;
static int g_caminhao_id = 1;

// ---------------------------------------------------------------------
// Função de configuração (chamada pelo main)
// ---------------------------------------------------------------------

void coletor_dados_config(
    BufferCircular<std::string>* buffer_tratada,
    std::mutex& mtx_tratada,
    std::condition_variable& cv_tratada,
    int caminhao_id)
{
    g_buffer_posicao_tratada = buffer_tratada;
    g_mtx_posicao_tratada = &mtx_tratada;
    g_cv_buffer_tratada = &cv_tratada;
    g_caminhao_id = caminhao_id;

    std::cout << "[coletor_dados] Configurado para caminhao_id = "
              << g_caminhao_id << std::endl;
}

// ---------------------------------------------------------------------
// Thread principal do coletor
// ---------------------------------------------------------------------

void tarefa_coletor_dados_run()
{
    if (!g_buffer_posicao_tratada || !g_mtx_posicao_tratada || !g_cv_buffer_tratada) {
        std::cerr << "[coletor_dados] ERRO: chame coletor_dados_config() antes de criar a thread.\n";
        return;
    }

    // Abre arquivo de log (append mode)
    std::string nome_arquivo = "coletor_dados_caminhao_" 
                             + std::to_string(g_caminhao_id) 
                             + ".txt";
    std::ofstream log(nome_arquivo, std::ios::app);

    if (!log.is_open()) {
        std::cerr << "[coletor_dados] ERRO: Não conseguiu abrir " << nome_arquivo << "\n";
        return;
    }

    std::cout << "[coletor_dados] Thread iniciada. Aguardando dados...\n";

    while (true) {
        // ============================
        // SEÇÃO CRÍTICA: ESPERA + LEITURA
        // ============================
        std::unique_lock<std::mutex> lock(*g_mtx_posicao_tratada);

        // Espera até haver dados (libera mutex enquanto dorme)
        g_cv_buffer_tratada->wait(lock, []{ 
            return !g_buffer_posicao_tratada->estaVazio(); 
        });

        // Acordou! Processa todos os itens disponíveis
        std::string dado;
        int contador = 0;
        
        // while (g_buffer_posicao_tratada->retirar(dado)) {
        //     // Timestamp
        //     auto now = std::chrono::system_clock::now();
        //     auto timestamp = std::chrono::system_clock::to_time_t(now);
        //     char time_str[100];
        //     std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&timestamp));

        //     // Escreve no log
        //     log << "[" << time_str << "] " << dado << std::endl;
        //     contador++;
        // }
        
        // log.flush(); // força escrita em disco

        // ============================
        // FIM SEÇÃO CRÍTICA
        // ============================

        if (contador > 0) {
            std::cout << "[coletor_dados] " << contador << " registros coletados.\n";
        }
    }

    log.close();
}

} // namespace atr
