# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 bonszaimusic
#
# The CI runner's tools in one linux/amd64 image: Python with Pillow / fontTools / PyYAML, clang and gcc, Node 22, the pinned Emscripten 3.1.64,
# JieLi's Linux toolchain, the three pinned AC79 SDK files (SHA-256 checked) and the pinned DaisySP reference. Built once by tools/docker_validate.sh.
FROM --platform=linux/amd64 ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq && apt-get install -y -qq python3 python3-pip curl xz-utils git clang build-essential ca-certificates rsync \
        libraqm0 libfribidi0 libharfbuzz0b nodejs npm >/dev/null \
    && pip3 install -q Pillow fonttools pyyaml \
    && rm -rf /var/lib/apt/lists/*
# Node 22 (the workflows use it): the official tarball
RUN curl -fsSL https://nodejs.org/dist/v22.11.0/node-v22.11.0-linux-x64.tar.xz | tar -xJ -C /usr/local --strip-components=1 \
    && node --version
# Emscripten 3.1.64, as the workflows install it
RUN git clone --depth 1 https://github.com/emscripten-core/emsdk.git /opt/emsdk && /opt/emsdk/emsdk install 3.1.64 >/dev/null && /opt/emsdk/emsdk activate 3.1.64 >/dev/null
# DaisySP at the pinned commit (the PHYS reference)
RUN git init /opt/DaisySP >/dev/null && git -C /opt/DaisySP remote add origin https://github.com/daisyaudio/DaisySP.git \
    && git -C /opt/DaisySP fetch --depth 1 origin 2c72eaf9eac5fc0dca1919d65d606da907832618 >/dev/null 2>&1 && git -C /opt/DaisySP checkout --detach FETCH_HEAD >/dev/null 2>&1
# JieLi toolchain and the three SDK files
RUN mkdir -p /opt/jieli && curl -fL --retry 3 https://pkgman.jieliapp.com/s/linux-toolchain | tar -xJ -C /opt/jieli \
    && TC="$(ls -d /opt/jieli/jieli-linux-toolchains-* | sort | tail -1)" && ln -sfn "$TC" /opt/jieli/toolchain && echo "toolchain $TC"
RUN set -eu; root=/opt/ac79-sdk/cpu/wl82/tools; mkdir -p $root/cfg; \
    base="https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/raw/AC79NN_SDK_V1.2.1_2023-12-13/cpu/wl82/tools"; \
    for f in uboot.boot cfg_tool.bin cfg/eq_cfg_hw.bin; do curl -fLsS --connect-timeout 8 --max-time 60 --retry 2 -o $root/$f $base/$f; done; \
    cd $root && echo "4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4  uboot.boot" > /tmp/sums \
    && echo "276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b  cfg_tool.bin" >> /tmp/sums \
    && echo "41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d  cfg/eq_cfg_hw.bin" >> /tmp/sums && sha256sum -c /tmp/sums
# the workflows call `python` (setup-python provides it on the runner)
RUN ln -s /usr/bin/python3 /usr/local/bin/python
ENV JIELI_TOOLCHAIN=/opt/jieli/toolchain AC79_SDK=/opt/ac79-sdk DAISYSP=/opt/DaisySP CC="cc -include stdint.h"
WORKDIR /work
