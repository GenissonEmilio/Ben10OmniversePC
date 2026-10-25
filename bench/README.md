# Replay local do kernel de geometria

Este diretório contém o harness, sem instruções nem dados do jogo. O preparador
extrai a função identificada **das fontes geradas localmente pelo usuário** e
grava o C resultante somente em `artifacts/`, que é ignorado pelo Git.
Não publique as fontes extraídas, dumps, saídas ou executáveis resultantes.

A captura usa um breakpoint na entrada de
`job_00844E80_spu_func_000002E0` do próprio processo de diagnóstico. Na revisão
fixa x86_64, `sizeof(spu_context)=268848`, o ponteiro `ls` está em `+264192`
e os primeiros 2048 bytes são os registradores. Capture o contexto completo
e os 262144 bytes apontados por `ls`; não confunda o ponteiro com a área
`ls_store`. Confirme esses offsets se mudar o SDK. O replay troca o ponteiro
capturado por sua própria LS; não reutiliza ponteiros do processo anterior.

O kernel escolhido só acessa registradores/LS e retorna ao dispatcher em
`0x5E0`. O harness não executa DMA ou o restante do job. Debugger e capturas
devem estar desativados durante a medição de FPS do jogo.

Depois de obter localmente `artifacts/vertex-context.bin` e
`artifacts/vertex-ls.bin`:

```bash
python3 bench/prepare_vertex_replay.py
```

Dentro do contêiner `ben10-omniverse-tools:local`, na pasta `/workspace`,
compile as variantes. Todos os caminhos de saída abaixo são privados:

```bash
inc='-Ithird_party/ps3recomp/include -Ithird_party/ps3recomp/runtime/spu -Ihost'
clang++-18 -O2 -DBEN10_VERTEX_FMA=1 -Ihost \
  -c host/vertex_float_env.cpp -o artifacts/vertex-replay/env.o

for variant in base integer fma both; do
  opts=''
  case "$variant" in
    integer) opts='-DVERTEX_INT' ;;
    fma) opts='-DVERTEX_FMA -DBEN10_VERTEX_ALWAYS_SCOPED' ;;
    both) opts='-DVERTEX_INT -DVERTEX_FMA -DBEN10_VERTEX_ALWAYS_SCOPED' ;;
  esac
  clang-18 -O3 -mavx2 -mfma -frounding-math -ffp-contract=off \
    -fno-strict-aliasing $inc $opts \
    artifacts/vertex-replay/kernel.c bench/vertex_replay.c \
    artifacts/vertex-replay/env.o -lstdc++ -lm \
    -o "artifacts/vertex-replay/$variant"
done
```

No host x86_64 com AVX2/FMA3, execute uma variante por vez, com os outros
testes de carga encerrados. Alterne a ordem em repetições posteriores:

```bash
for variant in base integer fma both; do
  timeout 30s "artifacts/vertex-replay/$variant" \
    artifacts/vertex-context.bin artifacts/vertex-ls.bin \
    "artifacts/vertex-replay/$variant.out"
done
sha256sum artifacts/vertex-replay/*.out
```

Exija saídas idênticas e o mesmo número de iterações/PC antes de interpretar
os tempos. A duração inclui a cópia do contexto/LS antes de cada repetição.
Não converta um ganho desse kernel em uma porcentagem de FPS.

Para comparar executáveis completos sem substituir o usado pelo launcher,
`scripts/probe-native.sh` aceita `BEN10_PROBE_BINARY=/caminho/absoluto` e
`BEN10_PROBE_WINDOW=0` para diagnóstico headless. Ele mantém o timeout,
os limites de recursos e os saves separados. O padrão abre janela.
