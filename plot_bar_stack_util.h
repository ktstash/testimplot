#pragma once
#include "implot.h"

#include <string>
#include <vector>

template <typename T1, typename T2>
void plot_bar_stack(std::string label_id, const T1* bar_length, std::vector<T2> bar_value, int item_count, double group_size, double shift, ImPlotBarGroupsFlags flags);