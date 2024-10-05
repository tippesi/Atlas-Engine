#include "CopyPasteHelper.h"

namespace Atlas::Editor {

    std::type_index CopyPasteHelper::typeInfo = typeid(CopyPasteHelper);

    void* CopyPasteHelper::data = nullptr;
    size_t CopyPasteHelper::elementCount = 0;
    size_t CopyPasteHelper::elementSize = 0;
    std::function<void(void*)> CopyPasteHelper::destructor;

}