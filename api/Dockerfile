## ETAPA 1: COMPILACIÓN (BUILD)
FROM alpine:latest AS build

RUN apk add --no-cache \
    build-base cmake git linux-headers \
    openssl-dev openssl-libs-static \
    zlib-dev zlib-static \
    boost-dev boost-static \
    sqlite-dev sqlite-static \
    nlohmann-json \
    ca-certificates

RUN echo "appuser:x:10001:10001:appuser:/:/sbin/nologin" > /etc/passwd_app && \
    mkdir -p /app/run/nexus && \
    chown -R 10001:10001 /app/run/nexus

RUN git clone https://github.com/Thalhammer/jwt-cpp.git /tmp/jwt && \
    mkdir -p /usr/local/include && \
    cp -r /tmp/jwt/include/* /usr/local/include/ && \
    rm -rf /tmp/jwt

WORKDIR /app
COPY ./backend/src ./src
RUN cd src && make

## ETAPA 2: IMAGEN FINAL (SCRATCH)
FROM alpine AS runtime
COPY --from=build /etc/passwd_app /etc/passwd
COPY --from=build /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
COPY --from=build --chown=10001:10001 /app/run/nexus /var/run/nexus
COPY --from=build /app/build/app /bin/app

USER appuser
ENTRYPOINT ["/bin/app"]