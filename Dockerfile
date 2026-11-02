# Build environment for the CakesCats PBA image.
#   docker build -t cakes-pba-build .
#   docker run --rm -v "$PWD:/src" -v cakes-pba-cache:/cache cakes-pba-build \
#       scripts/build.sh --profile default
FROM debian:trixie-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        bc bison build-essential bzip2 ca-certificates cpio file flex git \
        gzip libelf-dev libssl-dev patch perl python3 rsync sed unzip wget \
        xz-utils \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m builder && mkdir /cache && chown builder /cache
USER builder
ENV CAKES_WORK=/cache
WORKDIR /src
RUN git config --global --add safe.directory /src
