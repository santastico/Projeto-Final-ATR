/**
 * @file NotificadorEventos.cpp
 * @brief Declaração da classe NotificadorEventos.
 *
 * @objetivo Fornecer um mecanismo de sincronização thread-safe para 
 * que a tarefa de Monitoramento de Falhas possa "acordar" 
 * ou "notificar" as outras tarefas (Lógica, Controle, Coletor) 
 * instantaneamente sobre a ocorrência de um evento de falha.
 *
 * @mecanismo (Interno)
 * Esta classe encapsula um 'std::mutex' e uma 'std::condition_variable'
 * para implementar um padrão "publish-subscribe" de eventos.
 *
 * @entradas (Inputs) - Para a thread que espera
 * 1. Chamada de 'esperar_evento()' pelas threads assinantes 
 * (Lógica de Comando, Controle de Navegação, Coletor de Dados).
 *
 * @saidas (Outputs) - Para a thread que dispara
 * 1. Chamada de 'disparar_evento()' pela thread publicadora 
 * (Monitoramento de Falhas).
 */