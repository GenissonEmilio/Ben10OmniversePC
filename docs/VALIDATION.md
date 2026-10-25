# Validação Linux — 25/10/2026

Estado: desenvolvimento, sem release jogável. Fonte de entrada: disco PS3 USA
BLUS31017 01.00, importado localmente. Nenhum dado do jogo está no Git.

## Resultados observados

- 21 testes Python passaram: seis do importador/metadados, dois de
  referências de variáveis PRX, dois de descoberta de código/tabelas de saltos,
  três de aplicação incremental de patches/preservação de edições locais e
  dois de construtores SPU identificados pelos metadados ELF, um da seleção de
  exports SRE, um de publicação de tarefas antes da espera, três de variantes
  OVIS e um de consumidores independentes de metadados/pixels de vídeo.
- 26 testes CTest passaram: 17 do runtime público e nove regressões locais.
- Os testes públicos de flips GCM, scheduler SPURS e tabelas de saltos passaram.
- AVX2: oito milhões de casos de ponto flutuante e 200 mil casos de `shufb`
  coincidiram com a referência escalar. Não são medições de FPS do jogo.
- Os 19 patches reaplicam em ordem à revisão fixa do SDK e reproduzem exatamente
  os 16 arquivos do SDK alterados na validação local.
- 15.115 funções PPU do jogo, 22 imagens SPU residentes, quatro jobs brutos e
  21 variantes de plugins SPU foram traduzidos e vinculados localmente.
- Também foram traduzidos onze módulos PPU de mídia do firmware 4.25 do próprio
  disco, o plugin `flashATRAC.pic` e 24 programas SPU de codecs encontrados nesses
  módulos, com oito bancos OVIS de `libvdec` traduzidos separadamente. As
  bibliotecas PPU com inicialização observada retornaram sucesso. `libsre`
  publica somente os quatro exports OVIS, preservando o scheduler HLE.
- O binário Linux executou o PPU real, abriu Vulkan em 1280×720, leu dados via
  AIO e renderizou “Press Start”. Start acionou a criação de um perfil; outro
  teste enumerou esse mesmo perfil e leu os 4.096 bytes do arquivo SAV.
- No caminho de mídia recompilada, foram iniciadas as tarefas SPU de demux,
  ATRAC, AVC e pós-processamento. A fila ATRAC trocou comandos/respostas reais entre
  PPU e SPU. Isso ainda não demonstra áudio ou vídeo decodificado corretamente.

## Limites do resultado

A tela de título foi alcançada com `PS3_NO_SAIL=1`: esse modo diagnóstico
reporta falha de criação do player e deixa o jogo seguir seu tratamento de
erro. **Não demonstra reprodução de vídeos** e não é ativado automaticamente.

Com `BEN10_LLE_MEDIA=1`, o caminho normal agora passa da abertura do AVC,
antes bloqueada por argumentos de pilha perdidos. A correção do
cabeçalho da fila eliminou, no teste seguinte, as transferências de zero bytes
do endereço 1 e o erro fatal `0x80610205` do demux que aparecia antes.
O firmware do disco contém a implementação OVIS em `libsre`; ela foi traduzida
e vinculada. A descoberta de `.ctors`/`.dtors` e arrays ELF foi corrigida.
Também foi encontrada uma espera circular: o codec publicou novas tarefas no
taskset e esperou por elas, mas o runtime só examinava essas tarefas na saída
de outra tarefa. A admissão antes de WAIT_SIGNAL eliminou essa espera e iniciou
os workers AVC e de pós-processamento. O teste sintético executa o caminho de
produção e exige que os peers entreguem o sinal, sem simular uma saída anterior.
Ainda há uma espera posterior no pipeline, com tela preta. Capturas do debugger
confirmaram chamadas explícitas de saída de algumas tarefas; isso não certifica
que todo o trabalho de decodificação tenha sido concluído corretamente.

O ELF SPU de `libvdec` contém oito segmentos executáveis sobrepostos no mesmo
endereço LS. Eles agora são traduzidos individualmente e registrados por
assinatura e extensão. O loader carrega os segmentos residentes e deixa os bancos
OVIS para os DMAs do programa. Chamadas do código residente aos bancos usam a
resolução dinâmica. Um teste fabrica dois bancos com resultados diferentes e
verifica que nenhum deles é incorporado como código fixo na imagem residente.

O diagnóstico `BEN10_HOST_VIDEO=1` usa o decoder FFmpeg do runtime mantendo
SAIL, demux e áudio recompilados. Foram observados FFmpeg 60.31.102, AUs reais,
callbacks AUDONE/PICOUT e consultas GetPicItem pelo SAIL. O decoder alcança o
limite da fila sem completar a apresentação. Foram corrigidos o consumo
independente de metadados e pixels, a estabilidade dos metadados enquanto o
frame aguarda e o tamanho do buffer de imagem decodificada alinhado a 128 bytes.
Uma regressão percorre 40 frames fabricados, incluindo reutilização da fila,
compara pixels e PTS e exige EMPTY após consumir todos os metadados disponíveis.
Essas correções ainda não demonstram reprodução visível nem gameplay.

O teste sem player avançou de “Checking content” para “Loading content”, mas
terminou com tela vazia. Não foram validados menu principal, carregamento de
fase, combate ou performance de gameplay. A taxa de flips de uma tela de título
não é uma medição de FPS do jogo. Os testes usam o driver de áudio `dummy`;
a saída de áudio real também permanece sem validação.

Os testes reais usam escopos systemd com limite de RAM de 4 GiB, sem swap,
CPU limitada e timeout, separados das outras instalações. Os builds usam dois
workers e 4 GiB. Volumes de compilação compartilhados usam a opção SELinux
`:z`, evitando que um contêiner retire o acesso do outro no Fedora. Nenhum
teste desliga ou reinicia o computador.

## Correções reproduzíveis

- Boot POSIX: arena virtual alinhada a 4 GiB e commits de páginas testados.
- Importação: árvore Joliet preserva os nomes que o programa abre em Linux.
- GCM: referência inicial -1 e preservação da conclusão anterior quando um
  frame futuro entra na FIFO antes da entrega do callback.
- SPURS: JTS espera publicação tardia; filas PPU/SPU copiam dados reais,
  implementam FIFO, peek e bloqueio e notificam reservas e tarefas aguardando.
  O cabeçalho da fila usa tamanho em +0x08, buffer em +0x10 e direção em +0x1C,
  como o código SPU do codec consome; um vetor literal BE verifica esse formato.
- Atributos de taskset preservam o argumento de 64 bits entregue às tarefas,
  com testes de independência entre instâncias e reinicialização.
- Plugins SPU: entradas PIC preservadas antes/depois de rebase; o codec de
  firmware tem residência delimitada, independente do slot DSP que muda.
- SPU: construtores e destrutores de tabelas ELF com apenas um elemento
  também são traduzidos, sem depender de chamadas diretas no código.
- Tarefas publicadas pelo código SPU são admitidas antes de WAIT_SIGNAL, com
  proteção contra despachar novamente uma tarefa já em execução.
- Codecs OVIS preservam cada variante e usam residência delimitada por DMA.
- `cellVdecGetPicItem` avança seus metadados sem consumir os pixels; cada slot
  tem um bloco de metadados estável e informa o tamanho da imagem decodificada.
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
