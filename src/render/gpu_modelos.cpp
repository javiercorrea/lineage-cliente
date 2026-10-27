// Modelos 3D en el renderer (rama agente/modelos-3d): mallas con skin, el pase que las dibuja en
// las ranuras de la capa PAGINA_MODELOS del atlas y su shader. Ver gpu.h y docs/analisis/PLAN_3D.md.
#include <cstring>
#include "render/gpu.h"

namespace lin {

namespace {

// Skinning en el vertex shader: los vértices se leen del storage buffer por su índice (en un dibujo
// indexado, vertex_id es el valor del índice), con las matrices de hasta 4 huesos. Luz plana con
// un poco de sombreado para que no desentone con los sprites. MULTIPLATAFORMA: HLSL + shadercross.
const char* SHADER_MODELOS_MSL = R"(
#include <metal_stdlib>
using namespace metal;

struct Vertice { packed_float3 pos; packed_float3 normal; packed_float2 uv; uchar4 huesos; packed_float4 pesos; };
struct Uniformes { float4x4 mvp; float4x4 rotacion; float4x4 huesos[32]; };
struct Salida { float4 pos [[position]]; float2 uv; float3 normal; };

vertex Salida vs_modelo(uint vid [[vertex_id]], constant Uniformes& u [[buffer(0)]],
                        const device Vertice* v [[buffer(1)]]) {
    Vertice x = v[vid];
    float4 w = float4(x.pesos);
    float4x4 piel = u.huesos[x.huesos.x] * w.x + u.huesos[x.huesos.y] * w.y +
                    u.huesos[x.huesos.z] * w.z + u.huesos[x.huesos.w] * w.w;
    Salida o;
    o.pos = u.mvp * (piel * float4(float3(x.pos), 1.0));
    o.uv = float2(x.uv);
    o.normal = (u.rotacion * (piel * float4(float3(x.normal), 0.0))).xyz;
    return o;
}

fragment float4 fs_modelo(Salida in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler s [[sampler(0)]]) {
    float4 c = tex.sample(s, in.uv);
    float3 n = normalize(in.normal);
    float luz = 0.62 + 0.38 * max(dot(n, normalize(float3(-0.45, 0.75, 0.5))), 0.0);
    return float4(c.rgb * luz, 1.0);
}
)";

static_assert(sizeof(VerticeModelo) == 52, "debe coincidir con el struct Vertice del shader");

}  // namespace

bool Gpu::crear_pipeline_modelos() {
    SDL_GPUShaderCreateInfo vs{};
    vs.code = reinterpret_cast<const Uint8*>(SHADER_MODELOS_MSL);
    vs.code_size = std::strlen(SHADER_MODELOS_MSL);
    vs.entrypoint = "vs_modelo";
    vs.format = SDL_GPU_SHADERFORMAT_MSL;
    vs.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    vs.num_uniform_buffers = 1;
    vs.num_storage_buffers = 1;
    SDL_GPUShader* v = SDL_CreateGPUShader(dev_, &vs);
    SDL_GPUShaderCreateInfo fs = vs;
    fs.entrypoint = "fs_modelo";
    fs.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    fs.num_uniform_buffers = 0;
    fs.num_storage_buffers = 0;
    fs.num_samplers = 1;
    SDL_GPUShader* f = SDL_CreateGPUShader(dev_, &fs);
    if (!v || !f) {
        LIN_LOG("SDL_CreateGPUShader (modelos): %s", SDL_GetError());
        return false;
    }
    SDL_GPUColorTargetDescription ct{};
    ct.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;  // el del atlas
    SDL_GPUGraphicsPipelineCreateInfo pi{};
    pi.vertex_shader = v;
    pi.fragment_shader = f;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;  // las mallas de IA no tienen un sentido de giro fiable
    pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pi.depth_stencil_state.enable_depth_test = true;
    pi.depth_stencil_state.enable_depth_write = true;
    pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    pi.target_info.color_target_descriptions = &ct;
    pi.target_info.num_color_targets = 1;
    pi.target_info.has_depth_stencil_target = true;
    pi.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    pipeline_modelos_ = SDL_CreateGPUGraphicsPipeline(dev_, &pi);
    SDL_ReleaseGPUShader(dev_, v);
    SDL_ReleaseGPUShader(dev_, f);
    if (!pipeline_modelos_) {
        LIN_LOG("SDL_CreateGPUGraphicsPipeline (modelos): %s", SDL_GetError());
        return false;
    }
    SDL_GPUTextureCreateInfo ti{};
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    ti.width = TAM_PAGINA;
    ti.height = TAM_PAGINA;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    profundidad_modelos_ = SDL_CreateGPUTexture(dev_, &ti);
    return profundidad_modelos_ != nullptr;
}

bool Gpu::subir_malla(const Modelo& m, MallaGpu& salida) {
    const ImagenRGBA& tex = m.textura();
    const u32 bv = static_cast<u32>(m.vertices().size() * sizeof(VerticeModelo));
    const u32 bi = static_cast<u32>(m.indices().size() * sizeof(u32));
    const u32 bt = tex.vacia() ? 4u : static_cast<u32>(tex.px.size() * 4);
    SDL_GPUBufferCreateInfo b{};
    b.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
    b.size = bv;
    salida.vertices = SDL_CreateGPUBuffer(dev_, &b);
    b.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    b.size = bi;
    salida.indices = SDL_CreateGPUBuffer(dev_, &b);
    SDL_GPUTextureCreateInfo ti{};
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ti.width = tex.vacia() ? 1u : tex.ancho;
    ti.height = tex.vacia() ? 1u : tex.alto;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    salida.textura = SDL_CreateGPUTexture(dev_, &ti);
    salida.n_indices = static_cast<u32>(m.indices().size());
    SDL_GPUTransferBufferCreateInfo tb{};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = bv + bi + bt;
    SDL_GPUTransferBuffer* t = SDL_CreateGPUTransferBuffer(dev_, &tb);
    if (!salida.valida() || !t) {
        LIN_LOG("Malla del modelo: %s", SDL_GetError());
        if (t) SDL_ReleaseGPUTransferBuffer(dev_, t);
        liberar_malla(salida);
        return false;
    }
    u8* p = static_cast<u8*>(SDL_MapGPUTransferBuffer(dev_, t, false));
    std::memcpy(p, m.vertices().data(), bv);
    std::memcpy(p + bv, m.indices().data(), bi);
    if (tex.vacia()) std::memset(p + bv + bi, 0xFF, 4);  // sin textura: blanco
    else std::memcpy(p + bv + bi, tex.px.data(), bt);
    SDL_UnmapGPUTransferBuffer(dev_, t);
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev_);
    SDL_GPUCopyPass* copia = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src{t, 0};
    SDL_GPUBufferRegion dst{salida.vertices, 0, bv};
    SDL_UploadToGPUBuffer(copia, &src, &dst, false);
    src.offset = bv;
    dst = {salida.indices, 0, bi};
    SDL_UploadToGPUBuffer(copia, &src, &dst, false);
    SDL_GPUTextureTransferInfo ts{};
    ts.transfer_buffer = t;
    ts.offset = bv + bi;
    SDL_GPUTextureRegion dt{};
    dt.texture = salida.textura;
    dt.w = ti.width;
    dt.h = ti.height;
    dt.d = 1;
    SDL_UploadToGPUTexture(copia, &ts, &dt, false);
    SDL_EndGPUCopyPass(copia);
    SDL_GPUFence* listo = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(dev_, true, &listo, 1);
    SDL_ReleaseGPUFence(dev_, listo);
    SDL_ReleaseGPUTransferBuffer(dev_, t);
    return true;
}

void Gpu::liberar_malla(MallaGpu& m) {
    if (m.vertices) SDL_ReleaseGPUBuffer(dev_, m.vertices);
    if (m.indices) SDL_ReleaseGPUBuffer(dev_, m.indices);
    if (m.textura) SDL_ReleaseGPUTexture(dev_, m.textura);
    m = {};
}

RegionAtlas Gpu::dibujar_modelo(const MallaGpu& m, const float* mvp, const float* rotacion, const float (*huesos)[16], int n_huesos) {
    if (!pipeline_modelos_ || !m.valida() || n_modelos_ >= RANURAS_MODELOS) return {};
    const int ranura = n_modelos_++;
    ModeloEncolado& e = modelos_[ranura];
    e.malla = &m;
    std::memcpy(e.uniformes, mvp, 16 * sizeof(float));
    std::memcpy(e.uniformes + 16, rotacion, 16 * sizeof(float));
    std::memcpy(e.uniformes + 32, huesos, static_cast<size_t>(n_huesos) * 16 * sizeof(float));
    e.bytes_uniformes = static_cast<u32>((2 + MAX_HUESOS) * 16 * sizeof(float));  // el struct del shader entero
    const int por_fila = TAM_PAGINA / TAM_RANURA;
    RegionAtlas r;
    r.pagina = PAGINA_MODELOS;
    r.x = static_cast<u16>((ranura % por_fila) * TAM_RANURA);
    r.y = static_cast<u16>((ranura / por_fila) * TAM_RANURA);
    r.w = r.h = TAM_RANURA;
    return r;
}

void Gpu::pase_modelos(SDL_GPUCommandBuffer* cmd) {
    SDL_GPUColorTargetInfo ct{};
    ct.texture = atlas_;
    ct.layer_or_depth_plane = PAGINA_MODELOS;
    ct.clear_color = {0, 0, 0, 0};
    ct.load_op = SDL_GPU_LOADOP_CLEAR;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPUDepthStencilTargetInfo dt{};
    dt.texture = profundidad_modelos_;
    dt.clear_depth = 1;
    dt.load_op = SDL_GPU_LOADOP_CLEAR;
    dt.store_op = SDL_GPU_STOREOP_DONT_CARE;
    dt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    SDL_GPURenderPass* pase = SDL_BeginGPURenderPass(cmd, &ct, 1, &dt);
    SDL_BindGPUGraphicsPipeline(pase, pipeline_modelos_);
    const int por_fila = TAM_PAGINA / TAM_RANURA;
    for (int i = 0; i < n_modelos_; ++i) {
        const ModeloEncolado& e = modelos_[i];
        SDL_GPUViewport vp{static_cast<float>((i % por_fila) * TAM_RANURA), static_cast<float>((i / por_fila) * TAM_RANURA),
                           static_cast<float>(TAM_RANURA), static_cast<float>(TAM_RANURA), 0, 1};
        SDL_SetGPUViewport(pase, &vp);
        SDL_Rect recorte{(i % por_fila) * TAM_RANURA, (i / por_fila) * TAM_RANURA, TAM_RANURA, TAM_RANURA};
        SDL_SetGPUScissor(pase, &recorte);
        SDL_BindGPUVertexStorageBuffers(pase, 0, &e.malla->vertices, 1);
        SDL_GPUBufferBinding ib{e.malla->indices, 0};
        SDL_BindGPUIndexBuffer(pase, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_GPUTextureSamplerBinding tsb{e.malla->textura, muestreo_};
        SDL_BindGPUFragmentSamplers(pase, 0, &tsb, 1);
        SDL_PushGPUVertexUniformData(cmd, 0, e.uniformes, e.bytes_uniformes);
        SDL_DrawGPUIndexedPrimitives(pase, e.malla->n_indices, 1, 0, 0, 0);
        ++llamadas_;
    }
    SDL_EndGPURenderPass(pase);
    n_modelos_ = 0;
}

}  // namespace lin
