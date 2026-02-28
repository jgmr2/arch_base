FROM alpine:latest AS build
RUN apk add --no-cache \
    build-base \
    cmake \
    git \
    linux-headers \
    openssl \
    openssl-dev \
    openssl-libs-static \
    zlib-dev \
    zlib-static \
    asio-dev \
    sqlite-dev \
    sqlite-static \
    nlohmann-json
WORKDIR /app/certs

RUN echo "appuser:x:10001:10001:appuser:/:/sbin/nologin" > /etc/passwd_app

RUN git clone --branch v1.2.0 --depth 1 https://github.com/CrowCpp/Crow.git /tmp/crow && \
    mkdir -p /usr/local/include && \
    cp -r /tmp/crow/include/* /usr/local/include/ && \
    rm -rf /tmp/crow

RUN git clone https://github.com/Thalhammer/jwt-cpp.git /tmp/jwt && \
    mkdir -p /usr/local/include && \
    cp -r /tmp/jwt/include/* /usr/local/include/ && \
    rm -rf /tmp/jwt

WORKDIR /app
COPY ./backend/src ./src
RUN cd src && make

## ETAPA 2: IMAGEN DE PRODUCCIÓN FINAL (RUNTIME ULTRA-LIGERO)
FROM alpine AS runtime
COPY --from=build /etc/passwd_app /etc/passwd
COPY --from=build /app/build/app /bin/app


USER appuser
EXPOSE 8080
WORKDIR /bin
ENTRYPOINT ["/bin/app"]