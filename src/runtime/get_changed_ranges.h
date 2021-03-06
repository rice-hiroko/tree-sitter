#ifndef RUNTIME_GET_CHANGED_RANGES_H_
#define RUNTIME_GET_CHANGED_RANGES_H_

#include "runtime/tree.h"

unsigned ts_tree_get_changed_ranges(
  Tree *old_tree, Tree *new_tree, TreePath *path1, TreePath *path2,
  const TSLanguage *language, TSRange **ranges
);

#endif  // RUNTIME_GET_CHANGED_RANGES_H_
