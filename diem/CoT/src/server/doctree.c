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

static TreeNode* _tree;

static TreeNode* doctree_findNode(const char* start_ptr, const char* end_ptr, TreeNode* node)
{
    if (length == 0)
    {
    	char* end_ptr = strchr(start_ptr, '/');
    	//-1
    	length = (end_ptr == NULL) ? strlen(start_ptr) : end_ptr - start_ptr;
    }

    if (strncmp(start_ptr, node->name, length);)

    int cmp;
    if (length == -1)
    {
        //no '/' is found. current uri is like "something"
        cmp = strcmp(start_ptr, node->name);
        if (cmp == 0)
        {
            // that "something" from uri match with the name of the node
            // thus the whole uri points to this node.
            return node;
        }
    }
    else
    {
        // current uri is like "something/something-more..."
        // we want to compare only first part of it
        cmp = strncmp(start_ptr, node->name, length);
        if (cmp == 0)
        {
            // name of current node and the part of uri match
            if (start_ptr[length] == '\0')
            {	// recursion exit point:
                // we found node with the address = uri
                return node;
            }
            // search for the rest of uri
            TreeNode* child = doctree_findNode(start_ptr + length, 0, node->child);
            // if nothing is found in child nodes return current one
            // (may be address point somewhere inside the document
            return (child == NULL) ? node : child;
        }
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
    // assume that the first character in uri is always '/obix/'

    char* start_ptr = uri + 6;
    TreeNode* node = _tree;


}

int doctree_put(const char* uri, IXML_Element* data)
{}
