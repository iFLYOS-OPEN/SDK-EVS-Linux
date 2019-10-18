//
// Created by jan on 18-11-26.
//

#ifndef IFLYOSCLIENTSDK_IFLYOSINTERFACEIMPL_H
#define IFLYOSCLIENTSDK_IFLYOSINTERFACEIMPL_H

#include "IFLYOSInterface.h"

namespace iflyosInterface {

    int IFLYOS_InterfaceOSInit();
    int IFLYOS_InterfaceOSSendEvent(const char *evt, const char *msg = "");

}

#endif //IFLYOSCLIENTSDK_IFLYOSINTERFACEIMPL_H
