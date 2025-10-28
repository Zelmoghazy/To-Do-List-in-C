#ifndef TODO_H_
#define TODO_H_

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "../external/include/stb_ds.h"
#include "util.h"

#define MAX_TODO_SIZE            1024
#define MAX_TAG_SIZE             64
#define MAX_LIST_NAME_SIZE       64
#define MAX_NOTE_SIZE            4096

typedef struct 
{
    char tag[MAX_TAG_SIZE];
}todo_tag;

typedef struct 
{    
    char todo[MAX_TODO_SIZE];
    char note[MAX_NOTE_SIZE];
    todo_tag *tags;
    i32 priority;
    bool completed;
    time_t created;
    time_t deadline;
}todo_item;

typedef struct
{
    char name[MAX_LIST_NAME_SIZE];
    todo_item *todo_items;
}todo_list;

typedef struct
{
    int *indices;
} todo_filter;

extern todo_list *main_list;
extern todo_filter *main_filter;

void todo_list_new(const char *name);
void todo_list_add(todo_list *list, todo_item *item);
void todo_item_add_tag(todo_item *item, char *tag);
void todo_item_add_content(todo_item *item, char *content);
void todo_item_add_note(todo_item *item, char *note);

void todo_list_search_content(todo_list *list, char *text);
void todo_list_search_tag(todo_list *list, char *tag);
void todo_list_get_incomplete(todo_list *list);
void todo_list_get_overdue(todo_list *list);
bool todo_list_remove_by_created(todo_list *list, time_t created);
int compare_priority_asc(const void *a, const void *b);
int compare_priority_desc(const void *a, const void *b);
int compare_created_asc(const void *a, const void *b);
int compare_created_desc(const void *a, const void *b);
int compare_deadline_asc(const void *a, const void *b);
int compare_deadline_desc(const void *a, const void *b);
int compare_alphabetical(const void *a, const void *b);
void todo_list_sort(todo_list *list, const char *sort_type, bool ascending);


#endif // TODO_H
