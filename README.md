# Ben 10 Omniverse PC — Linux

Recompilação nativa em desenvolvimento da versão PS3 **BLUS31017 01.00** de
Ben 10 Omniverse. O arquivo fornecido foi identificado como PS3: o runtime PS2
do God of War II não é compatível com ele. Reaproveitamos o leitor de disco e a
organização da experiência de importação desse projeto; PPU/SPU usam ps3recomp.

**Ainda não há port jogável validado nem release.** Este repositório acompanha
o trabalho real de importação, tradução, compilação e diagnóstico. Não confunda
compilar o executável ou abrir uma janela com validar gameplay.

A execução nativa já chegou à tela **Press Start**, com renderização Vulkan,
entrada via API de controle e criação/reabertura de um save de teste. Isso foi
medido com o player de vídeo desativado apenas para diagnóstico. Também foram
traduzidos onze módulos PPU e 24 programas SPU de mídia do firmware do próprio
disco, além de oito variantes OVIS que compartilham endereços SPU. A reprodução
SAIL e a transição depois de “Loading content” ainda
impedem considerar o port pronto. Veja
[docs/VALIDATION.md](docs/VALIDATION.md).

## Preparação local

O importador valida metadados, versão, limites de arquivos e o SHA-256 do
executável antes de extrair. O ZIP/ISO original permanece inalterado.

```bash
git submodule update --init --recursive
python3 tools/apply_runtime_patches.py
docker build -t ben10-omniverse-tools:local -f docker/Dockerfile .
python3 tools/import_game.py '/caminho/jogo.iso'
```

Os arquivos importados ficam em `~/.local/share/Ben10OmniversePC/game`.
O executável SELF do disco precisa ser convertido para ELF com uma ferramenta
compatível. Na validação local, RPCS3 foi usado com `--decrypt` e `--installfw`;
não usamos emulação como substituto da recompilação escolhida para este projeto.

`tools/analyze_elf.py` valida o ELF, identifica funções/imports e extrai imagens
SPU. Os resultados são separados por SHA-256. As ferramentas de ps3recomp
traduzem esses arquivos localmente; `scripts/build-native.sh` compila as fontes
geradas com Clang 18 e o backend Vulkan de Linux. O build usa duas tarefas e um
limite de 4 GiB de RAM em um contêiner sem rede.

Uma tradução local das partes identificadas pode ser reproduzida depois de
importar e descriptografar o EBOOT:

```bash
data="$HOME/.local/share/Ben10OmniversePC"
elf="$data/game/PS3_GAME/USRDIR/EBOOT.elf"
hash=3ed23bca61cc9e392582e0c2cf0f114f24ce746f5724a2ab194e5aba944f1bea
python3 tools/analyze_elf.py "$elf" --output "$data/native/analysis"
python3 third_party/ps3recomp/tools/ppu_lifter.py "$elf" \
  --functions "$data/native/analysis/$hash/functions.json" \
  --hle-stubs "$data/native/analysis/$hash/imports.json" \
  --chunk-lines 150000 --jobs 2 --output "$data/native/ppu"
python3 third_party/ps3recomp/tools/build_spu_workloads.py \
  --images "$data/native/analysis/$hash/spu" --lifted "$data/native/spu" \
  --out "$data/native/spu/spu_workloads.c" \
  --register-fn ben10_register_embedded_spu --constructor --title ben10
python3 tools/lift_jobs.py "$elf" --output "$data/native/jobs"
python3 tools/lift_overlays.py "$elf" --images "$data/native/analysis/$hash/spu" \
  --output "$data/native/overlays"
# Opcional: tradução experimental da mídia, usando o firmware 4.25 do disco.
# "decrypted" deve conter os onze .prx descriptografados listados em
# tools/lift_firmware_ppu.py; dev_flash é a árvore extraída desse firmware.
python3 tools/lift_firmware_spu.py /caminho/dev_flash/sys/external/flashATRAC.pic \
  --output "$data/native/firmware-spu"
python3 tools/lift_firmware_ppu.py /caminho/decrypted \
  --output "$data/native/firmware-ppu"
python3 tools/lift_codec_spu.py "$data/native/firmware-ppu" \
  --output "$data/native/firmware-codecs"
scripts/build-native.sh
```

`analyze_elf.py` preserva análises anteriores: se a pasta do hash já existir,
use os resultados existentes ou escolha outro destino. As fontes traduzidas,
o ELF e o executável gerado são dados locais do usuário.

`scripts/test-linux.sh` executa os testes sintéticos; o GitHub Actions faz o
mesmo sem ROM ou firmware. Isso não certifica gameplay.
`scripts/probe-native.sh 30` faz um teste nativo limitado, com saves de
diagnóstico separados. Um timeout desse teste não significa sucesso.
A mídia recompilada continua opcional e pode ser diagnosticada com:

```bash
BEN10_LLE_MEDIA=1 \
BEN10_FIRMWARE_PPU_DIR="$data/native/firmware-ppu" \
PS3_DEV_FLASH=/caminho/dev_flash \
scripts/probe-native.sh 60
```

`libsre` fornece somente os quatro exports `cellOvis`; o scheduler continua
na implementação pública corrigida. O tradutor de codecs também separa os
segmentos OVIS sobrepostos, preservando o ELF original para fingerprints e DMA.
Para regenerar apenas essas variantes a partir de um inventário já traduzido:

```bash
python3 tools/lift_spu_variants.py "$data/native/firmware-codecs"
```

O diagnóstico `BEN10_HOST_VIDEO=1`, usado junto de `BEN10_LLE_MEDIA=1`, mantém
o player SAIL recompilado e usa o `cellVdec` com FFmpeg do runtime. Requer as
bibliotecas FFmpeg com a mesma versão principal dos headers usados no build.
Nesse teste foram observados decodificação de AUs e callbacks de vídeo; a
imagem ainda não foi apresentada corretamente. Não é uma opção pronta para uso.

O build local ativa AVX2 apenas quando o processador oferece esse recurso.
As regressões verificam a aritmética vetorial contra a referência escalar;
essa verificação não é uma medição de FPS do jogo.

Nada derivado do jogo deve ser adicionado ao Git. Dados, código traduzido,
firmware, saves, logs, binários e chaves são ignorados. Consulte
[THIRD_PARTY.md](THIRD_PARTY.md) para a procedência do código aberto.

## Referência e limitações

O [port macOS do mesmo título](https://github.com/andrebrumdev/ben10-omniverse-recomp)
documenta o início do jogo e depende de um fork privado de runtime. Esta
adaptação Linux usa a base pública: é necessário implementar e testar o que
estiver faltando, incluindo integração de jobs SPU, mídia e callbacks.

O projeto GodOfWar2PC de origem permanece intacto. Nenhum arquivo do jogo ou
arte original do Ben 10 é distribuído aqui. O código novo do projeto usa
GPL-3.0, com componentes MIT identificados separadamente.
