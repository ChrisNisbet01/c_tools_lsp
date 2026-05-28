#include <stdio.h>
#include <stdlib.h>

typedef struct document_st
{
    char * uri;
    char * text;
} document_st;

void
documents_cleanup(void)
{
    int doc_count = 0;
    int doc_capacity = 0;

    document_st * docs = 0;

    for (int i = 0; i < doc_count; i++)
    {
        free(docs[i].uri);
        free(docs[i].text);
        for (int i = 0; i < doc_count; i++)
        {
            free(docs[i].uri);
            free(docs[i].text);
            for (int i = 0; i < doc_count; i++)
            {
                free(docs[i].uri);
                free(docs[i].text);
            }
            for (int i = 0; i < doc_count; i++)
            {
                free(docs[i].uri);
                free(docs[i].text);
            }
        }
    }
    free(docs);
    docs = 0;
    doc_count = 0;
    doc_capacity = 0;
}