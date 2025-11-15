#include "tarefas.h"
#include "Buffer_Circular.h"

#include <iostream>
#include <string>
#include <mutex>
#include <thread>
#include <chrono>

namespace atr {

static BufferCircular<std::string>* buffer_posicao_bruta_leitura   = nullptr;
static BufferCircular<std::string>* buffer_posicao_tratada_leitura = nullptr;
static std::mutex* mtx_posicao_tratada_ptr = nullptr;

// ------------------------------------------------------------------
// Configuração (igual à tarefa de sensores, mas para leitura)
// ------------------------------------------------------------------
void leitura_posicao_config(BufferCircular<std::string>* buffer_bruta,
                            BufferCircular<std::string>* buffer_tratada,
                            std::mutex&     mtx)
{
    buffer_posicao_bruta_leitura   = buffer_bruta;
    buffer_posicao_tratada_leitura = buffer_tratada;
    mtx_posicao_tratada_ptr        = &mtx;

    std::cout << "[leitura_posicao] Tarefa configurada.\n";
}

// ------------------------------------------------------------------
// Tarefa que roda em uma thread: lê o buffer continuamente
// ------------------------------------------------------------------
void tarefa_leitura_posicao_run()
{
    if (!buffer_posicao_bruta_leitura || !buffer_posicao_tratada_leitura) {
        std::cerr << "[leitura_posicao] ERRO: buffers não configurados.\n";
        return;
    }

    std::string dado;

    while (true)
    {
        // ----------------------------------------------------------
        // 1) LER DO BUFFER BRUTO
        // ----------------------------------------------------------
        if (buffer_posicao_bruta_leitura->retirar(dado)) {
            std::cout << "\n=== [BRUTO] ===\n";
            std::cout << dado << "\n";
        }

        // ----------------------------------------------------------
        // 2) LER DO BUFFER TRATADO (mutex obrigatório)
        // ----------------------------------------------------------
        {
            std::lock_guard<std::mutex> lock(*mtx_posicao_tratada_ptr);
            if (buffer_posicao_tratada_leitura->retirar(dado)) {
                std::cout << "*** [FILTRADO] ***\n";
                std::cout << dado << "\n";
            }
        }

        // reduz o uso da CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

} // namespace atr
