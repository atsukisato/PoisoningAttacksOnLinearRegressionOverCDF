FROM ubuntu:22.04

# Install only essential packages
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    python3 \
    python3-pip \
    wget \
    zstd \
    git \
    nlohmann-json3-dev \
    libboost-all-dev \
    texlive-latex-base \
    texlive-latex-recommended \
    texlive-latex-extra \
    texlive-fonts-recommended \
    texlive-fonts-extra \
    texlive-plain-generic \
    texlive-science \
    texlive-publishers \
    latexmk \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace

# Install Python packages
COPY requirements.txt .
RUN pip3 install -r requirements.txt
