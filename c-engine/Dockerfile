FROM gcc:13-bookworm AS build
WORKDIR /src
COPY . .
RUN make

FROM debian:bookworm-slim
WORKDIR /app
COPY --from=build /src/bin/os_engine /app/os_engine
EXPOSE 5051
ENTRYPOINT ["/app/os_engine"]
CMD ["5051", "8"]
