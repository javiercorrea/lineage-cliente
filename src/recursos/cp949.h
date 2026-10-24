// Texto coreano CP949 (EUC-KR). El servidor manda nombres y mensajes en CP949 (ClientLanguage=1).
// Para dibujar no hace falta Unicode: el código KS X 1001 da directamente el índice del glifo en
// kor.fnt. La conversión a Unicode (para entrada IME y registros) queda pendiente.
#pragma once
#include "core/tipos.h"

namespace lin {

// Índice del glifo en kor.fnt para el par CP949 (b1, b2), o -1 si la fuente no lo tiene
// (hanja, símbolos y las sílabas extendidas de CP949 fuera de KS X 1001).
int indice_glifo_coreano(u8 b1, u8 b2);

}  // namespace lin
