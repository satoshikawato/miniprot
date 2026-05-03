# Miniprot Ganflu WebAssembly

This directory contains the browser-facing wrapper for the Ganflu-specific
Miniprot build.

Build the ESM module and wasm binary from the repository root:

```sh
make -f Makefile.wasm ganflu-wasm
```

The build writes:

- `web/dist/miniprot-ganflu.mjs`
- `web/dist/miniprot-ganflu.wasm`

If the generated files in `web/dist/` are committed or redistributed, keep
`web/THIRD_PARTY_NOTICES.md` with them. The wasm build uses Emscripten runtime
support and links zlib through `-sUSE_ZLIB=1`.

The wrapper exports a small fixed Ganflu API:

```js
import { createGanfluMiniprot } from './miniprot-ganflu.js';

const miniprot = await createGanfluMiniprot();
const gff3 = miniprot.run({
  genomeFasta,
  proteinFasta,
  prefix: 'MP',
  intronOpenPenalty: 15,
  bestN: 100,
  outputScoreRatio: 0.1,
  secondaryToPrimaryRatio: 0.1
});
```

The wasm runner is intentionally single-threaded and runs the equivalent of:

```sh
miniprot -t1 -P MP --gff -J 15 -N 100 --outs=0.1 -p 0.1 genome.fa proteins.faa
```

Pass plain FASTA text or bytes. The wrapper returns GFF3 text, including the
`##PAF` records produced by Miniprot's normal `--gff` output.
The same settings can also be passed with CLI-style keys: `N` or `-N`,
`outs` or `--outs`, and `p` or `-p`.
