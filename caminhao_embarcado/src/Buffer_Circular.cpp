/**
 * @file BufferCircular.cpp
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
 *
 * @entradas (Inputs) - (Métodos de Escrita: 'set_...')
 * - 'tratamento_sensores' (escreve posição tratada)
 * - 'logica_comando' (escreve estados 'e_defeito', 'e_automatico')
 * - 'coletor_dados' (escreve comandos 'c_man', 'c_rearme')
 * - 'controle_navegacao' (escreve 'velocidade', 'posicao_angular')
 * - 'planejamento_rota' (escreve 'setpoint_velocidade')
 *
 * @saidas (Outputs) - (Métodos de Leitura: 'get_...')
 * - 'logica_comando' (lê posição, comandos, valores de controle)
 * - 'coletor_dados' (lê estados, posição)
 * - 'controle_navegacao' (lê estados, setpoints)
 * - 'planejamento_rota' (lê posição)
 */