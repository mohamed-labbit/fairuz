FROM ubuntu:24.04 AS builder

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        clang \
        cmake \
        g++ \
        ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang \
        -DCMAKE_CXX_COMPILER=clang++ \
        -DBUILD_TESTS=OFF \
    && cmake --build build --target fairuz \
    && cmake --install build --prefix /opt/fairuz

FROM ubuntu:24.04 AS runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --system fairuz \
    && useradd --system --gid fairuz --create-home fairuz \
    && mkdir /work \
    && chown fairuz:fairuz /work

COPY --from=builder /opt/fairuz /opt/fairuz

LABEL org.opencontainers.image.source="https://github.com/mohamed-labbit/fairuz"

ENV PATH="/opt/fairuz/bin:${PATH}"
WORKDIR /work
USER fairuz

ENTRYPOINT ["fairuz"]
CMD ["--help"]
