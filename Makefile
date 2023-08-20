all:
	emcc --bind --std=c++11 -Wall -Werror -pedantic -O3 -x c++ subnet.cpp -o subnet.js -s WASM=1 --post-js=subnet_post.js

ts-types:
	emcc -lembind --std=c++11 -x c++ subnet.cpp --embind-emit-tsd interface.d.ts

# -s "EXPORTED_RUNTIME_METHODS=['ccall']"