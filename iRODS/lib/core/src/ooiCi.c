/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/

/* ooiCi.c - misc client routines
 */
#include "ooiCi.h"

int
dictSetAttr (dictionary_t *dictionary, char *key, char *type_PI, void *valptr)
{
    /* key and type_PI are replicated, but valptr is stolen */
    char **newKey;
    dictValue_t *newValue;
    int newLen;
    int i;
    int emptyInx = -1;

    if (dictionary == NULL) {
        return (SYS_INTERNAL_NULL_INPUT_ERR);
    }

    /* check if the keyword exists */

    for (i = 0; i < dictionary->len; i++) {
        if (strcmp (key, dictionary->key[i]) == 0) {
            free ( dictionary->value[i].ptr);
            dictionary->value[i].ptr = valptr;
            rstrcpy (dictionary->value[i].type_PI, type_PI, NAME_LEN);
            return (0);
        } else if (strlen (dictionary->key[i]) == 0) {
            emptyInx = i;
        }
    }

    if (emptyInx >= 0) {
        free (dictionary->key[emptyInx]);
        free (dictionary->value[emptyInx].ptr);
        dictionary->key[emptyInx] = strdup (key);
        dictionary->value[emptyInx].ptr = valptr;
        rstrcpy (dictionary->value[emptyInx].type_PI, type_PI, NAME_LEN);
        return (0);
    }

    if ((dictionary->len % PTR_ARRAY_MALLOC_LEN) == 0) {
        newLen = dictionary->len + PTR_ARRAY_MALLOC_LEN;
        newKey = (char **) malloc (newLen * sizeof (newKey));
        newValue = (dictValue_t *) malloc (newLen * sizeof (dictValue_t));
        memset (newKey, 0, newLen * sizeof (newKey));
        memset (newValue, 0, newLen * sizeof (dictValue_t));
        for (i = 0; i < dictionary->len; i++) {
            newKey[i] = dictionary->key[i];
            newValue[i] = dictionary->value[i];
        }
        if (dictionary->key != NULL)
            free (dictionary->key);
        if (dictionary->value != NULL)
            free (dictionary->value);
        dictionary->key = newKey;
        dictionary->value = newValue;
    }

    dictionary->key[dictionary->len] = strdup (key);
    dictionary->value[dictionary->len].ptr = valptr;
    rstrcpy (dictionary->value[dictionary->len].type_PI, type_PI, NAME_LEN);
    dictionary->len++;

    return (0);
}

dictValue_t *
dictGetAttr (dictionary_t *dictionary, char *key)
{
    int i;

    if (dictionary == NULL) {
        return (NULL);
    }

    for (i = 0; i < dictionary->len; i++) {
        if (strcmp (dictionary->key[i], key) == 0) {
            return (&dictionary->value[i]);
        }
    }
    return (NULL);
}

int
dictDelAttr (dictionary_t *dictionary, char *key) 
{
    int i, j;

    if (dictionary == NULL) {
        return (0);
    }

    for (i = 0; i < dictionary->len; i++) {
        if (dictionary->key[i] != NULL &&
          strcmp (dictionary->key[i], key) == 0) {
            free (dictionary->key[i]);
	    if (dictionary->value[i].ptr != NULL) {
                free (dictionary->value[i].ptr);
		dictionary->value[i].ptr = NULL;
            }
            dictionary->len--;
            for (j = i; j < dictionary->len; j++) {
                dictionary->key[j] = dictionary->key[j + 1];
                dictionary->value[j] = dictionary->value[j + 1];
            }
            if (dictionary->len <= 0) {
                free (dictionary->key);
                free (dictionary->value);
                dictionary->value = NULL;
                dictionary->key = NULL;
            }
            break;
        }
    }
    return (0);
}

int
clearDictionary (dictionary_t *dictionary)
{
    int i;

    if (dictionary == NULL || dictionary->len <= 0)
        return (0);

    for (i = 0; i < dictionary->len; i++) {
        free (dictionary->key[i]);
        free (dictionary->value[i].ptr);
    }

    free (dictionary->key);
    free (dictionary->value);
    bzero (dictionary, sizeof (keyValPair_t));
    return(0);
}

