# Assets do jogo

Coloque aqui os arquivos originais do Sonic Unleashed extraídos do seu disco.
Veja `docs/DUMPING-en.md` para instruções de extração do Xbox 360 / PS3.

Estrutura esperada:
```
game/
├── data.pak        ← Assets principais
├── audio/          ← Faixas de música
└── ...
```

O AssetManager NDK lerá estes arquivos automaticamente em tempo de execução,
substituindo o acesso ao disco rígido do PC original.
