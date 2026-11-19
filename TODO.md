# TODO: real offline AI for NasuaOS (NAI / NasuaAI)

## Goal

Replace the current rule-based replies in `utilities/applications/NAI/src/NAI.cpp`
with local text generation from a real, pretrained instruction model. The first
target is an English-first assistant that may also answer simple Polish prompts,
runs offline in the NAPP environment, and is usable in QEMU configured with
2 GiB of RAM. Removing the NAI application must not prevent NasuaOS from
building or booting.

This is integration of an existing pretrained model, not training a foundation
model from scratch. Training a comparable model would require a separately
licensed dataset and substantial compute.

## Proposed model and licensing

- Model family: **SmolLM2-135M-Instruct**, an instruction-tuned 135-million
  parameter model from Hugging Face.
- Initial weights: **Q2_K GGUF**, from
  `bartowski/SmolLM2-135M-Instruct-GGUF`, file
  `SmolLM2-135M-Instruct-Q2_K.gguf`.
- The Hugging Face model card and the quantized repository identify the model
  as **Apache License 2.0**. The reported Q2_K file size is **88,202,080
  bytes** (about 84 MiB), below GitHub's 100 MB per-file limit, so Git LFS
  should not be necessary. Confirm the exact file size and license files again
  before adding the binary; do not silently substitute a different quantization
  or repository.
- Preserve the model's Apache-2.0 license, copyright and attribution notices,
  and the quantized distributor's notices in the repository and in the built
  OS image. Include any required third-party notices for the inference engine.
  Verify redistribution terms for the exact model and quantized artifact
  before publishing; this is an engineering checklist, not legal advice.
- SmolLM2 is described by its model card as primarily English. English is the
  primary quality target; Polish responses are best-effort, not a guarantee.
  Quantization can further reduce response quality.
- Model reference: <https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct>
- Quantized weights reference:
  <https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF>
- License reference:
  <https://www.apache.org/licenses/LICENSE-2.0>

## Current constraints found in NasuaOS

- The NAPP loader in `kernel_64bit/src/system/applications/napp/napp.cpp`
  rejects application images larger than 1 MiB. Keep `NAI.napp` small; never
  link model weights into the NAPP executable.
- The NAPP API already exposes `clawfs_get_file_size`,
  `clawfs_read_file_range`, `allocate`, `deallocate`, and `enable_fpu`.
  The range-read wrapper can read files from the FAT rootfs as well as ClawFS.
- ClawFS limits an individual file to 32 MiB, so the model cannot be stored
  there. Package it as a separate FAT rootfs file, for example
  `/share/models/nai/SmolLM2-135M-Instruct-Q2_K.gguf`.
- `utilities/rootfs/GNUmakefile` currently creates a 40 MiB rootfs image.
  Increase this only enough to hold the model and normal system files, then
  verify the FAT format, ISO packaging, disk layout, and boot path with the
  larger image. Keep the model an optional build input so a checkout without
  it can still build and boot the rest of NasuaOS.
- The app build automatically discovers application directories. Do not add a
  hard dependency on NAI in the kernel boot path or in other applications.
- Application compile flags disable SSE and x87 by default. Before enabling
  floating-point/SIMD inference, verify NAPP FPU setup, scheduler context
  preservation, and QEMU CPU feature availability. Use a scalar/portable path
  if SIMD is not safe in the current kernel.

## Implementation plan

### 1. Make model delivery explicit and optional

- Add a documented model fetch/install target that downloads only the exact
  pinned Q2_K artifact and its license/notice files. Do not download weights
  implicitly during an ordinary build.
- Verify the downloaded artifact's size and SHA-256 against a recorded,
  reviewed value; fail with a clear message on mismatch.
- Install the model under `/share/models/nai/` in `utilities/rootfs/dir/` and
  update rootfs sizing/formatting and ISO packaging as needed.
- If weights are absent, continue building and booting normally. NAI should
  show an explicit “model not installed” message and remain usable only for
  that status/help screen; it must not pretend to generate model answers.
- Add the applicable Apache-2.0 license and attribution notices beside the
  model in the source tree and include them in the image.

### 2. Select and port an inference engine

- Evaluate a pinned release/commit of **llama.cpp / ggml**, whose project
  license is Apache-2.0, for the LLaMA architecture and GGUF Q2_K support used
  by SmolLM2.
- Record the exact upstream revision and copy its license/notice files. Keep
  only required source files; do not copy an unpinned moving branch.
- First build a small host-side reference executable using the same model,
  prompt format, and generation settings. This confirms the weights and
  expected output before porting into the kernel's freestanding environment.
- Then build a minimal, single-threaded CPU inference path suitable for NAPP:
  no network access, no Python/runtime dependency, bounded context and output,
  and no background downloads. Reuse llama.cpp/ggml only if its allocator,
  libc, threading, compiler, and CPU requirements can be met cleanly.
- If a direct llama.cpp port requires unsafe ABI/FPU assumptions or excessive
  kernel changes, stop and document the blocker rather than shipping an
  unstable OS. Consider a separate, optional hosted helper only as a distinct
  future design; it is not equivalent to native offline NAPP inference.

### 3. Load the GGUF safely from NAPP

- Add a small NAI model-loader/inference interface. Read metadata and tensor
  data through `clawfs_get_file_size` and `clawfs_read_file_range`, with strict
  bounds checks and errors for missing, truncated, unsupported, or invalid
  files.
- Avoid assuming a large contiguous allocation will always succeed. Establish
  the kernel heap's allocation limits; prefer range-backed or explicitly
  bounded loading if the inference engine supports it.
- Keep allocations bounded and release them when the NAI window closes.
  Handle allocation failures without crashing the system.
- Use the model's GGUF tokenizer and chat template, including the correct
  end-of-turn token. Add a short system prompt identifying NasuaAI and
  describing NasuaOS only to the extent supported by supplied context.
- Preserve a small conversation history with a fixed token/context budget.
  Provide a way to clear the conversation and cap generated tokens so
  inference time and memory use are predictable.
- Ensure model inference runs only while NAI is active. Do not block unrelated
  UI, kernel scheduling, or other applications for an unbounded interval.

### 4. Integrate with the chat UI

- Replace `answer_question()` and hard-coded topic rules in
  `utilities/applications/NAI/src/NAI.cpp` with the inference interface.
- Show loading/generating status and report model and inference errors in the
  NAI window. Never return success-shaped canned answers when inference fails.
- Keep the chat window responsive, bound input/history/output sizes, and test
  keyboard and mouse submit behavior.
- Explain in the UI that responses are generated locally, may be inaccurate,
  and the initial model is English-first.

### 5. Verify behavior and system safety

- Unit-test GGUF metadata parsing, bounds/error handling, tokenizer special
  tokens, prompt construction, output termination, and missing-model behavior.
- Compare a fixed English prompt with the host reference engine. Add a few
  simple Polish prompts as best-effort checks; do not claim fluent Polish
  unless evaluation supports it.
- Build and boot the full OS in QEMU with 2 GiB RAM. Measure model load time,
  first-token latency, generation speed, and peak memory; document the actual
  results rather than promising smooth performance in advance.
- Check that the OS stays responsive during generation and that closing NAI
  releases resources.
- Build and boot again without the model, then with the NAI application folder
  omitted. In both cases the remaining system must still build and boot.
- Run existing targeted tests and the normal build checks. Do not increase
  rootfs size or add expensive model assets to mandatory builds unless the
  no-model workflow has been verified.

## Acceptance criteria

- NAI produces text through pretrained model inference, not keyword rules or
  hard-coded answer selection.
- Model files are separate from the `.napp`, tracked without Git LFS only if
  the exact artifact remains below GitHub's per-file limit, and accompanied by
  reviewed licensing/attribution notices.
- English is the primary target; Polish quality is described accurately.
- With 2 GiB configured in QEMU, NAI either runs within measured memory/time
  bounds or clearly reports a limitation without destabilizing NasuaOS.
- Missing model or missing NAI never prevents the rest of NasuaOS from
  building and booting.
