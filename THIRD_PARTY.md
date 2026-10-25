# Procedência

- `third_party/ps3recomp`: sp00nznet e contribuidores, MIT. Revisão fixada no
  submódulo. Aviso completo em `docs/PS3RECOMP-LICENSE.txt`.
- `host/boot_linux.cpp`: adaptação do boot harness MIT do mesmo runtime,
  acrescentando a seleção do backend Vulkan no Linux. Não contém código do jogo.
- `tools/desktop/iso9660.py`: derivado do leitor usado por
  [GenissonEmilio/GodOfWar2PC](https://github.com/GenissonEmilio/GodOfWar2PC),
  GPL-3.0. Sua procedência original está no cabeçalho do arquivo. O limite de
  arquivos foi ajustado para o disco de PS3, que contém mais de 4.096 arquivos.
  A leitura da árvore Joliet foi acrescentada para preservar os nomes no Linux.
- `patches/`: alterações locais ao runtime MIT, incluindo regressões sintéticas.
  A arena POSIX, o tratamento de JTS, os estados de flip e o teclado SDL são
  identificados em `docs/VALIDATION.md`; não são aplicados aos outros ports.
- Organização de build: projeto GodOfWar3PS3 local, baseado no esquema aberto de
  [arnaldolima1/gow-ascension-pc](https://github.com/arnaldolima1/gow-ascension-pc), MIT.
- Referência técnica do mesmo título:
  [andrebrumdev/ben10-omniverse-recomp](https://github.com/andrebrumdev/ben10-omniverse-recomp).
  O fork privado necessário ao build original não está disponível neste projeto.
  Não copiamos o código do host macOS; o boot Linux usa o runtime público MIT.
- RPCS3: ferramenta externa usada exclusivamente em modo `--decrypt` para
  preparar o ELF local. Não executa o jogo neste projeto. O download local fica
  em `artifacts/`, fora do Git. Não é redistribuído como runtime do port.

Arquivos do jogo, firmware, chaves, C/C++ traduzido do jogo e o executável
resultante ficam apenas no computador do usuário e não integram o repositório.
