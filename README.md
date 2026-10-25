# Ben 10 Omniverse PC — Linux

Recompilação nativa em desenvolvimento da versão PS3 **BLUS31017 01.00** de
Ben 10 Omniverse. O arquivo fornecido foi identificado como PS3: o runtime PS2
do God of War II não é compatível com ele. Reaproveitamos o leitor de disco e a
organização da experiência de importação desse projeto; PPU/SPU usam ps3recomp.

**Ainda não há port jogável validado nem release.** Este repositório acompanha
o trabalho real de importação, tradução, compilação e diagnóstico. Não confunda
compilar o executável ou abrir uma janela com validar gameplay.

## Preparação local

O importador valida metadados, versão, limites de arquivos e o SHA-256 do
executável antes de extrair. O ZIP/ISO original permanece inalterado.

```bash
git submodule update --init --recursive
docker build -t ben10-omniverse-tools:local -f docker/Dockerfile .
python3 tools/import_game.py '/caminho/jogo.iso'
```

Os arquivos importados ficam em `~/.local/share/Ben10OmniversePC/game`.
O executável SELF do disco precisa ser convertido para ELF com uma ferramenta
compatível. Na validação local, RPCS3 foi usado exclusivamente com `--decrypt`;
não usamos emulação como substituto da recompilação escolhida para este projeto.

`tools/analyze_elf.py` valida o ELF, identifica funções/imports e extrai imagens
SPU. Os resultados são separados por SHA-256. As ferramentas de ps3recomp
traduzem esses arquivos localmente; `scripts/build-native.sh` compila as fontes
geradas com Clang 18 e o backend Vulkan de Linux. O build usa duas tarefas e um
limite de 4 GiB de RAM em um contêiner sem rede.

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
