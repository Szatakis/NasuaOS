#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../../../.." && pwd)
model_path=${1:-"$repo_root/utilities/applications/NAI/model/SmolLM2-135M-Instruct-IQ4_XS.gguf"}
build_root=${2:-"${TMPDIR:-/tmp}/nasuaos-nai-host-reference"}
source_dir="${build_root}/llama.cpp"
build_dir="${source_dir}/build"
revision=fc83a9e58479e4dd70054daa7afe5184c1bbe545
tag=b3991
expected_size=90897760
expected_sha256=d766661c2e39c3da78d8eddf15287af263dce1eedc1c9c8093ed8e54eb067dbc

if [ ! -f "$model_path" ]; then
    printf 'Bundled model not found: %s\n' "$model_path" >&2
    exit 1
fi

actual_size=$(wc -c < "$model_path" | tr -d '[:space:]')
actual_sha256=$(sha256sum "$model_path" | cut -d ' ' -f 1)
if [ "$actual_size" != "$expected_size" ] ||
   [ "$actual_sha256" != "$expected_sha256" ]; then
    printf 'Model verification failed: size=%s sha256=%s\n' \
        "$actual_size" "$actual_sha256" >&2
    exit 1
fi

mkdir -p "$build_root"
if [ ! -d "$source_dir/.git" ]; then
    git clone --depth 1 --branch "$tag" --recurse-submodules \
        https://github.com/ggml-org/llama.cpp.git "$source_dir"
fi

actual_revision=$(git -C "$source_dir" rev-parse HEAD)
if [ "$actual_revision" != "$revision" ]; then
    printf 'Unexpected llama.cpp revision: expected %s, got %s\n' \
        "$revision" "$actual_revision" >&2
    exit 1
fi
git -C "$source_dir" submodule update --init --recursive

cmake -S "$source_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DLLAMA_BUILD_TESTS=OFF \
    -DLLAMA_BUILD_SERVER=OFF \
    -DLLAMA_CURL=OFF
cmake --build "$build_dir" --target llama-cli -j2

prompt=$(printf '%s\n' \
    '<|im_start|>system' \
    'You are NasuaAI, an offline assistant. Be concise, and say when you are unsure.' \
    '<|im_end|>' \
    '<|im_start|>user' \
    'What is an operating system kernel?' \
    '<|im_end|>' \
    '<|im_start|>assistant')
"$build_dir/bin/llama-cli" \
    --model "$model_path" \
    --prompt "$prompt" \
    --n-predict 48 \
    --temp 0.2 \
    --top-p 0.9 \
    --repeat-penalty 1.1 \
    --seed 123 \
    --no-display-prompt
