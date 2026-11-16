#pragma once

#include <initializer_list>

class QSplitter;

namespace aur::theme
{

    void bindResponsiveSplitter(QSplitter *splitter, std::initializer_list<int> weights);

}
