# BUILD_ANDROID.md — Sonic Unleashed Port para Android

## Requisitos

| Ferramenta             | Versão mínima         | Download                                         |
|------------------------|-----------------------|--------------------------------------------------|
| Android Studio         | Hedgehog 2023.1.1+    | https://developer.android.com/studio             |
| NDK                    | r26c (26.3.11579264)  | SDK Manager → SDK Tools → NDK (Side by side)     |
| CMake                  | 3.22.1+               | SDK Manager → SDK Tools → CMake                  |
| Android SDK            | API 34 (compileSdk)   | SDK Manager → SDK Platforms                      |
| Java (JDK)             | 11+                   | Incluído no Android Studio                        |

---

## Estrutura do projeto

```
AndroidProject/
└── app/
    ├── build.gradle                        ← Configuração Gradle
    └── src/main/
        ├── AndroidManifest.xml
        ├── java/com/sega/sonicunleashed/
        │   └── MainActivity.java
        └── cpp/
            ├── CMakeLists.txt              ← Entry point do CMake
            ├── os/android/
            │   ├── logger_android.cpp/h    ← Log nativo (__android_log_print)
            │   ├── process_android.cpp/h   ← Lifecycle (APP_CMD_*)
            ├── input/
            │   ├── touch_input.cpp/h       ← Multitouch (AMotionEvent)
            │   └── virtual_gamepad.cpp/h   ← Mapeamento toque → Xbox 360
            ├── audio/
            │   └── audio_android.cpp/h     ← AAudio + fallback OpenSL ES
            ├── vulkan/
            │   ├── vulkan_android.cpp/h    ← Surface, Swapchain
            │   └── gpu_profile.h           ← Perfis Adreno / Mali
            ├── jni/
            │   └── jni_bridge.cpp/h        ← Ponte Java ↔ C++
            └── engine_core/                ← Submódulo Git (engine principal)
```

---

## Passo a passo — primeira build

### 1. Clonar com submódulos

```bash
git clone --recurse-submodules https://github.com/seu-fork/SonicPort
cd SonicPort/AndroidProject
```

Se já clonou sem submódulos:
```bash
git submodule update --init --recursive
```

### 2. Abrir no Android Studio

1. **File → Open** → selecione a pasta `AndroidProject/`
2. Android Studio vai detectar o `build.gradle` automaticamente
3. Aguarde o Gradle sync terminar (pode demorar na primeira vez)

### 3. Configurar NDK e CMake

Em **File → Project Structure → SDK Location**:
- Android SDK: confirme o caminho
- NDK: deve aparecer automaticamente após instalar via SDK Manager

Se não aparecer, no `local.properties` adicione:
```properties
ndk.dir=/path/to/ndk/26.3.11579264
```

### 4. Assets do jogo

Os assets originais do Sonic Unleashed **não estão incluídos** por questões legais.
Você precisa dumpar do seu próprio disco:

- Siga `docs/DUMPING-en.md` para extrair do Xbox 360 / PS3
- Coloque os arquivos extraídos em: `app/src/main/assets/game/`

### 5. Compilar

```bash
# Via terminal (na pasta AndroidProject/)
./gradlew assembleDebug

# Ou via Android Studio: Run → Run 'app'
```

O APK gerado estará em:
```
app/build/outputs/apk/debug/app-debug.apk
```

---

## Configurações de build

### Debug vs Release

| Config   | Flags                     | Uso                          |
|----------|---------------------------|------------------------------|
| debug    | `-O0 -g -DSONIC_DEBUG`    | Desenvolvimento, logcat ativo |
| release  | `-O3 -flto -fvisibility=hidden` | Distribuição              |

### ABI suportada

Apenas `arm64-v8a` está habilitado (cobertura >95% dos celulares Android modernos).
Para adicionar `x86_64` (emulador):

```groovy
// build.gradle
abiFilters 'arm64-v8a', 'x86_64'
```

---

## Dependências nativas (NDK)

Todas incluídas no NDK — nenhuma dependência externa necessária:

| Biblioteca  | Uso                                    |
|-------------|----------------------------------------|
| `libvulkan` | Renderização (Vulkan 1.1)              |
| `libaaudio` | Áudio low-latency (API 26+)            |
| `libOpenSLES` | Fallback de áudio (API < 26)         |
| `libandroid` | Lifecycle, AssetManager, NativeWindow |
| `liblog`    | __android_log_print (logcat)           |
| `libEGL`    | Contexto EGL (não Vulkan puro)         |
| `libGLESv3` | Fallback OpenGL ES 3                   |

---

## Depuração

### Logcat
```bash
adb logcat -s SonicPort:V
```

### Profile de GPU
O sistema detecta Adreno / Mali automaticamente via `gpu_profile.h`.
Para forçar um perfil:
```cpp
// Em vulkan_android.cpp, antes de CreateSwapchain:
GpuProfile profile = DetectGpuProfile("Adreno (TM) 740");
```

### Verificar se Vulkan está disponível
```bash
adb shell getprop ro.product.manufacturer
adb shell vulkaninfo | head -20  # requer vulkan tools no PATH
```

---

## Problemas comuns

| Erro | Causa | Solução |
|------|-------|---------|
| `INSTALL_FAILED_NO_MATCHING_ABIS` | APK só tem arm64 mas emulador é x86 | Adicionar `x86_64` ao `abiFilters` |
| `vkCreateAndroidSurfaceKHR failed` | Driver Vulkan desatualizado | Verificar `ro.hardware.vulkan` |
| `AAudio stream failed` | Dispositivo < API 26 | O fallback OpenSL ES ativa automaticamente |
| `Tela preta` | `nativeSurfaceCreated` não chamado | Verificar se `SurfaceHolder.Callback` está registrado |
| Crash em `JNI_OnLoad` | Biblioteca não encontrada | Verificar `System.loadLibrary("sonicengine")` |

---

## CI/CD

O workflow `.github/workflows/build-android.yml` verifica se o projeto compila
a cada push. Veja `docs/` para detalhes de configuração do GitHub Actions.
