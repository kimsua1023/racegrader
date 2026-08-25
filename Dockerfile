FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive          \
  LANG=C.UTF-8                              \
  GOPATH=/go                                \
  GOCACHE=/go/cache                         \
  GOMODCACHE=/go/pkg/mod                    \
  PATH="/usr/local/go/bin:/go/bin:${PATH}"

RUN apt-get update && apt-get install -y --no-install-recommends \
  build-essential \
  ca-certificates \
  curl \
  git \
  qemu-system-misc \
  gcc-riscv64-unknown-elf \
  gdb-multiarch \
  perl \
  bc \
  python3 \
  && rm -rf /var/lib/apt/lists/*

ARG GO_VERSION=1.26.5
RUN set -eux; \
  arch="$(dpkg --print-architecture)"; \
  curl -fsSL "https://go.dev/dl/go${GO_VERSION}.linux-${arch}.tar.gz" \
    | tar -C /usr/local -xz

COPY cli /opt/racegrader/cli
WORKDIR /opt/racegrader/cli
RUN test -f go.mod
RUN go build -ldflags="-s -w" -o /usr/local/bin/racegrader .

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

WORKDIR /github/workspace
ENTRYPOINT ["/entrypoint.sh"]
