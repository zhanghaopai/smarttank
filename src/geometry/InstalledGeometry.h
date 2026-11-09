#pragma once
#include "core/Project.h"
#include <TopoDS_Shape.hxx>

namespace smarttank {
TopoDS_Shape makeInstalledShape(const InstalledSurface& definition);
}
