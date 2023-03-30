# I think only clang supports wasm?
WCC?=clang
WASMCFLAGS=--target=wasm32 --no-standard-libraries -Wl,--export-all -Wl,--no-entry -Wl,--allow-undefined -O3 -ffreestanding -nostdinc -isystem Platform/Wasm -mbulk-memory
# -msimd128
# Safari doesn't yet support this (though they do in 16.4). Wait a bit
# before making it the default.
