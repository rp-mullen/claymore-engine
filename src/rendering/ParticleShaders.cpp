#include <bgfx/bgfx.h>
#include "ShaderManager.h"

bgfx::ProgramHandle LoadParticleProgram()
{
    static bgfx::ProgramHandle s_program = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(s_program))
    {
        s_program = ShaderManager::Instance().LoadProgram("vs_particle", "fs_particle");
    }
    return s_program;
}
