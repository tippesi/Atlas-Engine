#include "JobGroup.h"

namespace Atlas {

    JobGroup::JobGroup(JobPriority priority) : priority(priority) {



    }

    JobGroup::~JobGroup() {
        
        AE_ASSERT(HasFinished() && "Job group destructed before every job executed");

    }

    bool JobGroup::HasFinished() {

        return counter.load() == 0;

    }

}