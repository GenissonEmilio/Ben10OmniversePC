# Validação Linux — 24/10/2026

Estado: desenvolvimento, sem release jogável. Fonte de entrada: disco PS3 USA
BLUS31017 01.00, importado localmente. Nenhum dado do jogo está no Git.

## Resultados observados

- Seis testes do importador passaram, incluindo nomes Joliet em Linux,
  metadados inválidos, hash incorreto e preservação de destinos existentes.
- 19 testes CTest passaram: 17 do runtime público e dois novos testes da
  memória do boot e da fila SPURS com publicação tardia de trabalho.
- O teste GCM de flips passou, incluindo o novo caso em que um frame futuro
  é enfileirado entre o tick de conclusão anterior e sua entrega ao jogo.
- O teste público do scheduler SPURS passou com módulos sintéticos.
- 15.115 funções PPU, 22 imagens SPU residentes, quatro jobs brutos e 21
  variantes de plugins SPU foram traduzidos e vinculados localmente.
- O binário Linux iniciou o programa PPU real, abriu Vulkan em 1280×720,
  leu dados via AIO e renderizou a tela “Press Start”. O teste acionou Start e
  o jogo chamou a API de save. Ainda não foi validada a reabertura desse save.

## Limites do resultado

A tela de título foi alcançada com `PS3_NO_SAIL=1`: esse modo diagnóstico
reporta falha de criação do player e deixa o jogo seguir seu tratamento de
erro. **Não demonstra reprodução de vídeos**. O teste normal chega ao primeiro
PAM, mas o runtime público ainda tem APIs SAIL ausentes e playback incompleto.
Esse modo não é habilitado automaticamente pelo script de execução.

A transição após Start ainda permanece em “Checking content”. Não foi
validado menu principal, carregamento de fase, combate ou performance de
gameplay. A taxa de flips de uma tela de título não é uma medição de FPS do jogo.

Os testes reais usam escopos systemd com limite de RAM de 4 GiB, sem swap e
timeout, separados das outras instalações. Os builds usam dois workers e
4 GiB. Nenhum teste desliga ou reinicia o computador.

## Correções reproduzíveis

- Boot POSIX: arena virtual alinhada a 4 GiB; commits e conversão de ponteiros
  guest/host passam no teste real da API `vm_commit`.
- Importação: preferência pela árvore Joliet, preservando nomes como
  `Configurations/0000000100000001.cfg`.
- GCM: referência inicial -1; uma solicitação FIFO futura preserva o estado
  de conclusão do frame anterior até um reset explícito.
- SPURS: JTS mantém o consumidor na posição de espera; a publicação tardia
  de um job é executada, e shutdown interrompe a espera.
- Plugins: descoberta antes e depois de rebase para preservar entradas
  indiretas PIC; 21 assinaturas distintas verificadas no ELF original.
- Teclado SDL: WASD, IJKL, setas, Z/Espaço, X, C, V, Q/E, R/F, Enter e Tab.
  O mapeamento foi compilado; a validação interativa ainda está em andamento.

Os patches ficam em `patches/` e são aplicados apenas à revisão fixa do SDK.
O código PPU/SPU gerado, dumps e binário do jogo permanecem fora do Git.

Fontes para a semântica da API gráfica:
[RPCS3 cellGcmSys](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/Modules/cellGcmSys.cpp)
e [sys_rsx](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/lv2/sys_rsx.cpp).
RPCS3 foi usado como referência de código e para `--decrypt`, sem executar o
jogo por emulação nesta validação.
