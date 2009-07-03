/** @file
 * @todo add description here
 *
 * @author Andrey Litvinov
 * @version 1.0
 */

#include "doctree.h"

#include <string.h>

typedef struct _TreeNode
{
    char* name;
    struct _TreeNode* neighbor;
    struct _TreeNode* child;
}
TreeNode;

static TreeNode* _tree = NULL;

static TreeNode* doctree_findNode(const char* start_ptr,
                                  int length,
                                  TreeNode* node)
{
    if (length == 0)
    {
        char* end_ptr = strchr(start_ptr, '/');
        length = (end_ptr == NULL) ? strlen(start_ptr) : end_ptr - start_ptr;
    }

    if (strncmp(start_ptr, node->name, length) == 0)
    {
        if ((start_ptr[length] == '\0') || (start_ptr[length + 1] == '\0'))
        {	// recursion exit point:
            // we reached the end of the input URI and found the node with
            // the same address
            return node;
        }

        //this is still not the end
        // search for the rest of uri
        TreeNode* child = doctree_findNode(start_ptr + length + 1,
                                           0,
                                           node->child);
        // if nothing is found in child nodes than return last found node
        // (may be uri points to somewhere inside this node)
        return (child == NULL) ? node : child;
    }

    // name of current node doesn't match with uri.
    // search in neighbor nodes
    if (node->neighbor == NULL)
    {
        // No place to search left, nothing is found
        return NULL;
    }
    return doctree_findNode(start_ptr, length, node->neighbor);
}

IXML_Element* doctree_get(const char* uri)
{
    // parse folder name
    // assume that the every uri starts with '/obix/', thus shift uri.
    TreeNode* node = doctree_findNode(uri + 6, 0, _tree);

    if (node == NULL)
    {
    	return NULL;
    }

    return NULL;
}

int doctree_put(const char* uri, IXML_Element* data)
{
	// performs put in two phases:
	// first, it finds the node in a tree, which is the closest parent.
	// simultaneously checking that there is no node with the same uri
	// second,
	if (_tree == NULL)
	return 0;
}
