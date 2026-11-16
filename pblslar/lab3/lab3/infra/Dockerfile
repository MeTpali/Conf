FROM node:20-alpine
WORKDIR /app
COPY lab2/server /app/lab2/server
COPY lab3/backend /app/lab3/backend
ENV HTTPS=0 HOST=0.0.0.0 PORT=8082
EXPOSE 8082
CMD ["node", "/app/lab3/backend/server.js"]
