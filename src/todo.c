#include "todo.h"

todo_list *main_list;
todo_filter *main_filter;

void todo_list_new(const char *name)
{
    todo_list new_list = {0};
    strncpy(new_list.name, name, MAX_LIST_NAME_SIZE-1);
    new_list.todo_items = NULL;
    arrput(main_list, new_list);
}

void todo_list_add(todo_list *list, todo_item item)
{
    item.created = time(NULL);
    item.completed = false;
    item.tags = NULL;
    arrput(list->todo_items, item);
}

void todo_item_add_tag(todo_item *item, char *tag)
{
    todo_tag new_tag = {0};
    strncpy(new_tag.tag, tag, MAX_TAG_SIZE-1);
    arrput(item->tags, new_tag);
}

void todo_item_add_content(todo_item *item, char *content)
{
    strncpy(item->todo, content, MAX_TODO_SIZE-1);
}

void todo_item_add_note(todo_item *item, char *note)
{
    strncpy(item->note, note, MAX_TODO_SIZE-1);
}

void todo_list_search_content(todo_list *list, char *text)
{
    arrsetlen(main_filter->indices, 0);

    for(int i = 0; i<arrlen(list->todo_items); i++)
    {
        todo_item *item = &list->todo_items[i];

        if(strstr(item->todo, text) || strstr(item->note, text))
        {
            arrput(main_filter->indices, i);
        }
    }
}

void todo_list_search_tag(todo_list *list, char *tag)
{
    arrsetlen(main_filter->indices, 0);
    
    for(int i =0; i<arrlen(list->todo_items); i++)
    {
        todo_item *item = &list->todo_items[i];
        
        for(int j =0; j<arrlen(item->tags); j++)
        {
            if(strncmp(item->tags[j].tag, tag, MAX_TAG_SIZE) == 0)
            {
                arrput(main_filter->indices, i);
                break;
            }
        }
    }
}

void todo_list_get_incomplete(todo_list *list)
{
    arrsetlen(main_filter->indices, 0);

    for (int i = 0; i < arrlen(list->todo_items); i++)
    {
        todo_item *item = &list->todo_items[i];
        
        if (!item->completed)
        {
            arrput(main_filter->indices, i);
        }
    }
}

void todo_list_get_overdue(todo_list *list)
{
    arrsetlen(main_filter->indices, 0);

    time_t now = time(NULL);
    
    for(int i =0; i<arrlen(list->todo_items); i++)
    {
        todo_item *item = &list->todo_items[i];
        
        bool overdue = !item->completed && (item->deadline > 0) && (item->deadline < now);
        
        if(overdue)
        {
            arrput(main_filter->indices, i);
        }
    }
}

bool todo_list_remove_by_created(todo_list *list, time_t created)
{
    for (int i = 0; i < arrlen(list->todo_items); i++)
    {
        if (list->todo_items[i].created == created)
        {
            arrfree(list->todo_items[i].tags);
            
            arrdel(list->todo_items, i);
            
            return true;
        }
    }
    
    return false;
}


int compare_priority_asc(const void *a, const void *b) 
{
    const todo_item *itemA = (const todo_item*)a;
    const todo_item *itemB = (const todo_item*)b;
    return itemA->priority - itemB->priority;
}

int compare_priority_desc(const void *a, const void *b) 
{
    const todo_item *itemA = (const todo_item*)a;
    const todo_item *itemB = (const todo_item*)b;
    return itemB->priority - itemA->priority;
}

int compare_created_asc(const void *a, const void *b) 
{
    const todo_item *itemA = (const todo_item*)a;
    const todo_item *itemB = (const todo_item*)b;
    return (itemA->created > itemB->created) - (itemA->created < itemB->created);
}

int compare_created_desc(const void *a, const void *b) 
{
    const todo_item *itemA = (const todo_item*)a;
    const todo_item *itemB = (const todo_item*)b;
    return (itemB->created > itemA->created) - (itemB->created < itemA->created);
}

int compare_deadline_asc(const void *a, const void *b) 
{
    const todo_item *itemA = (const todo_item*)a;
    const todo_item *itemB = (const todo_item*)b;
    if (itemA->deadline == 0 && itemB->deadline == 0) return 0;
    if (itemA->deadline == 0) return 1;
    if (itemB->deadline == 0) return -1;
    return (itemA->deadline > itemB->deadline) - (itemA->deadline < itemB->deadline);
}

int compare_deadline_desc(const void *a, const void *b) 
{
    const todo_item *itemA = (const todo_item*)a;
    const todo_item *itemB = (const todo_item*)b;
    if (itemA->deadline == 0 && itemB->deadline == 0) return 0;
    if (itemA->deadline == 0) return 1;
    if (itemB->deadline == 0) return -1;
    return (itemB->deadline > itemA->deadline) - (itemB->deadline < itemA->deadline);
}

int compare_alphabetical(const void *a, const void *b) 
{
    const todo_item *itemA = (const todo_item*)a;
    const todo_item *itemB = (const todo_item*)b;
    return strcmp(itemA->todo, itemB->todo);
}

void todo_list_sort(todo_list *list, const char *sort_type, bool ascending) 
{
    if (arrlen(list->todo_items) <= 1) return;
    
    if (strcmp(sort_type, "priority") == 0) {
        qsort(list->todo_items, arrlen(list->todo_items), sizeof(todo_item), 
              ascending ? compare_priority_asc : compare_priority_desc);
    }
    else if (strcmp(sort_type, "created") == 0) {
        qsort(list->todo_items, arrlen(list->todo_items), sizeof(todo_item),
              ascending ? compare_created_asc : compare_created_desc);
    }
    else if (strcmp(sort_type, "deadline") == 0) {
        qsort(list->todo_items, arrlen(list->todo_items), sizeof(todo_item),
              ascending ? compare_deadline_asc : compare_deadline_desc);
    }
    else if (strcmp(sort_type, "alphabetical") == 0) {
        qsort(list->todo_items, arrlen(list->todo_items), sizeof(todo_item),
              compare_alphabetical);
        if (!ascending) {
            for (int i = 0; i < arrlen(list->todo_items) / 2; i++) {
                todo_item temp = list->todo_items[i];
                list->todo_items[i] = list->todo_items[arrlen(list->todo_items) - 1 - i];
                list->todo_items[arrlen(list->todo_items) - 1 - i] = temp;
            }
        }
    }
}
