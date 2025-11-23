#include "tarefas.h"
#include "Buffer_Circular.h"

#include <mqtt/async_client.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <deque>
#include <mutex>
#include <cmath>    // std::round

using json = nlohmann::json;

namespace atr {

// ---------------------------------------------------------------------
// Variáveis globais usadas pela tarefa
// (armazenam o que o main passar em tratamento_sensores)
// ---------------------------------------------------------------------

// ponteiros para os buffers criados no main
static BufferCircular<std::string>* buffer_posicao_bruta   = nullptr;
static BufferCircular<std::string>* buffer_posicao_tratada = nullptr;

// ponteiro para o mutex criado no main
static std::mutex* mtx_posicao_tratada_ptr = nullptr;

// id do caminhão (vem do main)
static int caminhao_id_global = 1;

// ordem da média móvel (quantas amostras usar)
static const std::size_t ORDEM_MEDIA = 5;

// filas para armazenar as últimas amostras inteiras
static std::deque<int> janela_x;
static std::deque<int> janela_y;
static std::deque<int> janela_ang;
static std::deque<int> janela_temp;

// ---------------------------------------------------------------------
// Função chamada pelo main para configurar a tarefa
// ---------------------------------------------------------------------

void tratamento_sensores(BufferCircular<std::string>* buffer_posicao_bruta_param,
                         BufferCircular<std::string>* buffer_posicao_tratada_param,
                         std::mutex&     mtx_posicao_tratada,
                         int             caminhao_id)
{
    // guarda os ponteiros / valores em variáveis globais
    buffer_posicao_bruta    = buffer_posicao_bruta_param;
    buffer_posicao_tratada  = buffer_posicao_tratada_param;
    mtx_posicao_tratada_ptr = &mtx_posicao_tratada;
    caminhao_id_global      = caminhao_id;

    std::cout << "[tratamento_sensores] Configurado para caminhao_id = "
              << caminhao_id_global << std::endl;
}


// ---------------------------------------------------------------------
// Função auxiliar: média móvel com inteiros
// (soma inteira / número de amostras)
// ---------------------------------------------------------------------

static int media_movel(std::deque<int>& janela, int novo_valor)
{
    janela.push_back(novo_valor);

    // mantém no máximo ORDEM_MEDIA amostras
    if (janela.size() > ORDEM_MEDIA) {
        janela.pop_front();
    }

    int soma = 0;
    for (int v : janela) {
        soma += v;
    }

    int quantidade = static_cast<int>(janela.size());
    if (quantidade == 0) return 0;

    // média inteira (se quiser “arredondar”, pode usar double + round)
    return soma / quantidade;
}


// ---------------------------------------------------------------------
// Função que processa UMA mensagem JSON vinda do simulador
// - escreve BRUTO no buffer_posicao_bruta
// - filtra, escreve FILTRADO no buffer_posicao_tratada
// ---------------------------------------------------------------------

static void processar_mensagem(const std::string& texto_json)
{
    if (!buffer_posicao_bruta || !buffer_posicao_tratada || !mtx_posicao_tratada_ptr) {
        std::cerr << "[tratamento_sensores] ERRO: tarefa nao configurada.\n";
        return;
    }

    // 1) Tenta gravar JSON BRUTO no buffer_posicao_bruta
    // Se falhar (cheio), ignoramos o retorno aqui pois vamos tratar no passo 2
    buffer_posicao_bruta->escrever(texto_json);

    // 2) Verifica se encheu (Lógica de Batch / Decimação)
    // O buffer bruto deve ter sido inicializado com capacidade = 10 no main
    if (buffer_posicao_bruta->estaCheio()) {
        
        // Variáveis para acumular a média do lote
        double soma_x = 0.0, soma_y = 0.0, soma_ang = 0.0, soma_temp = 0.0;
        int count = 0;
        
        // Armazena o último JSON para usar como base (metadata como truck_id)
        json ultimo_json_valido;
        std::string item_retirado;

        // 3) Esvazia o buffer e acumula valores
        while (buffer_posicao_bruta->retirar(item_retirado)) {
            try {
                json dados = json::parse(item_retirado, nullptr, false);
                if (!dados.is_discarded()) {
                    // Acumula valores
                    soma_x += dados.value("i_posicao_x", 0.0);
                    soma_y += dados.value("i_posicao_y", 0.0);
                    soma_ang += dados.value("i_angulo_x", 0.0);
                    soma_temp += dados.value("i_temperatura", 0.0);
                    
                    ultimo_json_valido = dados; // Guarda o último para copiar metadados depois
                    count++;
                }
            } catch (...) {
                // Ignora JSON malformado neste lote
            }
        }

        // 4) Se processou itens válidos, calcula média e escreve no tratado
        if (count > 0) {
            double media_x = soma_x / count;
            double media_y = soma_y / count;
            double media_ang = soma_ang / count;
            double media_temp = soma_temp / count;

            // Função auxiliar de arredondamento (3 casas)
            auto arred3 = [](double v) { return std::round(v * 1000.0) / 1000.0; };

            // Monta JSON Tratado usando o último como base
            json dados_filtrados = ultimo_json_valido;
            dados_filtrados["f_posicao_x"]   = arred3(media_x);
            dados_filtrados["f_posicao_y"]   = arred3(media_y);
            dados_filtrados["f_angulo_x"]    = arred3(media_ang);
            dados_filtrados["f_temperatura"] = arred3(media_temp);
            dados_filtrados["ordem_media"]   = count; // Indica quantos itens compuseram esta média

            std::string texto_filtrado = dados_filtrados.dump();

            // 5) Escreve no buffer tratado (com Mutex)
            {
                std::lock_guard<std::mutex> trava(*mtx_posicao_tratada_ptr);
                // Se buffer tratado encher, descartamos a amostra mais antiga ou ignoramos (aqui ignoramos)
                if (!buffer_posicao_tratada->escrever(texto_filtrado)) {
                    std::cerr << "[tratamento_sensores] buffer_posicao_tratada CHEIO ao gravar lote.\n";
                }
            }

            std::cout << "[LOTE PROCESSADO] Média de " << count << " amostras gravada no Buffer Tratado.\n";
            // std::cout << "[FILTRADO] " << texto_filtrado << "\n";
        }
    }
    // Se não estava cheio, apenas acumulamos (já foi escrito no passo 1)
}



// ---------------------------------------------------------------------
// Função que roda na thread (loop principal da tarefa)
// ---------------------------------------------------------------------

void tarefa_tratamento_sensores_run(const std::string& broker_uri)
{
    if (!buffer_posicao_bruta || !buffer_posicao_tratada || !mtx_posicao_tratada_ptr) {
        std::cerr << "[tratamento_sensores] ERRO: chame tratamento_sensores() antes de criar a thread.\n";
        return;
    }

    std::string id_cliente = "tratamento_sensores_all";
    const std::string topico = "atr/+/sensor/raw";   // único tópico, com wildcard

    mqtt::async_client cliente(broker_uri, id_cliente);
    mqtt::connect_options opcoes;
    opcoes.set_clean_session(true);

    try {
        std::cout << "[tratamento_sensores] Conectando em " << broker_uri << "...\n";
        cliente.connect(opcoes)->wait();
        std::cout << "[tratamento_sensores] Conectado.\n";

        cliente.start_consuming();
        cliente.subscribe(topico, 1)->wait();
        std::cout << "[tratamento_sensores] Assinado topico " << topico << "\n";

        while (true) {
            auto msg = cliente.consume_message();
            if (!msg) {
                if (!cliente.is_connected()) {
                    std::cerr << "[tratamento_sensores] Conexao MQTT perdida.\n";
                    break;
                }
                continue;
            }
            std::string texto = msg->to_string();
            processar_mensagem(texto);
        }

        cliente.stop_consuming();
        cliente.disconnect()->wait();
    }
    catch (const std::exception& e) {
        std::cerr << "[tratamento_sensores] ERRO MQTT: " << e.what() << "\n";
    }

    std::cout << "[tratamento_sensores] Thread encerrada.\n";
}

} // namespace atr
