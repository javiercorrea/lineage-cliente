# Uso: cmake -DBIN=<ejecutable> -P sin_control.cmake
# Falla si el ejecutable incluye el control en vivo (el texto de la ruta del socket).
file(STRINGS "${BIN}" encontrado REGEX "lineage-control-" LIMIT_COUNT 1)
if(encontrado)
    message(FATAL_ERROR "${BIN} incluye el control en vivo (LIN_CONTROL): no puede ser una versión final")
endif()
