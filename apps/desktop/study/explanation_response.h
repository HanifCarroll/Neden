#pragma once
#include <QString>

namespace ExplanationResponse {
// Render complete fields from a streamed JSON answer. Older Markdown answers pass through.
QString markdown(const QString &response, bool includeReferences = true);
bool isComplete(const QString &response);
}
