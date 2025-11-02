/**
 * @file tarefa_monitoramento_falhas.cpp
 * @brief Implementação da thread Monitoramento de Falhas.
 *
 * @objetivo Responsável por ler os dados brutos dos sensores de falha 
 * vindos do simulador, analisar esses dados (ex: verificar limites de 
 * temperatura) e disparar eventos de falha/alerta para as outras tarefas.
 *
 * @entradas (Inputs)
 * 1. MQTT (subscribe): Assina os tópicos "i_temperatura", "i_falha_hidraulica"
 * e "i_falha_eletrica".
 *
 * @saidas (Outputs)
 * 1. Notificador de Eventos (disparo): Dispara eventos 
 * (ex: "alerta_termico", "falha_termica") para as threads 
 * Logica de Comando, Controle de Navegação e Coletor de Dados.
 */