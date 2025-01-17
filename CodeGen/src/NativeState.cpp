// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "NativeState.h"

#include "Luau/UnwindBuilder.h"

#include "CustomExecUtils.h"
#include "Fallbacks.h"

#include "lbuiltins.h"
#include "lgc.h"
#include "ltable.h"
#include "lfunc.h"
#include "lvm.h"

#include <math.h>

#define CODEGEN_SET_FALLBACK(op, flags) data.context.fallback[op] = {execute_##op, flags}

static int luauF_missing(lua_State* L, StkId res, TValue* arg0, int nresults, StkId args, int nparams)
{
    return -1;
}

namespace Luau
{
namespace CodeGen
{

constexpr unsigned kBlockSize = 4 * 1024 * 1024;
constexpr unsigned kMaxTotalSize = 256 * 1024 * 1024;

NativeState::NativeState()
    : codeAllocator(kBlockSize, kMaxTotalSize)
{
}

NativeState::~NativeState() = default;

void initFallbackTable(NativeState& data)
{
    // TODO: lvmexecute_split.py could be taught to generate a subset of instructions we actually need
    CODEGEN_SET_FALLBACK(LOP_NEWCLOSURE, 0);
    CODEGEN_SET_FALLBACK(LOP_NAMECALL, 0);
    CODEGEN_SET_FALLBACK(LOP_CALL, kFallbackUpdateCi | kFallbackCheckInterrupt);
    CODEGEN_SET_FALLBACK(LOP_RETURN, kFallbackUpdateCi | kFallbackCheckInterrupt);
    CODEGEN_SET_FALLBACK(LOP_FORGPREP, kFallbackUpdatePc);
    CODEGEN_SET_FALLBACK(LOP_FORGLOOP, kFallbackUpdatePc | kFallbackCheckInterrupt);
    CODEGEN_SET_FALLBACK(LOP_FORGPREP_INEXT, kFallbackUpdatePc);
    CODEGEN_SET_FALLBACK(LOP_FORGPREP_NEXT, kFallbackUpdatePc);
    CODEGEN_SET_FALLBACK(LOP_GETVARARGS, 0);
    CODEGEN_SET_FALLBACK(LOP_DUPCLOSURE, 0);
    CODEGEN_SET_FALLBACK(LOP_PREPVARARGS, 0);
    CODEGEN_SET_FALLBACK(LOP_COVERAGE, 0);
    CODEGEN_SET_FALLBACK(LOP_BREAK, 0);

    // Fallbacks that are called from partial implementation of an instruction
    CODEGEN_SET_FALLBACK(LOP_GETGLOBAL, 0);
    CODEGEN_SET_FALLBACK(LOP_SETGLOBAL, 0);
    CODEGEN_SET_FALLBACK(LOP_GETTABLEKS, 0);
    CODEGEN_SET_FALLBACK(LOP_SETTABLEKS, 0);
}

void initHelperFunctions(NativeState& data)
{
    static_assert(sizeof(data.context.luauF_table) / sizeof(data.context.luauF_table[0]) == sizeof(luauF_table) / sizeof(luauF_table[0]),
        "fast call tables are not of the same length");

    // Replace missing fast call functions with an empty placeholder that forces LOP_CALL fallback
    for (size_t i = 0; i < sizeof(data.context.luauF_table) / sizeof(data.context.luauF_table[0]); i++)
        data.context.luauF_table[i] = luauF_table[i] ? luauF_table[i] : luauF_missing;

    data.context.luaV_lessthan = luaV_lessthan;
    data.context.luaV_lessequal = luaV_lessequal;
    data.context.luaV_equalval = luaV_equalval;
    data.context.luaV_doarith = luaV_doarith;
    data.context.luaV_dolen = luaV_dolen;
    data.context.luaV_prepareFORN = luaV_prepareFORN;
    data.context.luaV_gettable = luaV_gettable;
    data.context.luaV_settable = luaV_settable;
    data.context.luaV_getimport = luaV_getimport;
    data.context.luaV_concat = luaV_concat;

    data.context.luaH_getn = luaH_getn;
    data.context.luaH_new = luaH_new;
    data.context.luaH_clone = luaH_clone;
    data.context.luaH_resizearray = luaH_resizearray;

    data.context.luaC_barriertable = luaC_barriertable;
    data.context.luaC_barrierf = luaC_barrierf;
    data.context.luaC_barrierback = luaC_barrierback;
    data.context.luaC_step = luaC_step;

    data.context.luaF_close = luaF_close;

    data.context.libm_pow = pow;
}

} // namespace CodeGen
} // namespace Luau
