# NAI (NasuaAI)

NAI runs the bundled SmolLM2 model with a native NasuaOS inference backend.
The backend parses GGUF metadata and tensor directories, loads the quantized
weights from the boot ISO, executes RMSNorm, grouped-query attention, RoPE,
SwiGLU feed-forward blocks, and greedy token generation. No host process,
network service, or canned-answer table is used.

The chat window accepts typed prompts: printable keys edit the prompt,
Backspace removes characters, Left/Right move the cursor, and Enter generates
the local model response. Prompts are formatted with the SmolLM2 chat template
and are never sent over the network.

## Bundled model

NAI bundles SmolLM2-135M-Instruct IQ4_XS from
`bartowski/SmolLM2-135M-Instruct-GGUF` (Apache-2.0 metadata). Its expected
size is 90,897,760 bytes and its SHA-256 is
`d766661c2e39c3da78d8eddf15287af263dce1eedc1c9c8093ed8e54eb067dbc`.
The file is below GitHub's 100 MB limit, so it does not require Git LFS.
The GGUF and its Apache-2.0 license, attribution, and third-party notices are
kept together in [`model/`](./model). The ISO build verifies the pinned
artifact and stores it separately at `/system/NAI/SMOLLM2.GG`; it does not
download the model or put it in the rootfs. The rootfs remains 40 MiB. NAI
reads the ISO file on demand through the IDE/ATAPI drive, so the model's
contents are not loaded during normal system startup. This requires booting
from an ISO with an accessible optical drive; the HDD-only image has no ISO
model source.

The model's card describes it as English-first and warns that outputs may be
inaccurate; Polish quality is not guaranteed. The pinned llama.cpp revision
is used as the host reference and license source. NasuaOS uses its own native
GGUF transformer backend in the 64-bit kernel and exposes generation through
the NAPP ABI.

The quantizer states that it used llama.cpp `b3991`, pinned to commit
`fc83a9e58479e4dd70054daa7afe5184c1bbe545`. That llama.cpp revision is MIT
licensed (not Apache-2.0). Its MIT license is in
[`model/LLAMA-CPP-MIT.txt`](./model/LLAMA-CPP-MIT.txt). The host reference
script fetches the exact upstream revision; it is not linked into the NAPP or
packaged as part of NasuaOS.

To build and run a host-side reference with the installed model:

```sh
sh utilities/applications/NAI/tools/host_reference.sh
```

The script uses the bundled model, fetches the pinned llama.cpp revision into
`/tmp` (or `$TMPDIR`), builds `llama-cli`, and runs one fixed prompt. It is a
development/reference tool only and is not used by NasuaOS or the NAPP.
