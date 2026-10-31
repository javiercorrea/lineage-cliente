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
    MARCADOR, INVISIBLE, RESUCITAR, NOMBRE_ROSA, LICOR, ANILLO, REINICIO, GRUPO, BLOQUEADO, SONIDO, TIENDA, TIENDA_LINEA, TABLON, TABLON_TEMA, TABLON_LEER, CORREO,
    CASAS, CASA, CASA_MAPA, GUERRA, ASEDIO, SENOR_CASTILLO, CASTILLO_FONDOS, FECHA_ASEDIO,
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
// OB_NIVEL: el nivel del monstruo (en los personajes, 0).
enum : int { OB_X, OB_Y, OB_GFX, OB_ESTADO, OB_DIR, OB_LUZ, OB_VEL, OB_LEGAL, OB_FLAGS, OB_HP, OB_CANTIDAD, OB_CLAN, OB_NIVEL };
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
// EF_AREA: gráfico de un hechizo de área alrededor del lanzador (S_RangeSkill tipo 0), en su celda
// (EF_X, EF_Y): lo reparte el cliente según el hechizo (dibujar_area en hechizos_ui.cpp).
enum : int { EF_GFX, EF_X, EF_Y, EF_AREA };
// VELOCIDAD (S_SkillHaste / S_SkillBrave; id = objeto): clase (VE_HASTE o VE_BRAVE), tipo
// (haste: 0 normal, 1 rápido, 2 lento; brave: 0 nada, 1 poción, 3 waffle de elfo, 4 caminar
// rápido, 6 bloodlust) y segundos que dura (0 en lo que se manda a los demás).
enum : int { VE_CLASE, VE_TIPO, VE_SEG };
enum : int { VE_HASTE, VE_BRAVE };
// ICONO_EFECTO (S_SkillIconShield, S_SkillIconAura, S_SkillIconGFX): clase, tipo y segundos
// (0 = se terminó).
enum : int { IE_CLASE, IE_TIPO, IE_SEG };
// GRUPO (S_PacketBox 104–110): GR_TIPO GR_VACIAR (llega la lista entera), GR_MIEMBRO (e.id, s[0]
// nombre, GR_HP en % o 0xFF, GR_LIDER), GR_LIDER_NUEVO (e.id). BLOQUEADO (S_PacketBox 17–19): s[0]
// el nombre, v[0] 1 agregado / 0 quitado. SONIDO (S_Sound): v[0] el sonido.
enum : int { GR_TIPO, GR_HP, GR_LIDER };
// TIENDA (S_PrivateShop): e.id el vendedor, TP_TIPO 0 lo que vende / 1 lo que compra; después una
// TIENDA_LINEA por línea: TP_ORDEN, TP_CANTIDAD, TP_PRECIO, y en las de venta TP_GFX, TP_BENDICION y
// s[0] el nombre; en las de compra TP_ITEM, el ítem propio que se le puede vender.
// ACCION_OBJETO con la acción 70 (tienda abierta): s[0] el título.
// TABLON (S_Board): e.id el tablón; una TABLON_TEMA por tema: e.id el tema, s[0] "autor fecha",
// s[1] el título. TABLON_LEER (S_BoardRead): e.id el tema, s[0] "autor  fecha  título", s[1] el texto.
// CORREO (S_Mail): CO_TIPO el subtipo del paquete (0–2 lista: una CORREO por carta con CO_LISTA = 1;
// 0x10–0x12 el texto; 0x20/0x21 resultado del envío en CO_OK; 0x30–0x32 y 0x40 borrada o
// guardada; 0x50 aviso de carta nueva, CO_ENVIADA = es la copia propia). e.id la carta, CO_LEIDA,
// CO_FECHA (segundos), CO_ENVIADA, s[0] el otro (remitente o destinatario), s[1] el asunto o el texto.
// CASAS (S_HouseList): e.id el tablón de subastas; una CASA por casa: e.id la casa, s[0] el nombre,
// CS_AREA, CS_MES y CS_DIA (cierre de la subasta) y CS_PRECIO (puja actual).
// CASA_MAPA (S_HouseMap): e.id el tablón, v[0] la casa.
enum : int { CS_AREA, CS_MES, CS_DIA, CS_PRECIO };
// GUERRA (S_War): GU_TIPO (1 declara, 2 se rinde, 3 termina, 4 gana, 6/7 alianza, 8 "en guerra
// con"), s[0] y s[1] los clanes. ASEDIO (S_PacketBox 0–4): GU_TIPO (0 empieza, 1 termina, 2 sigue,
// 3 iniciativa, 4 ocupado) y GU_CASTILLO (1–8; 0 en 3 y 4).
enum : int { GU_TIPO, GU_CASTILLO };
// SENOR_CASTILLO (S_CastleMaster): e.id el personaje, v[0] el castillo (0 = ya no es señor).
// CASTILLO_FONDOS: e.id el personaje; CF_TIPO CF_TASA (S_TaxRate: CF_A mínimo, CF_B máximo),
// CF_DEPOSITAR (S_Deposit) o CF_RETIRAR (S_Drawal: CF_B los fondos).
enum : int { CF_TIPO, CF_A, CF_B };
enum : int { CF_TASA, CF_DEPOSITAR, CF_RETIRAR };
// FECHA_ASEDIO (S_WarTime): FA_N horarios desde FA_MINUTOS, en minutos desde la época Unix (hora
// local del servidor); s[0] la zona horaria.
enum : int { FA_N, FA_MINUTOS };
enum : int { CO_TIPO, CO_LISTA, CO_LEIDA, CO_FECHA, CO_ENVIADA, CO_OK };
enum : int { TP_TIPO, TP_ORDEN, TP_CANTIDAD, TP_PRECIO, TP_GFX, TP_BENDICION, TP_ITEM };
enum : int { GR_VACIAR, GR_MIEMBRO, GR_LIDER_NUEVO };
// REINICIO (S_CharReset, reiniciar atributos con la Candle of Reminiscence): RE_TIPO 1 empieza
// (RE_HP, RE_MP, RE_AC, RE_MAX), 2 estado tras cada paso (RE_NIVEL, RE_MAX, RE_HP, RE_MP, RE_AC y los
// 6 atributos desde RE_FUE: fue, int, sab, des, con, car), 3 faltan puntos de elixir (RE_PUNTOS).
enum : int { RE_TIPO, RE_NIVEL, RE_MAX, RE_HP, RE_MP, RE_AC, RE_FUE, RE_PUNTOS = RE_FUE + 6 };
enum : int { IE_ESCUDO, IE_AURA, IE_ICONO, IE_FUERZA, IE_DESTREZA, IE_EVA };
// CAMBIO_FORMA (S_ChangeShape; id = objeto): gfx nuevo y si conserva el arma (0 = se la sacó).
enum : int { CF_GFX, CF_ARMA };
// HTML (S_NPCTalkReturn / S_ShowHTML; id = objeto que la muestra): s[0] = id de la página
// ("<id>-e.html" en Text.pak; "monlist": lista de formas de Shape Change), s[1] = los datos que
// reemplazan a <var src="#N">, separados por SEPARADOR_DATOS; HT_DATOS = cuántos son.
// "agsel" (S_AuctionBoardRead, una casa del tablón de subastas) trae además la casa: HT_CASA.
constexpr char SEPARADOR_DATOS = '\x01';
enum : int { HT_DATOS, HT_CASA };
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
