#pragma once
#include <QString>

namespace Autostart
{
QString filePath();
bool enabled();
bool setEnabled(bool enabled, QString *error);
} // namespace Autostart
