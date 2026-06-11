# =============================================================================
# CMakeLists.txt — Sonic Unleashed Android
#
# CORREÇÕES aplicadas:
#   1. engine_core: stub compilável (engine_stubs_android.cpp) que resolve
#      os símbolos InitializeAndroidFileSystem e PurgeVolatileMemory
#   2. Novos módulos adicionados: apu/, hid/, user/
#   3. os_android.cpp adicionado (os::user, os::version, os::media)
#   4. Include paths expandidos para cobrir todos os novos headers
#   5. -ffast-math REMOVIDO do build de produção (quebra floating point
#      PPC emulation — o PC usa -ffp-model=strict)
#   6. ENGINE_CORE_DIR separado: quando engine real for integrada, apenas
#      mude ENGINE_CORE_REAL_DIR abaixo
# =============================================================================

cmake_minimum_required(VERSION 3.22)
project("sonicengine")

if(ANDROID)

    # -------------------------------------------------------------------------
    # Diretórios
    # -------------------------------------------------------------------------
    set(PORT_DIR         "${CMAKE_CURRENT_SOURCE_DIR}")
    set(ENGINE_CORE_DIR  "${PORT_DIR}/engine_core")

    # Quando a engine real for integrada, aponte para o submódulo:
    # set(ENGINE_CORE_REAL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../UnleashedRecompLib")

    # -------------------------------------------------------------------------
    # Coleta de fontes
    # -------------------------------------------------------------------------

    # engine_core: stubs temporários (substituir pela engine real)
    file(GLOB_RECURSE ENGINE_CORE_SRC "${ENGINE_CORE_DIR}/*.cpp")

    # Camadas Android
    file(GLOB OS_SRC      "${PORT_DIR}/os/android/*.cpp")
    file(GLOB INPUT_SRC   "${PORT_DIR}/input/*.cpp")
    file(GLOB JNI_SRC     "${PORT_DIR}/jni/*.cpp")
    file(GLOB AUDIO_SRC   "${PORT_DIR}/audio/*.cpp")
    file(GLOB VULKAN_SRC  "${PORT_DIR}/vulkan/*.cpp")
    file(GLOB HID_SRC     "${PORT_DIR}/hid/*.cpp")
    file(GLOB APU_SRC     "${PORT_DIR}/apu/*.cpp")
    file(GLOB USER_SRC    "${PORT_DIR}/user/*.cpp")

    # -------------------------------------------------------------------------
    # Biblioteca nativa
    # -------------------------------------------------------------------------
    add_library(sonicengine SHARED
        ${ENGINE_CORE_SRC}
        ${OS_SRC}
        ${INPUT_SRC}
        ${JNI_SRC}
        ${AUDIO_SRC}
        ${VULKAN_SRC}
        ${HID_SRC}
        ${APU_SRC}
        ${USER_SRC}
    )

    # -------------------------------------------------------------------------
    # Include paths
    # -------------------------------------------------------------------------
    target_include_directories(sonicengine PRIVATE
        "${PORT_DIR}"
        "${PORT_DIR}/os/android"
        "${PORT_DIR}/input"
        "${PORT_DIR}/jni"
        "${PORT_DIR}/audio"
        "${PORT_DIR}/vulkan"
        "${PORT_DIR}/hid"
        "${PORT_DIR}/apu"
        "${PORT_DIR}/user"
        "${ENGINE_CORE_DIR}"
    )

    # -------------------------------------------------------------------------
    # Flags de compilação
    #
    # REMOVIDO -ffast-math: o código PPC recompilado depende de comportamento
    # IEEE 754 correto (o PC usa -ffp-model=strict). -ffast-math pode causar
    # bugs sutis em física, colisão e lógica de jogo.
    #
    # MANTIDO -ftree-vectorize: auto-vetorização NEON é segura e benéfica.
    # -------------------------------------------------------------------------
    target_compile_options(sonicengine PRIVATE
        -Wall
        -Wextra
        -Wno-unused-parameter
        -Wno-unused-variable
        -Wno-unused-function
        -fno-strict-aliasing          # obrigatório: PPC recomp faz type punning
        -O2                           # O3 pode ser ativado após validação
        -flto
        -fvisibility=hidden
        -ftree-vectorize
        # -ffast-math INTENCIONALMENTE OMITIDO
    )

    target_compile_definitions(sonicengine PRIVATE
        ANDROID
        SONIC_PLATFORM_ANDROID
        VK_USE_PLATFORM_ANDROID_KHR
    )

    # -------------------------------------------------------------------------
    # Linkagem
    # -------------------------------------------------------------------------
    find_library(log-lib      log)
    find_library(android-lib  android)
    find_library(EGL-lib      EGL)
    find_library(OpenSLES-lib OpenSLES)

    target_link_libraries(sonicengine
        ${android-lib}
        ${log-lib}
        vulkan
        ${EGL-lib}
        ${OpenSLES-lib}
        aaudio
    )

endif()
