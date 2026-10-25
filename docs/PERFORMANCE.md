# Investigação de desempenho no Linux

Referência examinada em 25/10/2026:
[andrebrumdev/ben10-omniverse-recomp](https://github.com/andrebrumdev/ben10-omniverse-recomp),
revisão `9f534e239582f3add98316804d4f011e2b31a454`.
Ela documenta BLUS31017 1.00, mas utiliza Apple Silicon/Metal e um runtime
privado. A primeira fase está verificada pelo autor; a campanha restante não.
Os números do M5 não descrevem o desempenho deste computador Linux.
Nenhum código do host macOS foi incorporado.

## O que se aplica à nossa base

As correções públicas citadas no README da referência, PRs
[#206](https://github.com/sp00nznet/ps3recomp/pull/206),
[#207](https://github.com/sp00nznet/ps3recomp/pull/207) e
[#208](https://github.com/sp00nznet/ps3recomp/pull/208), já estão na revisão
fixa do SDK. Foram conferidos o teste de flags SSE, o reconhecimento do
epílogo PPU e o limite de 32 palavras dos comandos de constantes RSX.
As opções do runtime privado, como `PS3_VM_FAST_MASK` e `PS3_GIANT_HANDOFF`,
não existem nesta base; defini-las não implementaria essas otimizações.

As notas de custo por job orientaram uma investigação independente do
kernel de vértices `job_00844E80`, especialmente a função LS `0x2E0`.
O rebase da referência é diferente: não se devem copiar seus endereços.
Os caminhos SHUFB, ROTQBY e CSFLT do nosso x86 já tinham regressões e
otimizações próprias. O trabalho seguinte foi a aritmética desse kernel.

## Caminho x86 de geometria

`BEN10_VERTEX_FMA=ON` compila as quatro unidades de jobs identificados com FMA3,
AVX2 e `-frounding-math`. O patch 0029 delimita sua execução na thread do job:
truncamento em direção a zero, DAZ/FTZ e exceções mascaradas. A saída restaura
exatamente o MXCSR anterior, inclusive após HALT tratado pelo dispatcher.
Os IDs acelerados são 100–103. Jobs aninhados conservam seu próprio contexto. O PPU e as outras threads
não recebem esse modo de arredondamento.

Entradas com expoente 255 e saídas na fronteira de overflow IEEE voltam ao
oráculo inteiro do SDK, que representa o alcance estendido do SPU. Denormais
e zeros são normalizados. Operações double/FRDS restauram temporariamente o
ambiente normal do helper. Não se usa `fast-math` nem aproximação numérica.

O caminho excepcional fica fora do laço. Com Clang, `preserve_all` evita
que essa chamada rara force cópias desnecessárias dos registradores do
chamador. A unidade do job tem entrada obrigatoriamente delimitada pelo hook;
não consulta variáveis de ambiente ou TLS em cada operação aritmética.
O boot rejeita uma CPU sem AVX2/FMA3 quando esse build está ativo.

`host/vertex_integer.h` implementa seis operações de halfwords com instruções
vetoriais x86 e aritmética modular explícita, evitando shifts de inteiros
negativos em C. Esses helpers também se limitam à mesma unidade do job.

Uma primeira versão mantinha verificações de modo e caminhos grandes dentro
do laço. Embora passasse na comparação numérica, não demonstrou ganho no jogo.
Ela não foi adotada. O ganho abaixo é da versão com a entrada delimitada e
o caminho excepcional separado.

## Medição do mesmo trabalho

Foi capturado localmente um estado de entrada do kernel durante Training Time.
Cada execução repete o mesmo contexto/LS mil vezes, incluindo a cópia inicial
do estado. A saída compara todos os registradores e os 256 KiB de LS. O kernel
capturado executou 4.516 iterações e saiu em LS `0x5E0`.

Três passagens com o harness versionado, fixando a CPU lógica 0 (P-core) e
alternando a ordem dos quatro builds, produziram:

| Variante | Mediana µs por replay | Faixa µs |
| --- | ---: | ---: |
| Base AVX2 | 1.076,785 | 1.062,261–1.087,450 |
| Inteiros vetoriais | 1.032,780 | 1.028,564–1.058,071 |
| FMA3 delimitada | 692,769 | 681,957–704,742 |
| Inteiros + FMA3 | 655,102 | 652,665–671,138 |

As doze saídas tiveram o mesmo SHA-256:
`a6f072ee678ab9a32b23b404b362513070f98af7e28c5d2af2a76b3f5e4eca72`.
A redução mediana de aproximadamente 39% se refere **a esse replay do kernel**,
não ao FPS do jogo. As quatro variantes desta primeira medição usaram as mesmas
flags de ponto flutuante estrito para isolar os helpers. Uma passagem anterior,
sem afinidade, também encontrou aproximadamente 39%. Os dados, executáveis
de replay e resultados brutos ficam em `artifacts/`, fora do Git.

O processo para repetir a medição com as fontes e capturas locais está em
[bench/README.md](../bench/README.md). As regressões sintéticas verificam oito
milhões de casos de ponto flutuante por caminho (com e sem a garantia de entrada
delimitada), limites adversariais, quatro threads com jobs aninhados e dois
milhões de vetores de inteiros contra oráculos independentes.

## Critérios para o jogo

As execuções reais mantêm 30 Hz, dois workers, a mesma resolução, limite de
400% de CPU, RAM de 4 GiB e saves de diagnóstico separados. Menus, carregamento,
introdução e combate são fases distintas. Capturas e debugger não entram
na janela de medição. Áudio e janela também precisam ser identificados: um
ensaio headless/mudo não certifica a experiência final no launcher.

O contador de diagnóstico mede pedidos de flip, não uma análise da velocidade
da simulação. O modo 60 Hz da referência muda o passo de tempo do jogo; ativá-lo
antes de sustentar a taxa necessária pode reduzir a velocidade da simulação.
Ele permanece desativado. A compatibilidade da campanha e os logos de abertura
continuam pendentes conforme [VALIDATION.md](VALIDATION.md).

## Experimentos adicionais preservados em 25/10/2026

A meta de 30 FPS no combate **não foi atingida**. Ensaios headless da arena,
com quatro workers, cota de CPU de 600% e saves separados, continuaram na
faixa aproximada de 10–18 quadros novos/s. Variações de cena, transformação,
ataques e avisos do tutorial impedem atribuir pequenos deltas a uma mudança.

- `BEN10_VERTEX_CACHE=ON`: transformação local dos loops de geometria e
  skinning em registradores C locais. Exige fingerprints das funções,
  mantém a saída completa e conserva fallback para diagnósticos e aliasing de
  LS/contexto. Desligado por padrão. Os replays privados de geometria e skinning
  mantiveram os hashes das saídas; isso não certifica um aumento de FPS.
- Os helpers de inteiros incluem controles CHD/CHX e FI vetorial exato,
  comparados em dois milhões de vetores. A checagem de faixa do caminho FMA
  usa o resultado: entradas IEEE especiais necessariamente produzem um
  resultado especial nessas operações, e a fronteira de overflow RTZ também
  volta ao oráculo inteiro.
- Patch 0030 reutiliza staging coerente de uploads Vulkan com espera antes da
  reutilização. Patch 0031 consulta a ativação do sampler SPU uma vez por thread.
- `PS3RECOMP_VK_ASYNC_DRAWS=2..32`: slots com buffers, descritores e fence
  próprios. `PS3RECOMP_VK_BATCH_DRAWS=1` agrupa seus command buffers em uma
  submissão; ambos permanecem desligados no caminho padrão. Recursos são
  drenados antes de mutação, leitura, apresentação e destruição. O build
  nativo compilou. Uma sondagem curta com a camada Khronos e validação de
  sincronização ativa não registrou VUIDs; combate e ganho do batching não
  foram validados. Não é uma opção recomendada para uso final.
- `BEN10_JOB_PIPELINE=1`: comandos JOB/NOP/NEXT/RESET_PC/CALL podem avançar
  sem drenar a fila; comandos de sincronização e encerramento mantêm a
  espera. Desligado por padrão. A alteração não demonstrou o ganho necessário.
- `BEN10_JOB_STATS=1`, com `BEN10_WATCHDOG=1`, relata trabalho dos jobs,
  CPU, tempo de barreiras e contagem de comandos. `BEN10_FIFO_IDLE_MS=0..4`
  altera a espera diagnóstica da fila gráfica; o padrão continua em 4 ms.

Os contadores Vulkan distinguem apresentações totais de apresentações com
novos desenhos do jogo. Reapresentar a mesma imagem pode inflar o primeiro
número. O watchdog calcula a taxa do segundo com o intervalo real da amostra;
isso ainda não mede diretamente os ticks de simulação.

Uma inspeção da thread gráfica encontrou a espera por fence em
`vk_submit_and_wait → vk_eng_draw → sink_end`. As estatísticas da cadeia de
jobs também mostraram trabalho paralelo, portanto otimizar um kernel isolado
não basta para explicar o tempo de um quadro inteiro.

Experiências de separação da última iteração e cache de projeção de vértices
mantiveram a saída do replay, mas não melhoraram seu tempo. Foram descartadas.
As capturas, binários e perfis usados nessas comparações não fazem parte do Git.

### Outras referências examinadas

- [Ben 10 no Apple Silicon: custo por job](https://github.com/andrebrumdev/ben10-omniverse-recomp/blob/main/notes/2026-10-xx-task7-custo-por-job.md):
  redução do corpo de geometria, remoção de gates por operação e medição por job.
  O backend Metal agrupa os registros antes de enviar comandos à GPU.
- [God of War: Ascension](https://github.com/arnaldolima1/gow-ascension-pc):
  os patches de ponto flutuante x86 e operações de bytes ilustram o mesmo
  gargalo de geometria. O desempenho desse jogo não é uma previsão para este.
- [RPCS3: caminhos SHUFB a partir de constantes conhecidas](https://github.com/RPCS3/rpcs3/commit/2416d652625eaed5df2ff8d67063fa145021a883):
  oportunidade de especialização no compilador. Nenhum código desse commit
  foi incorporado.
- [NVIDIA: Vulkan Dos and Don'ts](https://developer.nvidia.com/blog/vulkan-dos-donts/)
  e [Khronos: command buffers](https://docs.vulkan.org/samples/latest/samples/performance/command_buffer_usage/README.html):
  reduzir submissões pequenas, evitar esperas por desenho e reutilizar recursos.

A revisão pública mais recente de ps3recomp consultada continua sendo
`a6d76c8`, já usada pelo projeto; atualizar a referência, por si só, não
adicionaria as partes privadas do port Metal.
