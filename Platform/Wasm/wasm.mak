ifeq ($(shell uname),Darwin)
WASMCC=/usr/local/Cellar/llvm/9.0.1/bin/clang
else
WASMCC=clang
endif
WASMCFLAGS=--target=wasm32 --no-standard-libraries -Wl,--export-all -Wl,--no-entry -Wl,--allow-undefined -O3 -ffreestanding -nostdinc -isystem Platform/Wasm -DWASM
