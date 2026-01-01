FROM devkitpro/devkita64:latest

SHELL ["/bin/bash", "-c"]

RUN apt-get update && apt-get install -y \
    autoconf automake libtool pkg-config \
    python3 python3-pip python3-mako ninja-build \
    bison flex \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install --break-system-packages meson>=1.3.0

RUN dkp-pacman -S --noconfirm --needed \
    libnx \
    switch-cmake \
    switch-pkg-config \
    switch-sdl2 \
    switch-mbedtls \
    switch-zlib \
    switch-bzip2 \
    switch-freetype \
    switch-libass \
    switch-libfribidi \
    switch-harfbuzz \
    switch-mesa \
    switch-glfw \
    switch-glad \
    switch-dav1d \
    switch-libopus \
    dkp-toolchain-vars \
    dkp-meson-scripts \
    deko3d \
    && dkp-pacman -Scc --noconfirm

COPY saffron/library/ffmpeg /tmp/ffmpeg
WORKDIR /tmp/ffmpeg
RUN source ${DEVKITPRO}/switchvars.sh && \
    ./configure --prefix=${PORTLIBS_PREFIX} --enable-gpl --disable-shared --enable-static \
        --cross-prefix=aarch64-none-elf- --enable-cross-compile \
        --arch=aarch64 --cpu=cortex-a57 --target-os=horizon --enable-pic \
        --extra-cflags='-D__SWITCH__ -D_GNU_SOURCE -O2 -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec' \
        --extra-cxxflags='-D__SWITCH__ -D_GNU_SOURCE -O2 -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIC -ftls-model=local-exec' \
        --extra-ldflags='-fPIE -L${PORTLIBS_PREFIX}/lib -L${DEVKITPRO}/libnx/lib' \
        --disable-runtime-cpudetect --disable-programs --disable-debug --disable-doc --disable-autodetect \
        --enable-asm --enable-neon \
        --disable-avdevice --disable-encoders --disable-muxers \
        --enable-swscale --enable-swresample --enable-network \
        --disable-protocols --enable-protocol=file,http,https,tcp,tls,httpproxy \
        --enable-zlib --enable-bzlib --enable-libass --enable-libfreetype --enable-libfribidi --enable-libdav1d \
        --enable-nvtegra && \
    make -j$(nproc) && \
    make DESTDIR="" install

COPY saffron/library/curl /tmp/curl
WORKDIR /tmp/curl
RUN source ${DEVKITPRO}/switchvars.sh && \
    ./buildconf && \
    LDFLAGS="-specs=${DEVKITPRO}/libnx/switch.specs ${LDFLAGS}" \
    ./configure \
        --prefix=${PORTLIBS_PREFIX} \
        --host=aarch64-none-elf \
        --disable-shared \
        --enable-static \
        --disable-ipv6 \
        --disable-unix-sockets \
        --disable-socketpair \
        --disable-manual \
        --disable-ntlm-wb \
        --disable-threaded-resolver \
        --disable-ldap \
        --disable-ldaps \
        --without-libpsl \
        --without-libssh2 \
        --without-libssh \
        --without-nghttp2 \
        --without-libidn2 \
        --enable-websockets \
        --with-libnx \
        --with-default-ssl-backend=libnx && \
    sed -i 's/#define HAVE_SOCKETPAIR 1//' lib/curl_config.h && \
    make -C lib -j$(nproc) && \
    make DESTDIR="" -C lib install && \
    make DESTDIR="" -C include install && \
    make DESTDIR="" install-binSCRIPTS install-pkgconfigDATA

COPY saffron/library/libuam /tmp/libuam
WORKDIR /tmp/libuam
RUN /opt/devkitpro/meson-cross.sh switch crossfile.txt build . && \
    meson compile -C build && \
    DESTDIR="" meson install -C build

COPY saffron/library/mpv /tmp/mpv
WORKDIR /tmp/mpv
RUN /opt/devkitpro/meson-cross.sh switch crossfile.txt build . \
    -Dlibmpv=true -Dcplayer=false -Dtests=false -Dlua=disabled -Ddeko3d=enabled \
    -Dgl=disabled -Dlibarchive=disabled && \
    meson compile -C build && \
    DESTDIR="" meson install -C build

RUN rm -rf /tmp/ffmpeg /tmp/curl /tmp/libuam /tmp/mpv

WORKDIR /build
