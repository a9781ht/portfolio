# Stage 1: Build
FROM golang:1.21-alpine AS builder

WORKDIR /build
COPY go.mod .
RUN go mod download
COPY main.go .
RUN go build -o logger_subscriber .

# Stage 2: Runtime
FROM alpine:3.19

RUN mkdir -p /logs
WORKDIR /app
COPY --from=builder /build/logger_subscriber .

CMD ["./logger_subscriber"]
