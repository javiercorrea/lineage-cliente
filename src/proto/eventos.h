// Eventos: cada paquete del servidor se convierte en un Evento POD de tamaño fijo que se aplica
// en lote al inicio del frame. Los textos son vistas a la arena del frame (válidas hasta su reset).
#pragma once
#include <string_view>
#include <vector>
#include "core/arena.h"
#include "proto/opcodes.h"

namespace lin {

enum class Ev : u8 {
    DESCONECTADO,
    VERSION_SERVIDOR, RESULTADO_LOGIN, NOTICIAS, CANTIDAD_PERSONAJES, PERSONAJE, RESULTADO_CREAR,
    PERSONAJE_NUEVO, PERSONAJE_BORRADO, ENTRAR_JUEGO,
    ESTADO_PROPIO, MAPA, PONER_OBJETO, MOVER_OBJETO, QUITAR_OBJETO, CAMBIAR_DIRECCION, LUZ,
    HORA_JUEGO, CLIMA, HP, MP, MENSAJE_SERVIDOR, CHAT, ACCION_OBJETO, CAMBIO_VISUAL,
    EXPERIENCIA, LEGAL, SP_MR, DEFENSA, ATRIBUTOS, PESO, COMIDA, ATAQUE, HP_OBJETO,
    INVENTARIO_VACIAR, ITEM, ITEM_QUITAR, ITEM_NOMBRE, ITEM_CANTIDAD, ITEM_BENDICION,
    HECHIZOS, EFECTO, VELOCIDAD, ICONO_EFECTO, CAMBIO_FORMA, HTML, TELETRANSPORTE, LISTA_TIENDA, ITEM_TIENDA,
    PREGUNTA, INTERCAMBIO_ABRIR, INTERCAMBIO_ITEM, INTERCAMBIO_FIN, TITULO, EMBLEMA, MAGIA_EN_VENTA,
    PUERTA, PARALISIS, VENENO, CEGUERA, PEDIR_CANTIDAD, MASCOTA, ELEGIR_OBJETIVO_MASCOTA, MARCA_OBJETIVO,
    MARCADOR,
    IGNORADO,
};

// Índices en Evento::v según el tipo (documentados en decodificar.cpp junto a cada paquete).
struct Evento {
    Ev tipo;
    Srv origen;
    u16 extra;
    u32 id;
    i32 v[16];
    std::string_view s[2];
};
static_assert(sizeof(Evento) <= 112, "Evento debe seguir siendo pequeño y POD");

// Campos de PERSONAJE / PERSONAJE_NUEVO (lista de personajes).
enum : int { PJ_CLASE, PJ_SEXO, PJ_LEGAL, PJ_HP, PJ_MP, PJ_CA, PJ_NIVEL, PJ_FUE, PJ_DES, PJ_CON, PJ_SAB, PJ_CAR, PJ_INT };
// Campos de PONER_OBJETO.
// OB_CANTIDAD: el campo "exp" de S_CharPack, que en los ítems del suelo (S_DropItem) es la cantidad.
// OB_CLAN: id del clan (0 sin clan; para pedir su emblema).
enum : int { OB_X, OB_Y, OB_GFX, OB_ESTADO, OB_DIR, OB_LUZ, OB_VEL, OB_LEGAL, OB_FLAGS, OB_HP, OB_CANTIDAD, OB_CLAN };
// Campos de ESTADO_PROPIO.
enum : int { EP_NIVEL, EP_EXP, EP_FUE, EP_INT, EP_SAB, EP_DES, EP_CON, EP_CAR, EP_HP, EP_HPMAX, EP_MP, EP_MPMAX,
             EP_CA, EP_COMIDA, EP_PESO, EP_LEGAL };
// Campos de DEFENSA (S_OwnCharAttrDef): CA y resistencias elementales.
enum : int { DF_CA, DF_FUEGO, DF_AGUA, DF_VIENTO, DF_TIERRA };
// Campos de MENSAJE_SERVIDOR: id en string-e.tbl, cantidad de argumentos (s[0], s[1]) y si es
// un mensaje azul (S_BlueMessage, mismo formato).
enum : int { MS_ID, MS_NARGS, MS_AZUL };
// Campos de ITEM (id = id del ítem, s[0] = nombre visible, con "$N" y sufijos como " ($9)").
// ITEM_CANTIDAD usa IT_CANTIDAD y s[0]; ITEM_BENDICION, IT_BENDICION.
// ITEM e ITEM_CANTIDAD: s[0] el nombre, s[1] los bytes de atributos (vacío si no está identificado).
enum : int { IT_USO, IT_CARGAS, IT_GFX, IT_BENDICION, IT_CANTIDAD, IT_IDENTIFICADO };
// Campos de ATAQUE (id = atacante): objetivo, acción del atacante, si acertó y su dirección. Los
// hechizos de ataque (S_UseAttackSkill) y las flechas (S_UseArrowSkill) traen además el gráfico
// del proyectil (AT_GFX, 0 si no hay) y las celdas de origen y destino.
enum : int { AT_OBJETIVO, AT_ACCION, AT_GOLPE, AT_DIR, AT_GFX, AT_X0, AT_Y0, AT_X1, AT_Y1 };
// HECHIZOS (S_AddSkill / S_DelSkill): HZ_AGREGAR (1) o quitar (0) y, desde HZ_FILAS, las
// FILAS_HECHIZOS filas de bits empaquetadas de a 4 bytes por entero (fila r, bit c = hechizo r*8+c+1).
constexpr int FILAS_HECHIZOS = 28;
enum : int { HZ_AGREGAR, HZ_FILAS };
// EFECTO (S_SkillSound, S_EffectLocation, S_RangeSkill): gráfico sobre un objeto (id) o en una
// celda (id = 0).
enum : int { EF_GFX, EF_X, EF_Y };
// VELOCIDAD (S_SkillHaste / S_SkillBrave; id = objeto): clase (VE_HASTE o VE_BRAVE), tipo
// (haste: 0 normal, 1 rápido, 2 lento; brave: 0 nada, 1 poción, 3 waffle de elfo, 4 caminar
// rápido, 6 bloodlust) y segundos que dura (0 en lo que se manda a los demás).
enum : int { VE_CLASE, VE_TIPO, VE_SEG };
enum : int { VE_HASTE, VE_BRAVE };
// ICONO_EFECTO (S_SkillIconShield, S_SkillIconAura, S_SkillIconGFX): clase, tipo y segundos
// (0 = se terminó).
enum : int { IE_CLASE, IE_TIPO, IE_SEG };
enum : int { IE_ESCUDO, IE_AURA, IE_ICONO };
// CAMBIO_FORMA (S_ChangeShape; id = objeto): gfx nuevo y si conserva el arma (0 = se la sacó).
enum : int { CF_GFX, CF_ARMA };
// HTML (S_NPCTalkReturn / S_ShowHTML; id = objeto que la muestra): s[0] = id de la página
// ("<id>-e.html" en Text.pak; "monlist": lista de formas de Shape Change), s[1] = los datos que
// reemplazan a <var src="#N">, separados por SEPARADOR_DATOS; HT_DATOS = cuántos son.
constexpr char SEPARADOR_DATOS = '\x01';
enum : int { HT_DATOS };
// TELETRANSPORTE (S_Teleport; id = personaje): el servidor espera C_Teleport para moverlo.
// LISTA_TIENDA (id = NPC) abre una lista: LT_TIPO (TipoLista) y LT_CANTIDAD; le siguen tantos
// ITEM_TIENDA (id = índice en la tienda o id del ítem, IT_GFX, TI_PRECIO, IT_CANTIDAD,
// IT_BENDICION, s[0] = nombre).
enum : int { LT_TIPO, LT_CANTIDAD };
// LISTA_REPARAR (S_SelectList / S_FixWeaponList): las armas dañadas; IT_CANTIDAD = el daño y
// TI_PRECIO = lo que cuesta repararla (200 adena por punto).
enum TipoLista : u8 { LISTA_COMPRA, LISTA_VENTA, LISTA_RETIRO, LISTA_MASCOTA, LISTA_REPARAR };
constexpr int TI_PRECIO = 6;  // en Evento::v, después de los campos IT_*
// PREGUNTA (S_MessageYN): una pregunta de sí o no. PR_NUMERO y PR_TIPO (id en string-e.tbl: 252
// intercambio, 953/954 grupo, 97 entrar al clan...) se devuelven en la respuesta (C_Attr);
// s[0], s[1] = los argumentos del mensaje.
enum : int { PR_NUMERO, PR_TIPO };
// INTERCAMBIO_ABRIR (S_Trade; s[0] = con quién), INTERCAMBIO_ITEM (S_TradeAddItem: IN_LADO 0 lo
// propio, 1 lo del otro; IN_GFX, IN_BENDICION, s[0] = nombre con la cantidad) e
// INTERCAMBIO_FIN (S_TradeStatus: IN_CANCELADO).
enum : int { IN_LADO, IN_GFX, IN_BENDICION };
// MAGIA_EN_VENTA (S_SkillBuy): los hechizos que el maestro de magia puede enseñar, como máscara
// de índices (índice i = skill id i + 1): bits 0–31 en MV_MASCARA y 32–63 en MV_MASCARA + 1.
enum : int { MV_MASCARA };
enum : int { IN_CANCELADO };
// TITULO (S_CharTitle; id = personaje, s[0] = título). EMBLEMA (S_Emblem; id = clan, s[0] = los
// 384 bytes: 16 × 12 píxeles RGB555).
constexpr size_t BYTES_EMBLEMA = 384;
// PUERTA (S_Door): la celda (PU_X, PU_Y) de una puerta se puede pasar o no (PU_BLOQUEADA).
enum : int { PU_X, PU_Y, PU_BLOQUEADA };
// PARALISIS (S_Paralysis): v[0] = subtipo (ver Paralisis en mundo.h). VENENO (S_Poison; id =
// objeto): VE_VERDE (veneno) y VE_GRIS (congelado). CEGUERA (S_CurseBlind): v[0] 0 ve, 1 ciego,
// 2 ciego con Floating Eye.
enum : int { VE_VERDE, VE_GRIS };
// PEDIR_CANTIDAD (S_InputAmount; id = NPC u objeto): PC_PRECIO, PC_INICIAL, PC_MIN, PC_MAX y
// s[0] = la acción que se devuelve con la cantidad (C_Amount), s[1] = el nombre del NPC si viene.
enum : int { PC_PRECIO, PC_INICIAL, PC_MIN, PC_MAX };
// MASCOTA (S_PetCtrlMenu; id = mascota): MA_AGREGAR (1 entra a la lista, 0 sale), s[0] = nombre.
// ELEGIR_OBJETIVO_MASCOTA (S_SelectTarget; id = mascota): el servidor pide elegir a quién ataca.
// MARCA_OBJETIVO (S_TrueTarget; id = objetivo marcado, s[0] = mensaje).
enum : int { MA_AGREGAR };
// MARCADOR (S_Bookmarks; id = marcador, s[0] = nombre): un lugar recordado para teletransportarse.
enum : int { MK_MAPA, MK_X, MK_Y };
// CHAT: v[0] = tipo de chat del servidor (TipoChat), id = quien habla (0 si no se sabe),
// s[0] = texto ya armado por el servidor ("Nombre: hola"), s[1] = remitente (solo susurros).
enum TipoChat : u8 {
    CHAT_NORMAL = 0, CHAT_GRITO = 2, CHAT_GLOBAL = 3, CHAT_CLAN = 4, CHAT_SISTEMA = 9,  // 9 también es el eco del susurro enviado
    CHAT_GRUPO = 11, CHAT_COMERCIO = 12, CHAT_ALIANZA = 13, CHAT_GRUPO_CHAT = 14, CHAT_SUSURRO = 16,
};

}  // namespace lin
