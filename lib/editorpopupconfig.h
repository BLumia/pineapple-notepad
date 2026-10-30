#pragma once

#include <QStringList>

namespace EditorPopupConfig {

constexpr int kSchemaVersion = 1;

QStringList items();
void setItems(const QStringList &items);
bool isCustomized();
void clearCustomization();

} // namespace EditorPopupConfig
