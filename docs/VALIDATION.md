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
- 35 testes CTest passaram, incluindo as regressões locais de janela RSX,
  relatórios de controle e executor paralelo.
- Os testes públicos de flips GCM, scheduler SPURS e tabelas de saltos passaram.
- AVX2: oito milhões de casos de ponto flutuante e 200 mil casos de `shufb`
  coincidiram com a referência escalar. Não são medições de FPS do jogo.
- Os 29 patches reaplicam em ordem à revisão fixa do SDK e reproduzem exatamente
  os 21 arquivos do SDK alterados na validação local.
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

A tela de título foi alcançada com `PS3_NO_SAIL=1`: esse modo
reporta falha de criação do player e deixa o jogo seguir seu tratamento de
erro. **Não demonstra reprodução de vídeos**. O launcher experimental usa
essa opção explicitamente. Os seis arquivos PAM encontrados em `Movies` são
logos de abertura; as cenas de história observadas na primeira fase são 3D.

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

A captura sem player chegou ao menu 3D, “Games”, “New Game”, seleção de
dificuldade, escolha de slot e gravação do save `BLUS31017_10000000`. O mapa
`AD5E48BA6819B809` da fase **Training Time** carregou, com diálogos e tutorial
de combate. A opção Continue também reabriu esse save e carregou a fase.

Os passes intermediários já tinham cenário e personagens, mas o pós-processamento
lia regiões pretas. O título usa `NV4097_SET_WINDOW_OFFSET` (0x02B8) para escrever
as imagens do atlas em endereços como 0x1200, 0x0C00 e 0x1400, mantendo viewport e
scissor relativos à janela. O decoder agora aplica o deslocamento ao endereço de
cada attachment, com pitch e tamanho de pixel próprios, incluindo zeta e AA.
`SURFACE_CLIP` permanece independente. A captura posterior confirmou a imagem
final 3D no menu e na introdução da fase. Os dumps contêm arte do jogo e ficam
somente em `artifacts/rsx69/` e `artifacts/probe69-present.ppm`, fora do Git.
O teste sintético cobre atlas, quatro pitches MRT, profundidade, AA, formatos,
reset e decodificação repetida. A suíte neutra do draw engine também passou.

Foram observados movimento via analógico, transformação em Four Arms e ataques
na arena. Os comandos passaram pela API cellPad, com capturas finais 3D em
`artifacts/probe72-move.png`, `probe72-transform.png` e `probe72-combat.png`.
Isso valida essas ações no começo da fase, sem certificar o jogo inteiro.
O ensaio posterior confirmou eventos de teclado pelo SDL e pela API cellPad:
Enter abriu o menu, W selecionou Games e Espaço confirmou menus, dificuldade
e slot de save. A simulação global XTest foi ignorada pelo Xwayland, inclusive
na consulta ao mapa de teclas do próprio servidor. O teste usou eventos X11
enviados diretamente à janela do processo. Não equivale a testar um controle
físico; as ações de combate acima foram inicialmente validadas por PAD_FILE.

Depois da recompilação com as operações x86 otimizadas, o launcher abriu a janela
em tela cheia de 1920×1080, com renderização interna de 1280×720. O mesmo caminho
de teclado SDL moveu Ben com W, acionou a transformação em Four Arms com a seta
para cima, executou ataques com C e abriu a pausa com Enter. As capturas ficam
em `artifacts/gui-final-keyboard-move2.png`, `gui-final-keyboard-transform2.png`,
`gui-final-keyboard-attack2.png` e `gui-final-keyboard-pause.png`.
Essa execução usou somente os eventos da janela, sem PAD_SCRIPT/PAD_FILE.
O contador de flips observou cerca de 12–14/s na introdução e 8–12/s em parte
do combate; continua insuficiente para considerar o port pronto. A comparação
não isola cenas idênticas entre builds, portanto não atribui uma porcentagem
de ganho no jogo às novas operações.

A investigação posterior do port macOS e as novas otimizações x86 estão em
[PERFORMANCE.md](PERFORMANCE.md). O hook do patch 0029 limita o ambiente de
ponto flutuante ao job de geometria 100. O caminho normal e o caminho com
entrada delimitada passaram separadamente pela suíte de oito milhões de
casos. As seis operações inteiras passaram por dois milhões de vetores com
oráculos independentes; quatro threads verificaram o aninhamento de jobs e
restauração exata do MXCSR em mil repetições por thread.

O replay de uma entrada real do kernel, com 4.516 iterações, consumiu mediana
1.076,785 µs na base e 655,102 µs com as duas otimizações, com todos os bytes
finais de registradores/LS idênticos nas doze execuções. Isso mede o kernel,
sem prometer a mesma proporção de ganho em FPS.

Dois ensaios headless de 360 s usaram dois workers, CPU de 400%, resolução
1280×720, áudio dummy e dump a cada dez presents. A versão anterior observou
aproximadamente 9–13,5 flips/s em partes da introdução e 9–12 no combate/tutorial;
a otimizada, 15–16,6 em partes da introdução e 8–15 no combate/tutorial. A cena
não ficou idêntica em todos os intervalos; houve comandos adicionais para
fechar tutoriais e transformar o personagem. Esses números descrevem as
execuções, sem isolar um ganho percentual no FPS. A otimizada carregou a arena,
renderizou Four Arms e respondeu aos comandos de movimento e ataque.

O ensaio final abriu uma janela SDL fullscreen de 1920×1080, com imagem interna
1280×720 e stream PulseAudio float32 estéreo de 48 kHz ativo e sem mute. Com
dumps espaçados a cada 120 presents, partes da introdução observaram cerca de
17–18 flips/s. O binário do build automático final foi idêntico por SHA-256 ao
usado nessa execução. O combate continua abaixo da taxa original de 30 Hz.
Eventos SDL da própria janela também confirmaram a transformação com a
seta para cima e Escape abrindo a pausa. A captura da pausa com Four Arms
ao fundo está em `artifacts/live54-keyboard-pause.png`.

O teste PulseAudio abriu o stream do próprio processo. Uma captura isolada desse
stream, com roteamento temporário restaurado ao final, mediu 966.656 amostras,
582.144 não nulas, pico 0,511 e RMS 0,114, sem valores não finitos. Não foi
capturado áudio de outros aplicativos. Isso verifica saída real do backend,
sem afirmar avaliação auditiva de todas as falas/efeitos.

O novo executor Linux opcional distribui os jobs entre workers com IDs SPU
exclusivos. Ele conserva callbacks e notificações reais e drena a fila antes de
barreiras, comandos de controle e cancelamento. O padrão permanece serial.
Dois workers produziram cerca de 12 flips/s na introdução, ante 6–7 no caminho
serial; no combate a taxa também varia com a cena. Ainda falta desempenho
suficiente e validação ampliada para uma entrega pronta.

Os testes sintéticos do executor cobrem concorrência, completamento único,
barreiras por chain, dois produtores, fila limitada e fallback serial. A operação
SPU `rotqby` ganhou uma implementação SSSE3 comparada ao loop escalar em dois
milhões de vetores. Os testes de cellPad verificam bytes BE, analógicos, botões,
combinação, duração, retorno ao neutro e comandos malformados.

A seleção de bytes SHUFB no x86 passou a mapear os seletores na ordem do host,
com uma tabela para as constantes especiais. Foram comparados 8.192 casos
exhaustivos de seletor/posição e dois milhões de vetores mistos à referência
escalar, além dos 200 mil casos do SDK. A conversão CSFLT sem escala ganhou
um caminho AVX2 que trunca explicitamente a mantissa. Foram comparados dois
milhões de vetores, todos os fatores de escala e os quatro modos de arredondamento
do host; zeros, INT_MIN e limites de precisão também são verificados. Os ganhos
dos microbenchmarks não são uma medição do FPS do jogo.

O launcher Tk foi exercitado com o executável real e dados de teste separados.
Inicia o jogo em escopo systemd, limita recursos, mantém diagnóstico limitado
a 700 linhas e encerra somente o próprio escopo. A janela foi conferida em
1000×760, incluindo o aviso de prévia. O atalho foi instalado e passou em
`desktop-file-validate`. A importação/tradução inicial ainda segue o procedimento
do README: o launcher abre arquivos já preparados e não promete preparação
automática de um disco novo. O script de bibliotecas copia dependências da
imagem local do build, preserva as do sistema e registra SHA-256/origem.

Os testes reais usam escopos systemd com limite de RAM de 4 GiB, sem swap,
CPU limitada e timeout, separados das outras instalações. Os builds usam dois
workers e 4 GiB. Volumes de compilação compartilhados usam a opção SELinux
`:z`, evitando que um contêiner retire o acesso do outro no Fedora. Nenhum
teste desliga ou reinicia o computador.

## Correções reproduzíveis

- RSX: janelas do framebuffer respeitam `WINDOW_OFFSET`, inclusive endereço local zero.
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
  Snapshot atômico alimentado pelos eventos da thread de vídeo; perda de foco
  libera as teclas. O teste usa relatórios BE reais e verifica foco, movimento,
  ataque e retorno ao neutro. Esc mapeia Start para pausar, em vez de encerrar
  o processo. Controle físico ainda não foi testado.

Os patches ficam em `patches/` e são aplicados apenas à revisão fixa do SDK.
Código PPU/SPU gerado, firmware, dumps e binário permanecem fora do Git.
O GitHub Actions valida somente a base sintética, sem dados do jogo.

O commit `4faabf5` passou também na
[validação do GitHub Actions](https://github.com/GenissonEmilio/Ben10OmniversePC/actions/runs/40182818846).

Referências de contratos e erros:
[RPCS3 cellGcmSys](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/Modules/cellGcmSys.cpp),
[sys_rsx](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/lv2/sys_rsx.cpp),
[cellSpurs](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/Modules/cellSpurs.cpp),
[cellDmux](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/Modules/cellDmux.h)
e [cellOvis](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/Modules/cellOvis.cpp).
RPCS3 foi usado como referência e nos modos `--decrypt` e `--installfw`, para
preparação local do ELF e extração do firmware. O jogo não foi executado por
emulação nesta validação.

A semântica de deslocamento foi conferida na implementação de referência
[RPCS3 RSXThread::get_framebuffer_layout](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/RSX/RSXThread.cpp),
na parte que trata os raster/window offsets. Foram usadas as regras de endereço;
a implementação e os vetores deste projeto são independentes.
