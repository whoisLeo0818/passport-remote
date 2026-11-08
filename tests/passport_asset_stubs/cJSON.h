#pragma once
#include <stddef.h>
/* Photo transaction tests intentionally do not replace a JSON implementation.
 * Profile packing/limits are exercised by the JS tests. */
typedef struct {char *valuestring;} cJSON;
static inline cJSON *cJSON_Parse(const char *s){(void)s;return NULL;}
static inline void cJSON_Delete(cJSON *j){(void)j;}
static inline cJSON *cJSON_GetObjectItemCaseSensitive(cJSON *j,const char *k){(void)j;(void)k;return NULL;}
static inline int cJSON_IsString(cJSON *j){(void)j;return 0;}
static inline cJSON *cJSON_CreateObject(void){return NULL;}
static inline void cJSON_AddStringToObject(cJSON *j,const char *k,const char *v){(void)j;(void)k;(void)v;}
static inline void cJSON_AddBoolToObject(cJSON *j,const char *k,int v){(void)j;(void)k;(void)v;}
static inline void cJSON_AddNumberToObject(cJSON *j,const char *k,double v){(void)j;(void)k;(void)v;}
static inline void cJSON_AddItemToObject(cJSON *j,const char *k,cJSON *v){(void)j;(void)k;(void)v;}
static inline cJSON *cJSON_AddArrayToObject(cJSON *j,const char *k){(void)j;(void)k;return NULL;}
static inline void cJSON_AddItemToArray(cJSON *j,cJSON *v){(void)j;(void)v;}
static inline char *cJSON_PrintUnformatted(cJSON *j){(void)j;return NULL;}
