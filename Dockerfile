# Ubuntu 24.04 をベースにする
FROM ubuntu:24.04

# コンパイルに必要なツール群をインストール
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gdb \
    git \
    libcurl4-openssl-dev \
    python3 \
    && rm -rf /var/lib/apt/lists/*

# コンテナ内の作業ディレクトリ
WORKDIR /app