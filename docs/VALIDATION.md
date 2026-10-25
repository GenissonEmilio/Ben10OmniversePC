# Validação Linux — 24/10/2026

Estado: desenvolvimento, sem release jogável. Fonte de entrada: disco PS3 USA
BLUS31017 01.00, importado localmente. Nenhum dado do jogo está no Git.

## Resultados observados

- Dez testes Python passaram: seis do importador/metadados, dois de referências
  de variáveis PRX e dois de descoberta de código/tabelas de saltos.
- 26 testes CTest passaram: 17 do runtime público e nove regressões locais.
- Os testes públicos de flips GCM, scheduler SPURS e tabelas de saltos passaram.
- AVX2: oito milhões de casos de ponto flutuante e 200 mil casos de `shufb`
  coincidiram com a referência escalar. Não são medições de FPS do jogo.
- Os 13 patches reaplicam em ordem à revisão fixa do SDK e reproduzem exatamente
  os 12 arquivos de runtime alterados na validação local.
- 15.115 funções PPU do jogo, 22 imagens SPU residentes, quatro jobs brutos e
  21 variantes de plugins SPU foram traduzidos e vinculados localmente.
- Também foram traduzidos dez módulos PPU de mídia do firmware 4.25 do próprio
  disco, o plugin `flashATRAC.pic` e 24 programas SPU de codecs encontrados nesses
  módulos. As bibliotecas PPU com inicialização observada retornaram sucesso.
- O binário Linux executou o PPU real, abriu Vulkan em 1280×720, leu dados via
  AIO e renderizou “Press Start”. Start acionou a criação de um perfil; outro
  teste enumerou esse mesmo perfil e leu os 4.096 bytes do arquivo SAV.
- No caminho de mídia recompilada, foram iniciadas as tarefas SPU de demux,
  ATRAC e inicialização AVC. A fila ATRAC trocou comandos/respostas reais entre
  PPU e SPU. Isso ainda não demonstra áudio ou vídeo decodificado corretamente.

## Limites do resultado

A tela de título foi alcançada com `PS3_NO_SAIL=1`: esse modo diagnóstico
reporta falha de criação do player e deixa o jogo seguir seu tratamento de
erro. **Não demonstra reprodução de vídeos** e não é ativado automaticamente.

Com `BEN10_LLE_MEDIA=1`, o caminho normal agora passa da abertura do AVC,
antes bloqueada por argumentos de pilha perdidos. A falha posterior observada
é `0x80610205` (erro fatal do demux), seguida do tratamento de erro e espera.
Ainda faltam APIs de overlays OVIS e desvinculação de fila usadas nesse caminho;
a inicialização AVC retorna antes de haver vídeo validado. Não há evidência
suficiente para atribuir todo o bloqueio a uma única API ausente.

O teste sem player avançou de “Checking content” para “Loading content”, mas
terminou com tela vazia. Não foram validados menu principal, carregamento de
fase, combate ou performance de gameplay. A taxa de flips de uma tela de título
não é uma medição de FPS do jogo. Os testes usam o driver de áudio `dummy`;
a saída de áudio real também permanece sem validação.

Os testes reais usam escopos systemd com limite de RAM de 4 GiB, sem swap,
CPU limitada e timeout, separados das outras instalações. Os builds usam dois
workers e 4 GiB. Nenhum teste desliga ou reinicia o computador.

## Correções reproduzíveis

- Boot POSIX: arena virtual alinhada a 4 GiB e commits de páginas testados.
- Importação: árvore Joliet preserva os nomes que o programa abre em Linux.
- GCM: referência inicial -1 e preservação da conclusão anterior quando um
  frame futuro entra na FIFO antes da entrega do callback.
- SPURS: JTS espera publicação tardia; filas PPU/SPU copiam dados reais,
  implementam FIFO, peek e bloqueio e notificam reservas e tarefas aguardando.
- Atributos de taskset preservam o argumento de 64 bits entregue às tarefas,
  com testes de independência entre instâncias e reinicialização.
- Plugins SPU: entradas PIC preservadas antes/depois de rebase; o codec de
  firmware tem residência delimitada, independente do slot DSP que muda.
- PRX: variáveis VREF vinculadas antes da tradução; código SPU embutido fica
  excluído da descoberta PPU; tabelas guardadas na pilha são identificadas.
- Chamadas PRX preservam a pilha do chamador, incluindo argumentos depois dos
  oito registradores. O teste cobre 13 argumentos e chamadas aninhadas.
- A primeira função de uma thread PPU recebe a área ELFv1 de 112 bytes para
  linkage e argumentos. O teste usa uma página protegida acima da pilha.
- Save: bridges User* respeitam os nove/dez argumentos reais e preservam o
  buffer de listagem fornecido pelo jogo. Teste grava e relê dados em disco.
- `_sys_strchr` converte ponteiros e retorna endereços no espaço guest.
- Teclado SDL: WASD, IJKL, setas, Z/Espaço, X, C, V, Q/E, R/F, Enter e Tab.
  Mapeamento compilado; validação interativa de teclado/controle ainda pendente.

Os patches ficam em `patches/` e são aplicados apenas à revisão fixa do SDK.
Código PPU/SPU gerado, firmware, dumps e binário permanecem fora do Git.
O GitHub Actions valida somente a base sintética, sem dados do jogo.

Referências de contratos e erros:
[RPCS3 cellGcmSys](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/Modules/cellGcmSys.cpp),
[sys_rsx](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/lv2/sys_rsx.cpp),
[cellSpurs](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/Modules/cellSpurs.cpp),
[cellDmux](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/Modules/cellDmux.h)
e [cellOvis](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/Modules/cellOvis.cpp).
RPCS3 foi usado como referência e nos modos `--decrypt` e `--installfw`, para
preparação local do ELF e extração do firmware. O jogo não foi executado por
emulação nesta validação.
