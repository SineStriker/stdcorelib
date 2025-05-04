#include "iterator.h"

namespace stdc {

    /*!
        \class reverse_iterator
        \brief Yet another implementation of std::reverse_iterator.

        The STL reverse_iterator's \c begin actually stores original iterator's \c end. When the
        user requires the reference, it creates a copy of \c end-1 and returns it. This is
        unacceptable for some containers that need to store data in the iterator.

        The \c begin of this reverse_iterator stores the \c end-1 of the original iterator verbatim.
    */

}