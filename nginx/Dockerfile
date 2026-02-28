# --- ETAPA 1: Compilación Nginx Mínimo con Módulos Custom ---
FROM alpine:3.19 AS nginx-builder
RUN apk add --no-cache build-base perl linux-headers pcre-dev zlib-dev zlib-static openssl-dev openssl-libs-static binutils curl tar
WORKDIR /src
RUN curl -fSL https://nginx.org/download/nginx-1.26.0.tar.gz -o nginx.tar.gz \
    && tar -zxvf nginx.tar.gz \
    && rm nginx.tar.gz
COPY custom/ nginx-1.26.0/src/http/modules/custom
WORKDIR /src/nginx-1.26.0
RUN ./configure \
    #--add-module=src/http/modules/custom \
    --prefix=/etc/nginx \
    --sbin-path=/usr/sbin/nginx \
    --conf-path=/etc/nginx/nginx.conf \
    --pid-path=/tmp/nginx.pid \
    --with-http_ssl_module \
    --with-threads \
    --without-http_ssi_module \
    --without-http_userid_module \
    --without-http_mirror_module \
    --without-http_autoindex_module \
    --without-http_geo_module \
    --without-http_map_module \
    --without-http_split_clients_module \
    --without-http_fastcgi_module \
    --without-http_uwsgi_module \
    --without-http_scgi_module \
    --without-http_grpc_module \
    --without-http_memcached_module \
    --without-http_browser_module \
    --with-cc-opt="-Os -fno-stack-protector" \
    --with-ld-opt="-static -s" && \
    make -j$(nproc) && make install
RUN mkdir -p /rootfs/tmp /rootfs/etc/nginx/logs /rootfs/var/log/nginx /rootfs/etc
RUN ln -sf /dev/stdout /rootfs/etc/nginx/logs/access.log && \
    ln -sf /dev/stderr /rootfs/etc/nginx/logs/error.log
RUN echo "root:x:0:0:root:/root:/bin/sh" > /rootfs/etc/passwd && \
    echo "nobody:x:65534:65534:nobody:/:/sbin/nologin" >> /rootfs/etc/passwd && \
    echo "root:x:0:" > /rootfs/etc/group && \
    echo "nobody:x:65534:" >> /rootfs/etc/group
FROM emscripten/emsdk:latest AS wasm-build
WORKDIR /app
COPY wasm/ ./wasm/
RUN cd wasm/src && make

### ETAPA 3: Construcción Frontend (Svelte + Bun)
FROM oven/bun:1.2-slim AS svelte-build
WORKDIR /app
COPY frontend/package.json frontend/bun.lockb* ./
RUN bun install --frozen-lockfile
COPY frontend/ .
COPY --from=wasm-build /app/wasm/dist/*.wasm ./static/
COPY --from=wasm-build /app/wasm/dist/*.js ./static/
RUN bun run build

### ETAPA 4: IMAGEN FINAL
FROM scratch
COPY --from=nginx-builder /usr/sbin/nginx /usr/sbin/nginx
COPY --from=nginx-builder /etc/nginx/mime.types /etc/nginx/mime.types
COPY --from=nginx-builder /rootfs/ / 
COPY conf/nginx.conf /etc/nginx/nginx.conf
COPY certs/server.crt /etc/nginx/certs/server.crt
COPY certs/server.key /etc/nginx/certs/server.key
COPY --from=svelte-build /app/build /usr/share/nginx/html

EXPOSE 443
ENTRYPOINT ["/usr/sbin/nginx", "-g", "daemon off;"]