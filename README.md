# Sonic Unleashed Recompiled — Android Port

Port não-oficial do Sonic Unleashed para Android baseado no projeto
[UnleashedRecomp](https://github.com/hedge-dev/unleashedrecomp) com
camada Android NDK completa.

> **ATENÇÃO:** Você precisa possuir o jogo original para usar este software.
> Veja [`docs/DUMPING-en.md`](docs/DUMPING-en.md) para extrair os arquivos do
> seu Xbox 360 ou PS3.

---

## Estrutura do repositório

```
UnleashedRecomp-Android/
│
├── AndroidProject/              ← Projeto Android Studio
│   └── app/src/main/
│       ├── cpp/
│       │   ├── engine_core/     ← Submódulo: UnleashedRecomp engine
│       │   ├── os/android/      ← Logger + ciclo de vida (ANativeActivity)
│       │   ├── input/           ← Multitouch + gamepad virtual Xbox 360
│       │   ├── audio/           ← AAudio (API 26+) com fallback OpenSL ES
│       │   ├── vulkan/          ← Surface, swapchain, GPU profile (Adreno/Mali)
│       │   └── jni/             ← Ponte Java ↔ C++
│       ├── java/                ← MainActivity.java
│       └── assets/game/         ← Assets do jogo (não incluídos)
│
├── UnleashedRecomp/             ← Engine principal (submódulo Git)
├── UnleashedRecompLib/          ← Biblioteca de configuração SWA
├── thirdparty/                  ← o1heap, MoltenVK
├── tools/                       ← Utilitários de build (fshasher, bc_diff…)
├── docs/                        ← BUILD.md, DUMPING-en.md
└── .github/workflows/           ← CI: build APK + validate engine
```

---

## Camadas Android — origem de cada módulo

| Arquivo | Projeto de origem | Motivo da escolha |
|---|---|---|
| `os/android/logger_android.cpp` | SonicPort_Fixed | Logger completo com LOGI/LOGW/LOGE/LOGD, buffer de 1 KB |
| `os/android/process_android.cpp` | SonicPort_Fixed | APP_CMD_* completo: window, pause, resume, config, low_memory |
| `input/touch_input.cpp` | SonicPort_Fixed | Multitouch completo com mutex, prevX/Y, pressure, cancel |
| `input/virtual_gamepad.cpp` | SonicPort_Fixed | Mapeamento normalizado [0,1] para todos os 10 botões Xbox 360 |
| `audio/audio_android.cpp` | SonicPort_Fixed | AAudio + fallback OpenSL ES, buffer circular, auto-restart |
| `vulkan/vulkan_android.cpp` | SonicPort_Fixed | VkAndroidSurfaceKHR correto, triple buffering, swapchain |
| `vulkan/gpu_profile.h` | SonicPort_Fixed | Detecção Adreno/Mali/Xclipse com níveis Low→Ultra |
| `jni/jni_bridge.cpp` | **FUSÃO** Fixed+port3+pretest | Lifecycle + filesystem virtual + PurgeVolatileMemory |
| `CMakeLists.txt` | **FUSÃO** Fixed+port3+pretest | GLOB_RECURSE, ftree-vectorize, ANDROID_STL=c++_shared |
| `build.gradle` | **FUSÃO** Fixed+port3 | minSdk 26, c++20, prefab, ANDROID_STL |
| `AndroidManifest.xml` | **FUSÃO** Fixed+port3 | Vulkan required + sensorLandscape + aspectRatio |
| `MainActivity.java` | SonicPort_Fixed | Imersivo API 30+, vibração API 31+, permissões corretas |

---

## Requisitos de build

| Ferramenta | Versão |
|---|---|
| Android Studio | Hedgehog 2023.1.1+ |
| NDK | r26c (26.3.11579264) |
| CMake | 3.22.1+ |
| compileSdk | 34 |
| Java | 11+ |

Veja [`docs/BUILD_ANDROID.md`](docs/BUILD_ANDROID.md) para o passo a passo completo.

---

## Compilar

```bash
cd AndroidProject
./gradlew assembleDebug
```

APK em: `app/build/outputs/apk/debug/app-debug.apk`

---

## Licença

Veja [COPYING](COPYING). Os assets do jogo original pertencem à SEGA e não estão incluídos.
