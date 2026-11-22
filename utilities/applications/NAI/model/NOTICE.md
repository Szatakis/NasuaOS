# SmolLM2 model attribution

This directory contains the pinned quantized model used by NAI.

The `SmolLM2-135M-Instruct-IQ4_XS.gguf` artifact is based on
`HuggingFaceTB/SmolLM2-135M-Instruct`, published by Hugging Face:
<https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct>

The quantized artifact is distributed by bartowski at
<https://huggingface.co/bartowski/SmolLM2-135M-Instruct-GGUF> and is identified
there as Apache-2.0. The quantizer's model card reports llama.cpp release
`b3991` as its quantization tool. The corresponding upstream revision is MIT
licensed; see `LLAMA-CPP-MIT.txt`. The reference engine is not bundled into
the NasuaOS image.

Pinned repository revision:
`09816acd5d99df7be770d85ea30822623dab342c`

Expected file size: `90897760` bytes  
Expected SHA-256: `d766661c2e39c3da78d8eddf15287af263dce1eedc1c9c8093ed8e54eb067dbc`

The model card describes SmolLM2 as primarily English and notes that outputs
may be inaccurate or inconsistent. This notice records provenance; consult the
linked model card and the accompanying Apache-2.0 license for the applicable
terms.
