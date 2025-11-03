/*
 * @Author: HoGC
 * @Date: 2022-03-20 18:41:37
 * @Last Modified time: 2022-03-20 18:41:37
 */

#include <stdlib.h>
#include <string.h>

#include "cc_list.h"
#include "cc_hal_sys.h"

/* Creates a list (node) and returns it
 * Arguments: The data the list will contain or NULL to create an empty
 * list/node
 */
cc_list_node* cc_list_create(void *data)
{
	cc_list_node *l = cc_hal_sys_malloc(sizeof(cc_list_node));
	if (l != NULL) {
		l->next = NULL;
		l->data = data;
	}

	return l;
}

/* Completely destroys a list
 * Arguments: A pointer to a pointer to a list
 */
void cc_list_destroy(cc_list_node **list)
{
	if (list == NULL) return;
	while (*list != NULL) {
		cc_list_remove(list, *list);
	}
}

/* Creates a list node and inserts it after the specified node
 * Arguments: A node to insert after and the data the new node will contain
 */
cc_list_node* cc_list_insert_after(cc_list_node *node, void *data)
{
	cc_list_node *new_node = cc_list_create(data);
	if (new_node) {
		new_node->next = node->next;
		node->next = new_node;
	}
	return new_node;
}

/* Creates a new list node and inserts it in the beginning of the list
 * Arguments: The list the node will be inserted to and the data the node will
 * contain
 */
cc_list_node* cc_list_insert_beginning(cc_list_node *list, void *data)
{
	cc_list_node *new_node = cc_list_create(data);
	if (new_node != NULL) { new_node->next = list; }
	return new_node;
}

/* Creates a new list node and inserts it at the end of the list
 * Arguments: The list the node will be inserted to and the data the node will
 * contain
 */
cc_list_node* cc_list_insert_end(cc_list_node *list, void *data)
{
	cc_list_node *new_node = cc_list_create(data);
	if (new_node != NULL) {
		for(cc_list_node *it = list; it != NULL; it = it->next) {
			if (it->next == NULL) {
				it->next = new_node;
				break;
			}
		}
	}
	return new_node;
}

/* Removes a node from the list
 * Arguments: The list and the node that will be removed
 */
void cc_list_remove(cc_list_node **list, cc_list_node *node)
{
	cc_list_node *tmp = NULL;
	if (list == NULL || *list == NULL || node == NULL) return;

	if (*list == node) {
		*list = (*list)->next;
		cc_hal_sys_free(node);
		node = NULL;
	} else {
		tmp = *list;
		while (tmp->next && tmp->next != node) tmp = tmp->next;
		if (tmp->next) {
			tmp->next = node->next;
			cc_hal_sys_free(node);
			node = NULL;
		}
	}
}

/* Removes an element from a list by comparing the data pointers
 * Arguments: A pointer to a pointer to a list and the pointer to the data
 */
void cc_list_remove_by_data(cc_list_node **list, void *data)
{
	if (list == NULL || *list == NULL || data == NULL) return;
	cc_list_remove(list, cc_list_find_by_data(*list, data));
}

/* Find an element in a list by the pointer to the element
 * Arguments: A pointer to a list and a pointer to the node/element
 */
cc_list_node* cc_list_find_node(cc_list_node *list, cc_list_node *node)
{
	while (list) {
		if (list == node) break;
		list = list->next;
	}
	return list;
}

/* Finds an elemt in a list by the data pointer
 * Arguments: A pointer to a list and a pointer to the data
 */
cc_list_node* cc_list_find_by_data(cc_list_node *list, void *data)
{
	while (list) {
		if (list->data == data) break;
		list = list->next;
	}
	return list;
}

/* Finds an element in the list by using the comparison function
 * Arguments: A pointer to a list, the comparison function and a pointer to the
 * data
 */
cc_list_node* cc_list_find(cc_list_node *list, int(*func)(cc_list_node*,void*), void *data)
{
	if (!func) return NULL;
	while(list) {
		if (func(list, data)) break;
		list = list->next;
	}
	return list;
}