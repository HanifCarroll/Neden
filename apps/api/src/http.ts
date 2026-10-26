import { ServiceError } from "./contracts";
export async function boundedJson(request: Request, limit = 48000) {
  if (!request.headers.get("content-type")?.startsWith("application/json"))
    throw new ServiceError(415, "content_type", "Use JSON for this request.");
  return JSON.parse(await boundedText(request, limit));
}
export async function boundedText(request: Request | Response, limit: number) {
  const reader = request.body?.getReader();
  if (!reader) return "";
  const chunks: Uint8Array[] = [];
  let size = 0;
  try {
    for (;;) {
      const { value, done } = await reader.read();
      if (done) break;
      size += value.length;
      if (size > limit)
        throw new ServiceError(413, "too_large", "This request is too large.");
      chunks.push(value);
    }
  } finally {
    await reader.cancel();
  }
  const bytes = new Uint8Array(size);
  let offset = 0;
  for (const chunk of chunks) {
    bytes.set(chunk, offset);
    offset += chunk.length;
  }
  return new TextDecoder().decode(bytes);
}
