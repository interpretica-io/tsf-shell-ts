# libghostty-vt: the terminal-state library of Ghostty, the preferred
# terminal-emulator backend of ta_shell. It is built with Zig from a
# pinned Ghostty commit. The stage runs on the build host's own
# architecture and cross-compiles for the image's, so an image built
# under emulation does not run the compiler emulated.
FROM --platform=$BUILDPLATFORM debian:bookworm AS ghostty-vt

ARG BUILDARCH
ARG TARGETARCH
ARG ZIG_VERSION=0.17.1
ARG GHOSTTY_REF=7fa26e8b675f3248975479d88274c937d7e63905

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        ca-certificates curl git xz-utils

# The Zig release is checked against its published SHA-256
# (https://ziglang.org/download/index.json); change both together.
ARG ZIG_SHA256_AARCH64=a1546d5646eb184437918d6635ca6b762a0e889eac09cef720ee7a1ed3fe7795
ARG ZIG_SHA256_X86_64=3581863e3cbcfd0fce0efc682ce7e9e93da96668a977791d356a26d2a7e9b17a

RUN case "${BUILDARCH}" in \
        arm64) zig_arch=aarch64 ; zig_sha="${ZIG_SHA256_AARCH64}" ;; \
        amd64) zig_arch=x86_64 ; zig_sha="${ZIG_SHA256_X86_64}" ;; \
        *) echo "unsupported build architecture ${BUILDARCH}" >&2 ; exit 1 ;; \
    esac && \
    curl -fsSL -o /tmp/zig.tar.xz \
        "https://ziglang.org/download/${ZIG_VERSION}/zig-${zig_arch}-linux-${ZIG_VERSION}.tar.xz" && \
    echo "${zig_sha}  /tmp/zig.tar.xz" | sha256sum -c - && \
    mkdir -p /opt/zig && \
    tar -xJf /tmp/zig.tar.xz -C /opt/zig --strip-components=1 && \
    rm /tmp/zig.tar.xz

RUN git init -q /src/ghostty && cd /src/ghostty && \
    git remote add origin https://github.com/ghostty-org/ghostty && \
    git fetch -q --depth 1 origin "${GHOSTTY_REF}" && \
    git checkout -q FETCH_HEAD && \
    test "$(git rev-parse HEAD)" = "${GHOSTTY_REF}"

# ReleaseSafe keeps the library's own assertions and integrity checks:
# a violated invariant aborts the agent's RPC server, and the test that
# drove it fails, instead of rendering on from a corrupt state.
RUN case "${TARGETARCH}" in \
        arm64) zig_target=aarch64-linux-gnu ;; \
        amd64) zig_target=x86_64-linux-gnu ;; \
        *) echo "unsupported target architecture ${TARGETARCH}" >&2 ; exit 1 ;; \
    esac && \
    cd /src/ghostty && \
    /opt/zig/zig build -Demit-lib-vt -Dtarget="${zig_target}" \
        -Doptimize=ReleaseSafe --prefix /usr/local

# Build environment
FROM debian:bookworm

ARG user
ARG uid
ARG gid
ARG docker_gid
ENV USER=${user}
ENV UID=${uid}
ENV GID=${gid}
ENV DOCKER_GID=${docker_gid}

RUN if [ "$DOCKER_GID" != "${GID}" ] ; then groupadd --gid ${DOCKER_GID} docker ; fi

RUN if [ ! -d /home/${USER} ] ; then mkdir -p /home/${USER} && chmod -R 777 /home/${USER} ; fi

# Install basic dependencies & useful tools. libvterm-dev gives the
# fallback terminal-emulator backend of ta_shell (vterm.h, -lvterm);
# the preferred one, libghostty-vt, is copied in below from its own
# build stage. coreutils/util-linux provide the stty the resize test
# drives on the agent's pty.
RUN apt-get update && \
    apt-get install -y \
        autoconf \
        automake \
        bison \
        build-essential \
        ccache \
        cmake \
        curl \
        docker.io \
        file \
        flex \
        gawk \
        libbsd-dev \
        libvterm-dev \
        coreutils \
        util-linux \
        libjansson-dev \
        libexpat1 \
        libexpat1-dev \
        libcurl4-openssl-dev \
        libglib2.0-dev \
        libnetconf2-dev \
        libncurses5 \
        libncurses5-dev \
        libpam0g \
        libpam0g-dev \
        libpcap0.8-dev \
        libpopt-dev \
        libreadline-dev \
        libssh-dev \
        libssh2-1-dev \
        libssl-dev \
        libusb-1.0-0 \
        libusb-1.0-0-dev \
        libxml2 \
        libxml2-dev \
        libxslt1-dev \
        libxslt1.1 \
        libpcre3-dev \
        libyaml-dev \
        m4 \
        meson \
        openssh-server \
        pkg-config \
        perl \
        python3 \
        rsync \
        ssh \
        sudo \
        tshark \
        tcl-dev \
        unzip \
        wget

RUN echo "${USER} ALL=NOPASSWD: ALL" >> /etc/sudoers

RUN if [ "$DOCKER_GID" != "${GID}" ] ; then groupadd --gid ${GID} ${USER} ; fi

RUN useradd --system \
            --gid ${GID} --gid ${DOCKER_GID} \
            --uid ${UID} \
            --shell /bin/bash \
            ${USER}
RUN usermod -a -G ${DOCKER_GID} ${USER}
RUN usermod -a -G docker ${USER}

# libghostty-vt: the static library only, so the agent carries it and
# nothing has to be installed where the agent runs.
COPY --from=ghostty-vt /usr/local/include/ghostty /usr/local/include/ghostty
COPY --from=ghostty-vt /usr/local/lib/libghostty-vt.a /usr/local/lib/
COPY --from=ghostty-vt /usr/local/lib/pkgconfig/libghostty-vt-static.pc \
     /usr/local/lib/pkgconfig/
