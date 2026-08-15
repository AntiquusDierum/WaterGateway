#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

int HttpServer_Init(void);

void HttpServer_Task(void);

void HttpServer_Close(void);

#endif /* HTTP_SERVER_H */
