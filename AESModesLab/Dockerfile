FROM debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY CMakeLists.txt ./
COPY cmake/ ./cmake/
COPY include/ ./include/
COPY src/ ./src/
COPY tests/ ./tests/

CMD ["sh", "-c", "cmake -S . -B mmh2026-build-gcc -G 'Unix Makefiles' -DCMAKE_CXX_COMPILER=/usr/bin/g++ -DCMAKE_BUILD_TYPE=Release -DAES_BUILD_EXECUTABLE=ON -DAES_BUILD_TESTS=ON -DAES_LIBRARY_TYPE=BOTH && cmake --build mmh2026-build-gcc --parallel && ctest --test-dir mmh2026-build-gcc --verbose"]
