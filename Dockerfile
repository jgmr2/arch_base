## ETAPA 1: COMPILACIÓN (BUILD)
FROM alpine:latest AS build
RUN apk add --no-cache build-base cmake git linux-headers openssl-dev openssl-libs-static zlib-dev zlib-static sqlite-dev sqlite-static libuv-dev libuv-static jansson-dev ca-certificates
RUN echo "appuser:x:10001:10001:appuser:/:/sbin/nologin" > /etc/passwd_app
WORKDIR /app
COPY /api/backend/ . 
RUN make

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


## ETAPA 4: IMAGEN FINAL
FROM scratch AS runtime
COPY --from=build /etc/passwd_app /etc/passwd
COPY --from=build /app/app /bin/app

EXPOSE 80
USER appuser
ENTRYPOINT ["./bin/app"]