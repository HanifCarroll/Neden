import { describe, expect, it } from "vitest";
import { accountEmail } from "../src/account-email";

describe("account emails", () => {
  it("keeps verification tokens out of visible HTML while preserving both destinations", () => {
    const url =
      "https://api.example.com/verify-email?token=synthetic-token&callbackURL=https%3A%2F%2Fexample.com%2Faccount%2F";
    const message = accountEmail("verification", url);
    const visibleText = message.html.replace(/<[^>]*>/g, "");
    expect(visibleText).toContain("Verify email");
    expect(visibleText).toContain("Open the verification page");
    expect(visibleText).not.toContain("synthetic-token");
    expect(message.html.match(/href="([^"]+)"/g)).toEqual([
      `href="${url.replaceAll("&", "&amp;")}"`,
      `href="${url.replaceAll("&", "&amp;")}"`,
    ]);
    expect(message.text).toContain(url);
  });

  it("uses password-reset wording and escapes URL attribute characters", () => {
    const message = accountEmail(
      "password-reset",
      'https://api.example.com/reset?token="<&',
    );
    expect(message.subject).toBe("Reset your Neden password");
    expect(message.html).toContain(">Reset password</a>");
    expect(message.html).toContain(
      'href="https://api.example.com/reset?token=&quot;&lt;&amp;"',
    );
    expect(message.html).not.toContain(
      'href="https://api.example.com/reset?token="<',
    );
  });
});
