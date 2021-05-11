# I think only clang supports wasm?
WASMCC=clang
WASMCFLAGS=--target=wasm32 --no-standard-libraries -Wl,--export-all -Wl,--no-entry -Wl,--allow-undefined -O3 -ffreestanding -nostdinc -isystem Platform/Wasm -DWASM
