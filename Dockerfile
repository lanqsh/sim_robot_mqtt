FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libssl-dev \
    libpaho-mqtt-dev \
    libpaho-mqttpp-dev \
    libsqlite3-dev \
    libgoogle-glog-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

RUN mkdir -p build \
    && cd build \
    && cmake .. \
    && make -j$(nproc)

CMD ["/app/build/robot"]
