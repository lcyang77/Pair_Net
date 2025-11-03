/*
 * @Author: HoGC
 * @Date: 2022-03-20 18:41:37
 * @Last Modified time: 2022-03-20 18:41:37
 */

#ifndef __CC_LIST_H__
#define __CC_LIST_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cc_list_node {
	void *data;
	struct cc_list_node *next;
} cc_list_node;

/* linked list */
cc_list_node* cc_list_create(void *data);
void cc_list_destroy(cc_list_node **list);
cc_list_node* cc_list_insert_after(cc_list_node *node, void *data);
cc_list_node* cc_list_insert_beginning(cc_list_node *list, void *data);
cc_list_node* cc_list_insert_end(cc_list_node *list, void *data);
void cc_list_remove(cc_list_node **list, cc_list_node *node);
void cc_list_remove_by_data(cc_list_node **list, void *data);
cc_list_node* cc_list_find_node(cc_list_node *list, cc_list_node *node);
cc_list_node* cc_list_find_by_data(cc_list_node *list, void *data);
cc_list_node* cc_list_find(cc_list_node *list, int(*func)(cc_list_node*,void*), void *data);

#ifdef __cplusplus
}
#endif

#endif