#include <cmath>
#include <algorithm>
#include <cstdlib>
#include "render/gpu.h"

namespace lin {

namespace {

// MULTIPLATAFORMA: SDL_GPU usa Metal en macOS y por eso este shader está en MSL. Para Vulkan
// (Linux) y D3D12 (Windows) hay que escribirlo en HLSL y compilarlo con SDL_shadercross a SPIR-V
// y DXIL, y elegir el formato con SDL_GetGPUShaderFormats(). Convención de recursos de SDL_GPU
// para MSL: buffer(0) = uniformes, buffer(1) = primer storage buffer; texture(0)/sampler(0).
const char* SHADER_MSL = R"(
#include <metal_stdlib>
using namespace metal;

struct Instancia { float4 destino; float4 uv; uint pagina; uint color; uint mezcla; uint relleno; };
struct Uniformes { float2 pantalla; float2 relleno; };
struct Salida {
    float4 pos [[position]];
    float2 uv;
    float4 color;
    uint pagina [[flat]];
    uint mezcla [[flat]];
};

vertex Salida vs_sprite(uint vid [[vertex_id]], uint iid [[instance_id]],
                        constant Uniformes& u [[buffer(0)]],
                        const device Instancia* inst [[buffer(1)]]) {
    const float2 esquinas[6] = { float2(0,0), float2(1,0), float2(0,1), float2(1,0), float2(1,1), float2(0,1) };
    Instancia i = inst[iid];
    float2 e = esquinas[vid];
    float2 p = i.destino.xy + e * i.destino.zw;
    Salida o;
    o.pos = float4(p.x / u.pantalla.x * 2.0 - 1.0, 1.0 - p.y / u.pantalla.y * 2.0, 0.0, 1.0);
    o.uv = mix(i.uv.xy, i.uv.zw, e);
    o.color = unpack_unorm4x8_to_float(i.color);
    o.pagina = i.pagina;
    o.mezcla = i.mezcla;
    return o;
}

fragment float4 fs_sprite(Salida in [[stage_in]], texture2d_array<float> atlas [[texture(0)]],
                          sampler s [[sampler(0)]]) {
    float4 c = atlas.sample(s, in.uv, in.pagina) * in.color;
    if (c.a <= 0.0) discard_fragment();
    // Alfa premultiplicado: con alfa 0 el color se suma al fondo sin taparlo (mezcla aditiva).
    return float4(c.rgb * c.a, in.mezcla == 1 ? 0.0 : c.a);
}
)";

}  // namespace

bool Gpu::iniciar(SDL_Window* ventana, int ancho_logico, int alto_logico) {
    ventana_ = ventana;
    ancho_ = ancho_min_ = ancho_logico;
    alto_ = alto_min_ = alto_logico;
    // MULTIPLATAFORMA: agregar SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL cuando
    // existan los shaders compilados para esos backends.
    dev_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_MSL, false, nullptr);
    if (!dev_) {
        LIN_LOG("SDL_CreateGPUDevice: %s", SDL_GetError());
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(dev_, ventana_)) {
        LIN_LOG("SDL_ClaimWindowForGPUDevice: %s", SDL_GetError());
        return false;
    }
    SDL_SetGPUSwapchainParameters(dev_, ventana_, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
    if (!crear_pipeline()) return false;

    SDL_GPUTextureCreateInfo ti{};
    ti.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
    ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width = TAM_PAGINA;
    ti.height = TAM_PAGINA;
    ti.layer_count_or_depth = PAGINAS_ATLAS;
    ti.num_levels = 1;
    atlas_ = SDL_CreateGPUTexture(dev_, &ti);

    ajustar_vista(false, 0);
    crear_destino();

    SDL_GPUSamplerCreateInfo si{};
    si.min_filter = SDL_GPU_FILTER_NEAREST;
    si.mag_filter = SDL_GPU_FILTER_NEAREST;
    si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    si.address_mode_u = si.address_mode_v = si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    muestreo_ = SDL_CreateGPUSampler(dev_, &si);

    SDL_GPUBufferCreateInfo bi{};
    bi.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    bi.size = MAX_INSTANCIAS * sizeof(Instancia);
    instancias_gpu_ = SDL_CreateGPUBuffer(dev_, &bi);

    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = MAX_INSTANCIAS * sizeof(Instancia);
    subida_inst_ = SDL_CreateGPUTransferBuffer(dev_, &tb);
    tb.size = TAM_SUBIDA_ATLAS;
    subida_atlas_ = SDL_CreateGPUTransferBuffer(dev_, &tb);
    if (!atlas_ || !destino_ || !muestreo_ || !instancias_gpu_ || !subida_inst_ || !subida_atlas_) {
        LIN_LOG("Recursos GPU: %s", SDL_GetError());
        return false;
    }

    // Para probar el reemplazo: menos páginas, o solo un rincón de cada una.
    const char* paginas = std::getenv("LIN_ATLAS_PAGINAS");
    const char* tam = std::getenv("LIN_ATLAS_TAM");
    if (paginas || tam)
        empaquetado_ = EmpaquetadoAtlas(static_cast<u16>(tam ? std::clamp(std::atoi(tam), 256, TAM_PAGINA) : TAM_PAGINA),
                                        static_cast<u16>(paginas ? std::clamp(std::atoi(paginas), 2, PAGINAS_ATLAS) : PAGINAS_ATLAS));
    inst_.reserve(MAX_INSTANCIAS);
    claves_.reserve(MAX_INSTANCIAS);
    orden_.reserve(MAX_INSTANCIAS);
    orden_tmp_.reserve(MAX_INSTANCIAS);
    inst_ordenadas_.reserve(MAX_INSTANCIAS);
    staging_atlas_.reserve(TAM_SUBIDA_ATLAS);

    ImagenRGBA b;
    b.preparar(2, 2);
    for (u32& p : b.px) p = BLANCO;
    subir(b, blanco_);
    return true;
}

bool Gpu::crear_pipeline() {
    SDL_GPUShaderCreateInfo vs{};
    vs.code = reinterpret_cast<const Uint8*>(SHADER_MSL);
    vs.code_size = std::strlen(SHADER_MSL);
    vs.entrypoint = "vs_sprite";
    vs.format = SDL_GPU_SHADERFORMAT_MSL;
    vs.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    vs.num_uniform_buffers = 1;
    vs.num_storage_buffers = 1;
    SDL_GPUShader* v = SDL_CreateGPUShader(dev_, &vs);
    SDL_GPUShaderCreateInfo fs = vs;
    fs.entrypoint = "fs_sprite";
    fs.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    fs.num_uniform_buffers = 0;
    fs.num_storage_buffers = 0;
    fs.num_samplers = 1;
    SDL_GPUShader* f = SDL_CreateGPUShader(dev_, &fs);
    if (!v || !f) {
        LIN_LOG("SDL_CreateGPUShader: %s", SDL_GetError());
        return false;
    }
    SDL_GPUColorTargetDescription ct{};
    ct.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ct.blend_state.enable_blend = true;
    // Alfa premultiplicado (el shader multiplica): una sola mezcla sirve para la normal y la
    // aditiva, y todo sigue en una llamada de dibujo.
    ct.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    ct.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    ct.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    ct.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    ct.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    ct.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    SDL_GPUGraphicsPipelineCreateInfo pi{};
    pi.vertex_shader = v;
    pi.fragment_shader = f;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pi.target_info.color_target_descriptions = &ct;
    pi.target_info.num_color_targets = 1;
    pipeline_ = SDL_CreateGPUGraphicsPipeline(dev_, &pi);
    SDL_ReleaseGPUShader(dev_, v);
    SDL_ReleaseGPUShader(dev_, f);
    if (!pipeline_) LIN_LOG("SDL_CreateGPUGraphicsPipeline: %s", SDL_GetError());
    return pipeline_ != nullptr;
}

void Gpu::cerrar() {
    if (!dev_) return;
    SDL_WaitForGPUIdle(dev_);
    SDL_ReleaseGPUTransferBuffer(dev_, subida_atlas_);
    SDL_ReleaseGPUTransferBuffer(dev_, subida_inst_);
    SDL_ReleaseGPUBuffer(dev_, instancias_gpu_);
    SDL_ReleaseGPUSampler(dev_, muestreo_);
    SDL_ReleaseGPUTexture(dev_, destino_);
    SDL_ReleaseGPUTexture(dev_, atlas_);
    SDL_ReleaseGPUGraphicsPipeline(dev_, pipeline_);
    SDL_ReleaseWindowFromGPUDevice(dev_, ventana_);
    SDL_DestroyGPUDevice(dev_);
    dev_ = nullptr;
}

bool Gpu::subir(const ImagenRGBA& img, RegionAtlas& r) {
    if (img.vacia()) return false;
    size_t bytes = img.px.size() * 4;
    if (staging_atlas_.size() + bytes > TAM_SUBIDA_ATLAS) return false;  // reintentar el próximo frame
    if (!empaquetado_.reservar(img.ancho, img.alto, frame_, r)) {
        if (!atlas_lleno_avisado_) LIN_LOG("Atlas: no hay lugar para %ux%u", img.ancho, img.alto);
        atlas_lleno_avisado_ = true;
        return false;
    }
    r.w = img.ancho;
    r.h = img.alto;
    r.ox = img.ox;
    r.oy = img.oy;
    SubidaPendiente p{r, static_cast<u32>(staging_atlas_.size())};
    const u8* src = reinterpret_cast<const u8*>(img.px.data());
    staging_atlas_.insert(staging_atlas_.end(), src, src + bytes);
    pendientes_.push_back(p);
    return true;
}

void Gpu::dibujar_escalado(const RegionAtlas& r, float x, float y, float w, float h, u64 clave, u32 color, Mezcla mezcla) {
    if (!r.valida() || inst_.size() >= MAX_INSTANCIAS) return;
    if (x >= ancho_ || y >= alto_ || x + w <= 0 || y + h <= 0) return;  // fuera de pantalla
    empaquetado_.usar(r.pagina, frame_);
    // Al píxel real más cercano: con muestreo "nearest" el arte no se deforma en posiciones
    // fraccionarias (cada texel ocupa siempre escala × escala píxeles).
    const float e = static_cast<float>(escala_ > 0 ? escala_ : 1);
    x = std::round(x * e) / e;
    y = std::round(y * e) / e;
    constexpr float inv = 1.0f / TAM_PAGINA;
    inst_.push_back({x, y, w, h, r.x * inv, r.y * inv, (r.x + r.w) * inv, (r.y + r.h) * inv, r.pagina, color,
                     static_cast<u32>(mezcla), 0});
    claves_.push_back(clave);
}

void Gpu::dibujar(const RegionAtlas& r, int x, int y, u64 clave, u32 color) {
    dibujar_escalado(r, static_cast<float>(x + r.ox), static_cast<float>(y + r.oy), r.w, r.h, clave, color);
}

void Gpu::dibujar_en(const RegionAtlas& r, float x, float y, u64 clave, u32 color, Mezcla mezcla) {
    dibujar_escalado(r, x + r.ox, y + r.oy, r.w, r.h, clave, color, mezcla);
}

void Gpu::rectangulo(int x, int y, int w, int h, u64 clave, u32 color) {
    // Muestrea solo el centro del blanco de 2×2 para no tocar el margen del atlas.
    RegionAtlas c = blanco_;
    c.w = c.h = 1;
    dibujar_escalado(c, static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h), clave, color);
}

// Radix sort LSD estable sobre los 5 bytes altos de la clave (los 3 bajos son siempre 0).
void Gpu::ordenar() {
    size_t n = inst_.size();
    orden_.resize(n);
    orden_tmp_.resize(n);
    for (u32 i = 0; i < n; ++i) orden_[i] = i;
    for (int byte = 3; byte < 8; ++byte) {
        u32 cuenta[257] = {};
        int desp = byte * 8;
        for (u32 i : orden_) ++cuenta[((claves_[i] >> desp) & 0xFF) + 1];
        if (cuenta[1 + ((claves_[orden_[0]] >> desp) & 0xFF)] == n) continue;  // byte igual en todas
        for (int k = 0; k < 256; ++k) cuenta[k + 1] += cuenta[k];
        for (u32 i : orden_) orden_tmp_[cuenta[(claves_[i] >> desp) & 0xFF]++] = i;
        orden_.swap(orden_tmp_);
    }
    inst_ordenadas_.resize(n);
    for (size_t i = 0; i < n; ++i) inst_ordenadas_[i] = inst_[orden_[i]];
}

int Gpu::escala_maxima() const {
    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(ventana_, &pw, &ph);
    return std::max(1, std::min(pw / ancho_min_, ph / alto_min_));
}

void Gpu::ajustar_vista(bool ampliable, int zoom) {
    int maxima = escala_maxima();
    if (!ampliable) {
        escala_ = maxima;
        ancho_ = ancho_min_;
        alto_ = alto_min_;
        return;
    }
    int pw = 0, ph = 0;
    SDL_GetWindowSizeInPixels(ventana_, &pw, &ph);
    escala_ = zoom > 0 ? std::min(zoom, maxima) : maxima;
    ancho_ = std::max(ancho_min_, pw / escala_);
    alto_ = std::max(alto_min_, ph / escala_);
}

bool Gpu::crear_destino() {
    int w = ancho_ * escala_, h = alto_ * escala_;
    if (destino_ && w == destino_w_ && h == destino_h_) return true;
    if (destino_) SDL_ReleaseGPUTexture(dev_, destino_);  // SDL la libera cuando la GPU termina de usarla
    SDL_GPUTextureCreateInfo td{};
    td.type = SDL_GPU_TEXTURETYPE_2D;
    td.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    td.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    td.width = static_cast<Uint32>(w);
    td.height = static_cast<Uint32>(h);
    td.layer_count_or_depth = 1;
    td.num_levels = 1;
    destino_ = SDL_CreateGPUTexture(dev_, &td);
    destino_w_ = w;
    destino_h_ = h;
    return destino_ != nullptr;
}

void Gpu::preparar_frame() {
    if (!inst_.empty()) ordenar();
    else inst_ordenadas_.clear();
    preparado_ = true;
}

void Gpu::presentar() {
    if (!preparado_) preparar_frame();
    preparado_ = false;
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev_);
    if (!cmd) return;

    // 1. Subidas: píxeles nuevos del atlas e instancias del frame.
    SDL_GPUCopyPass* copia = SDL_BeginGPUCopyPass(cmd);
    if (!pendientes_.empty()) {
        void* p = SDL_MapGPUTransferBuffer(dev_, subida_atlas_, true);
        std::memcpy(p, staging_atlas_.data(), staging_atlas_.size());
        SDL_UnmapGPUTransferBuffer(dev_, subida_atlas_);
        for (const SubidaPendiente& s : pendientes_) {
            SDL_GPUTextureTransferInfo src{};
            src.transfer_buffer = subida_atlas_;
            src.offset = s.offset;
            src.pixels_per_row = s.r.w;
            src.rows_per_layer = s.r.h;
            SDL_GPUTextureRegion dst{};
            dst.texture = atlas_;
            dst.layer = s.r.pagina;
            dst.x = s.r.x;
            dst.y = s.r.y;
            dst.w = s.r.w;
            dst.h = s.r.h;
            dst.d = 1;
            SDL_UploadToGPUTexture(copia, &src, &dst, false);
        }
        pendientes_.clear();
        staging_atlas_.clear();
    }
    u32 n = static_cast<u32>(inst_ordenadas_.size());
    if (n) {
        void* p = SDL_MapGPUTransferBuffer(dev_, subida_inst_, true);
        std::memcpy(p, inst_ordenadas_.data(), n * sizeof(Instancia));
        SDL_UnmapGPUTransferBuffer(dev_, subida_inst_);
        SDL_GPUTransferBufferLocation src{subida_inst_, 0};
        SDL_GPUBufferRegion dst{instancias_gpu_, 0, n * static_cast<u32>(sizeof(Instancia))};
        SDL_UploadToGPUBuffer(copia, &src, &dst, true);
    }
    SDL_EndGPUCopyPass(copia);

    // 2. Dibujo a la textura de destino (lógica × escala): una sola llamada instanciada. El
    // shader sigue trabajando en coordenadas lógicas.
    crear_destino();
    SDL_GPUColorTargetInfo objetivo{};
    objetivo.texture = destino_;
    objetivo.clear_color = {0, 0, 0, 1};
    objetivo.load_op = SDL_GPU_LOADOP_CLEAR;
    objetivo.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pase = SDL_BeginGPURenderPass(cmd, &objetivo, 1, nullptr);
    llamadas_ = 0;
    if (n) {
        SDL_BindGPUGraphicsPipeline(pase, pipeline_);
        SDL_BindGPUVertexStorageBuffers(pase, 0, &instancias_gpu_, 1);
        SDL_GPUTextureSamplerBinding tsb{atlas_, muestreo_};
        SDL_BindGPUFragmentSamplers(pase, 0, &tsb, 1);
        float uni[4] = {static_cast<float>(ancho_), static_cast<float>(alto_), 0, 0};
        SDL_PushGPUVertexUniformData(cmd, 0, uni, sizeof uni);
        SDL_DrawGPUPrimitives(pase, 6, n, 0, 0);
        llamadas_ = 1;
    }
    SDL_EndGPURenderPass(pase);

    if (!ruta_captura_.empty()) cmd = guardar_captura(cmd);

    // 3. Copia 1:1 a la ventana, centrada (la textura ya tiene el tamaño escalado).
    SDL_GPUTexture* swap = nullptr;
    Uint32 sw = 0, sh = 0;
    if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, ventana_, &swap, &sw, &sh) && swap) {
        Uint32 dw = static_cast<Uint32>(ancho_ * escala_), dh = static_cast<Uint32>(alto_ * escala_);
        SDL_GPUBlitInfo b{};
        b.source.texture = destino_;
        b.source.w = dw;
        b.source.h = dh;
        b.destination.texture = swap;
        b.destination.x = sw > dw ? (sw - dw) / 2 : 0;
        b.destination.y = sh > dh ? (sh - dh) / 2 : 0;
        b.destination.w = std::min(dw, sw);
        b.destination.h = std::min(dh, sh);
        b.source.w = b.destination.w;
        b.source.h = b.destination.h;
        b.load_op = SDL_GPU_LOADOP_CLEAR;
        b.clear_color = {0, 0, 0, 1};
        b.filter = SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(cmd, &b);
    }
    SDL_SubmitGPUCommandBuffer(cmd);

    instancias_previas_ = n;
    inst_.clear();
    claves_.clear();
    ++frame_;
}

SDL_GPUCommandBuffer* Gpu::guardar_captura(SDL_GPUCommandBuffer* cmd) {
    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    const int w = ancho_ * escala_, h = alto_ * escala_;
    tb.size = static_cast<Uint32>(w * h * 4);
    SDL_GPUTransferBuffer* bajada = SDL_CreateGPUTransferBuffer(dev_, &tb);
    SDL_GPUCopyPass* copia = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion src{};
    src.texture = destino_;
    src.w = static_cast<Uint32>(w);
    src.h = static_cast<Uint32>(h);
    src.d = 1;
    SDL_GPUTextureTransferInfo dst{};
    dst.transfer_buffer = bajada;
    SDL_DownloadFromGPUTexture(copia, &src, &dst);
    SDL_EndGPUCopyPass(copia);
    // Se envía este comando y se espera a la GPU para poder leer los píxeles.
    SDL_GPUFence* f = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(dev_, true, &f, 1);
    SDL_ReleaseGPUFence(dev_, f);
    // La captura se guarda a la resolución lógica (un píxel de cada bloque de escala × escala).
    const u32* px = static_cast<const u32*>(SDL_MapGPUTransferBuffer(dev_, bajada, false));
    std::vector<u32> logica(static_cast<size_t>(ancho_) * alto_);
    for (int y = 0; y < alto_; ++y)
        for (int x = 0; x < ancho_; ++x) logica[static_cast<size_t>(y) * ancho_ + x] = px[static_cast<size_t>(y * escala_) * w + x * escala_];
    SDL_Surface* s = SDL_CreateSurfaceFrom(ancho_, alto_, SDL_PIXELFORMAT_ABGR8888, logica.data(), ancho_ * 4);
    if (s && SDL_SaveBMP(s, ruta_captura_.c_str())) LIN_LOG("Captura guardada en %s", ruta_captura_.c_str());
    SDL_DestroySurface(s);
    SDL_UnmapGPUTransferBuffer(dev_, bajada);
    SDL_ReleaseGPUTransferBuffer(dev_, bajada);
    ruta_captura_.clear();
    return SDL_AcquireGPUCommandBuffer(dev_);  // el resto del frame va en un comando nuevo
}

void Gpu::a_logico(float wx, float wy, int& lx, int& ly) const {
    int pw = 0, ph = 0, ww = 0, wh = 0;
    SDL_GetWindowSizeInPixels(ventana_, &pw, &ph);
    SDL_GetWindowSize(ventana_, &ww, &wh);
    float densidad = ww > 0 ? static_cast<float>(pw) / ww : 1.0f;
    float x = wx * densidad - (pw - ancho_ * escala_) / 2.0f;
    float y = wy * densidad - (ph - alto_ * escala_) / 2.0f;
    lx = static_cast<int>(x / escala_);
    ly = static_cast<int>(y / escala_);
}

}  // namespace lin
