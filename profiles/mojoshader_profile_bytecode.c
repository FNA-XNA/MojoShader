/**
 * MojoShader; generate shader programs from bytecode of compiled
 *  Direct3D shaders.
 *
 * Please see the file LICENSE.txt in the source's root directory.
 *
 *  This file written by Ryan C. Gordon.
 */

#define __MOJOSHADER_INTERNAL__ 1
#include "mojoshader_profile_bytecode.h"

#if SUPPORT_PROFILE_BYTECODE

void emit_BYTECODE_start(Context *ctx, const char *profilestr)
{
    ctx->ignores_ctab = 1;
} // emit_BYTECODE_start

void emit_BYTECODE_finalize(Context *ctx)
{
    // just copy the whole token stream and make all other emitters no-ops.
    if (set_output(ctx, &ctx->mainline))
    {
        const size_t len = ((size_t) (ctx->tokens - ctx->orig_tokens)) * sizeof (uint32);
        buffer_append(ctx->mainline, (const char *) ctx->orig_tokens, len);
    } // if
} // emit_BYTECODE_finalize

void emit_BYTECODE_end(Context *ctx) {}
void emit_BYTECODE_phase(Context *ctx) {}
void emit_BYTECODE_global(Context *ctx, RegisterType t, int n) {}
void emit_BYTECODE_array(Context *ctx, VariableList *var) {}
void emit_BYTECODE_sampler(Context *c, int s, TextureType t, int tb) {}
void emit_BYTECODE_const_array(Context *ctx, const ConstantsList *c,
                                         int base, int size) {}
void emit_BYTECODE_uniform(Context *ctx, RegisterType t, int n,
                                  const VariableList *var) {}
void emit_BYTECODE_attribute(Context *ctx, RegisterType t, int n,
                                       MOJOSHADER_usage u, int i, int w,
                                       int f) {}

const char *get_BYTECODE_varname(Context *ctx, RegisterType rt, int regnum)
{
    char regnum_str[16];
    const char *regtype_str = get_D3D_register_string(ctx, rt, regnum,
                                              regnum_str, sizeof (regnum_str));
    char buf[64];
    snprintf(buf, sizeof (buf), "%s%s", regtype_str, regnum_str);
    return StrDup(ctx, buf);
} // get_BYTECODE_varname

const char *get_BYTECODE_const_array_varname(Context *ctx, int base, int size)
{
    char buf[64];
    snprintf(buf, sizeof (buf), "c_array_%d_%d", base, size);
    return StrDup(ctx, buf);
} // get_BYTECODE_const_array_varname

#define EMIT_BYTECODE_OPCODE_FUNC(op) \
    void emit_BYTECODE_##op(Context *ctx) {}

EMIT_BYTECODE_OPCODE_FUNC(RESERVED)
EMIT_BYTECODE_OPCODE_FUNC(NOP)
EMIT_BYTECODE_OPCODE_FUNC(MOV)
EMIT_BYTECODE_OPCODE_FUNC(ADD)
EMIT_BYTECODE_OPCODE_FUNC(SUB)
EMIT_BYTECODE_OPCODE_FUNC(MAD)
EMIT_BYTECODE_OPCODE_FUNC(MUL)
EMIT_BYTECODE_OPCODE_FUNC(RCP)
EMIT_BYTECODE_OPCODE_FUNC(RSQ)
EMIT_BYTECODE_OPCODE_FUNC(DP3)
EMIT_BYTECODE_OPCODE_FUNC(DP4)
EMIT_BYTECODE_OPCODE_FUNC(MIN)
EMIT_BYTECODE_OPCODE_FUNC(MAX)
EMIT_BYTECODE_OPCODE_FUNC(SLT)
EMIT_BYTECODE_OPCODE_FUNC(SGE)
EMIT_BYTECODE_OPCODE_FUNC(EXP)
EMIT_BYTECODE_OPCODE_FUNC(LOG)
EMIT_BYTECODE_OPCODE_FUNC(LIT)
EMIT_BYTECODE_OPCODE_FUNC(DST)
EMIT_BYTECODE_OPCODE_FUNC(LRP)
EMIT_BYTECODE_OPCODE_FUNC(FRC)
EMIT_BYTECODE_OPCODE_FUNC(M4X4)
EMIT_BYTECODE_OPCODE_FUNC(M4X3)
EMIT_BYTECODE_OPCODE_FUNC(M3X4)
EMIT_BYTECODE_OPCODE_FUNC(M3X3)
EMIT_BYTECODE_OPCODE_FUNC(M3X2)
EMIT_BYTECODE_OPCODE_FUNC(CALL)
EMIT_BYTECODE_OPCODE_FUNC(CALLNZ)
EMIT_BYTECODE_OPCODE_FUNC(LOOP)
EMIT_BYTECODE_OPCODE_FUNC(RET)
EMIT_BYTECODE_OPCODE_FUNC(ENDLOOP)
EMIT_BYTECODE_OPCODE_FUNC(LABEL)
EMIT_BYTECODE_OPCODE_FUNC(POW)
EMIT_BYTECODE_OPCODE_FUNC(CRS)
EMIT_BYTECODE_OPCODE_FUNC(SGN)
EMIT_BYTECODE_OPCODE_FUNC(ABS)
EMIT_BYTECODE_OPCODE_FUNC(NRM)
EMIT_BYTECODE_OPCODE_FUNC(SINCOS)
EMIT_BYTECODE_OPCODE_FUNC(REP)
EMIT_BYTECODE_OPCODE_FUNC(ENDREP)
EMIT_BYTECODE_OPCODE_FUNC(IF)
EMIT_BYTECODE_OPCODE_FUNC(ELSE)
EMIT_BYTECODE_OPCODE_FUNC(ENDIF)
EMIT_BYTECODE_OPCODE_FUNC(BREAK)
EMIT_BYTECODE_OPCODE_FUNC(MOVA)
EMIT_BYTECODE_OPCODE_FUNC(TEXKILL)
EMIT_BYTECODE_OPCODE_FUNC(TEXBEM)
EMIT_BYTECODE_OPCODE_FUNC(TEXBEML)
EMIT_BYTECODE_OPCODE_FUNC(TEXREG2AR)
EMIT_BYTECODE_OPCODE_FUNC(TEXREG2GB)
EMIT_BYTECODE_OPCODE_FUNC(TEXM3X2PAD)
EMIT_BYTECODE_OPCODE_FUNC(TEXM3X2TEX)
EMIT_BYTECODE_OPCODE_FUNC(TEXM3X3PAD)
EMIT_BYTECODE_OPCODE_FUNC(TEXM3X3TEX)
EMIT_BYTECODE_OPCODE_FUNC(TEXM3X3SPEC)
EMIT_BYTECODE_OPCODE_FUNC(TEXM3X3VSPEC)
EMIT_BYTECODE_OPCODE_FUNC(EXPP)
EMIT_BYTECODE_OPCODE_FUNC(LOGP)
EMIT_BYTECODE_OPCODE_FUNC(CND)
EMIT_BYTECODE_OPCODE_FUNC(TEXREG2RGB)
EMIT_BYTECODE_OPCODE_FUNC(TEXDP3TEX)
EMIT_BYTECODE_OPCODE_FUNC(TEXM3X2DEPTH)
EMIT_BYTECODE_OPCODE_FUNC(TEXDP3)
EMIT_BYTECODE_OPCODE_FUNC(TEXM3X3)
EMIT_BYTECODE_OPCODE_FUNC(TEXDEPTH)
EMIT_BYTECODE_OPCODE_FUNC(CMP)
EMIT_BYTECODE_OPCODE_FUNC(BEM)
EMIT_BYTECODE_OPCODE_FUNC(DP2ADD)
EMIT_BYTECODE_OPCODE_FUNC(DSX)
EMIT_BYTECODE_OPCODE_FUNC(DSY)
EMIT_BYTECODE_OPCODE_FUNC(TEXLDD)
EMIT_BYTECODE_OPCODE_FUNC(TEXLDL)
EMIT_BYTECODE_OPCODE_FUNC(BREAKP)
EMIT_BYTECODE_OPCODE_FUNC(BREAKC)
EMIT_BYTECODE_OPCODE_FUNC(IFC)
EMIT_BYTECODE_OPCODE_FUNC(SETP)
EMIT_BYTECODE_OPCODE_FUNC(DEF)
EMIT_BYTECODE_OPCODE_FUNC(DEFI)
EMIT_BYTECODE_OPCODE_FUNC(DEFB)
EMIT_BYTECODE_OPCODE_FUNC(DCL)
EMIT_BYTECODE_OPCODE_FUNC(TEXCRD)
EMIT_BYTECODE_OPCODE_FUNC(TEXLD)

#undef EMIT_BYTECODE_OPCODE_FUNC

#endif  // SUPPORT_PROFILE_BYTECODE