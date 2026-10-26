#pragma once

#include "config_manager.h"

namespace ProfileIO
{
bool read(const QString &path, FanProfile *profile, QString *error);
bool write(const QString &path, const FanProfile &profile, QString *error);
} // namespace ProfileIO
