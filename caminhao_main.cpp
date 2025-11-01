#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>
#include <vector>

bool e_defeito = false;
bool e_automatico = false;
bool c_automatico = false;
bool c_man = false;
bool c_rearme = false;
int c_acelera = 0;
int c_direita = 0;
int c_esquerda = 0;

struct Dados {
    int i_posicao_x = 0;
    int i_posicao_y = 0;
    int i_angulo_x = 0;
    int i_temperatura = 0;
    bool i_falha_eletrica = false;
    bool i_falha_hidraulica = false;
    int o_aceleracao = 0;
    int o_direcao = 0; 
};   


void TrataSensores() {
    
}

void Monitora_Falhas() {
    
}

void ColetaDados() {
    
}

void Logica_Comando() {
    
}


int main()  {
    
}