#include "../tree/tree.c"

int main() {
    Tree *root   = new_tree("Root");
    Tree *child1 = add_node(root, "Child 1");
    Tree *child2 = add_node(root, "Child 2");
    add_node(child1, "Grandchild 1");
    add_node(child2, "Grandchild 2");

    char *tree_str = print_tree(root);
    printf("%s", tree_str);

    free(tree_str);
    free_tree(root);
    return 0;
}