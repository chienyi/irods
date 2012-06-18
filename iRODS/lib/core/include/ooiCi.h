/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/
/* ooici.h - header file for general Obj Info
 */


#ifndef OOI_CI_H
#define OOI_CI_H

#include "rods.h"

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct DictValue {
    char type_PI[NAME_LEN];   /* the packing instruction of the ptr */
    void *ptr;
} dictValue_t;

#define DictValue_PI "piStr type_PI[NAME_LEN]; ?type_PI *ptr;"

typedef struct Dictionary {
    int len;
    char **key;     		/* array of keyword */
    dictValue_t *value;        /* pointer to an array of values */
} dictionary_t;

#define Dictionary_PI "int dictLen; str *key[dictLen]; struct *DictValue_PI[dictLen];" 

/* array of dictionary */
typedef struct DictArray {
    int len;
    dictionary_t **dictionary;
} dictArray_t;

#define DictArray_PI "int dictArrayLen; struct *Dictionary_PI[dictArrayLen];" 

int
dictSetAttr (dictionary_t *dictionary, char *key, char *type_PI, 
void *valptr);
dictValue_t *
dictGetAttr (dictionary_t *dictionary, char *key);
int
dictDelAttr (dictionary_t *dictionary, char *key);
int
clearDictionary (dictionary_t *dictionary);
#ifdef  __cplusplus
}
#endif

#endif	/* OOI_CI_H */

