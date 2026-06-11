# UnleashedRecomp Android — Guia de Integração

## O que foi corrigido nesta rodada

### Problemas críticos resolvidos

**Problema 1 — `engine_core/` vazio causava `undefined reference`**

O `jni_bridge.cpp` declarava `extern void InitializeAndroidFileSystem(...)` e `extern void PurgeVolatileMemory()` mas nenhum arquivo os implementava, fazendo o linker falhar.

**Solução:** criado `engine_core/engine_stubs_android.cpp` que fornece implementações stub funcionais para ambas as funções. Quando a engine real for integrada, este arquivo pode ser removido do `CMakeLists.txt`.

---

**Problema 2 — `nativeSurfaceChanged` não fazia nada**

A função no JNI apenas logava a mensagem, sem reconstruir o swapchain Vulkan. Ao rotacionar a tela ou trocar de app, o Vulkan perdia o contexto e o jogo travava ou ficava com tela preta.

**Solução:** `nativeSurfaceChanged` agora chama `Vulkan_OnWindowDestroyed()` seguido de `Vulkan_OnWindowCreated()` com a nova `ANativeWindow`. A assinatura JNI também foi atualizada para receber o objeto `Surface` (necessário para obter a `ANativeWindow`).

---

**Problema 3 — Módulos `os::user`, `os::version`, `os::media` ausentes**

O PC tem implementações para Linux/macOS/Win32, mas o Android não tinha nenhuma. Qualquer código da engine que incluísse `<os/user.h>`, `<os/version.h>` ou `<os/media.h>` falharia no link.

**Solução:** criado `os/android/os_android.cpp` com implementações corretas usando NDK (`__system_property_get` para versão, stubs seguros para tema e mídia).

---

**Problema 4 — Sistema de caminhos de armazenamento ausente**

A engine do PC usa `GetGamePath()`, `GetUserPath()`, `GetSaveFilePath()` etc. O Android não tinha equivalente. O `jni_bridge` usava caminhos hardcoded como `/sdcard/SonicUnleashed`.

**Solução:** criados `user/paths_android.h` e `user/paths_android.cpp` com implementação completa. O Java agora chama `nativeSetStoragePaths()` no `onCreate()` antes de qualquer outra função nativa, passando os caminhos reais do `Context`.

---

**Problema 5 — HID (controles físicos) não implementado**

Controles Bluetooth/USB eram completamente ignorados. O virtual gamepad só era consultado quando não havia controle conectado, mas sem o binding de eventos Android → C++.

**Solução:** criados `hid/hid_android.h` e `hid/hid_android.cpp`. A `MainActivity` agora sobrescreve `dispatchKeyEvent()` e `dispatchGenericMotionEvent()` e os repassa para o C++ via JNI.

---

**Problema 6 — Driver XAudio para Android ausente**

A engine chama `XAudioInitializeSystem()`, `XAudioRegisterClient()`, `XAudioSubmitFrame()` etc. Esses símbolos não existiam no Android — só existia a infraestrutura AAudio em `audio_android.cpp`.

**Solução:** criado `apu/apu_android.cpp` que implementa toda a API XAudio, fazendo a ponte entre o formato Xbox 360 (big-endian, 5.1 surround) e o AAudio (little-endian, estéreo, float32). Inclui downmix 5.1→2ch e conversão de byte order.

---

**Problema 7 — `-ffast-math` no CMakeLists**

O código PPC recompilado depende de comportamento IEEE 754 correto. O PC usa `-ffp-model=strict`. A flag `-ffast-math` pode causar bugs sutis em física, colisão e lógica de jogo.

**Solução:** `-ffast-math` removido do `CMakeLists.txt`. `-ftree-vectorize` foi mantido (seguro para NEON).

---

**Problema 8 — `MainActivity.java` incompleta**

A `surfaceChanged()` não passava a `Surface` para o C++. Não havia binding para `KeyEvent`/`MotionEvent` de controles. `nativeSetStoragePaths()` não existia.

**Solução:** `MainActivity.java` reescrita com todos os bindings corretos.

---

## Estrutura de arquivos adicionados

```
AndroidProject/app/src/main/cpp/
├── engine_core/
│   └── engine_stubs_android.cpp   ← stub: InitializeAndroidFileSystem, PurgeVolatileMemory
├── os/android/
│   └── os_android.cpp             ← os::user, os::version, os::media
├── user/
│   ├── paths_android.h
│   └── paths_android.cpp          ← GetGamePath(), GetUserPath(), GetSaveFilePath()
├── hid/
│   ├── hid_android.h
│   └── hid_android.cpp            ← HID_GetState(), HID_SetState(), eventos de controle
├── apu/
│   ├── apu_android.h
│   └── apu_android.cpp            ← XAudioInitializeSystem(), XAudioSubmitFrame(), etc.
└── jni/
    └── jni_bridge.cpp             ← atualizado com novas funções JNI
```

---

## O que ainda falta (para rodar o jogo de verdade)

### 1. A engine recompilada (maior bloqueio)

O `engine_core/` ainda contém apenas stubs. O jogo real precisa dos arquivos gerados por `XenonRecomp` e `XenosRecomp`. Esses arquivos ficam em `UnleashedRecompLib/ppc/` e `UnleashedRecompLib/private/` no projeto PC — são submódulos Git **não incluídos** no ZIP público.

**Para integrar:**
```cmake
# Em CMakeLists.txt, altere ENGINE_CORE_DIR para apontar para o submódulo:
set(ENGINE_CORE_DIR "${CMAKE_SOURCE_DIR}/../../UnleashedRecompLib")
add_subdirectory("${CMAKE_SOURCE_DIR}/../../UnleashedRecomp")
```

### 2. O renderer real do jogo

O Vulkan atual só cria `VkInstance + VkSurface + VkSwapchain`. Faltam `VkRenderPass`, `VkPipeline`, `VkCommandBuffer`, descritores, shaders SPIR-V convertidos do Xenos, etc.

Esses estão em `UnleashedRecomp/gpu/` no PC. Para Android, precisam compilar com ARM64 e sem extensões desktop (sem `VK_EXT_descriptor_indexing` em alguns Androids antigos — verificar `gpu_profile.h`).

### 3. Sistema de assets do jogo

O usuário precisa colocar os arquivos do jogo em:
```
/sdcard/Android/data/com.sega.sonicunleashed/files/
  game/     ← conteúdo do diretório game do PC
  update/   ← conteúdo do diretório update do PC
  dlc/      ← (opcional) DLC
```

### 4. stdafx.h e framework.h

O código da engine inclui `<stdafx.h>` e `<framework.h>`. Esses headers precisam ser adaptados para Android (sem `<windows.h>`, substituindo SDL por equivalentes NDK onde necessário).

### 5. Bibliotecas de terceiros (thirdparty/)

O PC usa: `fmt`, `xxHash`, `imgui`, `implot`, `ddspp`, `concurrentqueue`, `o1heap`, `unordered_dense`, `stb`, `msdf-atlas-gen`. Todas precisam compilar em ARM64.

A maioria é header-only ou CMake-compatível. Sugestão: usar `vcpkg` com triplet `arm64-android` ou compilar via NDK.

---

## Como compilar (quando engine integrada)

```bash
# Pré-requisitos
# - Android Studio com NDK r25+
# - CMake 3.22+
# - VCPKG com triplet arm64-android

cd AndroidProject
./gradlew assembleDebug
```

O APK gerado não vai rodar sem os arquivos do jogo na pasta externa.

---

## Dispositivos mínimos recomendados

| Componente | Mínimo | Recomendado |
|------------|--------|-------------|
| Android | 8.0 (API 26, AAudio) | 11+ |
| GPU | Vulkan 1.1 | Vulkan 1.2+ |
| RAM | 4 GB | 6 GB+ |
| Armazenamento | 15 GB livre | 20 GB livre |
| SoC | Snapdragon 730 | Snapdragon 8 Gen 1+ / Dimensity 9000+ |
