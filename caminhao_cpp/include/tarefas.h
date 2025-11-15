#pragma once

#include <string>

class BufferCircular; // forward global (definida em outro header)

namespace atr {

// Tratamento de sensores
void tratamento_sensores(BufferCircular* buffer_ptr, int caminhao_id);
void tarefa_tratamento_sensores_run(const std::string& broker = "localhost");

} // namespace atr
