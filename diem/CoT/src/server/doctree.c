/* *****************************************************************************
 * Copyright (c) 2009 Andrey Litvinov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 * ****************************************************************************/
/** @file
 * Implementation of tree storage for XML data.
 *
 * @see doctree.h
 *
 * @author Andrey Litvinov
 */

#include "doctree.h"

#include <string.h>

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
