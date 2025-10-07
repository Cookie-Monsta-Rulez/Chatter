
#ifndef GLAD_GLAD_H_
#define GLAD_GLAD_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*GLADloadproc)(const char *name);
int gladLoadGLLoader(GLADloadproc);

#ifdef __cplusplus
}
#endif

#endif
