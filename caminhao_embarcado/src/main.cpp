/**
 * @file main.cpp
 * @brief Ponto de entrada (entry point) do software embarcado do caminhão.
 *
 * @objetivo Orquestrar o início do processo do caminhão. 
 * É responsável por:
 * 1. Instanciar os objetos de estado compartilhado (BufferCircular, 
 * NotificadorEventos).
 * 2. Criar e lançar as 6 threads de tarefas principais 
 * (sensores, falhas, logica, etc.).
 * 3. Passar as referências dos objetos compartilhados para as threads.
 * 4. Manter o processo principal vivo (aguardando as threads com .join()).
 *
 * @entradas (Inputs)
 * 1. Argumentos de Linha de Comando (argv): Recebe o ID do caminhão 
 * (ex: "./caminhao_embarcado 1") para que as threads saibam 
 * em quais tópicos MQTT se conectar.
 *
 * @saidas (Outputs)
 * 1. Processo em Execução: Lança as 6 threads (sensores, falhas, 
 * logica, coletor, navegacao, planejamento) que compõem o 
 * sistema funcional do caminhão.
 */