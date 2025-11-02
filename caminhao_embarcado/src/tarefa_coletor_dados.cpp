/**
 * @file tarefa_coletor_dados.cpp
 * @brief Implementação da thread Coletor de Dados.
 *
 * @objetivo Servir como a "caixa preta" e interface local do caminhão. 
 * É responsável por:
 * 1. Receber comandos do operador (via IPC) e escrevê-los no Buffer Circular.
 * 2. Ler o estado, posição e falhas e salvar tudo em arquivos de log.
 * 3. Enviar o estado atual do caminhão de volta para a Interface Local (via IPC).
 *
 * @entradas (Inputs)
 * 1. Buffer Circular (leitura): Lê "e_defeito", "e_automatico", "i_pos_x", 
 * "i_pos_y", "i_angulo_x".
 * 2. Notificador de Eventos (recebimento): Recebe eventos de falha.
 * 3. IPC (recebimento): Recebe comandos da Interface Local 
 * (ex: "c_automatico", "c_man", "c_rearme").
 *
 * @saidas (Outputs)
 * 1. Buffer Circular (escrita): Escreve os comandos recebidos da 
 * Interface Local (ex: "c_automatico", "c_man").
 * 2. Armazenamento (escrita): Salva logs de dados e eventos em arquivo.
 * 3. IPC (envio): Envia dados de estado (posição, falhas, modo) 
 * para a Interface Local.
 */