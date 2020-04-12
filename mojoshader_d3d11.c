/**
 * MojoShader; generate shader programs from bytecode of compiled
 *  Direct3D shaders.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define __MOJOSHADER_INTERNAL__ 1
#include "mojoshader_internal.h"

// Error state...
static char error_buffer[1024] = { '\0' };

static void set_error(const char *str)
{
    snprintf(error_buffer, sizeof (error_buffer), "%s", str);
} // set_error

static inline void out_of_memory(void)
{
    set_error("out of memory");
} // out_of_memory

// profile-specific implementations...

#if SUPPORT_PROFILE_HLSL
#ifdef MOJOSHADER_EFFECT_SUPPORT

#include <d3d11.h>

/* Structs */
struct MOJOSHADER_d3d11Shader
{
    const MOJOSHADER_parseData *parseData;
    ID3D11Buffer *ubo;
    int numInternalBuffers;
};

typedef struct MOJOSHADER_d3d11Effect
{
    MOJOSHADER_effect *effect;
    unsigned int num_shaders;
    MOJOSHADER_d3d11Shader *shaders;
    unsigned int *shader_indices;
    unsigned int num_preshaders;
    unsigned int *preshader_indices;
    MOJOSHADER_d3d11Shader *current_vert;
    MOJOSHADER_d3d11Shader *current_frag;
    MOJOSHADER_effectShader *current_vert_raw;
    MOJOSHADER_effectShader *current_frag_raw;
    MOJOSHADER_d3d11Shader *prev_vert;
    MOJOSHADER_d3d11Shader *prev_frag;
} MOJOSHADER_d3d11Effect;

/* Internal register utilities */

// Max entries for each register file type...
#define MAX_REG_FILE_F 8192
#define MAX_REG_FILE_I 2047
#define MAX_REG_FILE_B 2047

// The constant register files...
// !!! FIXME: Man, it kills me how much memory this takes...
// !!! FIXME:  ... make this dynamically allocated on demand.
float vs_reg_file_f[MAX_REG_FILE_F * 4];
int vs_reg_file_i[MAX_REG_FILE_I * 4];
uint8 vs_reg_file_b[MAX_REG_FILE_B];
float ps_reg_file_f[MAX_REG_FILE_F * 4];
int ps_reg_file_i[MAX_REG_FILE_I * 4];
uint8 ps_reg_file_b[MAX_REG_FILE_B];

// !!! FIXME: Move this out to mojoshader_internal.h? -caleb
static inline void copy_parameter_data(MOJOSHADER_effectParam *params,
                                       unsigned int *param_loc,
                                       MOJOSHADER_symbol *symbols,
                                       unsigned int symbol_count,
                                       float *regf, int *regi, uint8 *regb)
{
    int i, j, r, c;

    i = 0;
    for (i = 0; i < symbol_count; i++)
    {
        const MOJOSHADER_symbol *sym = &symbols[i];
        const MOJOSHADER_effectValue *param = &params[param_loc[i]].value;

        // float/int registers are vec4, so they have 4 elements each
        const uint32 start = sym->register_index << 2;

        if (param->type.parameter_type == MOJOSHADER_SYMTYPE_FLOAT)
            memcpy(regf + start, param->valuesF, sym->register_count << 4);
        else if (sym->register_set == MOJOSHADER_SYMREGSET_FLOAT4)
        {
            // Structs are a whole different world...
            if (param->type.parameter_class == MOJOSHADER_SYMCLASS_STRUCT)
                memcpy(regf + start, param->valuesF, sym->register_count << 4);
            else
            {
                // Sometimes int/bool parameters get thrown into float registers...
                j = 0;
                do
                {
                    c = 0;
                    do
                    {
                        regf[start + (j << 2) + c] = (float) param->valuesI[(j << 2) + c];
                    } while (++c < param->type.columns);
                } while (++j < sym->register_count);
            } // else
        } // else if
        else if (sym->register_set == MOJOSHADER_SYMREGSET_INT4)
            memcpy(regi + start, param->valuesI, sym->register_count << 4);
        else if (sym->register_set == MOJOSHADER_SYMREGSET_BOOL)
        {
            j = 0;
            r = 0;
            do
            {
                c = 0;
                do
                {
                    // regb is not a vec4, enjoy that 'start' bitshift! -flibit
                    regb[(start >> 2) + r + c] = param->valuesI[(j << 2) + c];
                    c++;
                } while (c < param->type.columns && ((r + c) < sym->register_count));
                r += c;
                j++;
            } while (r < sym->register_count);
        } // else if
    } // for
} // copy_parameter_data

/* Uniform buffer utilities */

static void update_uniform_buffer(MOJOSHADER_d3d11Shader *shader)
{
    if (shader == NULL || shader->ubo == NULL)
        return;

    float *regF; int *regI; uint8 *regB;
    if (shader->parseData->shader_type == MOJOSHADER_TYPE_VERTEX)
    {
        regF = vs_reg_file_f;
        regI = vs_reg_file_i;
        regB = vs_reg_file_b;
    } // if
    else
    {
        regF = ps_reg_file_f;
        regI = ps_reg_file_i;
        regB = ps_reg_file_b;
    } // else

    /* TODO */
    // UBO_predraw(shader->ubo);
    // void *buf = shader->ubo->internalBuffers[shader->ubo->currentFrame];
    // void *contents = UBO_buffer_contents(buf) + shader->ubo->internalOffset;

    // int offset = 0;
    // for (int i = 0; i < shader->parseData->uniform_count; i++)
    // {
    //     int idx = shader->parseData->uniforms[i].index;
    //     int arrayCount = shader->parseData->uniforms[i].array_count;
    //     int size = arrayCount ? arrayCount : 1;

    //     switch (shader->parseData->uniforms[i].type)
    //     {
    //         case MOJOSHADER_UNIFORM_FLOAT:
    //             memcpy(
    //                 contents + (offset * 16),
    //                 &regF[4 * idx],
    //                 size * 16
    //             );
    //             break;

    //         case MOJOSHADER_UNIFORM_INT:
    //             // !!! FIXME: Need a test case
    //             memcpy(
    //                 contents + (offset * 16),
    //                 &regI[4 * idx],
    //                 size * 16
    //             );
    //             break;

    //         case MOJOSHADER_UNIFORM_BOOL:
    //             // !!! FIXME: Need a test case
    //             memcpy(
    //                 contents + offset,
    //                 &regB[idx],
    //                 size
    //             );
    //             break;

    //         default:
    //             assert(0); // This should never happen.
    //             break;
    //     } // switch

    //     offset += size;
    // } // for
} // update_uniform_buffer

/* Public API */

MOJOSHADER_d3d11Effect *MOJOSHADER_d3d11CompileEffect(MOJOSHADER_effect *effect,
                                                      void *mtlDevice,
                                                      int numBackingBuffers)
{
    int i;
    MOJOSHADER_malloc m = effect->malloc;
    MOJOSHADER_free f = effect->free;
    void *d = effect->malloc_data;
    int current_shader = 0;
    int current_preshader = 0;
    int src_len = 0;

    MOJOSHADER_d3d11Effect *retval;
    retval = (MOJOSHADER_d3d11Effect *) (m(sizeof(MOJOSHADER_d3d11Effect), d));
    if (retval == NULL)
    {
        out_of_memory();
        return NULL;
    } // if
    memset(retval, '\0', sizeof (MOJOSHADER_d3d11Effect));

    /* TODO */
    // // Count the number of shaders before allocating
    // for (i = 0; i < effect->object_count; i++)
    // {
    //     MOJOSHADER_effectObject *object = &effect->objects[i];
    //     if (object->type == MOJOSHADER_SYMTYPE_PIXELSHADER
    //      || object->type == MOJOSHADER_SYMTYPE_VERTEXSHADER)
    //     {
    //         if (object->shader.is_preshader)
    //             retval->num_preshaders++;
    //         else
    //         {
    //             retval->num_shaders++;
    //             src_len += object->shader.shader->output_len;
    //         } // else
    //     } // if
    // } // for

    // // Alloc shader source buffer
    // char *shader_source = (char *) m(src_len + 1, d);
    // memset(shader_source, '\0', src_len + 1);
    // int src_pos = 0;

    // // Copy all the source text into the buffer
    // for (i = 0; i < effect->object_count; i++)
    // {
    //     MOJOSHADER_effectObject *object = &effect->objects[i];
    //     if (object->type == MOJOSHADER_SYMTYPE_PIXELSHADER
    //      || object->type == MOJOSHADER_SYMTYPE_VERTEXSHADER)
    //     {
    //         if (!object->shader.is_preshader)
    //         {
    //             int output_len = object->shader.shader->output_len;
    //             memcpy(&shader_source[src_pos], object->shader.shader->output, output_len);
    //             src_pos += output_len;
    //         } // if
    //     } // if
    // } // for

    // // Alloc shader information
    // retval->shaders = (MOJOSHADER_mtlShader *) m(retval->num_shaders * sizeof (MOJOSHADER_mtlShader), d);
    // if (retval->shaders == NULL)
    // {
    //     f(retval, d);
    //     out_of_memory();
    //     return NULL;
    // } // if
    // memset(retval->shaders, '\0', retval->num_shaders * sizeof (MOJOSHADER_mtlShader));
    // retval->shader_indices = (unsigned int *) m(retval->num_shaders * sizeof (unsigned int), d);
    // if (retval->shader_indices == NULL)
    // {
    //     f(retval->shaders, d);
    //     f(retval, d);
    //     out_of_memory();
    //     return NULL;
    // } // if
    // memset(retval->shader_indices, '\0', retval->num_shaders * sizeof (unsigned int));

    // // Alloc preshader information
    // if (retval->num_preshaders > 0)
    // {
    //     retval->preshader_indices = (unsigned int *) m(retval->num_preshaders * sizeof (unsigned int), d);
    //     if (retval->preshader_indices == NULL)
    //     {
    //         f(retval->shaders, d);
    //         f(retval->shader_indices, d);
    //         f(retval, d);
    //         out_of_memory();
    //         return NULL;
    //     } // if
    //     memset(retval->preshader_indices, '\0', retval->num_preshaders * sizeof (unsigned int));
    // } // if

    // // Compile the source into a library
    // void *compileError = NULL;
    // void *shader_source_ns = cstr_to_nsstr(shader_source);
    // void *library = objc_msgSend_PTR_PTR_PTR(
    //     mtlDevice,
    //     selNewLibraryWithSource,
    //     shader_source_ns,
    //     NULL,
    //     &compileError
    // );
    // retval->library = library;
    // f(shader_source, d);
    // objc_msgSend(shader_source_ns, selRelease);

    // if (library == NULL)
    // {
    //     // Set the error
    //     void *error_nsstr = objc_msgSend(compileError, selLocalizedDescription);
    //     set_error(nsstr_to_cstr(error_nsstr));

    //     goto compile_shader_fail;
    // } // if

    // // Run through the shaders again, tracking the object indices
    // for (i = 0; i < effect->object_count; i++)
    // {
    //     MOJOSHADER_effectObject *object = &effect->objects[i];
    //     if (object->type == MOJOSHADER_SYMTYPE_PIXELSHADER
    //      || object->type == MOJOSHADER_SYMTYPE_VERTEXSHADER)
    //     {
    //         if (object->shader.is_preshader)
    //         {
    //             retval->preshader_indices[current_preshader++] = i;
    //             continue;
    //         } // if

    //         MOJOSHADER_mtlShader *curshader = &retval->shaders[current_shader];
    //         curshader->parseData = object->shader.shader;
    //         curshader->numInternalBuffers = numBackingBuffers;
    //         curshader->ubo = create_ubo(curshader, mtlDevice, m, d);
    //         curshader->library = library;

    //         retval->shader_indices[current_shader] = i;

    //         current_shader++;
    //     } // if
    // } // for

    retval->effect = effect;
    return retval;

compile_shader_fail:
    f(retval->shader_indices, d);
    f(retval->shaders, d);
    f(retval, d);
    return NULL;
} // MOJOSHADER_d3d11CompileEffect

void MOJOSHADER_d3d11DeleteEffect(MOJOSHADER_d3d11Effect *d3dEffect)
{
    MOJOSHADER_free f = d3dEffect->effect->free;
    void *d = d3dEffect->effect->malloc_data;

    int i;
    for (i = 0; i < d3dEffect->num_shaders; i++)
    {
        /* Release the uniform buffers */
        /* TODO */
    } // for

    f(d3dEffect->shader_indices, d);
    f(d3dEffect->preshader_indices, d);
    f(d3dEffect, d);
} // MOJOSHADER_d3d11DeleteEffect


void MOJOSHADER_d3d11EffectBegin(MOJOSHADER_d3d11Effect *d3dEffect,
                                 unsigned int *numPasses,
                                 int saveShaderState,
                                 MOJOSHADER_effectStateChanges *stateChanges)
{
    *numPasses = d3dEffect->effect->current_technique->pass_count;
    d3dEffect->effect->restore_shader_state = saveShaderState;
    d3dEffect->effect->state_changes = stateChanges;

    if (d3dEffect->effect->restore_shader_state)
    {
        d3dEffect->prev_vert = d3dEffect->current_vert;
        d3dEffect->prev_frag = d3dEffect->current_frag;
    } // if
} // MOJOSHADER_d3d11EffectBegin

// Predeclare
void MOJOSHADER_d3d11EffectCommitChanges(MOJOSHADER_d3d11Effect *d3dEffect,
                                         MOJOSHADER_d3d11ShaderState *shState);

void MOJOSHADER_d3d11EffectBeginPass(MOJOSHADER_d3d11Effect *d3dEffect,
                                     unsigned int pass,
                                     MOJOSHADER_d3d11ShaderState *shState)
{
    int i, j;
    MOJOSHADER_effectPass *curPass;
    MOJOSHADER_effectState *state;
    MOJOSHADER_effectShader *rawVert = d3dEffect->current_vert_raw;
    MOJOSHADER_effectShader *rawFrag = d3dEffect->current_frag_raw;
    int has_preshader = 0;

    assert(shState != NULL);
    assert(d3dEffect->effect->current_pass == -1);
    d3dEffect->effect->current_pass = pass;
    curPass = &d3dEffect->effect->current_technique->passes[pass];

    // !!! FIXME: I bet this could be stored at parse/compile time. -flibit
    for (i = 0; i < curPass->state_count; i++)
    {
        state = &curPass->states[i];
        #define ASSIGN_SHADER(stype, raw, d3ds) \
            (state->type == stype) \
            { \
                j = 0; \
                do \
                { \
                    if (*state->value.valuesI == d3dEffect->shader_indices[j]) \
                    { \
                        raw = &d3dEffect->effect->objects[*state->value.valuesI].shader; \
                        d3dEffect->d3ds = &d3dEffect->shaders[j]; \
                        break; \
                    } \
                    else if (d3dEffect->num_preshaders > 0 \
                          && *state->value.valuesI == d3dEffect->preshader_indices[j]) \
                    { \
                        raw = &d3dEffect->effect->objects[*state->value.valuesI].shader; \
                        has_preshader = 1; \
                        break; \
                    } \
                } while (++j < d3dEffect->num_shaders); \
            }
        if ASSIGN_SHADER(MOJOSHADER_RS_VERTEXSHADER, rawVert, current_vert)
        else if ASSIGN_SHADER(MOJOSHADER_RS_PIXELSHADER, rawFrag, current_frag)
        #undef ASSIGN_SHADER
    } // for

    d3dEffect->effect->state_changes->render_state_changes = curPass->states;
    d3dEffect->effect->state_changes->render_state_change_count = curPass->state_count;

    d3dEffect->current_vert_raw = rawVert;
    d3dEffect->current_frag_raw = rawFrag;

    /* If this effect pass has an array of shaders, we get to wait until
     * CommitChanges to actually bind the final shaders.
     * -flibit
     */
    if (!has_preshader)
    {
        if (d3dEffect->current_vert != NULL)
        {
            MOJOSHADER_d3d11Shader *vert = d3dEffect->current_vert;
            shState->vertexShader = vert;
            /* TODO */
            // shState->vertexUniformBuffer = get_uniform_buffer(vert);
            // shState->vertexUniformOffset = get_uniform_offset(vert);
        } // if

        if (d3dEffect->current_frag != NULL)
        {
            MOJOSHADER_d3d11Shader *frag = d3dEffect->current_frag;
            shState->fragmentShader = frag;
            /* TODO */
            // shState->fragmentUniformBuffer = get_uniform_buffer(frag);
            // shState->fragmentUniformOffset = get_uniform_offset(frag);
        } // if

        if (d3dEffect->current_vert_raw != NULL)
        {
            d3dEffect->effect->state_changes->vertex_sampler_state_changes = rawVert->samplers;
            d3dEffect->effect->state_changes->vertex_sampler_state_change_count = rawVert->sampler_count;
        } // if
        if (d3dEffect->current_frag_raw != NULL)
        {
            d3dEffect->effect->state_changes->sampler_state_changes = rawFrag->samplers;
            d3dEffect->effect->state_changes->sampler_state_change_count = rawFrag->sampler_count;
        } // if
    } // if

    MOJOSHADER_d3d11EffectCommitChanges(d3dEffect, shState);
} // MOJOSHADER_d3d11EffectBeginPass

void MOJOSHADER_d3d11EffectCommitChanges(MOJOSHADER_d3d11Effect *d3dEffect,
                                         MOJOSHADER_d3d11ShaderState *shState)
{
    MOJOSHADER_effectShader *rawVert = d3dEffect->current_vert_raw;
    MOJOSHADER_effectShader *rawFrag = d3dEffect->current_frag_raw;

    /* Used for shader selection from preshaders */
    int i, j;
    MOJOSHADER_effectValue *param;
    float selector;
    int shader_object;
    int selector_ran = 0;

    /* For effect passes with arrays of shaders, we have to run a preshader
     * that determines which shader to use, based on a parameter's value.
     * -flibit
     */
    // !!! FIXME: We're just running the preshaders every time. Blech. -flibit
    #define SELECT_SHADER_FROM_PRESHADER(raw, d3ds) \
        if (raw != NULL && raw->is_preshader) \
        { \
            i = 0; \
            do \
            { \
                param = &d3dEffect->effect->params[raw->preshader_params[i]].value; \
                for (j = 0; j < (param->value_count >> 2); j++) \
                    memcpy(raw->preshader->registers + raw->preshader->symbols[i].register_index + j, \
                           param->valuesI + (j << 2), \
                           param->type.columns << 2); \
            } while (++i < raw->preshader->symbol_count); \
            MOJOSHADER_runPreshader(raw->preshader, &selector); \
            shader_object = d3dEffect->effect->params[raw->params[0]].value.valuesI[(int) selector]; \
            raw = &d3dEffect->effect->objects[shader_object].shader; \
            i = 0; \
            do \
            { \
                if (shader_object == d3dEffect->shader_indices[i]) \
                { \
                    d3ds = &d3dEffect->shaders[i]; \
                    break; \
                } \
            } while (++i < d3dEffect->num_shaders); \
            selector_ran = 1; \
        }
    SELECT_SHADER_FROM_PRESHADER(rawVert, d3dEffect->current_vert)
    SELECT_SHADER_FROM_PRESHADER(rawFrag, d3dEffect->current_frag)
    #undef SELECT_SHADER_FROM_PRESHADER
    if (selector_ran)
    {
        if (d3dEffect->current_vert != NULL)
            shState->vertexShader = d3dEffect->current_vert;

        if (d3dEffect->current_frag != NULL)
            shState->fragmentShader = d3dEffect->current_frag;

        if (d3dEffect->current_vert_raw != NULL)
        {
            d3dEffect->effect->state_changes->vertex_sampler_state_changes = rawVert->samplers;
            d3dEffect->effect->state_changes->vertex_sampler_state_change_count = rawVert->sampler_count;
        } // if
        if (d3dEffect->current_frag_raw != NULL)
        {
            d3dEffect->effect->state_changes->sampler_state_changes = rawFrag->samplers;
            d3dEffect->effect->state_changes->sampler_state_change_count = rawFrag->sampler_count;
        } // if
    } // if

    /* This is where parameters are copied into the constant buffers.
     * If you're looking for where things slow down immensely, look at
     * the copy_parameter_data() and MOJOSHADER_runPreshader() functions.
     * -flibit
     */
    // !!! FIXME: We're just copying everything every time. Blech. -flibit
    // !!! FIXME: We're just running the preshaders every time. Blech. -flibit
    // !!! FIXME: Will the preshader ever want int/bool registers? -flibit
    #define COPY_PARAMETER_DATA(raw, stage) \
        if (raw != NULL) \
        { \
            copy_parameter_data(d3dEffect->effect->params, raw->params, \
                                raw->shader->symbols, \
                                raw->shader->symbol_count, \
                                stage##_reg_file_f, \
                                stage##_reg_file_i, \
                                stage##_reg_file_b); \
            if (raw->shader->preshader) \
            { \
                copy_parameter_data(d3dEffect->effect->params, raw->preshader_params, \
                                    raw->shader->preshader->symbols, \
                                    raw->shader->preshader->symbol_count, \
                                    raw->shader->preshader->registers, \
                                    NULL, \
                                    NULL); \
                MOJOSHADER_runPreshader(raw->shader->preshader, stage##_reg_file_f); \
            } \
        }
    COPY_PARAMETER_DATA(rawVert, vs)
    COPY_PARAMETER_DATA(rawFrag, ps)
    #undef COPY_PARAMETER_DATA

    /* TODO */
    // update_uniform_buffer(shState->vertexShader);
    // shState->vertexUniformBuffer = get_uniform_buffer(shState->vertexShader);
    // shState->vertexUniformOffset = get_uniform_offset(shState->vertexShader);

    // update_uniform_buffer(shState->fragmentShader);
    // shState->fragmentUniformBuffer = get_uniform_buffer(shState->fragmentShader);
    // shState->fragmentUniformOffset = get_uniform_offset(shState->fragmentShader);
} // MOJOSHADER_d3dEffectCommitChanges


void MOJOSHADER_d3d11EffectEndPass(MOJOSHADER_d3d11Effect *d3dEffect)
{
    assert(d3dEffect->effect->current_pass != -1);
    d3dEffect->effect->current_pass = -1;
} // MOJOSHADER_d3d11EffectEndPass


void MOJOSHADER_d3d11EffectEnd(MOJOSHADER_d3d11Effect *d3dEffect,
                               MOJOSHADER_d3d11ShaderState *shState)
{
    if (d3dEffect->effect->restore_shader_state)
    {
        d3dEffect->effect->restore_shader_state = 0;
        shState->vertexShader = d3dEffect->prev_vert;
        shState->fragmentShader = d3dEffect->prev_frag;
        /* TODO */
        // shState->vertexUniformBuffer = get_uniform_buffer(d3dEffect->prev_vert);
        // shState->fragmentUniformBuffer = get_uniform_buffer(d3dEffect->prev_frag);
        // shState->vertexUniformOffset = get_uniform_offset(d3dEffect->prev_vert);
        // shState->fragmentUniformOffset = get_uniform_offset(d3dEffect->prev_frag);
    } // if

    d3dEffect->effect->state_changes = NULL;
} // MOJOSHADER_d3dEffectEnd

void *MOJOSHADER_d3d11GetFunctionHandle(MOJOSHADER_d3d11Shader *shader)
{
    if (shader == NULL)
        return NULL;

    /* TODO */
    return NULL;
} // MOJOSHADER_d3d11GetFunctionHandle

int MOJOSHADER_d3d11GetVertexAttribLocation(MOJOSHADER_d3d11Shader *vert,
                                            MOJOSHADER_usage usage, int index)
{
    if (vert == NULL)
        return -1;

    for (int i = 0; i < vert->parseData->attribute_count; i++)
    {
        if (vert->parseData->attributes[i].usage == usage
            && vert->parseData->attributes[i].index == index)
        {
            return i;
        } // if
    } // for

    // failure, couldn't find requested attribute
    return -1;
} // MOJOSHADER_d3d11GetVertexAttribLocation

const char *MOJOSHADER_d3d11GetError(void)
{
    return error_buffer;
} // MOJOSHADER_d3d11GetError

#endif /* MOJOSHADER_EFFECT_SUPPORT */
#endif /* SUPPORT_PROFILE_HLSL */

// end of mojoshader_metal.c ...